# Mavistra — Mobile App BLE Reference

This document is the complete contract between the mobile app and any Mavistra device. The device firmware uses `MavistraController` which implements this spec exactly.

---

## 1. Discovery

Scan for BLE peripherals advertising the Mavistra service UUID:

```
Service UUID: 19b10000-e8f2-537e-4f6c-d104768a1214
```

The device name is included in the scan response (e.g. `"Vanshaj's Bot"`). You can filter by either. Connect to the first matching peripheral found, or let the user pick from a list.

---

## 2. Characteristics

| Role | UUID | Properties | Direction |
|------|------|------------|-----------|
| RX | `19b10001-e8f2-537e-4f6c-d104768a1214` | WRITE, WRITE\_NO\_RESPONSE | App → Device |
| TX | `19b10002-e8f2-537e-4f6c-d104768a1214` | READ, NOTIFY | Device → App |

After connecting:
1. **Subscribe to NOTIFY** on the TX characteristic to receive status events.
2. **Write to RX** to send button commands.

---

## 3. Sending Commands (App → Device)

### Wire format

```
NAME\n
```

or with an optional payload for configurable buttons:

```
NAME:payload\n
```

- UTF-8 encoded
- Newline `\n` terminated
- Max recommended frame size: 20 bytes (fits in a single BLE MTU)

### Heartbeat model

The device uses a **heartbeat** to determine button state — there is no explicit button-up event.

| State | What the app does |
|-------|-------------------|
| Button held | Send the frame repeatedly, every **50 ms** |
| Button released | Stop sending — device auto-releases after **300 ms** of silence |

This means:
- A single write = a brief tap (active for up to 300 ms)
- Continuous writes at 50 ms = held button
- Disconnect = all buttons instantly released on the device

### Standard command names

#### Left D-pad
| Button | Command |
|--------|---------|
| Up | `L_UP` |
| Down | `L_DOWN` |
| Left | `L_LEFT` |
| Right | `L_RIGHT` |
| Center | `L_CENTER` |

#### Right D-pad
| Button | Command |
|--------|---------|
| Up | `R_UP` |
| Down | `R_DOWN` |
| Left | `R_LEFT` |
| Right | `R_RIGHT` |
| Center | `R_CENTER` |

#### Middle configurable buttons
| Button | Command | Payload |
|--------|---------|---------|
| Button 1 | `BTN_1` | optional |
| Button 2 | `BTN_2` | optional |
| Button 3 | `BTN_3` | optional |
| Button 4 | `BTN_4` | optional |

The configurable buttons accept an optional string payload after `:`, e.g. `BTN_1:boost\n`. The device firmware can read this payload. If unused, send without payload: `BTN_1\n`.

---

## 4. Receiving Status (Device → App)

Subscribe to NOTIFY on the TX characteristic. The device sends one of two messages:

| Event | Value |
|-------|-------|
| App connected | `status:connected` |
| App disconnected | `status:disconnected` |

These are UTF-8 strings. No other data is sent from device to app — the device does not push sensor readings, button state, or arbitrary events.

---

## 5. Connection Lifecycle

```
App                              Device
 |                                  |
 |------- BLE scan ---------------→ |  (advertising continuously)
 |←------ advertisement --------   |  (includes service UUID + device name)
 |------- connect ----------------→ |
 |←------ "status:connected" ----  |  (TX notify)
 |                                  |
 |------- "L_UP\n" every 50ms ---→ |  (while left stick held up)
 |------- "L_UP\n" every 50ms ---→ |
 |        (stop sending)            |  → device auto-releases after 300ms
 |                                  |
 |------- disconnect -------------→ |
 |←------ "status:disconnected" -  |  (TX notify)
 |                                  |  (device restarts advertising)
```

---

## 6. React Native Integration

Library: [`react-native-ble-plx`](https://github.com/dotintent/react-native-ble-plx)

```bash
npx expo install react-native-ble-plx
```

### Constants

```ts
const SERVICE_UUID  = '19b10000-e8f2-537e-4f6c-d104768a1214';
const RX_UUID       = '19b10001-e8f2-537e-4f6c-d104768a1214';
const TX_UUID       = '19b10002-e8f2-537e-4f6c-d104768a1214';
```

### Scanning and connecting

```ts
import { BleManager, Device } from 'react-native-ble-plx';
import { Buffer } from 'buffer';

const manager = new BleManager();

function startScan(onFound: (device: Device) => void) {
  manager.startDeviceScan([SERVICE_UUID], null, (error, device) => {
    if (error || !device) return;
    onFound(device);
  });
}

async function connect(device: Device) {
  const connected = await device.connect();
  await connected.discoverAllServicesAndCharacteristics();

  // Subscribe to TX status notifications
  connected.monitorCharacteristicForService(SERVICE_UUID, TX_UUID, (error, char) => {
    if (error || !char?.value) return;
    const msg = Buffer.from(char.value, 'base64').toString('utf-8');
    if (msg === 'status:connected')    { /* update UI */ }
    if (msg === 'status:disconnected') { /* update UI */ }
  });

  return connected;
}
```

### Sending commands

```ts
function encodeFrame(name: string, payload?: string): string {
  const frame = payload ? `${name}:${payload}\n` : `${name}\n`;
  return Buffer.from(frame, 'utf-8').toString('base64');
}

// Write without response — faster, no ACK needed for heartbeat
function sendCommand(device: Device, name: string, payload?: string) {
  device.writeCharacteristicWithoutResponseForService(
    SERVICE_UUID, RX_UUID, encodeFrame(name, payload)
  );
}
```

### Heartbeat loop

Run this while a button is held; clear the interval on release.

```ts
const intervals = new Map<string, ReturnType<typeof setInterval>>();

function onButtonDown(device: Device, command: string, payload?: string) {
  sendCommand(device, command, payload);                   // immediate first send
  intervals.set(command, setInterval(() => {
    sendCommand(device, command, payload);
  }, 50));
}

function onButtonUp(command: string) {
  const id = intervals.get(command);
  if (id !== undefined) {
    clearInterval(id);
    intervals.delete(command);
  }
  // No explicit "release" frame — device times out after 300 ms.
}
```

### Disconnect and cleanup

```ts
async function disconnect(device: Device) {
  await device.cancelConnection();
}

// Call when the screen unmounts
function cleanup() {
  manager.destroy();
}
```

---

## 7. UUIDs at a Glance

```
Service  19b10000-e8f2-537e-4f6c-d104768a1214
RX       19b10001-e8f2-537e-4f6c-d104768a1214
TX       19b10002-e8f2-537e-4f6c-d104768a1214
```

---

## 8. Quick Checklist

- [ ] Scan filtered by service UUID (not just device name — name can vary per device)
- [ ] Subscribe to TX NOTIFY immediately after connecting
- [ ] Send heartbeats every **50 ms** while a button is held
- [ ] Stop sending to release — do **not** send a button-up frame
- [ ] Handle `status:disconnected` notify to update UI
- [ ] Restart scan after disconnect (device re-advertises automatically)
