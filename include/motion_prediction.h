#pragma once
#include <stdint.h>
#include "continuous_rotation.h"

namespace motion_prediction {
// Small bounded latency compensation. Does not interpolate between frames.
inline float predict(float baseDegrees, float rollDegrees, float angularSpeedDps,
                     uint32_t sampleAgeMs, uint32_t leadMs) {
  uint32_t sum = sampleAgeMs + leadMs;
  if (sum > 50) sum = 50;
  return continuous_rotation::wrap360(
      baseDegrees + rollDegrees + angularSpeedDps * (sum / 1000.0f));
}
} // namespace motion_prediction
