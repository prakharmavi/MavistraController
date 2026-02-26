#include "MavistraController.h"
#include <NimBLEDevice.h>
#include <string.h>
#include <string>

namespace {
constexpr const char* kServiceUuid    = "19b10000-e8f2-537e-4f6c-d104768a1214";
constexpr const char* kRxCommandUuid  = "19b10001-e8f2-537e-4f6c-d104768a1214";
constexpr const char* kTxEventUuid    = "19b10002-e8f2-537e-4f6c-d104768a1214";

constexpr const char* kStatusConnected    = "status:connected";
constexpr const char* kStatusDisconnected = "status:disconnected";

// ---------------------------------------------------------------------------
// Server callbacks — forward connect/disconnect to the controller via a raw
// pointer. The controller outlives all BLE callbacks because NimBLE is torn
// down in release() before the controller is destroyed.
// ---------------------------------------------------------------------------
class MavistraServerCallbacks : public NimBLEServerCallbacks {
 public:
  explicit MavistraServerCallbacks(NimBLECharacteristic* txCharacteristic)
      : txCharacteristic_(txCharacteristic) {}

  void onConnect(NimBLEServer* server) {
    (void)server;
    if (txCharacteristic_ != nullptr) {
      txCharacteristic_->setValue(kStatusConnected);
      txCharacteristic_->notify();
    }
    Serial.println("[MavistraController] BLE client connected");
  }

  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) {
    (void)connInfo;
    onConnect(server);
  }

  void onDisconnect(NimBLEServer* server) {
    (void)server;
    if (txCharacteristic_ != nullptr) {
      txCharacteristic_->setValue(kStatusDisconnected);
      txCharacteristic_->notify();
    }
    Serial.println("[MavistraController] BLE client disconnected");
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) {
    (void)connInfo;
    (void)reason;
    onDisconnect(server);
  }

 private:
  NimBLECharacteristic* txCharacteristic_;
};

// ---------------------------------------------------------------------------
// RX characteristic callbacks — parse incoming frames and update command state.
//
// Frame format: NAME\n  or  NAME:payload\n
// Trailing \r\n is stripped. Split on first ':' to separate name from payload.
// ---------------------------------------------------------------------------
class MavistraRxCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit MavistraRxCallbacks(std::map<std::string, MavistraController::CommandEntry>* commands)
      : commands_(commands) {}

  void onWrite(NimBLECharacteristic* characteristic) {
    if (characteristic == nullptr || commands_ == nullptr) {
      return;
    }

    std::string raw = characteristic->getValue();
    if (raw.empty()) {
      return;
    }

    // Strip trailing CR / LF.
    while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r')) {
      raw.pop_back();
    }
    if (raw.empty()) {
      return;
    }

    // Split on first ':'.
    std::string name;
    const auto colonPos = raw.find(':');
    if (colonPos == std::string::npos) {
      name = raw;
    } else {
      name = raw.substr(0U, colonPos);
    }

    if (name.empty()) {
      return;
    }

    // Update heartbeat state. Insert a new entry if this name is new.
    auto& entry = (*commands_)[name];
    entry.lastSeenMs = static_cast<uint32_t>(millis());
    entry.active = true;

    Serial.print("[MavistraController] cmd: ");
    Serial.println(name.c_str());
  }

  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) {
    (void)connInfo;
    onWrite(characteristic);
  }

 private:
  std::map<std::string, MavistraController::CommandEntry>* commands_;
};
}  // namespace

// ---------------------------------------------------------------------------
// MavistraController
// ---------------------------------------------------------------------------

MavistraController::MavistraController(const char* advertisingName)
    : commandTimeoutMs_(kDefaultCommandTimeoutMs),
      commands_(),
      initialized_(false),
      connected_(false),
      lastLoggedConnected_(false),
      lastCommandMs_(0U),
      advertisingName_{0},
      bleActive_(false),
      server_(nullptr),
      commandService_(nullptr),
      rxCommandCharacteristic_(nullptr),
      txEventCharacteristic_(nullptr) {
  if (advertisingName == nullptr || advertisingName[0] == '\0') {
    strncpy(advertisingName_, "MavistraController", sizeof(advertisingName_) - 1U);
    advertisingName_[sizeof(advertisingName_) - 1U] = '\0';
  } else {
    strncpy(advertisingName_, advertisingName, sizeof(advertisingName_) - 1U);
    advertisingName_[sizeof(advertisingName_) - 1U] = '\0';
  }
}

MavistraController::~MavistraController() { release(); }

bool MavistraController::begin() {
  if (initialized_) {
    Serial.println("[MavistraController] begin() already initialized");
    return true;
  }

  Serial.print("[MavistraController] Initializing BLE as: ");
  Serial.println(advertisingName_);

  NimBLEDevice::init(advertisingName_);
  server_ = NimBLEDevice::createServer();
  if (server_ == nullptr) {
    Serial.println("[MavistraController] ERROR: createServer failed");
    NimBLEDevice::deinit(true);
    return false;
  }

  commandService_ = server_->createService(kServiceUuid);
  if (commandService_ == nullptr) {
    Serial.println("[MavistraController] ERROR: createService failed");
    NimBLEDevice::deinit(true);
    server_ = nullptr;
    return false;
  }

  rxCommandCharacteristic_ = commandService_->createCharacteristic(
      kRxCommandUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  if (rxCommandCharacteristic_ == nullptr) {
    Serial.println("[MavistraController] ERROR: create RX characteristic failed");
    NimBLEDevice::deinit(true);
    server_ = nullptr;
    commandService_ = nullptr;
    return false;
  }

  txEventCharacteristic_ = commandService_->createCharacteristic(
      kTxEventUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  if (txEventCharacteristic_ == nullptr) {
    Serial.println("[MavistraController] ERROR: create TX characteristic failed");
    NimBLEDevice::deinit(true);
    server_ = nullptr;
    commandService_ = nullptr;
    rxCommandCharacteristic_ = nullptr;
    return false;
  }

  rxCommandCharacteristic_->setCallbacks(new MavistraRxCallbacks(&commands_));
  server_->setCallbacks(new MavistraServerCallbacks(txEventCharacteristic_));

  commandService_->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  if (advertising == nullptr) {
    Serial.println("[MavistraController] ERROR: getAdvertising failed");
    NimBLEDevice::deinit(true);
    server_ = nullptr;
    commandService_ = nullptr;
    rxCommandCharacteristic_ = nullptr;
    txEventCharacteristic_ = nullptr;
    return false;
  }

  advertising->addServiceUUID(kServiceUuid);
  advertising->start();
  Serial.println("[MavistraController] Advertising started");
  bleActive_ = true;

  initialized_ = true;
  connected_ = false;
  lastLoggedConnected_ = false;
  lastCommandMs_ = millis();

  return true;
}

void MavistraController::loop() {
  if (!initialized_) {
    return;
  }

  // Track connection state for advertising restart on disconnect.
  if (server_ != nullptr) {
    connected_ = server_->getConnectedCount() > 0;
    if (connected_ != lastLoggedConnected_) {
      if (!connected_) {
        clearAllActive();
        NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
        if (advertising != nullptr) {
          advertising->start();
          Serial.println("[MavistraController] Advertising restarted");
        } else {
          Serial.println("[MavistraController] ERROR: cannot restart advertising");
        }
      }
      lastLoggedConnected_ = connected_;
    }
  }

  // Heartbeat timeout sweep — mark commands inactive if no frame arrived in time.
  const uint32_t now = millis();
  for (auto& kv : commands_) {
    CommandEntry& entry = kv.second;
    if (entry.active && (now - entry.lastSeenMs) > commandTimeoutMs_) {
      entry.active = false;
    }
  }
}

bool MavistraController::isConnected() const { return connected_; }

bool MavistraController::isActive(const char* name) const {
  if (name == nullptr) {
    return false;
  }
  const auto it = commands_.find(std::string(name));
  return it != commands_.end() && it->second.active;
}

void MavistraController::setCommandTimeout(uint32_t ms) {
  commandTimeoutMs_ = ms;
}

void MavistraController::reset() {
  release();
  initialized_ = false;
  connected_ = false;
  lastCommandMs_ = 0U;
  commands_.clear();
}

void MavistraController::clearAllActive() {
  for (auto& kv : commands_) {
    kv.second.active = false;
  }
}

void MavistraController::release() {
  if (bleActive_) {
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (advertising != nullptr) {
      advertising->stop();
    }
    NimBLEDevice::deinit(true);
    bleActive_ = false;
  }

  server_ = nullptr;
  commandService_ = nullptr;
  rxCommandCharacteristic_ = nullptr;
  txEventCharacteristic_ = nullptr;
}
