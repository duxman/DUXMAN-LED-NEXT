/*
 * duxman-led next - v0.3.11-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/audio-reactive/EffectAudioSectionStrobe.h
 * Effect: Audio Reactive Section Strobe
 */

#pragma once

#include "effects/EffectEngine.h"

/**
 * EffectAudioSectionStrobe: Strobe de secciones reactivo al audio (estilo discoteca).
 *
 * CONCEPTO
 *   Los LEDs se dividen en 2-4 secciones (según ledCount).
 *   Cada sección reacciona INDEPENDIENTEMENTE al audio.
 *   Las secciones se iluminan por turno en cada beat (efecto tipo Knight Rider circular).
 *
 * COMPORTAMIENTO
 *   - Las secciones se iluminan secuencialmente en cada beat
 *   - Color: Ciclado por sección (Rojo → Verde → Azul → Magenta)
 *   - Flash intenso con decay rápido (tipo strobe/flash)
 *   - Velocidad de rotación depende del audioLevel
 *   - En silencio: pulsación lenta y constante
 *   - En música: cambios rápidos y dramáticos
 *
 * PARÁMETROS DE CoreState usados
 *   audioLevel     → nivel RMS instantáneo       [0..255]
 *   beatDetected   → pulso de beat               bool
 *   brightness     → brillo global
 *   speed          → velocidad de rotación
 */
class EffectAudioSectionStrobe final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
  void onActivate() override;

private:
  // Sección activa (0 = primera, 1 = segunda, etc)
  uint8_t activeSectionIndex_ = 0;

  // Tiempo del último beat
  unsigned long beatMs_ = 0;

  // Paleta de colores para las secciones
  static constexpr uint32_t kColorSectionA = 0xFF0000; // Rojo
  static constexpr uint32_t kColorSectionB = 0x00FF00; // Verde
  static constexpr uint32_t kColorSectionC = 0x0000FF; // Azul
  static constexpr uint32_t kColorSectionD = 0xFF00FF; // Magenta

  // Duración del flash (ms)
  static constexpr unsigned long kFlashDurationMs = 80;

  // Número de secciones (dinámico según ledCount)
  static constexpr uint8_t kMinSections = 2;
  static constexpr uint8_t kMaxSections = 4;
};
