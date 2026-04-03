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
