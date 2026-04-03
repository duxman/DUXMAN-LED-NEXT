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
