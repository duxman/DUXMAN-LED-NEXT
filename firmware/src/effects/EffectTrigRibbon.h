#pragma once

#include "effects/EffectEngine.h"

// Trig-Ribbon:
// - effectSpeed: desplazamiento temporal del patron
// - effectLevel: frecuencia secundaria / nitidez visual
// - sectionCount: repeticiones de onda
class EffectTrigRibbon final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
