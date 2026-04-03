#include "effects/EffectRandomColorPop.h"

#include "effects/EffectRegistry.h"

#include <math.h>

namespace {
uint32_t lcg(uint32_t &seed) {
  seed = seed * 1103515245u + 12345u;
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

bool EffectRandomColorPop::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectRandomColorPop;
}

void EffectRandomColorPop::onActivate() {
  seed_ = (millis() << 1) ^ 0x55AA11EEu;
}

void EffectRandomColorPop::onDeactivate() {
  seed_ = 7;
}

void EffectRandomColorPop::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float t = normalizedTimeSec();
  const float speed01 = (s.effectSpeed - 1) / 99.0f;
  const float level01 = (s.effectLevel - 1) / 9.0f;
  const float fadeSpeed = 0.12f + speed01 * 4.2f;
  const float spawn = 0.02f + level01 * 0.30f;
  const float gain = s.brightness / 255.0f;
  const float stepRate = 1.0f + speed01 * 14.0f;
  const uint32_t frameKey = static_cast<uint32_t>(t * stepRate);

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) continue;

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      uint32_t r = hash32(seed_ ^ frameKey ^ outIdx);
      uint8_t idx = static_cast<uint8_t>(r % 3u);
      float env = 0.25f + 0.75f * (0.5f + 0.5f * sinf(t * fadeSpeed * 2.0f * PI));
      led.setOutputColor(outIdx, scaleColorFloat(s.primaryColors[idx], env * gain));
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      uint32_t r = hash32(seed_ ^ frameKey ^ (outIdx * 263u + px * 1013u));
      float rnd = (r & 0xFFFF) / 65535.0f;
      float pop = 0.0f;
      if (rnd < spawn) {
        pop = 0.75f + 0.25f * level01;
      } else {
        pop = 0.06f + 0.30f * (0.5f + 0.5f * sinf((t * fadeSpeed + px * 0.031f) * 2.0f * PI));
      }

      uint8_t idx = static_cast<uint8_t>((r >> 8) % 3u);
      uint32_t base = scaleColorFloat(s.backgroundColor, gain * (0.12f + 0.28f * (1.0f - level01)));
      uint32_t light = scaleColorFloat(s.primaryColors[idx], clamp01(pop) * gain);
      led.setPixelColor(outIdx, px, addColor(base, light));
    }
  }

  led.show();
}
