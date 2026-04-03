#include "effects/EffectTrigRibbon.h"

#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectTrigRibbon::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectTrigRibbon;
}

void EffectTrigRibbon::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float speedNorm = speed01(s.effectSpeed);
  const float levelNorm = level01(s.effectLevel);
  const float levelCurve = powf(levelNorm, 1.2f);
  const float baseSpeed = 0.06f + speedNorm * 2.7f;
  const float secondaryFreq = 0.9f + levelCurve * 5.4f;
  const float repeats = static_cast<float>(max<uint8_t>(1, s.sectionCount));
  const float globalGain = s.brightness / 255.0f;

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) {
      continue;
    }

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      const uint8_t colorIdx = static_cast<uint8_t>(fmodf(t * baseSpeed * 3.0f, 3.0f));
      led.setOutputColor(outIdx, scaleColorFloat(s.primaryColors[colorIdx], globalGain));
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      const float x = normalizedX(px, out.ledCount);
      const float waveA = sinf((x * repeats + t * baseSpeed) * 2.0f * PI);
      const float waveB = sinf((x * repeats * secondaryFreq - t * baseSpeed * 1.7f) * 2.0f * PI);

      const float ribbon = clamp01(0.5f + (0.30f + 0.20f * levelNorm) * waveA + 0.22f * waveB);
      const float split = clamp01(0.5f + 0.5f * waveA);

      const uint32_t mixed = lerpColor(s.primaryColors[0], s.primaryColors[1], split);
      const uint32_t tinted = lerpColor(mixed, s.primaryColors[2], clamp01(0.5f + 0.5f * waveB));
      const uint32_t base = scaleColorFloat(s.backgroundColor, globalGain * (0.12f + 0.24f * (1.0f - levelNorm)));
      const uint32_t glow = scaleColorFloat(tinted, ribbon * globalGain * (0.58f + 0.42f * levelNorm));
      led.setPixelColor(outIdx, px, addColor(base, glow));
    }
  }

  led.show();
}
