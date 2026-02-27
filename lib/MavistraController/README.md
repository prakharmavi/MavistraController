# MavistraController

`MavistraController` is the **standard embedded controller library** for Mavistra educational devices. Any device firmware includes this library to receive remote instructions from a companion mobile app over BLE. The app is designed for kids — it connects to a device and sends named commands. The library handles all BLE transport. The device sends only **connection status** back to the app over TX.

## Hardware Target

| Field     | Value                           |
|-----------|---------------------------------|
| Board     | uPesy WROVER (ESP32)            |
| Framework | Arduino via PlatformIO          |
| Flash     | 4 MB (`huge_app.csv` partition) |
| RAM       | 320 KB internal SRAM            |
| BLE stack | `h2zero/NimBLE-Arduino`         |

## Public API

```cpp
MavistraController controller("DeviceName");

controller.begin();                  // call once from setup()
controller.loop();                   // call continuously from loop()
controller.isConnected();            // true when a client is connected
controller.isActive("L_UP");         // true while app is sending this command
controller.setCommandTimeout(150);   // ms before an unrefreshed command goes inactive
controller.reset();                  // tear down BLE and reset all state
```

### Command model — heartbeat

The app sends a named frame repeatedly (e.g. every 50 ms) while a button is held. The library tracks when each command was last received. `isActive()` returns `true` while frames keep arriving and `false` after the timeout window passes with no frame — no explicit button-up event needed. All commands clear immediately on BLE disconnect.

```cpp
void loop() {
  controller.loop();
  motor.setForward(controller.isActive("L_UP"));
  motor.setBack(controller.isActive("L_DOWN"));
}
```

## BLE Contract

### Service and characteristics

| Role              | UUID                                   | Properties      |
|-------------------|----------------------------------------|-----------------|
| Service           | `19b10000-e8f2-537e-4f6c-d104768a1214` | —               |
| RX (app → device) | `19b10001-e8f2-537e-4f6c-d104768a1214` | WRITE, WRITE_NR |
| TX (device → app) | `19b10002-e8f2-537e-4f6c-d104768a1214` | READ, NOTIFY    |

### RX frame format (app → device)

```
NAME\n
NAME:payload\n
```

- `NAME` — command name (see standard names below). Case-sensitive.
- `payload` — optional UTF-8 string after `:`. Used by configurable buttons.
- Trailing `\r` is stripped. Frames with an empty name are ignored.
- Unknown names are logged to Serial and silently dropped.

### Standard command names

| Input              | Name       |
|--------------------|------------|
| Left D-pad up      | `L_UP`     |
| Left D-pad down    | `L_DOWN`   |
| Left D-pad left    | `L_LEFT`   |
| Left D-pad right   | `L_RIGHT`  |
| Left D-pad center  | `L_CENTER` |
| Right D-pad up     | `R_UP`     |
| Right D-pad down   | `R_DOWN`   |
| Right D-pad left   | `R_LEFT`   |
| Right D-pad right  | `R_RIGHT`  |
| Right D-pad center | `R_CENTER` |
| Middle button 1    | `BTN_1`    |
| Middle button 2    | `BTN_2`    |
| Middle button 3    | `BTN_3`    |
| Middle button 4    | `BTN_4`    |

The command set is open-ended — any string not in the table above is tracked and available via `isActive()`.

### TX notifications (device → app)

| Event         | Value                  |
|---------------|------------------------|
| Connected     | `status:connected`     |
| Disconnected  | `status:disconnected`  |

Subscribe to NOTIFY on the TX characteristic to receive these. No other data is sent from device to app.

## Usage

```cpp
#include <MavistraController.h>

MavistraController controller("MyRobot");

void setup() {
  Serial.begin(115200);
  delay(300);
  controller.begin();
}

void loop() {
  controller.loop();
  // poll any command by name
  motor.setForward(controller.isActive("L_UP"));
}
```

### Serial output (115200 baud)

```
[MavistraController] Initializing BLE as: MyRobot
[MavistraController] Advertising started
[MavistraController] BLE client connected
[MavistraController] cmd: L_UP
[MavistraController] unknown command: JUMP
[MavistraController] BLE client disconnected
[MavistraController] Advertising restarted
```

## Development Setup

```ini
; platformio.ini
[env:upesy_wrover]
platform = espressif32
board = upesy_wrover
board_build.partitions = huge_app.csv
framework = arduino
monitor_speed = 115200
lib_deps =
  h2zero/NimBLE-Arduino
```

```bash
pio run                 # build
pio run --target upload # flash
pio device monitor      # serial monitor
```

## Roadmap

- [x] BLE transport with NimBLE-Arduino
- [x] Connect / disconnect state tracking and Serial diagnostics
- [x] Auto-restart advertising after disconnect
- [x] Command protocol v1 — heartbeat-based named commands with optional payload
- [x] `isActive()` command state polling
- [x] Connection status notification on TX (`status:connected` / `status:disconnected`)
- [x] BLE contract documentation
- [ ] Example device sketch and mobile integration guide

## License

TBD.
