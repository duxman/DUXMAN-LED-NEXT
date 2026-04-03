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
