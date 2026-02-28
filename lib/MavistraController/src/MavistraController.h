#pragma once

#include <Arduino.h>
#include <map>
#include <string>

class NimBLEServer;
class NimBLEService;
class NimBLECharacteristic;

/**
 * @brief BLE-capable controller facade for Mavistra devices.
 *
 * This class is the public entry point for initializing controller transport,
 * processing runtime communication, and exposing connection state to firmware.
 *
 * ## Singleton usage
 *
 * Only one instance may exist. Obtain it via `instance()`:
 *
 * ```cpp
 * // In setup() — pass the advertising name once:
 * MavistraController::instance("MyDevice").begin();
 *
 * // Everywhere else — omit the name:
 * MavistraController::instance().loop();
 * bool active = MavistraController::instance().isActive("L_UP");
 * ```
 *
 * Constructing directly is a compile error:
 * ```cpp
 * MavistraController ctrl("x");   // ❌ error: constructor is private
 * ```
 *
 * ## Command model
 *
 * The mobile app sends named command frames over BLE RX while a button is held
 * (heartbeat pattern). Any received command is tracked automatically — no
 * registration required. Firmware polls `isActive()` in `loop()` to read
 * button state, exactly like `digitalRead()`.
 *
 * Wire format: `NAME\n` or `NAME:payload\n`
 *
 * Standard command names:
 *   L_UP  L_DOWN  L_LEFT  L_RIGHT  L_CENTER
 *   R_UP  R_DOWN  R_LEFT  R_RIGHT  R_CENTER
 *   BTN_1  BTN_2  BTN_3  BTN_4
 */
class MavistraController {
 public:
  /**
   * @brief Construct the controller with a BLE advertising name.
   *
   * Only one instance may exist per program. Constructing a second one aborts
   * immediately with a Serial message — caught at development time the moment
   * the firmware boots.
   *
   * @param advertisingName Device name presented during BLE advertising.
   */
  explicit MavistraController(const char* advertisingName);

  /**
   * @brief Release any allocated controller resources.
   */
  ~MavistraController();

  MavistraController(const MavistraController&)            = delete;
  MavistraController(MavistraController&&)                 = delete;
  MavistraController& operator=(const MavistraController&) = delete;
  MavistraController& operator=(MavistraController&&)      = delete;

  /**
   * @brief Initialize the controller and prepare communication transport.
   *
   * Call once from `setup()`.
   *
   * @return `true` if initialization succeeds, otherwise `false`.
   */
  bool begin();

  /**
   * @brief Process controller runtime tasks.
   *
   * Call continuously from Arduino `loop()`. Drives the heartbeat timeout
   * sweep that marks timed-out commands as inactive.
   */
  void loop();

  /**
   * @brief Report whether a remote client is currently connected.
   * @return `true` when connected, otherwise `false`.
   */
  bool isConnected() const;

  /**
   * @brief Reset runtime state and release active resources.
   */
  void reset();

  /**
   * @brief Report whether a named command is currently active.
   *
   * A command is active while the app continues to send heartbeat frames for
   * it within the timeout window. Returns `false` when the button is released
   * (frames stop arriving) or no frame for this name has ever been received.
   *
   * @param name Command name (e.g. "L_UP", "BTN_1").
   * @return `true` if a frame was received within the timeout window.
   */
  bool isActive(const char* name) const;

  /**
   * @brief Override the heartbeat timeout used to determine command release.
   *
   * Call before `begin()`. Default is 150 ms. The app should send heartbeats
   * at an interval well below this value (e.g. every 50 ms).
   *
   * @param ms Timeout in milliseconds.
   */
  void setCommandTimeout(uint32_t ms);

 /// Internal per-command heartbeat state. Exposed for use by BLE callbacks in
  /// the implementation file; not intended for use by firmware authors.
  struct CommandEntry {
    uint32_t lastSeenMs; ///< Timestamp of the most recent received frame.
    bool     active;     ///< True while within the timeout window.
  };

 private:
  // ---- heartbeat timeout configuration ----
  // commandTimeoutMs_ is the sole backing store for setCommandTimeout().
  static constexpr uint32_t kDefaultCommandTimeoutMs = 150U;
  uint32_t commandTimeoutMs_;

  // ---- command heartbeat tracking ----
  std::map<std::string, CommandEntry> commands_;

  // ---- BLE state ----
  bool initialized_;
  bool connected_;
  bool lastLoggedConnected_;
  uint32_t lastCommandMs_;
  char advertisingName_[32];
  bool bleActive_;
  NimBLEServer* server_;
  NimBLEService* commandService_;
  NimBLECharacteristic* rxCommandCharacteristic_;
  NimBLECharacteristic* txEventCharacteristic_;

  /**
   * @brief Free or detach owned runtime resources.
   */
  void release();

  /**
   * @brief Clear active state on all tracked commands immediately.
   */
  void clearAllActive();
};
