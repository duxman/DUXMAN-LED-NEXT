/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectAudioPulse.h
 * Last commit: a67d822 - 2026-04-12
 */

#pragma once

#include "effects/EffectEngine.h"

/**
 * EffectAudioPulse: VU metro simétrico reactivo al audio.
 *
 * Comportamiento:
 *  - La tira se rellena simétricamente desde el CENTRO hacia los extremos
 *    en proporción al nivel de audio (audioLevel).
 *  - Un píxel "peak hold" (P6) marca el nivel pico reciente y cae lentamente.
 *  - En cada beat detectado (P2) se produce un destello blanco de ~130 ms.
 *  - El color hace hue-shift entre primaryColors[0→1→2] según el volumen (P4).
 *  - Sin audio reactivo activo: muestra color fijo sólido con primaryColors[0].
 *
 * Requiere: reactiveToAudio = true en CoreState para activar toda la reactividad.
 */
class EffectAudioPulse final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
  void onActivate() override;

private:
  // Timestamp del último beat detectado, para calcular el fade del destello.
  unsigned long beatFlashMs_ = 0;
};
