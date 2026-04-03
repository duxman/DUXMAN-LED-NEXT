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
  const float baseSpeed = 0.05f + (s.effectSpeed / 100.0f) * 2.0f;
  const float secondaryFreq = 0.8f + ((s.effectLevel - 1) / 9.0f) * 5.2f;
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

      const float ribbon = clamp01(0.5f + 0.35f * waveA + 0.25f * waveB);
      const float split = clamp01(0.5f + 0.5f * waveA);

      const uint32_t mixed = lerpColor(s.primaryColors[0], s.primaryColors[1], split);
      const uint32_t tinted = lerpColor(mixed, s.primaryColors[2], clamp01(0.5f + 0.5f * waveB));
      const uint32_t base = scaleColorFloat(s.backgroundColor, globalGain);
      const uint32_t glow = scaleColorFloat(tinted, ribbon * globalGain);
      led.setPixelColor(outIdx, px, addColor(base, glow));
    }
  }

  led.show();
}
