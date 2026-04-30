/*
 * duxman-led next - v0.3.9-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectAudioSpectrum.h
 */

#pragma once

#include "effects/EffectEngine.h"

/**
 * EffectAudioSpectrum: VU-meter de 3 bandas (bajos / medios / altos).
 *
 * BANDAS Y COLORES
 *   primaryColors[0] → Bajos   : nivel derivado de beatDetected + audioLevel bajo.
 *   primaryColors[1] → Medios  : nivel proporcional a audioLevel.
 *   primaryColors[2] → Altos   : nivel proporcional a audioPeakHold.
 *
 * DISTRIBUCIÓN EN LA TIRA
 *   - Si hay ≥3 salidas activas: cada salida recibe una banda completa.
 *   - Si hay 1-2 salidas: la tira se divide en sectionCount segmentos y
 *     el segmento i dibuja la banda (i % 3).
 *   - Cada banda crece desde FUERA hacia el CENTRO del segmento (VU clásico).
 *
 * MODO SIN AUDIO REACTIVO
 *   Muestra los 3 colores primarios fijos en sus respectivos segmentos.
 *
 * PARÁMETROS DE CoreState usados
 *   audioLevel     → nivel RMS instantáneo       [0..255]
 *   audioPeakHold  → pico con decaimiento lento  [0..255]
 *   beatDetected   → pulso de beat               bool
 *   sectionCount   → número de segmentos en modo tira única
 *   brightness     → brillo global
 *   primaryColors  → color de cada banda
 */
class EffectAudioSpectrum final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
  void onActivate() override;

private:
  // Nivel suavizado por banda, 0.0..1.0.
  float levelLow_  = 0.0f;
  float levelMid_  = 0.0f;
  float levelHigh_ = 0.0f;

  // Tiempo del último beat para generar el flash de bajos.
  unsigned long beatMs_ = 0;

  // Dibuja un segmento de tira como VU de una banda concreta.
  void drawBand(LedDriver &led, uint8_t outIdx,
                uint16_t startPx, uint16_t segLen,
                float bandLevel, float beatFlash,
                uint32_t color, float brightness) const;
};
