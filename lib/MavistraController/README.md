# MavistraController

`MavistraController` is an embedded controller library for ESP32-based devices that connects to a mobile app over BLE and exposes a clean command interface for downstream product features.

## Goals

- Provide a stable controller API for firmware developers.
- Allow a mobile app to securely connect and send commands.
- Translate app commands into structured actions inside firmware.
- Keep BLE overhead low to preserve CPU, RAM, and flash for product logic.

## Planned Use Case

1. Device boots and starts `MavistraController`.
2. Mobile app discovers and connects to the device over BLE.
3. App sends commands 1,2,3,4,5,6,7,8,9,10 to the device.
4. Library validates/parses command payloads.
5. Library triggers registered firmware callbacks.
6. Firmware returns status/events back to the app.

## Scope (Library Responsibilities)

- BLE transport setup and lifecycle.
- Command protocol parsing and validation.
- Command dispatch to application handlers.
- Response and event publishing.
- Connection state tracking.
- Error codes and diagnostics.

## Out of Scope (Application Responsibilities)

- Business logic for device-specific behavior.
- Motor/sensor/actuator control implementation.
- Product policy decisions (authorization rules, feature gates).
- Mobile app UI/UX.

## Connectivity Choice

- BLE stack: `NimBLE-Arduino` (`h2zero/NimBLE-Arduino`)
- Reason: lower memory and flash overhead than legacy ESP32 BLE options.

`platformio.ini`:

```ini
lib_deps =
  h2zero/NimBLE-Arduino
```

## Current Status

Early draft stage. API and wire protocol are still being defined.

## Proposed API Direction (Draft)

```cpp
class MavistraController {
public:
  bool begin();
  void loop();
  bool isConnected() const;

  void onCommand(const char* commandName, void (*handler)(const char* payload));
  void sendEvent(const char* eventName, const char* payload);
  void sendStatus(int code, const char* message);
};
```

This is a direction draft, not final API.

## Command Model (Draft)

- Command frame:
  - `id`: unique request id from app
  - `cmd`: command name
  - `payload`: command data (JSON/string/binary, TBD)
- Response frame:
  - `id`: mirrored request id
  - `ok`: success/failure
  - `code`: status/error code
  - `payload`: result data

## Resource Planning Notes (ESP32 uPesy WROVER)

- Board baseline: 4 MB flash, 320 KB internal RAM.
- Bluetooth enablement reserves part of internal DRAM on ESP32.
- NimBLE is chosen to reduce BLE footprint versus older BLE stacks.

Memory use depends on:
- Central vs peripheral role
- Number of simultaneous connections
- MTU size
- Number of services/characteristics
- Logging level and debug options

## Security Direction (Draft)

- Pairing/bonding support for trusted app-device relationships.
- Optional app-level authentication token for command channel.
- Input validation and strict command schema checks.
- Rate limiting for malformed or excessive requests.

## Roadmap

1. Define command protocol v1.
2. Implement minimal BLE service with connect/send/receive.
3. Add callback-based command dispatcher.
4. Add structured status/event responses.
5. Add security hardening (pairing + app auth layer).
6. Publish example firmware and mobile integration guide.

## Development Setup

Project uses PlatformIO.

```bash
pio run
```

If `pio` is not available in shell, install PlatformIO Core first.

## Contributing

Contributions are welcome while the API is in draft:
- Propose command schema changes via issues/PRs.
- Keep the public API minimal and stable.
- Include examples and memory impact notes with BLE-related changes.

## License

TBD.
