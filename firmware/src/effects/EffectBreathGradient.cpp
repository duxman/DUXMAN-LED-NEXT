/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectBreathGradient.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

#include "effects/EffectBreathGradient.h"

#include "effects/EffectRegistry.h"
#include <math.h>

bool EffectBreathGradient::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectBreathGradient;
}

void EffectBreathGradient::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float period = 8.0f - 7.5f * speed01(s.effectSpeed);
  const float breathe = 0.5f + 0.5f * sinf(2.0f * PI * t / period);

  const float depth = level01(s.effectLevel);
  const float envelope = (1.0f - depth) + depth * breathe;
  const float finalGain = clamp01(envelope * (s.brightness / 255.0f));

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) {
      continue;
    }

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      led.setOutputColor(outIdx, scaleColorFloat(s.primaryColors[outIdx % 3], finalGain));
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      const uint32_t gradColor = gradientColor(
          s.primaryColors[0], s.primaryColors[1], s.primaryColors[2], px, out.ledCount);
      led.setPixelColor(outIdx, px, scaleColorFloat(gradColor, finalGain));
    }
  }
  led.show();
}
