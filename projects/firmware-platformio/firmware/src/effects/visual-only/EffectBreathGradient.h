/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectBreathGradient.h
 * Last commit: 2c35a63 - 2026-04-28
 */

#pragma once

#include "effects/EffectEngine.h"

// Global Gradient Breathe: el degradado completo (3 colores) respira con una
// envolvente senoidal. effectSpeed controla la velocidad, effectLevel la
// profundidad del fade.
class EffectBreathGradient final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
