#pragma once

#include "DynoTypes.h"

class SerialTelemetry {
 public:
  void printHeader() const;
  void publishMeasurement(const dyno::ForceMeasurement &measurement) const;
  void publishEffortEvent(const dyno::EffortEvent &event) const;
  void publishLastEffort(const dyno::EffortSummary &effort) const;
};
