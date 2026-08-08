#pragma once

#include <stdint.h>

namespace dyno {

struct EffortConfig {
  float startForceNewtons;
  float endForceNewtons;
  uint32_t endHoldMs;
};

struct ForceMeasurement {
  uint32_t sequence;
  uint32_t timestampMs;
  int32_t rawCounts;
  float forceNewtons;
  uint32_t effortId;
  bool effortActive;
};

struct EffortSummary {
  uint32_t id;
  uint32_t startTimestampMs;
  uint32_t endTimestampMs;
  uint32_t durationMs;
  float peakForceNewtons;
};

enum class EffortEventType : uint8_t {
  None,
  Started,
  Completed,
  Cancelled,
};

struct EffortEvent {
  EffortEventType type;
  EffortSummary effort;
  float triggerForceNewtons;
};

}  // namespace dyno
