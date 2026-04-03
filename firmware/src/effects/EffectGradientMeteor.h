#pragma once

#include "effects/EffectEngine.h"

// Gradient Meteor:
// - effectSpeed: velocidad de avance del meteoro
// - effectLevel: longitud de cola
class EffectGradientMeteor final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
