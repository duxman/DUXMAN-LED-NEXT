#pragma once

#include "core/CoreState.h"
#include "drivers/LedDriver.h"
#include "effects/EffectBlinkFixed.h"
#include "effects/EffectBlinkGradient.h"
#include "effects/EffectBreathFixed.h"
#include "effects/EffectBreathGradient.h"
#include "effects/EffectBouncingPhysics.h"
#include "effects/EffectDiagnostic.h"
#include "effects/EffectEngine.h"
#include "effects/EffectFixed.h"
#include "effects/EffectGradientMeteor.h"
#include "effects/EffectGradient.h"
#include "effects/EffectLavaFlow.h"
#include "effects/EffectScanningPulse.h"
#include "effects/EffectPolarIce.h"
#include "effects/EffectRandomColorPop.h"
#include "effects/EffectStellarTwinkle.h"
#include "effects/EffectTripleChase.h"
#include "effects/EffectTrigRibbon.h"

class EffectManager {
public:
  EffectManager(CoreState &state, LedDriver &driver);

  void begin();
  void renderFrame();

private:
  static constexpr size_t kEffectCount = 16;
  EffectEngine &resolveActiveEffect();
  EffectEngine *effects_[kEffectCount] = {};
  uint8_t lastEffectId_ = 255;  // 255 = ninguno activo aún

  CoreState &state_;
  LedDriver &driver_;
  EffectFixed fixedEffect_;
  EffectGradient gradientEffect_;
  EffectBlinkFixed blinkFixedEffect_;
  EffectBlinkGradient blinkGradientEffect_;
  EffectDiagnostic diagnosticEffect_;
  EffectBreathFixed breathFixedEffect_;
  EffectBreathGradient breathGradientEffect_;
  EffectTripleChase tripleChaseEffect_;
  EffectGradientMeteor gradientMeteorEffect_;
  EffectScanningPulse scanningPulseEffect_;
  EffectTrigRibbon trigRibbonEffect_;
  EffectLavaFlow lavaFlowEffect_;
  EffectPolarIce polarIceEffect_;
  EffectStellarTwinkle stellarTwinkleEffect_;
  EffectRandomColorPop randomColorPopEffect_;
  EffectBouncingPhysics bouncingPhysicsEffect_;
};