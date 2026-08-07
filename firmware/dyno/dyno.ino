#include <Arduino.h>
#include "HX711.h"

constexpr int HX711_DOUT_PIN = D4;
constexpr int HX711_SCK_PIN = D5;

HX711 loadcell;

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("Starting HX711...");

  loadcell.begin(HX711_DOUT_PIN, HX711_SCK_PIN);

  if (!loadcell.wait_ready_timeout(2000)) {
    Serial.println("HX711 not detected. Check power and wiring.");
    return;
  }

  Serial.println("HX711 detected.");
  loadcell.tare(20);
  Serial.println("Tared.");
}

void loop() {
  if (loadcell.is_ready()) {
    const long raw = loadcell.read();
    Serial.println(raw);
  }

  delay(1);
}