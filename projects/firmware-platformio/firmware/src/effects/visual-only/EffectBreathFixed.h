/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectBreathFixed.h
 * Last commit: 2c35a63 - 2026-04-28
 */

#pragma once

#include "effects/EffectEngine.h"

// Triple Fixed Breathe: las secciones de color fijo respiran con una envolvente
// senoidal. effectSpeed controla la velocidad del ciclo, effectLevel controla
// la profundidad del fade (0 = sin cambio, 100 = fade completo a negro).
class EffectBreathFixed final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
