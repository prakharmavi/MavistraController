#include "MavistraController.h"
#include <NimBLEDevice.h>
#include <string.h>
#include <string>

namespace {
constexpr const char* kServiceUuid = "19b10000-e8f2-537e-4f6c-d104768a1214";
constexpr const char* kRxCommandUuid = "19b10001-e8f2-537e-4f6c-d104768a1214";
constexpr const char* kTxEventUuid = "19b10002-e8f2-537e-4f6c-d104768a1214";

class MavistraServerCallbacks : public NimBLEServerCallbacks {
 public:
  void onConnect(NimBLEServer* server) {
    (void)server;
  }

  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) {
    (void)connInfo;
    onConnect(server);
  }

  void onDisconnect(NimBLEServer* server) {
    (void)server;
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) {
    (void)connInfo;
    (void)reason;
    onDisconnect(server);
  }
};

class MavistraRxCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit MavistraRxCallbacks(NimBLECharacteristic* txCharacteristic)
      : txCharacteristic_(txCharacteristic) {}

  void onWrite(NimBLECharacteristic* characteristic) {
    if (characteristic == nullptr) {
      return;
    }

    const std::string raw = characteristic->getValue();
    Serial.print("[MavistraController] RX raw: ");
    if (raw.empty()) {
      Serial.println("<empty>");
      return;
    }

    Serial.println(raw.c_str());

    if (txCharacteristic_ != nullptr) {
      const std::string response = std::string("recieved ") + raw;
      txCharacteristic_->setValue(response);
      txCharacteristic_->notify();
      Serial.print("[MavistraController] TX: ");
      Serial.println(response.c_str());
    }
  }

  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) {
    (void)connInfo;
    onWrite(characteristic);
  }

 private:
  NimBLECharacteristic* txCharacteristic_;
};
}  // namespace

MavistraController::MavistraController(const char* advertisingName)
    : initialized_(false),
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
  server_->setCallbacks(new MavistraServerCallbacks());

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
  rxCommandCharacteristic_->setCallbacks(new MavistraRxCallbacks(txEventCharacteristic_));

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

  if (server_ != nullptr) {
    connected_ = server_->getConnectedCount() > 0;
    if (connected_ != lastLoggedConnected_) {
      if (connected_) {
        Serial.println("[MavistraController] BLE client connected");
      } else {
        Serial.println("[MavistraController] BLE client disconnected");
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

  // TODO: process BLE events, parse incoming commands, dispatch handlers.
}

bool MavistraController::isConnected() const { return connected_; }

void MavistraController::reset() {
  release();
  initialized_ = false;
  connected_ = false;
  lastCommandMs_ = 0U;
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
