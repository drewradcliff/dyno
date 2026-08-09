#pragma once

#include <Arduino.h>

#include <stddef.h>

#include "DynoTypes.h"

class BLECharacteristic;
class BLEServer;
class DynoBleCommandCallbacks;
class DynoBleServerCallbacks;

class BleTelemetry {
 public:
  static const char *const DEVICE_NAME;
  static const char *const SERVICE_UUID;
  static const char *const FORCE_CHARACTERISTIC_UUID;
  static const char *const COMMAND_CHARACTERISTIC_UUID;

  BleTelemetry();

  void begin();
  void poll();
  void publishMeasurement(const dyno::ForceMeasurement &measurement);
  bool takeCommand(char *destination, size_t destinationSize);

 private:
  friend class DynoBleCommandCallbacks;
  friend class DynoBleServerCallbacks;

  static constexpr size_t COMMAND_BUFFER_SIZE = 96;
  static constexpr uint32_t NOTIFICATION_INTERVAL_MS = 25;
  static constexpr uint32_t ADVERTISING_RESTART_DELAY_MS = 250;

  void onConnect();
  void onDisconnect();
  void queueCommand(const char *command, size_t length);

  BLEServer *server_;
  BLECharacteristic *forceCharacteristic_;

  bool connected_;
  bool advertisingRestartPending_;
  uint32_t disconnectedAtMs_;
  bool connectedEventPending_;
  bool disconnectedEventPending_;
  bool commandTooLongEventPending_;
  bool commandQueueFullEventPending_;
  bool hasNotificationTimestamp_;
  uint32_t lastNotificationTimestampMs_;

  bool commandPending_;
  size_t pendingCommandLength_;
  char pendingCommand_[COMMAND_BUFFER_SIZE];

  portMUX_TYPE stateMux_ = portMUX_INITIALIZER_UNLOCKED;
};
