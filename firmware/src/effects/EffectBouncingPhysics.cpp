#include "effects/EffectBouncingPhysics.h"

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
  const float speed = 0.12f + speedNorm * (2.2f + 1.6f * levelNorm);
  const uint8_t balls = static_cast<uint8_t>(1 + static_cast<uint8_t>(levelNorm * 5.0f)); // 1..6
  const float gain = s.brightness / 255.0f;

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) continue;

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      uint8_t idx = static_cast<uint8_t>(fmodf(t * speed * 3.0f, 3.0f));
      led.setOutputColor(outIdx, scaleColorFloat(s.primaryColors[idx], gain));
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      uint32_t color = scaleColorFloat(s.backgroundColor, gain);
      const float x = normalizedX(px, out.ledCount);

      for (uint8_t b = 0; b < balls; ++b) {
        const float phase = t * speed + b * 0.217f;
        const float center = 0.5f + 0.5f * sinf(phase * 2.0f * PI);
        const float width = 0.02f + 0.05f * levelNorm + 0.008f * b;
        const float dist = fabsf(x - center);
        if (dist <= width) {
          const float n = 1.0f - (dist / width);
          const float intensity = smoothstep(0.0f, 1.0f, n);
          const uint32_t ball = scaleColorFloat(s.primaryColors[b % 3], intensity * gain * (0.7f + 0.3f * levelNorm));
          color = addColor(color, ball);
        }
      }

      led.setPixelColor(outIdx, px, color);
    }
  }

  led.show();
}
