#pragma once

#include "effects/EffectEngine.h"

class EffectRandomColorPop final : public EffectEngine {
public:
  using EffectEngine::EffectEngine;

  bool supports(uint8_t effectId) const override;
  void renderFrame() override;
  void onActivate() override;
  void onDeactivate() override;

private:
  uint32_t seed_ = 7;
};
