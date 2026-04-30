/*
 * duxman-led next - v0.3.11-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/audio-reactive/EffectAudioSpectrumChase.h
 * Effect: Audio Reactive Spectrum Chase
 */

#pragma once

#include "effects/EffectEngine.h"

/**
 * EffectAudioSpectrumChase: Chase de espectro reactivo al audio.
 *
 * CONCEPTO
 *   Los LEDs "corren" como un patrón type Knight Rider 360°.
 *   La velocidad depende de audioLevel (silencio lento, música rápido).
 *   El color depende del rango de frecuencia:
 *     - Bajos (audioLevel 0-85): Rojo
 *     - Medios (audioLevel 85-170): Verde
 *     - Altos (audioLevel 170-255): Azul
 *
 * COMPORTAMIENTO
 *   - Una línea brillante "cabeza" que corre alrededor de la tira
 *   - Una cola que decae suavemente detrás de la cabeza
 *   - La velocidad se multiplica por el audioLevel
 *   - En cada beat: aumento de velocidad y cambio de color dramático
 *
 * PARÁMETROS DE CoreState usados
 *   audioLevel     → nivel RMS instantáneo       [0..255]
 *   beatDetected   → pulso de beat               bool
 *   brightness     → brillo global
 *   speed          → velocidad base del chase
 */
class EffectAudioSpectrumChase final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
  void onActivate() override;

private:
  // Posición de la cabeza del chase (0.0 = inicio, 1.0 = fin)
  float chasePosition_ = 0.0f;

  // Tiempo del último beat
  unsigned long beatMs_ = 0;

  // Paleta de colores por rango
  static constexpr uint32_t kColorBass    = 0xFF0000; // Rojo
  static constexpr uint32_t kColorMid     = 0x00FF00; // Verde
  static constexpr uint32_t kColorTreble  = 0x0000FF; // Azul

  // Ancho de la cola (en píxeles relativos)
  static constexpr float kTailWidth = 0.20f;

  // Duración del boost de beat (ms)
  static constexpr unsigned long kBeatFlashMs = 150;
};
