/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectStellarTwinkle.cpp
 * Last commit: a67d822 - 2026-04-12
 */

#include "effects/EffectStellarTwinkle.h"

#include "effects/EffectRegistry.h"

#include <math.h>

namespace {
uint32_t lcg(uint32_t &seed) {
  seed = seed * 1664525u + 1013904223u;
  return seed;
}

uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}
} // namespace

bool EffectStellarTwinkle::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectStellarTwinkle;
}

void EffectStellarTwinkle::onActivate() {
  seed_ = millis() ^ 0xA5A55A5Au;
}

void EffectStellarTwinkle::onDeactivate() {
  seed_ = 1;
}

void EffectStellarTwinkle::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float speedNorm = speed01(s.effectSpeed);
  const float levelNorm = level01(s.effectLevel);
  const float audio01 = s.reactiveToAudio ? clamp01(static_cast<float>(s.audioLevel) / 255.0f) : 0.0f;
  const float speed = 0.15f + speedNorm * 4.5f + audio01 * 3.0f;
  const float density = 0.03f + levelNorm * 0.40f + audio01 * 0.18f;
  const float gain = s.brightness / 255.0f;
  const float stepRate = 2.0f + speedNorm * 16.0f + audio01 * 12.0f;
  const uint32_t frameKey = static_cast<uint32_t>(t * stepRate);
  const float beatFlash = s.reactiveToAudio && s.beatDetected ? 1.0f : 0.0f;
  const uint32_t audioTint = audioColorShift(s.primaryColors[0], s.primaryColors[1], s.primaryColors[2]);

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) continue;

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      uint32_t r = hash32(seed_ ^ frameKey ^ outIdx);
      float flash = ((r & 0xFF) / 255.0f) < density ? (0.65f + 0.35f * levelNorm + 0.35f * audio01) : 0.08f;
      uint32_t flatColor = s.reactiveToAudio ? audioTint : s.primaryColors[outIdx % 3];
      flatColor = lerpColor(flatColor, 0xFFFFFF, beatFlash * 0.45f);
      led.setOutputColor(outIdx, scaleColorFloat(flatColor, flash * gain));
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      uint32_t r = hash32(seed_ ^ frameKey ^ (outIdx * 131u + px * 977u));
      const float rnd = (r & 0xFFFF) / 65535.0f;
      float sparkle = 0.0f;
      if (rnd < density) {
        const float pulse = 0.5f + 0.5f * sinf((t * speed + px * 0.071f) * 2.0f * PI);
        sparkle = smoothstep(0.0f, 1.0f, pulse) * (0.45f + 0.55f * levelNorm);
      }

      const uint8_t colorIdx = static_cast<uint8_t>(r % 3u);
      const uint32_t base = scaleColorFloat(s.backgroundColor, gain * (0.10f + 0.16f * (1.0f - levelNorm) + beatFlash * 0.08f));
      uint32_t starColor = s.primaryColors[colorIdx];
      if (s.reactiveToAudio) {
        starColor = lerpColor(starColor, audioTint, 0.60f * audio01);
        starColor = lerpColor(starColor, 0xFFFFFF, beatFlash * 0.55f);
      }
      const uint32_t star = scaleColorFloat(starColor, sparkle * gain * (1.0f + 0.35f * audio01));
      led.setPixelColor(outIdx, px, addColor(base, star));
    }
  }

  led.show();
}
