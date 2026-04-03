#include "effects/EffectTripleChase.h"

#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectTripleChase::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectTripleChase;
}

void EffectTripleChase::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float repeats = static_cast<float>(max<uint8_t>(1, s.sectionCount));
  const float speedHz = 0.05f + (s.effectSpeed / 100.0f) * 2.45f;
  const float phase = t * speedHz;

  // effectLevel 1..10 -> ancho 0.06..0.45 del ciclo.
  const float trainWidth = 0.06f + ((s.effectLevel - 1) / 9.0f) * 0.39f;
  const float globalGain = s.brightness / 255.0f;

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) {
      continue;
    }

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      const uint8_t colorIdx = static_cast<uint8_t>(fmodf(phase * 3.0f, 3.0f));
      led.setOutputColor(outIdx, scaleColorFloat(s.primaryColors[colorIdx], globalGain));
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      const float x = normalizedX(px, out.ledCount);
      const float patternPos = fmodf(x * repeats - phase, 1.0f);
      const float localPos = patternPos < 0.0f ? patternPos + 1.0f : patternPos;

      float intensity = 0.0f;
      if (localPos < trainWidth) {
        // Head mas intensa, cola mas suave.
        const float ramp = 1.0f - (localPos / trainWidth);
        intensity = smoothstep(0.0f, 1.0f, ramp);
      }

      const uint8_t colorIdx = static_cast<uint8_t>((static_cast<uint32_t>(x * repeats) % 3u));
      uint32_t color = scaleColorFloat(s.backgroundColor, globalGain);
      if (intensity > 0.0f) {
        const uint32_t chaseColor = scaleColorFloat(s.primaryColors[colorIdx], intensity * globalGain);
        color = addColor(color, chaseColor);
      }

      led.setPixelColor(outIdx, px, color);
    }
  }

  led.show();
}
