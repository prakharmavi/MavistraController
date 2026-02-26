# MavistraController — Project Guide

## Project Overview

Educational embedded firmware library for ESP32 devices. It is the **standard controller library** included in any Mavistra-compatible device firmware. A companion mobile app (not yet built), designed for kids, discovers the device over BLE, connects, and sends named commands. The library dispatches those commands to firmware callbacks. The only feedback the device sends back to the app over TX is **connection status** — no arbitrary events or status payloads.

## Hardware / Toolchain

- **Board:** uPesy WROVER (ESP32)
- **Framework:** Arduino via PlatformIO
- **BLE stack:** `h2zero/NimBLE-Arduino` (lower overhead than legacy ESP32 BLE)
- **Serial baud:** 115200
- **Build:** `pio run`
- **Monitor:** `pio device monitor`

## Repository Layout

```
src/main.cpp                         # Minimal Arduino sketch (setup + loop)
lib/MavistraController/src/
  MavistraController.h               # Public API header
  MavistraController.cpp             # BLE implementation
lib/MavistraController/README.md     # Library documentation
platformio.ini                       # PlatformIO build config
```

## Current Implementation State

The BLE transport layer is working. Command dispatch is **not yet implemented**.

**Working:**
- BLE advertising with configurable device name
- Client connect / disconnect detection (logged to Serial)
- Auto-restart advertising after disconnect
- Raw write echo: RX write → TX notify `"recieved <data>"`
- `isConnected()` polling via `loop()`

**Not yet implemented:**
- Command protocol parsing (`onCommand` dispatcher)
- Connection status notification to app over TX (connected / disconnected)
- Light security for classroom use (e.g. simple shared token or device name allowlist)

## BLE UUIDs

| Role              | UUID                                   |
|-------------------|----------------------------------------|
| Service           | `19b10000-e8f2-537e-4f6c-d104768a1214` |
| RX (app → device) | `19b10001-e8f2-537e-4f6c-d104768a1214` |
| TX (device → app) | `19b10002-e8f2-537e-4f6c-d104768a1214` |

## Public API

```cpp
MavistraController controller("DeviceName");
controller.begin();      // call from setup()
controller.loop();       // call from loop()
controller.isConnected();
controller.reset();
```

## Code Conventions

- C++ class, no global state — single `MavistraController` instance in `main.cpp`
- All members initialized in constructor initializer list
- Copy and move constructors/assignments are `= delete`
- Private helpers in anonymous namespace in `.cpp`
- Serial log prefix: `[MavistraController]`
- All pointer members set to `nullptr` after release
- `strncpy` with explicit null terminator for `advertisingName_` buffer (32 bytes)

## Educational Context

- **Target users:** Kids operating physical devices through a simple mobile app.
- **Mobile app:** Not yet created. This library defines the BLE protocol the app must speak.
- **Standard library role:** Any Mavistra device firmware `#include`s this library and registers named command handlers — no BLE knowledge required from the device author.
- **Design principle:** Keep commands simple and named. A button press in the app fires one named callback in firmware. TX carries only connection status; there are no arbitrary push notifications from device to app.

## Planned Next Steps (Roadmap)

1. Define command protocol v1 — simple named commands with an optional string payload (human-readable, easy to debug over Serial).
2. Implement `onCommand(name, handler)` callback dispatcher.
3. Implement connection status notification on TX (connected / disconnected) so the app knows device state.
4. Document the BLE contract (UUIDs + frame schema) for the companion mobile app team.
5. Add light session security suitable for a classroom environment (simple shared token or device name allowlist).
6. Publish an example device sketch and mobile integration guide.
