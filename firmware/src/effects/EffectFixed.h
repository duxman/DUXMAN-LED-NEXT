/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectFixed.h
 * Last commit: 38e7ffb - 2026-04-02
 */

#pragma once

#include "effects/EffectEngine.h"

class EffectFixed final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};