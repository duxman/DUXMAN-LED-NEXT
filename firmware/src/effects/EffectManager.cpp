/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectManager.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

#include "effects/EffectManager.h"

#include "effects/EffectRegistry.h"

EffectManager::EffectManager(CoreState &state, LedDriver &driver)
    : state_(state), driver_(driver), fixedEffect_(state, driver), gradientEffect_(state, driver),
      blinkFixedEffect_(state, driver), blinkGradientEffect_(state, driver),
      diagnosticEffect_(state, driver), breathFixedEffect_(state, driver),
      breathGradientEffect_(state, driver), tripleChaseEffect_(state, driver),
      gradientMeteorEffect_(state, driver), scanningPulseEffect_(state, driver),
  trigRibbonEffect_(state, driver), lavaFlowEffect_(state, driver),
  polarIceEffect_(state, driver), stellarTwinkleEffect_(state, driver),
      randomColorPopEffect_(state, driver), bouncingPhysicsEffect_(state, driver),
  audioPulseEffect_(state, driver) {
  effects_[0] = &fixedEffect_;
  effects_[1] = &gradientEffect_;
  effects_[2] = &blinkFixedEffect_;
  effects_[3] = &blinkGradientEffect_;
  effects_[4] = &diagnosticEffect_;
  effects_[5] = &breathFixedEffect_;
  effects_[6] = &breathGradientEffect_;
  effects_[7] = &tripleChaseEffect_;
  effects_[8] = &gradientMeteorEffect_;
  effects_[9] = &scanningPulseEffect_;
  effects_[10] = &trigRibbonEffect_;
  effects_[11] = &lavaFlowEffect_;
  effects_[12] = &polarIceEffect_;
  effects_[13] = &stellarTwinkleEffect_;
  effects_[14] = &randomColorPopEffect_;
  effects_[15] = &bouncingPhysicsEffect_;
  effects_[16] = &audioPulseEffect_;
}

void EffectManager::begin() {
  fixedEffect_.begin();
}

void EffectManager::renderFrame() {
  if (!state_.lock()) {
    return;
  }

  if (!driver_.isInitialized()) {
    driver_.begin();
  }

  if (!state_.power) {
    driver_.clear();
    driver_.show();
    state_.unlock();
    return;
  }

  EffectEngine &activeEffect = resolveActiveEffect();

  // Ciclo de vida: detectar cambio de efecto y notificar onActivate/onDeactivate.
  if (state_.effectId != lastEffectId_) {
    if (lastEffectId_ != 255) {
      for (size_t i = 0; i < kEffectCount; ++i) {
        if (effects_[i] && effects_[i]->supports(lastEffectId_)) {
          effects_[i]->onDeactivate();
          break;
        }
      }
    }
    // Limpiar todos los LEDs antes de activar el nuevo efecto para que no
    // queden pixels residuales del efecto anterior.
    driver_.clear();
    driver_.show();

    // Activar log de diagnostico para el primer frame del nuevo efecto.
    driver_.scheduleShowLog();

    activeEffect.onActivate();
    lastEffectId_ = state_.effectId;
  }

  activeEffect.renderFrame();
  state_.unlock();
}

EffectEngine &EffectManager::resolveActiveEffect() {
  for (size_t i = 0; i < kEffectCount; ++i) {
    if (effects_[i] != nullptr && effects_[i]->supports(state_.effectId)) {
      return *effects_[i];
    }
  }

  return fixedEffect_;
}