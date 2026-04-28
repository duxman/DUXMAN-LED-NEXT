/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectLavaFlow.h
 * Last commit: 4c84f5d - 2026-04-03
 */

#pragma once

#include "effects/EffectEngine.h"

class EffectLavaFlow final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
