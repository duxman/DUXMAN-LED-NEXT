#pragma once

#include "effects/EffectEngine.h"

class EffectLavaFlow final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
};
