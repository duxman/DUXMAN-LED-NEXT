/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectAudioPulse.cpp
 * Last commit: a67d822 - 2026-04-12
 */

#include "effects/EffectAudioPulse.h"

#include "effects/EffectRegistry.h"

bool EffectAudioPulse::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectAudioPulse;
}

void EffectAudioPulse::onActivate() {
  // Evita que aparezca un destello espurio justo al activar el efecto.
  beatFlashMs_ = millis() - 200UL;
}

void EffectAudioPulse::renderFrame() {
  CoreState &s  = state();
  LedDriver  &led = driver();

  const float audio01   = clamp01(static_cast<float>(s.audioLevel)    / 255.0f);
  const float peak01    = clamp01(static_cast<float>(s.audioPeakHold) / 255.0f);
  const float brightness = s.brightness / 255.0f;

  // --- Beat flash (P2) ---
  const unsigned long nowMs = millis();
  if (s.reactiveToAudio && s.beatDetected) {
    beatFlashMs_ = nowMs;
  }
  // flash decae a cero en 130 ms desde el último beat
  const float beatIntensity = clamp01(1.0f - static_cast<float>(nowMs - beatFlashMs_) / 130.0f);

  // --- Color dinámico: hue-shift por nivel de audio (P4) ---
  // Silencio → primaryColors[0], volumen medio → [1], pico → [2]
  const uint32_t activeColor = audioColorShift(
      s.primaryColors[0], s.primaryColors[1], s.primaryColors[2]);

  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled) continue;

    // Salidas sin control por píxel: solo brillo global con beat flash.
    if (!led.supportsPerPixelColor(outIdx) || out.ledCount <= 1) {
      const float g = s.reactiveToAudio
          ? clamp01(audio01 + beatIntensity * 0.5f)
          : 1.0f;
      led.setOutputColor(outIdx, scaleColorFloat(activeColor, g * brightness));
      continue;
    }

    // Ancho efectivo de un píxel en espacio normalizado [0..1] para peak hold.
    const float pixelWidth = 2.5f / static_cast<float>(out.ledCount);

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      const float x = normalizedX(px, out.ledCount); // [0.0 .. 1.0]
      // distFromCenter: 0 en el centro, 1 en los extremos (simetría doble).
      const float distFromCenter = fabsf(x - 0.5f) * 2.0f;

      float intensity = 0.0f;

      if (s.reactiveToAudio) {
        // 1. VU fill: rellena desde el centro hacia los extremos.
        //    Píxel activo cuando distFromCenter <= audio01.
        if (audio01 > 0.0f && distFromCenter <= audio01) {
          // Degradado de brillo: máximo en el centro, mínimo en el frente del fill.
          const float proximity = 1.0f - (distFromCenter / audio01);
          intensity = 0.35f + proximity * 0.65f;
        }

        // 2. Peak hold (P6): par de píxeles brillantes en la posición del pico.
        if (fabsf(distFromCenter - peak01) < pixelWidth) {
          intensity = max(intensity, peak01 * 0.95f);
        }

        // 3. Beat flash overlay: sube toda la tira hacia blanco (P2).
        intensity = clamp01(intensity + beatIntensity * 0.85f);

      } else {
        // Sin reactividad: color sólido a brillo completo.
        intensity = 1.0f;
      }

      // Lavado blanco durante el beat flash.
      uint32_t col = activeColor;
      if (beatIntensity > 0.0f) {
        col = lerpColor(col, 0xFFFFFF, beatIntensity * 0.55f);
      }

      led.setPixelColor(outIdx, px, scaleColorFloat(col, clamp01(intensity) * brightness));
    }
  }
  led.show();
}
