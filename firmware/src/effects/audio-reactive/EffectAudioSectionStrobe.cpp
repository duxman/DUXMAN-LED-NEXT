/*
 * duxman-led next - v0.3.11-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/audio-reactive/EffectAudioSectionStrobe.cpp
 */

#include "effects/audio-reactive/EffectAudioSectionStrobe.h"
#include "effects/EffectRegistry.h"

#include <math.h>

bool EffectAudioSectionStrobe::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectAudioSectionStrobe;
}

void EffectAudioSectionStrobe::onActivate() {
  activeSectionIndex_ = 0;
  beatMs_ = millis() - kFlashDurationMs;
}

uint32_t getSectionColor(uint8_t sectionIndex) {
  switch (sectionIndex % 4) {
    case 0: return 0xFF0000; // Rojo
    case 1: return 0x00FF00; // Verde
    case 2: return 0x0000FF; // Azul
    case 3: return 0xFF00FF; // Magenta
    default: return 0xFFFFFF;
  }
}

void EffectAudioSectionStrobe::renderFrame() {
  CoreState &s   = state();
  LedDriver  &led = driver();

  const float brightness = clamp01(static_cast<float>(s.brightness) / 255.0f);
  const unsigned long nowMs = millis();

  // ── Beat detection y rotación de secciones ──────────────────────────
  const float rawAudio = clamp01(static_cast<float>(s.audioLevel) / 255.0f);

  if (s.reactiveToAudio && s.beatDetected) {
    beatMs_ = nowMs;
    // Avanzar a la siguiente sección en cada beat
    activeSectionIndex_ = (activeSectionIndex_ + 1) % kMaxSections;
  }

  // Flash decay
  const float flashDecay = clamp01(
      1.0f - static_cast<float>(nowMs - beatMs_) / static_cast<float>(kFlashDurationMs));

  // ── Renderizar en cada salida ────────────────────────────────────────
  for (uint8_t outIdx = 0; outIdx < led.outputCount(); ++outIdx) {
    const LedDriverOutputConfig &out = led.outputConfig(outIdx);
    if (!out.enabled || out.ledCount == 0) continue;

    if (!led.supportsPerPixelColor(outIdx)) {
      // Sin soporte per-pixel: mostrar color único por sección
      uint32_t secColor = getSectionColor(activeSectionIndex_);
      uint32_t solidColor = scaleColorFloat(secColor, (0.15f + flashDecay * 0.85f) * brightness);
      led.setOutputColor(outIdx, solidColor);
      continue;
    }

    // Calcular número de secciones basado en ledCount
    const uint8_t numSections = max<uint8_t>(
        kMinSections,
        min<uint8_t>(kMaxSections, max<uint8_t>(1, out.ledCount / 8))
    );
    const uint16_t pixelsPerSection = out.ledCount / numSections;

    for (uint16_t px = 0; px < out.ledCount; ++px) {
      // Determinar a qué sección pertenece este píxel
      const uint8_t sectionIdx = px / pixelsPerSection;
      const uint8_t sectionIdxClamped = min<uint8_t>(sectionIdx, numSections - 1);

      // Posición dentro de la sección (0.0..1.0)
      const uint16_t posInSection = px % pixelsPerSection;
      const float normalizedPos = static_cast<float>(posInSection) / static_cast<float>(max<uint16_t>(1, pixelsPerSection));

      // ── Intensidad base ────────────────────────────────────────────
      float intensity = rawAudio * 0.15f; // halo de fondo

      // Si es la sección activa: flash intenso
      if (sectionIdxClamped == activeSectionIndex_) {
        // Perfil de flash: máximo en el centro de la sección
        const float distFromCenter = fabsf(normalizedPos - 0.5f);
        const float flashProfile = 1.0f - (distFromCenter * 2.0f); // 0 en bordes, 1 en centro
        intensity += flashProfile * flashDecay * 0.95f;
      }

      // Color según sección
      uint32_t pixelColor = getSectionColor(sectionIdxClamped);

      intensity = clamp01(intensity);
      uint32_t finalColor = scaleColorFloat(pixelColor, intensity * brightness);

      led.setPixelColor(outIdx, px, finalColor);
    }
  }

  led.show();
}
