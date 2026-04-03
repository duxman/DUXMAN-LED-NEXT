#include "effects/EffectGradientMeteor.h"

#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectGradientMeteor::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectGradientMeteor;
}

void EffectGradientMeteor::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float speedHz = 0.05f + (s.effectSpeed / 100.0f) * 2.0f;
  const float globalGain = s.brightness / 255.0f;

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) {
      continue;
    }

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      const uint32_t color = gradientColor(s.primaryColors[0], s.primaryColors[1], s.primaryColors[2],
                                           static_cast<uint16_t>(fmodf(t * speedHz * 100.0f, 100.0f)), 100);
      led.setOutputColor(outIdx, scaleColorFloat(color, globalGain));
      continue;
    }

    const float ledCountF = static_cast<float>(out.ledCount);
    const float headPos = fmodf(t * speedHz * ledCountF, ledCountF);

    // effectLevel 1..10 -> cola de 8%..55% de la tira
    const float tailLen = max(1.0f, ledCountF * (0.08f + ((s.effectLevel - 1) / 9.0f) * 0.47f));

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      float dist = headPos - static_cast<float>(px);
      if (dist < 0.0f) {
        dist += ledCountF;
      }

      float intensity = 0.0f;
      if (dist <= tailLen) {
        const float norm = 1.0f - (dist / tailLen);
        intensity = smoothstep(0.0f, 1.0f, norm);
      }

      uint32_t color = scaleColorFloat(s.backgroundColor, globalGain);
      if (intensity > 0.0f) {
        const uint32_t meteorColor = gradientColor(s.primaryColors[0], s.primaryColors[1], s.primaryColors[2], px, out.ledCount);
        color = addColor(color, scaleColorFloat(meteorColor, intensity * globalGain));
      }

      led.setPixelColor(outIdx, px, color);
    }
  }

  led.show();
}
