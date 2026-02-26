#pragma once

#include <Arduino.h>

class NimBLEServer;
class NimBLEService;
class NimBLECharacteristic;

/**
 * @brief BLE-capable controller facade for Mavistra devices.
 *
 * This class is the public entry point for initializing controller transport,
 * processing runtime communication, and exposing connection state to firmware.
 */
class MavistraController {
 public:
  /**
   * @brief Construct a controller with a BLE advertising name.
   * @param advertisingName Device name presented during BLE advertising.
   */
  explicit MavistraController(const char* advertisingName);

  /**
   * @brief Release any allocated controller resources.
   */
  ~MavistraController();

  /**
   * @brief Copy constructor is disabled to prevent duplicating runtime state.
   */
  MavistraController(const MavistraController& other) = delete;

  /**
   * @brief Move constructor is disabled to keep ownership stable.
   */
  MavistraController(MavistraController&& other) = delete;

  /**
   * @brief Copy assignment is disabled to prevent state aliasing.
   */
  MavistraController& operator=(const MavistraController& other) = delete;

  /**
   * @brief Move assignment is disabled to keep ownership stable.
   */
  MavistraController& operator=(MavistraController&& other) = delete;

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
   * Call continuously from Arduino `loop()`.
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

 private:
  /**
   * @brief Free or detach owned runtime resources.
   */
  void release();

  /// True once `begin()` has completed successfully.
  bool initialized_;

  /// True when an app/client connection is currently active.
  bool connected_;

  /// Last connection state logged to Serial.
  bool lastLoggedConnected_;

  /// Timestamp of the last command/activity in milliseconds.
  uint32_t lastCommandMs_;

  /// BLE advertising name buffer (null-terminated).
  char advertisingName_[32];

  /// True when NimBLE runtime has been initialized and advertising started.
  bool bleActive_;

  /// BLE server instance.
  NimBLEServer* server_;

  /// Primary command service.
  NimBLEService* commandService_;

  /// RX command characteristic (app -> device).
  NimBLECharacteristic* rxCommandCharacteristic_;

  /// TX event/status characteristic (device -> app).
  NimBLECharacteristic* txEventCharacteristic_;
};
