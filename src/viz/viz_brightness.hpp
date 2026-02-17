#pragma once

// System for min/max brightness of a visualization (pipeline values, not the UI slider).
// Each viz defines what "minimum" and "maximum" brightness mean; the UI maps user input
// (e.g. 0–100) onto [minBrightness, maxBrightness]. Max can be data-dependent so it changes
// with the current viz state (e.g. point count, so the scale doesn't blow out).

namespace viz {

// Lower bound of the brightness scale sent to the shader. 0 = black; no separate "per color" min.
constexpr float kMinBrightness = 0.0f;

// Upper bound in abstract terms: shader multiplier is clamped so we never exceed this.
// Trigram uses additive blend, so effective max depends on data; see trigramMaxBrightness().
constexpr float kMaxBrightnessCap = 1.0f;

// Maps user value in [0, 100] to a value in [min, max]. Curve: 0 -> min, 100 -> max.
// Optional power (e.g. 3) gives more resolution at the low end.
inline float userToPipeline(float user0to100, float minB, float maxB, float power = 1.0f) {
  if (user0to100 <= 0.0f) return minB;
  float t = user0to100 / 100.0f;
  if (power != 1.0f) {
    t = t * t * t;  // cubic: more resolution at low end
  }
  return minB + t * (maxB - minB);
}

}  // namespace viz
