#include "SerialTelemetry.h"

#include <Arduino.h>

void SerialTelemetry::printHeader() const {
  Serial.println(F("timestamp_ms,raw,force_newtons"));
}

void SerialTelemetry::publishMeasurement(
    const dyno::ForceMeasurement &measurement) const {
  Serial.print(measurement.timestampMs);
  Serial.print(',');
  Serial.print(measurement.rawCounts);
  Serial.print(',');
  Serial.println(measurement.forceNewtons, 3);
}

void SerialTelemetry::publishEffortEvent(
    const dyno::EffortEvent &event) const {
  if (event.type == dyno::EffortEventType::None) {
    return;
  }

  if (event.type == dyno::EffortEventType::Started) {
    Serial.print(F("# effort_start,effort_id="));
    Serial.print(event.effort.id);
    Serial.print(F(",timestamp_ms="));
    Serial.print(event.effort.startTimestampMs);
    Serial.print(F(",force_newtons="));
    Serial.println(event.triggerForceNewtons, 3);
    return;
  }

  Serial.print(event.type == dyno::EffortEventType::Completed
                   ? F("# effort_end,effort_id=")
                   : F("# effort_cancelled,effort_id="));
  Serial.print(event.effort.id);
  Serial.print(F(",start_timestamp_ms="));
  Serial.print(event.effort.startTimestampMs);
  Serial.print(F(",end_timestamp_ms="));
  Serial.print(event.effort.endTimestampMs);
  Serial.print(F(",duration_ms="));
  Serial.print(event.effort.durationMs);
  Serial.print(F(",peak_force_newtons="));
  Serial.println(event.effort.peakForceNewtons, 3);
}

void SerialTelemetry::publishLastEffort(
    const dyno::EffortSummary &effort) const {
  Serial.print(F("# last_effort,effort_id="));
  Serial.print(effort.id);
  Serial.print(F(",start_timestamp_ms="));
  Serial.print(effort.startTimestampMs);
  Serial.print(F(",end_timestamp_ms="));
  Serial.print(effort.endTimestampMs);
  Serial.print(F(",duration_ms="));
  Serial.print(effort.durationMs);
  Serial.print(F(",peak_force_newtons="));
  Serial.println(effort.peakForceNewtons, 3);
}
