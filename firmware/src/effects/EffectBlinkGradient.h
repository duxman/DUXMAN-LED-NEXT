/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectBlinkGradient.h
 * Last commit: 2deea99 - 2026-04-02
 */

#pragma once

#include "effects/EffectEngine.h"

class EffectBlinkGradient final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;

private:
  bool visible_ = true;
  unsigned long lastToggleAtMs_ = 0;
};
