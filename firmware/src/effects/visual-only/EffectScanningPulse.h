/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectScanningPulse.h
 * Last commit: 2c35a63 - 2026-04-28
 */

#pragma once

#include "effects/EffectEngine.h"

// Scanning Pulse:
// - effectSpeed: velocidad de barrido ida/vuelta
// - effectLevel: anchura del pulso
class EffectScanningPulse final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
