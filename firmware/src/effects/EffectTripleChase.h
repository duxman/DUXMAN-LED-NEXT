#pragma once

#include "effects/EffectEngine.h"

// Triple Chase:
// - effectSpeed: velocidad del desplazamiento
// - effectLevel: ancho del tren brillante
// - sectionCount: repeticiones del patron a lo largo de la tira
class EffectTripleChase final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
