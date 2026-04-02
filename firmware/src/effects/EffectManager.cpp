#include "effects/EffectManager.h"

#include "effects/EffectRegistry.h"

EffectManager::EffectManager(CoreState &state, LedDriver &driver)
    : state_(state), driver_(driver), fixedEffect_(state, driver), gradientEffect_(state, driver),
      diagnosticEffect_(state, driver) {
  effects_[0] = &fixedEffect_;
  effects_[1] = &gradientEffect_;
  effects_[2] = &diagnosticEffect_;
}

void EffectManager::begin() {
  fixedEffect_.begin();
}

void EffectManager::renderFrame() {
  if (!driver_.isInitialized()) {
    driver_.begin();
  }

  if (!state_.power) {
    driver_.clear();
    driver_.show();
    return;
  }

  EffectEngine &activeEffect = resolveActiveEffect();
  activeEffect.renderFrame();
}

EffectEngine &EffectManager::resolveActiveEffect() {
  for (size_t i = 0; i < sizeof(effects_) / sizeof(effects_[0]); ++i) {
    if (effects_[i] != nullptr && effects_[i]->supports(state_.effectId)) {
      return *effects_[i];
    }
  }

  return fixedEffect_;
}