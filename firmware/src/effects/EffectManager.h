#pragma once

#include "core/CoreState.h"
#include "drivers/LedDriver.h"
#include "effects/EffectDiagnostic.h"
#include "effects/EffectEngine.h"
#include "effects/EffectFixed.h"
#include "effects/EffectGradient.h"

class EffectManager {
public:
  EffectManager(CoreState &state, LedDriver &driver);

  void begin();
  void renderFrame();

private:
  EffectEngine &resolveActiveEffect();
  EffectEngine *effects_[3] = {nullptr, nullptr, nullptr};

  CoreState &state_;
  LedDriver &driver_;
  EffectFixed fixedEffect_;
  EffectGradient gradientEffect_;
  EffectDiagnostic diagnosticEffect_;
};