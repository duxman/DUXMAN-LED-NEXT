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
