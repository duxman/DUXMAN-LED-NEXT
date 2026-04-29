/*
 * duxman-led next - v0.3.10-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectAudioNeonEq.h
 */

#pragma once

#include "effects/EffectEngine.h"

// AUDIO · Neon EQ
// Ecualizador visual de 3 bandas con barras segmentadas, cabezal brillante
// y flash de beat. Usa primaryColors[0..2] como bajos/medios/altos.
class EffectAudioNeonEq final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
  void onActivate() override;

private:
  unsigned long beatFlashAtMs_ = 0;
  float lowEnv_ = 0.0f;
  float midEnv_ = 0.0f;
  float highEnv_ = 0.0f;
};
