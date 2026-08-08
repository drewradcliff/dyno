#include "EffortDetector.h"

#include <math.h>

namespace dyno {

namespace {

constexpr uint32_t MAX_END_HOLD_MS = 10UL * 60UL * 1000UL;

EffortSummary emptySummary() {
  const EffortSummary summary = {0, 0, 0, 0, 0.0f};
  return summary;
}

}  // namespace

EffortDetector::EffortDetector(const EffortConfig &config)
    : config_(config),
      nextEffortId_(1),
      active_(false),
      belowEndThreshold_(false),
      belowEndThresholdSinceMs_(0),
      currentEffort_(emptySummary()),
      hasLastCompletedEffort_(false),
      lastCompletedEffort_(emptySummary()) {}

bool EffortDetector::isValidConfig(const EffortConfig &config) {
  return isfinite(config.startForceNewtons) &&
         isfinite(config.endForceNewtons) &&
         config.startForceNewtons > config.endForceNewtons &&
         config.endForceNewtons >= 0.0f && config.endHoldMs > 0 &&
         config.endHoldMs <= MAX_END_HOLD_MS;
}

bool EffortDetector::setConfig(const EffortConfig &config) {
  if (!isValidConfig(config)) {
    return false;
  }

  config_ = config;
  return true;
}

const EffortConfig &EffortDetector::config() const { return config_; }

EffortEvent EffortDetector::update(float forceNewtons,
                                   uint32_t timestampMs) {
  if (!active_) {
    if (forceNewtons <= config_.startForceNewtons) {
      return noEvent();
    }

    active_ = true;
    belowEndThreshold_ = false;
    currentEffort_.id = allocateEffortId();
    currentEffort_.startTimestampMs = timestampMs;
    currentEffort_.endTimestampMs = 0;
    currentEffort_.durationMs = 0;
    currentEffort_.peakForceNewtons = forceNewtons;

    const EffortEvent event = {EffortEventType::Started, currentEffort_,
                               forceNewtons};
    return event;
  }

  if (forceNewtons > currentEffort_.peakForceNewtons) {
    currentEffort_.peakForceNewtons = forceNewtons;
  }

  if (forceNewtons >= config_.endForceNewtons) {
    belowEndThreshold_ = false;
    return noEvent();
  }

  if (!belowEndThreshold_) {
    belowEndThreshold_ = true;
    belowEndThresholdSinceMs_ = timestampMs;
    return noEvent();
  }

  if (timestampMs - belowEndThresholdSinceMs_ < config_.endHoldMs) {
    return noEvent();
  }

  currentEffort_.endTimestampMs = timestampMs;
  currentEffort_.durationMs = timestampMs - currentEffort_.startTimestampMs;
  lastCompletedEffort_ = currentEffort_;
  hasLastCompletedEffort_ = true;

  const EffortEvent event = {EffortEventType::Completed, currentEffort_,
                             forceNewtons};
  clearCurrentEffort();
  return event;
}

EffortEvent EffortDetector::cancel(uint32_t timestampMs) {
  if (!active_) {
    return noEvent();
  }

  currentEffort_.endTimestampMs = timestampMs;
  currentEffort_.durationMs = timestampMs - currentEffort_.startTimestampMs;
  const EffortEvent event = {EffortEventType::Cancelled, currentEffort_, 0.0f};
  clearCurrentEffort();
  return event;
}

bool EffortDetector::active() const { return active_; }

uint32_t EffortDetector::currentEffortId() const {
  return active_ ? currentEffort_.id : 0;
}

float EffortDetector::currentPeakForceNewtons() const {
  return active_ ? currentEffort_.peakForceNewtons : 0.0f;
}

bool EffortDetector::hasLastCompletedEffort() const {
  return hasLastCompletedEffort_;
}

const EffortSummary &EffortDetector::lastCompletedEffort() const {
  return lastCompletedEffort_;
}

EffortEvent EffortDetector::noEvent() const {
  const EffortEvent event = {EffortEventType::None, emptySummary(), 0.0f};
  return event;
}

uint32_t EffortDetector::allocateEffortId() {
  const uint32_t allocatedId = nextEffortId_;
  ++nextEffortId_;
  if (nextEffortId_ == 0) {
    nextEffortId_ = 1;
  }
  return allocatedId;
}

void EffortDetector::clearCurrentEffort() {
  active_ = false;
  belowEndThreshold_ = false;
  belowEndThresholdSinceMs_ = 0;
  currentEffort_ = emptySummary();
}

}  // namespace dyno
