#include <Arduino.h>
#include <Preferences.h>
#include <esp_sleep.h>

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "BleTelemetry.h"
#include "DynoTypes.h"
#include "EffortDetector.h"
#include "HX711.h"
#include "SerialTelemetry.h"

constexpr int HX711_DOUT_PIN = D4;
constexpr int HX711_SCK_PIN = D5;
constexpr int WAKE_BUTTON_PIN = D1;

// R2 holds D1 high and SW1 pulls it low. Requiring a hold avoids putting the
// dyno to sleep during an accidental tap; sleep begins after the button is
// released so the same low level cannot immediately wake it again.
constexpr uint32_t WAKE_BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t WAKE_BUTTON_SLEEP_HOLD_MS = 2000;

// A positive factor means raw count rises with applied force. Use a negative
// factor if it falls. Serial calibration commands are persisted in flash.
constexpr float DEFAULT_CALIBRATION_FACTOR_COUNTS_PER_NEWTON = 1461.258f;
constexpr dyno::EffortConfig DEFAULT_EFFORT_CONFIG = {20.0f, 10.0f, 1000};

constexpr uint8_t TARE_SAMPLE_COUNT = 16;
constexpr float FILTER_ALPHA = 0.20f;
constexpr size_t COMMAND_BUFFER_SIZE = 96;

HX711 loadcell;
Preferences preferences;
SerialTelemetry serialTelemetry;
BleTelemetry bleTelemetry;
dyno::EffortDetector effortDetector(DEFAULT_EFFORT_CONFIG);

bool hx711Initialized = false;
bool hx711Detected = false;
bool preferencesReady = false;
float calibrationFactor = DEFAULT_CALIBRATION_FACTOR_COUNTS_PER_NEWTON;
long tareOffset = 0;

bool tareInProgress = false;
uint8_t tareSamplesCollected = 0;
int64_t tareAccumulator = 0;

bool filterInitialized = false;
float filteredCounts = 0.0f;
bool hasMeasurement = false;
uint32_t nextSampleSequence = 0;

bool wakeButtonRawPressed = false;
bool wakeButtonStablePressed = false;
bool wakeButtonArmed = false;
bool sleepWhenWakeButtonReleased = false;
uint32_t wakeButtonRawChangedAtMs = 0;
uint32_t wakeButtonPressedAtMs = 0;

char commandBuffer[COMMAND_BUFFER_SIZE];
size_t commandLength = 0;
bool discardCommandUntilNewline = false;

void publishMeasurement(const dyno::ForceMeasurement &measurement) {
  serialTelemetry.publishMeasurement(measurement);
  bleTelemetry.publishMeasurement(measurement);
}

void publishEffortEvent(const dyno::EffortEvent &event) {
  serialTelemetry.publishEffortEvent(event);
}

void cancelActiveEffort() {
  publishEffortEvent(effortDetector.cancel(millis()));
}

void printWakeCause() {
  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  if (wakeCause == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println(F("# wake_cause=button"));
  } else {
    Serial.print(F("# wake_cause="));
    Serial.println(static_cast<int>(wakeCause));
  }
}

void enterDeepSleep() {
  cancelActiveEffort();
  if (hx711Initialized) {
    loadcell.power_down();
  }
  if (preferencesReady) {
    preferences.end();
    preferencesReady = false;
  }

  const esp_err_t wakeResult = esp_deep_sleep_enable_gpio_wakeup(
      1ULL << WAKE_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  if (wakeResult != ESP_OK) {
    Serial.print(F("# error,wake_button_config_failed="));
    Serial.println(esp_err_to_name(wakeResult));
    Serial.flush();
    delay(1000);
    ESP.restart();
  }

  Serial.println(F("# deep_sleep,wake_source=D1,active_level=low"));
  Serial.flush();
  delay(20);
  esp_deep_sleep_start();

  // esp_deep_sleep_start() does not return; keep the compiler aware of that.
  while (true) {
  }
}

void initializeWakeButton() {
  // The schematic includes an external 10 kOhm pull-up (R2). INPUT_PULLUP also
  // keeps the input defined if R2 is omitted during breadboard testing.
  pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
  wakeButtonRawPressed = digitalRead(WAKE_BUTTON_PIN) == LOW;
  wakeButtonStablePressed = wakeButtonRawPressed;
  wakeButtonRawChangedAtMs = millis();
  wakeButtonPressedAtMs = millis();

  // A button still held after waking must be released before it can request
  // sleep. Otherwise a long wake press would cause a sleep/wake loop.
  wakeButtonArmed = !wakeButtonStablePressed;
}

void pollWakeButton() {
  const uint32_t nowMs = millis();
  const bool rawPressed = digitalRead(WAKE_BUTTON_PIN) == LOW;

  if (rawPressed != wakeButtonRawPressed) {
    wakeButtonRawPressed = rawPressed;
    wakeButtonRawChangedAtMs = nowMs;
  }

  if (wakeButtonStablePressed != wakeButtonRawPressed &&
      nowMs - wakeButtonRawChangedAtMs >= WAKE_BUTTON_DEBOUNCE_MS) {
    wakeButtonStablePressed = wakeButtonRawPressed;
    if (wakeButtonStablePressed) {
      if (wakeButtonArmed) {
        wakeButtonPressedAtMs = nowMs;
      }
    } else {
      wakeButtonArmed = true;
      if (sleepWhenWakeButtonReleased) {
        enterDeepSleep();
      }
    }
  }

  if (wakeButtonArmed && wakeButtonStablePressed &&
      !sleepWhenWakeButtonReleased &&
      nowMs - wakeButtonPressedAtMs >= WAKE_BUTTON_SLEEP_HOLD_MS) {
    sleepWhenWakeButtonReleased = true;
    Serial.println(F("# sleep_requested,release_button_to_sleep"));
  }
}

void beginTare() {
  cancelActiveEffort();
  tareInProgress = true;
  tareSamplesCollected = 0;
  tareAccumulator = 0;
  filterInitialized = false;
  hasMeasurement = false;
  Serial.println(F("# tare_start"));
}

void collectTareSample(long raw) {
  tareAccumulator += raw;
  ++tareSamplesCollected;

  if (tareSamplesCollected < TARE_SAMPLE_COUNT) {
    return;
  }

  tareOffset = static_cast<long>(tareAccumulator / TARE_SAMPLE_COUNT);
  tareInProgress = false;

  Serial.print(F("# tare_complete,offset="));
  Serial.println(tareOffset);
}

void persistCalibrationFactor() {
  if (preferencesReady) {
    preferences.putFloat("factor", calibrationFactor);
  }
}

void persistEffortConfig() {
  if (!preferencesReady) {
    return;
  }

  const dyno::EffortConfig &config = effortDetector.config();
  preferences.putFloat("start_n", config.startForceNewtons);
  preferences.putFloat("end_n", config.endForceNewtons);
  preferences.putUInt("hold_ms", config.endHoldMs);
}

void printCalibrationFactor() {
  Serial.print(F("# calibration_factor_counts_per_newton="));
  Serial.println(calibrationFactor, 6);
}

void printEffortConfig() {
  const dyno::EffortConfig &config = effortDetector.config();
  Serial.print(F("# effort_config,start_force_newtons="));
  Serial.print(config.startForceNewtons, 3);
  Serial.print(F(",end_force_newtons="));
  Serial.print(config.endForceNewtons, 3);
  Serial.print(F(",end_hold_ms="));
  Serial.println(config.endHoldMs);
}

void printHelp() {
  Serial.println(F("# commands: tare | factor <counts_per_newton> | calibrate <known_newtons> | thresholds <start_n> <end_n> <hold_ms> | last | status | sleep | help"));
}

void printLastEffort() {
  if (!effortDetector.hasLastCompletedEffort()) {
    Serial.println(F("# last_effort,none"));
    return;
  }

  serialTelemetry.publishLastEffort(effortDetector.lastCompletedEffort());
}

void printStatus() {
  const dyno::EffortConfig &config = effortDetector.config();
  Serial.print(F("# status,tare_offset="));
  Serial.print(tareOffset);
  Serial.print(F(",calibration_factor_counts_per_newton="));
  Serial.print(calibrationFactor, 6);
  Serial.print(F(",effort_active="));
  Serial.print(effortDetector.active() ? 1 : 0);
  Serial.print(F(",effort_id="));
  Serial.print(effortDetector.currentEffortId());
  Serial.print(F(",current_peak_force_newtons="));
  Serial.print(effortDetector.currentPeakForceNewtons(), 3);
  Serial.print(F(",start_force_newtons="));
  Serial.print(config.startForceNewtons, 3);
  Serial.print(F(",end_force_newtons="));
  Serial.print(config.endForceNewtons, 3);
  Serial.print(F(",end_hold_ms="));
  Serial.println(config.endHoldMs);
}

void skipWhitespace(const char *&cursor) {
  while (isspace(static_cast<unsigned char>(*cursor))) {
    ++cursor;
  }
}

bool parseFloatToken(const char *&cursor, float &value) {
  if (cursor == nullptr) {
    return false;
  }

  skipWhitespace(cursor);
  char *end = nullptr;
  value = strtof(cursor, &end);
  if (end == cursor || !isfinite(value)) {
    return false;
  }

  cursor = end;
  return true;
}

bool parseUInt32Token(const char *&cursor, uint32_t &value) {
  if (cursor == nullptr) {
    return false;
  }

  skipWhitespace(cursor);
  if (*cursor == '-') {
    return false;
  }

  char *end = nullptr;
  const unsigned long parsed = strtoul(cursor, &end, 10);
  if (end == cursor || parsed > UINT32_MAX) {
    return false;
  }

  value = static_cast<uint32_t>(parsed);
  cursor = end;
  return true;
}

bool isEndOfArguments(const char *cursor) {
  if (cursor == nullptr) {
    return true;
  }

  skipWhitespace(cursor);
  return *cursor == '\0';
}

bool parseSingleFloatArgument(const char *argument, float &value) {
  return parseFloatToken(argument, value) && isEndOfArguments(argument);
}

bool parseEffortConfig(const char *argument, dyno::EffortConfig &config) {
  return parseFloatToken(argument, config.startForceNewtons) &&
         parseFloatToken(argument, config.endForceNewtons) &&
         parseUInt32Token(argument, config.endHoldMs) &&
         isEndOfArguments(argument) &&
         dyno::EffortDetector::isValidConfig(config);
}

void applyCalibrationFactor(float newFactor) {
  cancelActiveEffort();
  calibrationFactor = newFactor;
  persistCalibrationFactor();
  printCalibrationFactor();
}

void applyEffortConfig(const dyno::EffortConfig &config) {
  cancelActiveEffort();
  effortDetector.setConfig(config);
  persistEffortConfig();
  printEffortConfig();
}

void handleCommand(char *command) {
  while (isspace(static_cast<unsigned char>(*command))) {
    ++command;
  }

  char *commandEnd = command + strlen(command);
  while (commandEnd > command &&
         isspace(static_cast<unsigned char>(commandEnd[-1]))) {
    --commandEnd;
  }
  *commandEnd = '\0';

  if (*command == '\0') {
    return;
  }

  char *argument = command;
  while (*argument != '\0' &&
         !isspace(static_cast<unsigned char>(*argument))) {
    ++argument;
  }
  if (*argument != '\0') {
    *argument = '\0';
    ++argument;
  } else {
    argument = nullptr;
  }

  if (strcmp(command, "tare") == 0) {
    if (!hx711Detected) {
      Serial.println(F("# error,hx711_not_detected"));
      return;
    }

    beginTare();
    return;
  }

  if (strcmp(command, "factor") == 0) {
    float requestedFactor = 0.0f;
    if (!parseSingleFloatArgument(argument, requestedFactor) ||
        fabsf(requestedFactor) < 0.000001f) {
      Serial.println(
          F("# error,usage=factor <non-zero counts_per_newton>"));
      return;
    }

    applyCalibrationFactor(requestedFactor);
    return;
  }

  if (strcmp(command, "calibrate") == 0) {
    float knownForceNewtons = 0.0f;
    if (!parseSingleFloatArgument(argument, knownForceNewtons) ||
        knownForceNewtons <= 0.0f) {
      Serial.println(
          F("# error,usage=calibrate <positive_known_newtons>"));
      return;
    }

    if (!hasMeasurement || fabsf(filteredCounts) < 1.0f) {
      Serial.println(F("# error,apply_known_force_before_calibrating"));
      return;
    }

    applyCalibrationFactor(filteredCounts / knownForceNewtons);
    return;
  }

  if (strcmp(command, "thresholds") == 0) {
    dyno::EffortConfig config = DEFAULT_EFFORT_CONFIG;
    if (!parseEffortConfig(argument, config)) {
      Serial.println(
          F("# error,usage=thresholds <start_n> <end_n> <hold_ms>; start_n must be greater than end_n"));
      return;
    }

    applyEffortConfig(config);
    return;
  }

  if (strcmp(command, "last") == 0) {
    printLastEffort();
    return;
  }

  if (strcmp(command, "status") == 0) {
    printStatus();
    return;
  }

  if (strcmp(command, "sleep") == 0) {
    if (digitalRead(WAKE_BUTTON_PIN) == LOW) {
      sleepWhenWakeButtonReleased = true;
      Serial.println(F("# sleep_requested,release_button_to_sleep"));
    } else {
      enterDeepSleep();
    }
    return;
  }

  if (strcmp(command, "help") == 0) {
    printHelp();
    return;
  }

  Serial.println(F("# error,unknown_command"));
  printHelp();
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    const char nextCharacter = static_cast<char>(Serial.read());

    if (nextCharacter == '\r') {
      continue;
    }

    if (nextCharacter == '\n') {
      if (discardCommandUntilNewline) {
        discardCommandUntilNewline = false;
        commandLength = 0;
        continue;
      }

      commandBuffer[commandLength] = '\0';
      handleCommand(commandBuffer);
      commandLength = 0;
      continue;
    }

    if (discardCommandUntilNewline) {
      continue;
    }

    if (commandLength < COMMAND_BUFFER_SIZE - 1) {
      commandBuffer[commandLength++] = nextCharacter;
    } else {
      commandLength = 0;
      discardCommandUntilNewline = true;
      Serial.println(F("# error,command_too_long"));
    }
  }
}

void readBleCommands() {
  char command[COMMAND_BUFFER_SIZE];
  if (bleTelemetry.takeCommand(command, sizeof(command))) {
    handleCommand(command);
  }
}

void processSample(long raw, uint32_t timestampMs) {
  const float netCounts = static_cast<float>(raw - tareOffset);

  if (!filterInitialized) {
    filteredCounts = netCounts;
    filterInitialized = true;
  } else {
    filteredCounts += FILTER_ALPHA * (netCounts - filteredCounts);
  }

  hasMeasurement = true;
  const float forceNewtons = filteredCounts / calibrationFactor;
  publishEffortEvent(effortDetector.update(forceNewtons, timestampMs));

  const dyno::ForceMeasurement measurement = {
      nextSampleSequence++,
      timestampMs,
      static_cast<int32_t>(raw),
      forceNewtons,
      effortDetector.currentEffortId(),
      effortDetector.active(),
  };
  publishMeasurement(measurement);
}

void loadPersistentSettings() {
  preferencesReady = preferences.begin("dyno", false);
  if (!preferencesReady) {
    return;
  }

  const float storedFactor = preferences.getFloat(
      "factor", DEFAULT_CALIBRATION_FACTOR_COUNTS_PER_NEWTON);
  if (isfinite(storedFactor) && fabsf(storedFactor) >= 0.000001f) {
    calibrationFactor = storedFactor;
  }

  dyno::EffortConfig storedConfig = {
      preferences.getFloat("start_n",
                           DEFAULT_EFFORT_CONFIG.startForceNewtons),
      preferences.getFloat("end_n", DEFAULT_EFFORT_CONFIG.endForceNewtons),
      preferences.getUInt("hold_ms", DEFAULT_EFFORT_CONFIG.endHoldMs),
  };
  effortDetector.setConfig(storedConfig);
}

void setup() {
  initializeWakeButton();
  Serial.begin(115200);
  delay(1500);

  Serial.println(F("# dyno_starting"));
  printWakeCause();
  bleTelemetry.begin();

  loadcell.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
  hx711Initialized = true;
  loadcell.power_up();
  if (!loadcell.wait_ready_timeout(2000)) {
    Serial.println(F("# error,hx711_not_detected"));
    printHelp();
    return;
  }

  hx711Detected = true;
  loadPersistentSettings();
  printCalibrationFactor();
  printEffortConfig();
  printHelp();
  serialTelemetry.printHeader();
  beginTare();
}

void loop() {
  pollWakeButton();
  bleTelemetry.poll();
  readSerialCommands();
  readBleCommands();

  if (!hx711Detected || !loadcell.is_ready()) {
    delay(1);
    return;
  }

  // Consume every conversion once. Filtering happens after the individual raw
  // sample is read, so 80 SPS operation retains its time resolution.
  const long raw = loadcell.read();
  const uint32_t timestampMs = millis();

  if (tareInProgress) {
    collectTareSample(raw);
    return;
  }

  processSample(raw, timestampMs);
}
