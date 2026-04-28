/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectEngine.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

#include "effects/EffectEngine.h"

namespace {
uint8_t blendChannel(uint8_t start, uint8_t end, uint16_t step, uint16_t maxStep) {
  if (maxStep == 0) {
    return start;
  }
  const int delta = static_cast<int>(end) - static_cast<int>(start);
  return static_cast<uint8_t>(start + (delta * step) / maxStep);
}
} // namespace

EffectEngine::EffectEngine(CoreState &state, LedDriver &driver) : state_(state), driver_(driver) {}

void EffectEngine::begin() {
  driver_.begin();
}

float EffectEngine::reactiveAudio01() const {
  if (!state_.reactiveToAudio) {
    return 1.0f;
  }
  return static_cast<float>(state_.audioLevel) / 255.0f;
}

float EffectEngine::reactiveGain(float minGain, float maxGain) const {
  if (!state_.reactiveToAudio) {
    return 1.0f;
  }
  const float audio = clamp01(reactiveAudio01());
  // Curva agresiva: hace más visibles las variaciones en niveles bajos/medios.
  const float shaped = powf(audio, 0.55f);
  const float tunedMin = min(minGain, 0.20f);
  const float tunedMax = max(maxGain, 2.40f);
  return tunedMin + (tunedMax - tunedMin) * shaped;
}

uint32_t EffectEngine::scaleColor(uint32_t color, uint8_t brightness) const {
  const float baseGain = static_cast<float>(brightness) / 255.0f;
  const float gain = clamp01(baseGain * reactiveGain());
  const uint32_t red = static_cast<uint32_t>(((color >> 16) & 0xFF) * gain);
  const uint32_t green = static_cast<uint32_t>(((color >> 8) & 0xFF) * gain);
  const uint32_t blue = static_cast<uint32_t>((color & 0xFF) * gain);
  return (red << 16) | (green << 8) | blue;
}

namespace {
uint32_t blendColor(uint32_t start, uint32_t end, uint16_t step, uint16_t maxStep) {
  const uint8_t startRed = static_cast<uint8_t>((start >> 16) & 0xFF);
  const uint8_t startGreen = static_cast<uint8_t>((start >> 8) & 0xFF);
  const uint8_t startBlue = static_cast<uint8_t>(start & 0xFF);
  const uint8_t endRed = static_cast<uint8_t>((end >> 16) & 0xFF);
  const uint8_t endGreen = static_cast<uint8_t>((end >> 8) & 0xFF);
  const uint8_t endBlue = static_cast<uint8_t>(end & 0xFF);

  return (static_cast<uint32_t>(blendChannel(startRed, endRed, step, maxStep)) << 16) |
         (static_cast<uint32_t>(blendChannel(startGreen, endGreen, step, maxStep)) << 8) |
         static_cast<uint32_t>(blendChannel(startBlue, endBlue, step, maxStep));
}
} // namespace

uint32_t EffectEngine::gradientColor(uint32_t colorA, uint32_t colorB, uint32_t colorC,
                                     uint16_t pixelIndex, uint16_t pixelCount) {
  if (pixelCount <= 1) {
    return colorA;
  }

  const uint32_t position = static_cast<uint32_t>(pixelIndex) * 512UL /
                            static_cast<uint32_t>(pixelCount - 1);
  if (position <= 256UL) {
    return blendColor(colorA, colorB, static_cast<uint16_t>(position), 256);
  }

  return blendColor(colorB, colorC, static_cast<uint16_t>(position - 256UL), 256);
}

uint16_t EffectEngine::resolveSectionSize(uint16_t ledCount, uint8_t sectionCount) {
  const uint8_t safeSections = max<uint8_t>(1, sectionCount);
  return max<uint16_t>(1, static_cast<uint16_t>((ledCount + safeSections - 1) / safeSections));
}

unsigned long EffectEngine::effectIntervalMs(uint8_t speedScale) const {
  const float s = speed01(speedScale);
  // 1 -> ~1200 ms (lento), 100 -> ~40 ms (rapido)
  unsigned long base = static_cast<unsigned long>(1200.0f - 1160.0f * s);
  // P3: el audio acelera los efectos animados cuando reactiveToAudio está activo.
  if (state_.reactiveToAudio) {
    const float audio      = clamp01(static_cast<float>(state_.audioLevel) / 255.0f);
    const float speedBoost = 1.0f + audio * 2.0f; // 1x en silencio → 3x en volumen máximo
    base = static_cast<unsigned long>(static_cast<float>(base) / speedBoost);
  }
  return max(base, 20UL);
}

float EffectEngine::speed01(uint8_t speedScale) {
  return (constrain(speedScale, static_cast<uint8_t>(1), static_cast<uint8_t>(100)) - 1) / 99.0f;
}

float EffectEngine::level01(uint8_t levelScale) {
  return (constrain(levelScale, static_cast<uint8_t>(1), static_cast<uint8_t>(10)) - 1) / 9.0f;
}

// ── Helpers matemáticos para efectos dinámicos ────────────────────────────

float EffectEngine::normalizedX(uint16_t pixelIndex, uint16_t pixelCount) {
  if (pixelCount <= 1) return 0.0f;
  return static_cast<float>(pixelIndex) / static_cast<float>(pixelCount - 1);
}

float EffectEngine::normalizedTimeSec() {
  return static_cast<float>(millis()) / 1000.0f;
}

float EffectEngine::clamp01(float v) {
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

float EffectEngine::smoothstep(float edge0, float edge1, float x) {
  const float t = clamp01((x - edge0) / (edge1 - edge0));
  return t * t * (3.0f - 2.0f * t);
}

uint32_t EffectEngine::lerpColor(uint32_t colorA, uint32_t colorB, float t) {
  const float tc = clamp01(t);
  const float inv = 1.0f - tc;
  const uint8_t r = static_cast<uint8_t>(((colorA >> 16) & 0xFF) * inv + ((colorB >> 16) & 0xFF) * tc);
  const uint8_t g = static_cast<uint8_t>(((colorA >> 8)  & 0xFF) * inv + ((colorB >> 8)  & 0xFF) * tc);
  const uint8_t b = static_cast<uint8_t>((colorA & 0xFF)         * inv + (colorB & 0xFF)         * tc);
  return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

uint32_t EffectEngine::addColor(uint32_t colorA, uint32_t colorB) {
  const uint32_t r = min(255u, ((colorA >> 16) & 0xFF) + ((colorB >> 16) & 0xFF));
  const uint32_t g = min(255u, ((colorA >> 8)  & 0xFF) + ((colorB >> 8)  & 0xFF));
  const uint32_t b = min(255u, (colorA & 0xFF)         + (colorB & 0xFF));
  return (r << 16) | (g << 8) | b;
}

uint32_t EffectEngine::scaleColorFloat(uint32_t color, float gain) const {
  const float g = clamp01(gain * reactiveGain());
  const uint8_t r = static_cast<uint8_t>(((color >> 16) & 0xFF) * g);
  const uint8_t gr = static_cast<uint8_t>(((color >> 8)  & 0xFF) * g);
  const uint8_t b = static_cast<uint8_t>((color & 0xFF)          * g);
  return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(gr) << 8) | b;
}

uint32_t EffectEngine::audioColorShift(uint32_t cA, uint32_t cB, uint32_t cC) const {
  // P4: Desplazamiento de color por nivel de audio.
  // Silencio → cA, volumen medio → cB, pico → cC.
  if (!state_.reactiveToAudio) {
    return cA;
  }
  const float level = clamp01(static_cast<float>(state_.audioLevel) / 255.0f);
  if (level <= 0.5f) {
    return lerpColor(cA, cB, level * 2.0f);
  }
  return lerpColor(cB, cC, (level - 0.5f) * 2.0f);
}

uint32_t EffectEngine::applyGamma(uint32_t color) {
  auto gammaChannel = [](uint8_t v) -> uint8_t {
    return static_cast<uint8_t>(powf(v / 255.0f, 2.2f) * 255.0f + 0.5f);
  };
  return (static_cast<uint32_t>(gammaChannel((color >> 16) & 0xFF)) << 16) |
         (static_cast<uint32_t>(gammaChannel((color >> 8)  & 0xFF)) << 8) |
         static_cast<uint32_t>(gammaChannel(color & 0xFF));
}
