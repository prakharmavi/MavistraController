# MavistraController

A standard BLE controller library for ESP32-based Mavistra devices, paired with a kid-facing mobile app. Firmware authors include this library and get a working BLE gamepad interface — no BLE knowledge required.

## What is Mavistra?

Mavistra is an educational platform where kids control physical devices (robots, LED rigs, etc.) through a simple mobile app. The app has two D-pads and four configurable buttons. The device reacts in real time.

This repository is the **device-side firmware library**. The companion mobile app is a separate project.

## Repository Layout

```
lib/MavistraController/       # The reusable library (include this in any device firmware)
  src/
    MavistraController.h      # Public API
    MavistraController.cpp    # BLE implementation (NimBLE)
  README.md                   # Library API reference
  library.json                # PlatformIO registry manifest

src/main.cpp                  # Example sketch — LED bar + buzzer demo on uPesy WROVER

docs/
  MOBILE_APP.md               # BLE contract reference for the mobile app team

platformio.ini                # PlatformIO build config (espressif32, huge_app partition)
```

## How It Works

```
Mobile App  ──BLE──►  MavistraController  ──callbacks──►  Device firmware
                            │
                            └──►  TX notify: connection status
```

1. Device boots, `controller.begin()` starts BLE advertising.
2. App scans for the Mavistra service UUID and connects.
3. App sends named command frames to the RX characteristic while buttons are held (heartbeat pattern — 50 ms interval).
4. Firmware polls `controller.isActive("L_UP")` etc. in its `loop()` to drive motors, LEDs, or anything else.
5. Device notifies the app of connect/disconnect state on the TX characteristic.

## Using the Library

```cpp
#include <MavistraController.h>

MavistraController controller("MyRobot");

void setup() {
  Serial.begin(115200);
  controller.begin();
}

void loop() {
  controller.loop();
  motor.setForward(controller.isActive("L_UP"));
  motor.setBack(controller.isActive("L_DOWN"));
}
```

Via `platformio.ini`:
```ini
lib_deps = https://github.com/prakharmavi/MavistraController.git
```

## Hardware

- **Board:** uPesy WROVER (ESP32)
- **BLE stack:** `h2zero/NimBLE-Arduino`
- **Example peripherals:** 10-LED bar (GPIO 4, 5, 18, 19, 21, 22, 23, 25, 26, 27) + passive buzzer (GPIO 15)

## Mobile App

The app is built in React Native. See [`docs/MOBILE_APP.md`](docs/MOBILE_APP.md) for the full BLE contract — UUIDs, wire format, command names, heartbeat protocol, and React Native code snippets.

## Roadmap

- [x] BLE transport (NimBLE)
- [x] Heartbeat command model with `isActive()` polling
- [x] TX connection status notifications
- [x] LED bar + buzzer example sketch
- [x] Mobile app BLE reference
- [ ] React Native companion app
- [ ] Classroom security (device pairing / allowlist)

## License

MIT
