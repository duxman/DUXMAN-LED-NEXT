/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectBreathFixed.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

#include "effects/EffectBreathFixed.h"

#include "effects/EffectRegistry.h"
#include <math.h>

bool EffectBreathFixed::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectBreathFixed;
}

void EffectBreathFixed::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  // Envolvente senoidal: period en segundos (speed bajo -> lento, speed alto -> rapido)
  const float t = normalizedTimeSec();
  const float period = 8.0f - 7.5f * speed01(s.effectSpeed);
  const float breathe = 0.5f + 0.5f * sinf(2.0f * PI * t / period); // 0..1

  // effectLevel llega en escala 1..10 desde API/UI.
  // depth = 0 → envelope constante en 1.0 (sin respirar)
  // depth = 1 → envelope oscila completo entre 0 y 1
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

    const uint16_t sectionSize = resolveSectionSize(out.ledCount, s.sectionCount);
    for (uint16_t px = 0; px < out.ledCount; ++px) {
      const uint8_t sectionIdx = static_cast<uint8_t>((px / sectionSize) % 3);
      led.setPixelColor(outIdx, px, scaleColorFloat(s.primaryColors[sectionIdx], finalGain));
    }
  }
  led.show();
}
