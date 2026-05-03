/*
 * duxman-led next - v0.3.11-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/audio-reactive/EffectAudioRainbowWave.cpp
 */

#include "effects/audio-reactive/EffectAudioRainbowWave.h"
#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectAudioRainbowWave::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectAudioRainbowWave;
}

void EffectAudioRainbowWave::onActivate() {
  wavePosition_ = 0.0f;
  beatMs_ = millis() - kBeatFlashMs;
}

void EffectAudioRainbowWave::renderFrame() {
  CoreState &s   = state();
  LedDriver  &led = driver();

  const float brightness = clamp01(static_cast<float>(s.brightness) / 255.0f);
  const unsigned long nowMs = millis();

  // ── Audio driven color and wave animation ───────────────────────────
  const float rawAudio = clamp01(static_cast<float>(s.audioLevel) / 255.0f);

  // Color según nivel de audio: Rojo → Naranja → Amarillo → Blanco
  uint32_t waveColor;
  if (rawAudio < 0.33f) {
    // Rojo a Naranja
    const float t = rawAudio / 0.33f;
    waveColor = lerpColor(kColorRed, kColorOrange, t);
  } else if (rawAudio < 0.66f) {
    // Naranja a Amarillo
    const float t = (rawAudio - 0.33f) / 0.33f;
    waveColor = lerpColor(kColorOrange, kColorYellow, t);
  } else {
    // Amarillo a Blanco
    const float t = (rawAudio - 0.66f) / 0.34f;
    waveColor = lerpColor(kColorYellow, kColorWhite, t);
  }

  // Beat flash
  if (s.reactiveToAudio && s.beatDetected) {
    beatMs_ = nowMs;
  }
  const float beatFlash = clamp01(
      1.0f - static_cast<float>(nowMs - beatMs_) / static_cast<float>(kBeatFlashMs));

  // Onda que viaja: amplificar velocidad con audio
  const float speedMultiplier = 0.5f + rawAudio * 1.5f; // 0.5x a 2x
  const float speedNorm = speed01(s.effectSpeed);
  const float waveSpeed = speedNorm * 0.015f * speedMultiplier;
  wavePosition_ += waveSpeed;
  if (wavePosition_ > 1.0f) {
    wavePosition_ -= 1.0f;
  }

  // ── Renderizar en cada salida ────────────────────────────────────────
  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled || out.ledCount == 0) continue;

    if (!led.supportsPerPixelColor(outIdx)) {
      // Sin soporte per-pixel: mostrar color único
      uint32_t solidColor = scaleColorFloat(waveColor, 0.3f + rawAudio * 0.7f * brightness);
      led.setOutputColor(outIdx, solidColor);
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      // Normalizar posición del píxel
      const float normalizedPx = static_cast<float>(px) / static_cast<float>(out.ledCount);

      // Distancia a la onda
      const float distToWave = fabsf(normalizedPx - wavePosition_);
      const float wrapDist = fminf(distToWave, 1.0f - distToWave); // envolver cíclicamente

      // Perfil de la onda: suave (gaussiana aproximada)
      float intensity = 0.0f;
      if (wrapDist < kWaveWidth) {
        // Curva suave: máximo en el centro de la onda, baja hacia afuera
        const float t = wrapDist / kWaveWidth;
        intensity = 1.0f - t * t; // parábola suave
      }

      // Base de brillo (incluso fuera de la onda)
      intensity += rawAudio * 0.3f; // halo de fondo reactivo

      // Flash de beat
      intensity += beatFlash * 0.4f;

      intensity = clamp01(intensity);

      // Aplicar color y brillo
      uint32_t pixelColor = scaleColorFloat(waveColor, intensity * brightness);
      led.setPixelColor(outIdx, px, pixelColor);
    }
  }

  led.show();
}
