/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectPolarIce.h
 * Last commit: ec3d96f - 2026-04-28
 */

#pragma once

#include "effects/EffectEngine.h"

class EffectPolarIce final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
