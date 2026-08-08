#pragma once

#include "DynoTypes.h"

namespace dyno {

class EffortDetector {
 public:
  explicit EffortDetector(const EffortConfig &config);

  static bool isValidConfig(const EffortConfig &config);
  bool setConfig(const EffortConfig &config);
  const EffortConfig &config() const;

  EffortEvent update(float forceNewtons, uint32_t timestampMs);
  EffortEvent cancel(uint32_t timestampMs);

  bool active() const;
  uint32_t currentEffortId() const;
  float currentPeakForceNewtons() const;

  bool hasLastCompletedEffort() const;
  const EffortSummary &lastCompletedEffort() const;

 private:
  EffortEvent noEvent() const;
  uint32_t allocateEffortId();
  void clearCurrentEffort();

  EffortConfig config_;
  uint32_t nextEffortId_;
  bool active_;
  bool belowEndThreshold_;
  uint32_t belowEndThresholdSinceMs_;
  EffortSummary currentEffort_;
  bool hasLastCompletedEffort_;
  EffortSummary lastCompletedEffort_;
};

}  // namespace dyno
