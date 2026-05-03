/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectBouncingPhysics.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

#include "effects/audio-reactive/EffectBouncingPhysics.h"

#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectBouncingPhysics::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectBouncingPhysics;
}

void EffectBouncingPhysics::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float speedNorm = speed01(s.effectSpeed);
  const float levelNorm = level01(s.effectLevel);
  const float audio01 = s.reactiveToAudio ? clamp01(static_cast<float>(s.audioLevel) / 255.0f) : 0.0f;
  const float speed = 0.12f + speedNorm * (2.2f + 1.6f * levelNorm) + audio01 * 2.4f;
  const uint8_t balls = static_cast<uint8_t>(1 + static_cast<uint8_t>(levelNorm * 4.0f) + static_cast<uint8_t>(audio01 * 2.0f)); // 1..7
  const float gain = s.brightness / 255.0f;
  const float beatFlash = s.reactiveToAudio && s.beatDetected ? 1.0f : 0.0f;
  const uint32_t audioTint = audioColorShift(s.primaryColors[0], s.primaryColors[1], s.primaryColors[2]);

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) continue;

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      uint8_t idx = static_cast<uint8_t>(fmodf(t * speed * 3.0f, 3.0f));
      uint32_t flatColor = s.reactiveToAudio ? audioTint : s.primaryColors[idx];
      flatColor = lerpColor(flatColor, 0xFFFFFF, beatFlash * 0.35f);
      led.setOutputColor(outIdx, scaleColorFloat(flatColor, gain * (0.8f + 0.2f * audio01)));
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      uint32_t color = scaleColorFloat(s.backgroundColor, gain * (0.25f + beatFlash * 0.10f));
      const float x = normalizedX(px, out.ledCount);

      for (uint8_t b = 0; b < balls; ++b) {
        const float phase = t * speed + b * 0.217f;
        const float center = 0.5f + 0.5f * sinf(phase * 2.0f * PI);
        const float width = 0.02f + 0.05f * levelNorm + 0.02f * audio01 + 0.008f * b;
        const float dist = fabsf(x - center);
        if (dist <= width) {
          const float n = 1.0f - (dist / width);
          const float intensity = smoothstep(0.0f, 1.0f, n);
          uint32_t ballColor = s.primaryColors[b % 3];
          if (s.reactiveToAudio) {
            ballColor = lerpColor(ballColor, audioTint, 0.60f * audio01);
            ballColor = lerpColor(ballColor, 0xFFFFFF, beatFlash * 0.40f);
          }
          const uint32_t ball = scaleColorFloat(ballColor, intensity * gain * (0.7f + 0.3f * levelNorm + 0.35f * audio01));
          color = addColor(color, ball);
        }
      }

      led.setPixelColor(outIdx, px, color);
    }
  }

  led.show();
}
