/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectLavaFlow.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

#include "effects/visual-only/EffectLavaFlow.h"

#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectLavaFlow::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectLavaFlow;
}

void EffectLavaFlow::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float speedNorm = speed01(s.effectSpeed);
  const float levelNorm = level01(s.effectLevel);
  const float levelCurve = powf(levelNorm, 1.2f);
  const float speed = 0.10f + speedNorm * 2.2f;
  const float deform = 0.06f + levelCurve * 0.56f;
  const float repeats = static_cast<float>(max<uint8_t>(1, s.sectionCount));
  const float gain = s.brightness / 255.0f;

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) continue;

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      uint32_t c = lerpColor(s.primaryColors[0], s.primaryColors[1], 0.5f + 0.5f * sinf(t * speed));
      led.setOutputColor(outIdx, scaleColorFloat(c, gain));
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      const float x = normalizedX(px, out.ledCount);
      const float w1 = sinf((x * repeats + t * speed) * 2.0f * PI);
      const float w2 = sinf((x * repeats * 2.3f - t * speed * 1.3f) * 2.0f * PI);
      const float mix = clamp01(0.5f + 0.5f * (w1 * (1.0f - deform) + w2 * deform));

      const uint32_t hot = lerpColor(s.primaryColors[0], s.primaryColors[1], mix);
      const uint32_t lava = lerpColor(hot, s.primaryColors[2], clamp01(0.2f + 0.8f * mix));
      const uint32_t base = scaleColorFloat(s.backgroundColor, gain * (0.08f + 0.20f * (1.0f - levelNorm)));
      const uint32_t glow = scaleColorFloat(lava, gain * (0.60f + 0.40f * levelNorm));
      led.setPixelColor(outIdx, px, addColor(base, glow));
    }
  }

  led.show();
}
