#include "BleTelemetry.h"

#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEService.h>

#include <stdio.h>
#include <string.h>

const char *const BleTelemetry::DEVICE_NAME = "Dyno";
const char *const BleTelemetry::SERVICE_UUID =
    "7b7e1000-6ba3-4d8f-9e2f-4f2f0c7a0000";
const char *const BleTelemetry::FORCE_CHARACTERISTIC_UUID =
    "7b7e1001-6ba3-4d8f-9e2f-4f2f0c7a0000";
const char *const BleTelemetry::COMMAND_CHARACTERISTIC_UUID =
    "7b7e1002-6ba3-4d8f-9e2f-4f2f0c7a0000";

class DynoBleServerCallbacks : public BLEServerCallbacks {
 public:
  explicit DynoBleServerCallbacks(BleTelemetry &telemetry)
      : telemetry_(telemetry) {}

  void onConnect(BLEServer *) override { telemetry_.onConnect(); }

  void onDisconnect(BLEServer *) override { telemetry_.onDisconnect(); }

 private:
  BleTelemetry &telemetry_;
};

class DynoBleCommandCallbacks : public BLECharacteristicCallbacks {
 public:
  explicit DynoBleCommandCallbacks(BleTelemetry &telemetry)
      : telemetry_(telemetry) {}

  void onWrite(BLECharacteristic *characteristic) override {
    const auto value = characteristic->getValue();
    telemetry_.queueCommand(value.c_str(), value.length());
  }

 private:
  BleTelemetry &telemetry_;
};

BleTelemetry::BleTelemetry()
    : server_(nullptr),
      forceCharacteristic_(nullptr),
      connected_(false),
      advertisingRestartPending_(false),
      disconnectedAtMs_(0),
      connectedEventPending_(false),
      disconnectedEventPending_(false),
      commandTooLongEventPending_(false),
      commandQueueFullEventPending_(false),
      hasNotificationTimestamp_(false),
      lastNotificationTimestampMs_(0),
      commandPending_(false),
      pendingCommandLength_(0),
      pendingCommand_{} {}

void BleTelemetry::begin() {
  // No security callbacks or encrypted properties are configured: clients can
  // connect and use this debugging service without pairing or bonding.
  BLEDevice::init(DEVICE_NAME);

  server_ = BLEDevice::createServer();
  server_->setCallbacks(new DynoBleServerCallbacks(*this));

  BLEService *service = server_->createService(SERVICE_UUID);
  forceCharacteristic_ = service->createCharacteristic(
      FORCE_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  forceCharacteristic_->setValue("0.000");
  forceCharacteristic_->addDescriptor(new BLE2902());

  BLECharacteristic *commandCharacteristic = service->createCharacteristic(
      COMMAND_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_WRITE_NR);
  commandCharacteristic->setCallbacks(new DynoBleCommandCallbacks(*this));

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.print(F("# ble_advertising,device_name="));
  Serial.print(DEVICE_NAME);
  Serial.print(F(",service_uuid="));
  Serial.println(SERVICE_UUID);
}

void BleTelemetry::poll() {
  bool shouldRestartAdvertising = false;
  bool connectedEvent = false;
  bool disconnectedEvent = false;
  bool commandTooLongEvent = false;
  bool commandQueueFullEvent = false;

  portENTER_CRITICAL(&stateMux_);
  if (advertisingRestartPending_ &&
      millis() - disconnectedAtMs_ >= ADVERTISING_RESTART_DELAY_MS) {
    advertisingRestartPending_ = false;
    shouldRestartAdvertising = true;
  }
  connectedEvent = connectedEventPending_;
  disconnectedEvent = disconnectedEventPending_;
  commandTooLongEvent = commandTooLongEventPending_;
  commandQueueFullEvent = commandQueueFullEventPending_;
  connectedEventPending_ = false;
  disconnectedEventPending_ = false;
  commandTooLongEventPending_ = false;
  commandQueueFullEventPending_ = false;
  portEXIT_CRITICAL(&stateMux_);

  if (connectedEvent) {
    Serial.println(F("# ble_connected"));
  }
  if (disconnectedEvent) {
    Serial.println(F("# ble_disconnected"));
  }
  if (commandTooLongEvent) {
    Serial.println(F("# error,ble_command_too_long"));
  }
  if (commandQueueFullEvent) {
    Serial.println(F("# error,ble_command_queue_full"));
  }
  if (shouldRestartAdvertising) {
    BLEDevice::startAdvertising();
    Serial.println(F("# ble_advertising_restarted"));
  }
}

void BleTelemetry::publishMeasurement(
    const dyno::ForceMeasurement &measurement) {
  if (forceCharacteristic_ == nullptr) {
    return;
  }

  bool shouldNotify = false;
  portENTER_CRITICAL(&stateMux_);
  if (connected_ &&
      (!hasNotificationTimestamp_ ||
       measurement.timestampMs - lastNotificationTimestampMs_ >=
           NOTIFICATION_INTERVAL_MS)) {
    hasNotificationTimestamp_ = true;
    lastNotificationTimestampMs_ = measurement.timestampMs;
    shouldNotify = true;
  }
  portEXIT_CRITICAL(&stateMux_);

  if (!shouldNotify) {
    return;
  }

  char forceText[24];
  const int length =
      snprintf(forceText, sizeof(forceText), "%.3f", measurement.forceNewtons);
  if (length <= 0) {
    return;
  }

  const size_t valueLength =
      static_cast<size_t>(length) < sizeof(forceText)
          ? static_cast<size_t>(length)
          : sizeof(forceText) - 1;
  forceCharacteristic_->setValue(
      reinterpret_cast<const uint8_t *>(forceText), valueLength);
  forceCharacteristic_->notify();
}

bool BleTelemetry::takeCommand(char *destination, size_t destinationSize) {
  if (destination == nullptr || destinationSize == 0) {
    return false;
  }

  bool hasCommand = false;
  portENTER_CRITICAL(&stateMux_);
  if (commandPending_) {
    const size_t copyLength =
        pendingCommandLength_ < destinationSize - 1
            ? pendingCommandLength_
            : destinationSize - 1;
    memcpy(destination, pendingCommand_, copyLength);
    destination[copyLength] = '\0';
    commandPending_ = false;
    pendingCommandLength_ = 0;
    hasCommand = true;
  }
  portEXIT_CRITICAL(&stateMux_);

  return hasCommand;
}

void BleTelemetry::onConnect() {
  portENTER_CRITICAL(&stateMux_);
  connected_ = true;
  advertisingRestartPending_ = false;
  connectedEventPending_ = true;
  hasNotificationTimestamp_ = false;
  portEXIT_CRITICAL(&stateMux_);
}

void BleTelemetry::onDisconnect() {
  portENTER_CRITICAL(&stateMux_);
  connected_ = false;
  advertisingRestartPending_ = true;
  disconnectedAtMs_ = millis();
  disconnectedEventPending_ = true;
  hasNotificationTimestamp_ = false;
  portEXIT_CRITICAL(&stateMux_);
}

void BleTelemetry::queueCommand(const char *command, size_t length) {
  if (command == nullptr || length == 0) {
    return;
  }

  if (length >= COMMAND_BUFFER_SIZE) {
    portENTER_CRITICAL(&stateMux_);
    commandTooLongEventPending_ = true;
    portEXIT_CRITICAL(&stateMux_);
    return;
  }

  portENTER_CRITICAL(&stateMux_);
  if (!commandPending_) {
    memcpy(pendingCommand_, command, length);
    pendingCommand_[length] = '\0';
    pendingCommandLength_ = length;
    commandPending_ = true;
  } else {
    commandQueueFullEventPending_ = true;
  }
  portEXIT_CRITICAL(&stateMux_);
}
