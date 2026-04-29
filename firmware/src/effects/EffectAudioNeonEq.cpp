/*
 * duxman-led next - v0.3.10-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectAudioNeonEq.cpp
 */

#include "effects/EffectAudioNeonEq.h"

#include "effects/EffectRegistry.h"

#include <math.h>

namespace {
constexpr unsigned long kBeatFlashMs = 110;

float smoothEnv(float current, float target) {
  const float rise = 0.30f;
  const float decay = 0.09f;
  const float k = (target > current) ? rise : decay;
  return current + (target - current) * k;
}
} // namespace

bool EffectAudioNeonEq::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectAudioNeonEq;
}

void EffectAudioNeonEq::onActivate() {
  beatFlashAtMs_ = millis() - kBeatFlashMs;
  lowEnv_ = 0.0f;
  midEnv_ = 0.0f;
  highEnv_ = 0.0f;
}

void EffectAudioNeonEq::renderFrame() {
  CoreState &s = state();
  LedDriver &led = driver();

  const float bright = clamp01(static_cast<float>(s.brightness) / 255.0f);
  const float speedNorm = speed01(s.effectSpeed);
  const float levelNorm = level01(s.effectLevel);
  const float nowSec = normalizedTimeSec();

  const float audio01 = s.reactiveToAudio ? clamp01(static_cast<float>(s.audioLevel) / 255.0f) : 0.35f;
  const float peak01 = s.reactiveToAudio ? clamp01(static_cast<float>(s.audioPeakHold) / 255.0f) : 0.50f;

  if (s.reactiveToAudio && s.beatDetected) {
    beatFlashAtMs_ = millis();
  }
  const float beatFlash = s.reactiveToAudio
      ? clamp01(1.0f - static_cast<float>(millis() - beatFlashAtMs_) / static_cast<float>(kBeatFlashMs))
      : 0.0f;

  // Derivacion simple de bandas desde nivel y pico.
  const float lowTarget = clamp01(audio01 * 0.75f + beatFlash * 0.65f);
  const float midTarget = clamp01((audio01 * 0.55f + peak01 * 0.45f) * (0.75f + 0.25f * levelNorm));
  const float highTarget = clamp01(peak01 * (0.70f + 0.30f * audio01));

  lowEnv_ = smoothEnv(lowEnv_, lowTarget);
  midEnv_ = smoothEnv(midEnv_, midTarget);
  highEnv_ = smoothEnv(highEnv_, highTarget);

  const uint32_t cols[3] = {s.primaryColors[0], s.primaryColors[1], s.primaryColors[2]};
  const float envs[3] = {lowEnv_, midEnv_, highEnv_};

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) {
      continue;
    }

    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 2) {
      const uint8_t bi = outIdx % 3;
      const float g = clamp01(envs[bi] + beatFlash * (bi == 0 ? 0.45f : 0.15f));
      uint32_t c = cols[bi];
      c = lerpColor(c, 0xFFFFFF, beatFlash * 0.35f);
      led.setOutputColor(outIdx, scaleColorFloat(c, g * bright));
      continue;
    }

    const uint8_t bars = max<uint8_t>(3, s.sectionCount);
    const uint16_t barSize = resolveSectionSize(out.ledCount, bars);
    const float drift = nowSec * (0.45f + speedNorm * 3.2f);

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      const uint8_t bar = min<uint8_t>(bars - 1, static_cast<uint8_t>(px / max<uint16_t>(1, barSize)));
      const uint8_t bi = bar % 3;
      const uint16_t barStart = static_cast<uint16_t>(bar) * barSize;
      const uint16_t local = static_cast<uint16_t>(px - barStart);
      const uint16_t usable = min<uint16_t>(barSize, static_cast<uint16_t>(out.ledCount - barStart));

      // Alterna direccion de llenado por barra para look mas dinamico.
      const bool reverse = (bar & 0x01u) != 0;
      const float pos01 = usable > 1
          ? static_cast<float>(reverse ? (usable - 1 - local) : local) / static_cast<float>(usable - 1)
          : 0.0f;

      const float wobble = 0.08f * sinf((static_cast<float>(bar) * 0.9f + pos01 * 7.0f + drift) * 2.0f * PI);
      const float barLevel = clamp01(envs[bi] + wobble);
      const float headPos = clamp01(barLevel);

      float intensity = 0.0f;
      if (pos01 <= barLevel) {
        // Cuerpo de barra con caida suave para efecto neón.
        intensity = 0.20f + 0.80f * smoothstep(0.0f, 1.0f, 1.0f - ((barLevel - pos01) / max(barLevel, 0.001f)));
      }

      // Cabezal brillante (peak dot) en el frente de cada barra.
      const float headDist = fabsf(pos01 - headPos);
      if (headDist < 0.05f) {
        intensity = max(intensity, 0.85f + 0.15f * beatFlash);
      }

      uint32_t c = cols[bi];
      c = lerpColor(c, 0xFFFFFF, beatFlash * (bi == 0 ? 0.45f : 0.18f));

      // Glow de fondo global para que nunca quede completamente apagado.
      const uint32_t base = scaleColorFloat(s.backgroundColor, bright * (0.06f + 0.10f * beatFlash));
      const uint32_t fg = scaleColorFloat(c, clamp01(intensity) * bright);
      led.setPixelColor(outIdx, px, addColor(base, fg));
    }
  }

  led.show();
}
