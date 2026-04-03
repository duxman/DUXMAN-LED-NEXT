#include "effects/EffectLavaFlow.h"

#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectLavaFlow::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectLavaFlow;
}

void EffectLavaFlow::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float speed = 0.08f + (s.effectSpeed / 100.0f) * 1.8f;
  const float deform = 0.05f + ((s.effectLevel - 1) / 9.0f) * 0.45f;
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
      const uint32_t base = scaleColorFloat(s.backgroundColor, gain);
      const uint32_t glow = scaleColorFloat(lava, gain);
      led.setPixelColor(outIdx, px, addColor(base, glow));
    }
  }

  led.show();
}
