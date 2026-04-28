/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectTripleChase.h
 * Last commit: 2c35a63 - 2026-04-28
 */

#pragma once

#include "effects/EffectEngine.h"

// Triple Chase:
// - effectSpeed: velocidad del desplazamiento
// - effectLevel: ancho del tren brillante
// - sectionCount: repeticiones del patron a lo largo de la tira
class EffectTripleChase final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
