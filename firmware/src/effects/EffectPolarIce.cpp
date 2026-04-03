#include "effects/EffectPolarIce.h"

#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectPolarIce::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectPolarIce;
}

void EffectPolarIce::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float speed = 0.05f + (s.effectSpeed / 100.0f) * 1.6f;
  const float contrast = 0.2f + ((s.effectLevel - 1) / 9.0f) * 0.8f;
  const float repeats = static_cast<float>(max<uint8_t>(1, s.sectionCount));
  const float gain = s.brightness / 255.0f;

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) continue;

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      uint32_t c = lerpColor(s.primaryColors[1], s.primaryColors[2], 0.5f + 0.5f * sinf(t * speed));
      led.setOutputColor(outIdx, scaleColorFloat(c, gain));
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      const float x = normalizedX(px, out.ledCount);
      const float a = sinf((x * repeats + t * speed) * 2.0f * PI);
      const float b = sinf((x * repeats * 1.7f - t * speed * 1.2f) * 2.0f * PI);
      const float mix = clamp01(0.5f + 0.5f * (a * (1.0f - 0.5f * contrast) + b * 0.5f * contrast));

      const uint32_t cold = lerpColor(s.primaryColors[2], s.primaryColors[1], mix);
      const uint32_t ice = lerpColor(cold, s.primaryColors[0], 0.15f * (1.0f - mix));
      const uint32_t base = scaleColorFloat(s.backgroundColor, gain);
      const uint32_t glow = scaleColorFloat(ice, gain);
      led.setPixelColor(outIdx, px, addColor(base, glow));
    }
  }

  led.show();
}
