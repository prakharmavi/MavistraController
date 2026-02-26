# MavistraController

`MavistraController` is the **standard embedded controller library** for Mavistra educational devices. Any device firmware includes this library to receive remote instructions from a companion mobile app over BLE. The app (not yet built) is designed for kids — it connects to a device and sends named commands. The library handles all BLE transport and dispatches commands to firmware callbacks. The device sends only **connection status** back to the app over TX.

## Goals

- Be the standard, reusable library that any Mavistra device firmware includes.
- Allow a kid-facing mobile app to discover, connect, and send simple named commands.
- Dispatch app commands to registered firmware callbacks with minimal boilerplate.
- Send connection status (connected / disconnected) back to the app on TX — nothing else.
- Keep BLE overhead low to preserve CPU, RAM, and flash for device-specific logic.

## Current Status

**Active development — BLE transport is working. Command dispatch is not yet implemented.**

What works today:
- BLE advertising under a configurable device name.
- Client connect / disconnect detection with Serial logging.
- Automatic re-advertising after a client disconnects.
- Raw write reception on the RX characteristic (echoed back on TX as `"recieved <data>"`).
- `isConnected()` state tracking via `loop()`.

What is not yet implemented:
- Command protocol parsing and validation.
- Callback-based command dispatcher (`onCommand`).
- Connection status notification to app on TX (connected / disconnected).
- Light session security for classroom environments.

## Hardware Target

| Field     | Value                              |
|-----------|------------------------------------|
| Board     | uPesy WROVER (ESP32)               |
| Framework | Arduino                            |
| Flash     | 4 MB                               |
| RAM       | 320 KB internal SRAM               |
| BLE stack | `h2zero/NimBLE-Arduino`            |

NimBLE is chosen over the legacy ESP32 BLE stack for lower memory and flash footprint.

## BLE Service Definition

| Role            | UUID                                   | Properties          |
|-----------------|----------------------------------------|---------------------|
| Service         | `19b10000-e8f2-537e-4f6c-d104768a1214` | —                   |
| RX (app → device) | `19b10001-e8f2-537e-4f6c-d104768a1214` | WRITE, WRITE_NR     |
| TX (device → app) | `19b10002-e8f2-537e-4f6c-d104768a1214` | READ, NOTIFY        |

The mobile app writes commands to the RX characteristic. The device publishes events and responses by notifying the TX characteristic.

## Public API

```cpp
class MavistraController {
public:
  explicit MavistraController(const char* advertisingName);

  bool begin();        // Call once from setup()
  void loop();         // Call continuously from loop()
  bool isConnected() const;
  void reset();        // Tears down BLE and resets state
};
```

`begin()` initializes NimBLE, registers the command service and both characteristics, and starts advertising. Returns `false` on any initialization failure.

`loop()` polls connection state and restarts advertising automatically after a client disconnects.

## Usage

```cpp
#include <MavistraController.h>

MavistraController controller("MavistraCtrl");

void setup() {
  Serial.begin(115200);
  delay(300);
  controller.begin();
}

void loop() {
  controller.loop();
}
```

### Serial output (115200 baud)

```
[MavistraController] Initializing BLE as: MavistraCtrl
[MavistraController] Advertising started
[MavistraController] BLE client connected
[MavistraController] RX raw: hello
[MavistraController] TX: recieved hello
[MavistraController] BLE client disconnected
[MavistraController] Advertising restarted
```

## Planned Use Case (Target Flow)

1. Device boots and calls `controller.begin()` — BLE advertising starts.
2. Kid opens the mobile app; app discovers the device by name or service UUID and connects.
3. Library notifies the app of connection status on TX.
4. Kid taps a button in the app; app writes a named command frame to the RX characteristic.
5. Library parses the command and triggers the registered firmware callback.
6. Device executes the action (motor, LED, etc.) — no payload is sent back to the app.

## Command Protocol (Planned)

Command frame (app → device):

| Field     | Description                          |
|-----------|--------------------------------------|
| `id`      | Unique request ID from the app       |
| `cmd`     | Command name                         |
| `payload` | Command data (JSON / string / binary, TBD) |

Response frame (device → app):

| Field     | Description            |
|-----------|------------------------|
| `id`      | Mirrored request ID    |
| `ok`      | Success / failure flag |
| `code`    | Status or error code   |
| `payload` | Result data            |

## Planned API Extensions

```cpp
// Register a handler for a named command sent from the mobile app
void onCommand(const char* commandName, void (*handler)(const char* payload));
```

`sendEvent` and `sendStatus` are **not** part of this library's scope. The only device-to-app communication is connection status, which the library manages internally on the TX characteristic.

## Security Direction (Planned)

Designed for a classroom environment — not enterprise security, but enough to prevent accidental cross-device interference:
- Simple shared token or device name allowlist so only the intended app connects.
- Input validation and command schema checks to prevent malformed frames crashing firmware.

## Scope

**Library responsibilities:**
- BLE transport setup and lifecycle.
- Command protocol parsing and validation.
- Command dispatch to application handlers.
- Response and event publishing.
- Connection state tracking.
- Error codes and diagnostics.

**Application responsibilities (out of scope):**
- Device-specific business logic (motor, LED, sensor control).
- Mobile app UI/UX — that is the companion app's domain.
- Sending arbitrary data back to the app; the library only communicates connection status.

## Roadmap

- [x] BLE transport with NimBLE-Arduino.
- [x] Connect / disconnect state tracking and Serial diagnostics.
- [x] Auto-restart advertising after disconnect.
- [x] Raw RX write receive and TX echo.
- [ ] Define command protocol v1 — named commands with optional string payload.
- [ ] Callback-based command dispatcher (`onCommand`).
- [ ] Connection status notification on TX (connected / disconnected).
- [ ] Document BLE contract (UUIDs + frame schema) for the companion mobile app.
- [ ] Light classroom security (simple token or device name allowlist).
- [ ] Example device sketch and mobile integration guide.

## Development Setup

Requires PlatformIO with the `espressif32` platform.

```bash
pio run
```

Monitor serial output:

```bash
pio device monitor
```

`platformio.ini`:

```ini
[env:upesy_wrover]
platform = espressif32
board = upesy_wrover
framework = arduino
monitor_speed = 115200
lib_deps =
  h2zero/NimBLE-Arduino
```

## Resource Notes (ESP32 uPesy WROVER)

Memory use varies with:
- Number of simultaneous BLE connections
- MTU size
- Number of GATT services and characteristics
- NimBLE logging level

NimBLE is significantly lighter than the legacy Arduino BLE stack on ESP32; exact footprint depends on build configuration.

## Contributing

Contributions are welcome while the API is in development:
- Propose command schema changes via issues/PRs.
- Keep the public API minimal and stable.
- Include memory impact notes with any BLE-related changes.

## License

TBD.
