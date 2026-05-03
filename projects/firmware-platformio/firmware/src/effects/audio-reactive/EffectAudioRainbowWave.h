/*
 * duxman-led next - v0.3.11-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/audio-reactive/EffectAudioRainbowWave.h
 * Effect: Audio Reactive Rainbow Wave
 */

#pragma once

#include "effects/EffectEngine.h"

/**
 * EffectAudioRainbowWave: Onda de colores arcoíris reactiva al audio.
 *
 * CONCEPTO
 *   Los LEDs se dividen en secciones que cambian de color en función del nivel de audio.
 *   La paleta de colores va: Rojo → Naranja → Amarillo → Blanco (según audioLevel).
 *   Una onda de brillo amplificado corre de lado a lado en cada beat.
 *
 * COMPORTAMIENTO
 *   - Silencio (audioLevel 0-50): Rojo dimmed
 *   - Bajo-Medio (audioLevel 50-150): Naranja → Amarillo
 *   - Alto (audioLevel 150-255): Amarillo → Blanco brillante
 *   - En cada beat: flash de blanco/amplificación de brillo en el centro
 *   - Onda que viaja: se mueve progresivamente de un extremo al otro
 *
 * PARÁMETROS DE CoreState usados
 *   audioLevel     → nivel RMS instantáneo       [0..255]
 *   beatDetected   → pulso de beat               bool
 *   brightness     → brillo global
 *   speed          → velocidad de la onda
 */
class EffectAudioRainbowWave final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
  void onActivate() override;

private:
  // Posición de la onda (0.0 = inicio, 1.0 = fin)
  float wavePosition_ = 0.0f;

  // Tiempo del último beat
  unsigned long beatMs_ = 0;

  // Paleta de colores para la onda
  static constexpr uint32_t kColorRed     = 0xFF0000;
  static constexpr uint32_t kColorOrange  = 0xFF8000;
  static constexpr uint32_t kColorYellow  = 0xFFFF00;
  static constexpr uint32_t kColorWhite   = 0xFFFFFF;

  // Ancho de la onda (en píxeles relativos, 0.0..1.0)
  static constexpr float kWaveWidth = 0.15f;

  // Duración del flash de beat (ms)
  static constexpr unsigned long kBeatFlashMs = 100;

  // Suavizado de la onda
  static constexpr float kWaveSmoothK = 0.08f;
};
