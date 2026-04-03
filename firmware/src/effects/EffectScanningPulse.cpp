#include "effects/EffectScanningPulse.h"

#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectScanningPulse::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectScanningPulse;
}

void EffectScanningPulse::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float speedHz = 0.05f + (s.effectSpeed / 100.0f) * 2.45f;
  const float globalGain = s.brightness / 255.0f;

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) {
      continue;
    }

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      const uint8_t colorIdx = static_cast<uint8_t>(fmodf(t * speedHz * 3.0f, 3.0f));
      led.setOutputColor(outIdx, scaleColorFloat(s.primaryColors[colorIdx], globalGain));
      continue;
    }

    const float ledCountF = static_cast<float>(out.ledCount);
    const float phase = fmodf(t * speedHz, 2.0f);
    const float pingPong = phase <= 1.0f ? phase : (2.0f - phase);
    const float center = pingPong * (ledCountF - 1.0f);

    // effectLevel 1..10 -> anchura 4%..30% de la tira
    const float halfWidth = max(1.0f, ledCountF * (0.04f + ((s.effectLevel - 1) / 9.0f) * 0.26f));

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      const float dist = fabsf(static_cast<float>(px) - center);
      float intensity = 0.0f;
      if (dist <= halfWidth) {
        const float norm = 1.0f - (dist / halfWidth);
        intensity = smoothstep(0.0f, 1.0f, norm);
      }

      const float x = normalizedX(px, out.ledCount);
      const uint8_t colorIdx = static_cast<uint8_t>((static_cast<uint32_t>(x * max<uint8_t>(1, s.sectionCount) * 3.0f)) % 3u);
      uint32_t color = scaleColorFloat(s.backgroundColor, globalGain);
      if (intensity > 0.0f) {
        color = addColor(color, scaleColorFloat(s.primaryColors[colorIdx], intensity * globalGain));
      }
      led.setPixelColor(outIdx, px, color);
    }
  }

  led.show();
}
