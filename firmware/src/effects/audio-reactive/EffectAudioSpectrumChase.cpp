/*
 * duxman-led next - v0.3.11-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/audio-reactive/EffectAudioSpectrumChase.cpp
 */

#include "effects/audio-reactive/EffectAudioSpectrumChase.h"
#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectAudioSpectrumChase::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectAudioSpectrumChase;
}

void EffectAudioSpectrumChase::onActivate() {
  chasePosition_ = 0.0f;
  beatMs_ = millis() - kBeatFlashMs;
}

void EffectAudioSpectrumChase::renderFrame() {
  CoreState &s   = state();
  LedDriver  &led = driver();

  const float brightness = clamp01(static_cast<float>(s.brightness) / 255.0f);
  const unsigned long nowMs = millis();

  // ── Determine color based on audio spectrum ──────────────────────────
  const float rawAudio = clamp01(static_cast<float>(s.audioLevel) / 255.0f);

  uint32_t chaseColor;
  if (rawAudio < 0.33f) {
    // Bajos: Rojo puro
    chaseColor = kColorBass;
  } else if (rawAudio < 0.66f) {
    // Medios: Rojo → Verde
    const float t = (rawAudio - 0.33f) / 0.33f;
    chaseColor = lerpColor(kColorBass, kColorMid, t);
  } else {
    // Altos: Verde → Azul
    const float t = (rawAudio - 0.66f) / 0.34f;
    chaseColor = lerpColor(kColorMid, kColorTreble, t);
  }

  // Beat boost
  if (s.reactiveToAudio && s.beatDetected) {
    beatMs_ = nowMs;
  }
  const float beatBoost = clamp01(
      1.0f - static_cast<float>(nowMs - beatMs_) / static_cast<float>(kBeatFlashMs));

  // ── Chase movement ──────────────────────────────────────────────────
  // Velocidad base multiplicada por audio level + beat boost
  const float speedMultiplier = 1.0f + rawAudio * 2.0f + beatBoost * 1.5f; // 1x a 4.5x
  const float speedNorm = speed01(s.effectSpeed);
  const float chaseSpeed = speedNorm * 0.02f * speedMultiplier;
  
  chasePosition_ += chaseSpeed;
  if (chasePosition_ > 1.0f) {
    chasePosition_ -= 1.0f;
  }

  // ── Renderizar en cada salida ────────────────────────────────────────
  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled || out.ledCount == 0) continue;

    if (!led.supportsPerPixelColor(outIdx)) {
      // Sin soporte per-pixel: mostrar color único
      uint32_t solidColor = scaleColorFloat(chaseColor, 0.5f + rawAudio * 0.5f * brightness);
      led.setOutputColor(outIdx, solidColor);
      continue;
    }

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      // Normalizar posición del píxel
      const float normalizedPx = static_cast<float>(px) / static_cast<float>(out.ledCount);

      // Distancia a la cabeza del chase (considerando wrap)
      const float distToChase = fabsf(normalizedPx - chasePosition_);
      const float wrapDist = fminf(distToChase, 1.0f - distToChase);

      // Perfil de la cola: suave decay desde la cabeza
      float intensity = 0.0f;
      if (wrapDist < kTailWidth) {
        // Decay exponencial: máximo en la cabeza, baja suavemente
        const float t = wrapDist / kTailWidth;
        intensity = powf(1.0f - t, 2.0f); // decay cuadrático
      }

      // Amplificar con audio level
      intensity *= (0.5f + rawAudio * 0.5f);

      // Boost de beat para la cabeza
      if (wrapDist < 0.02f) {
        intensity += beatBoost * 0.8f;
      }

      intensity = clamp01(intensity);

      // Aplicar color y brillo
      uint32_t pixelColor = scaleColorFloat(chaseColor, intensity * brightness);
      led.setPixelColor(outIdx, px, pixelColor);
    }
  }

  led.show();
}
