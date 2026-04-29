/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectManager.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

#include "effects/EffectManager.h"

#include "effects/EffectAudioPulse.h"
#include "effects/EffectBlinkFixed.h"
#include "effects/EffectBlinkGradient.h"
#include "effects/EffectBouncingPhysics.h"
#include "effects/EffectBreathFixed.h"
#include "effects/EffectBreathGradient.h"
#include "effects/EffectDiagnostic.h"
#include "effects/EffectFixed.h"
#include "effects/EffectGradient.h"
#include "effects/EffectGradientMeteor.h"
#include "effects/EffectLavaFlow.h"
#include "effects/EffectPolarIce.h"
#include "effects/EffectRandomColorPop.h"
#include "effects/EffectRegistry.h"
#include "effects/EffectScanningPulse.h"
#include "effects/EffectStellarTwinkle.h"
#include "effects/EffectTrigRibbon.h"
#include "effects/EffectTripleChase.h"

struct EffectManager::Impl {
  static constexpr size_t kEffectCount = 17;

  CoreState &state;
  LedDriver &driver;
  EffectEngine *effects[kEffectCount] = {};
  uint8_t lastEffectId = 255; // 255 = ninguno activo aun

  EffectFixed fixedEffect;
  EffectGradient gradientEffect;
  EffectBlinkFixed blinkFixedEffect;
  EffectBlinkGradient blinkGradientEffect;
  EffectDiagnostic diagnosticEffect;
  EffectBreathFixed breathFixedEffect;
  EffectBreathGradient breathGradientEffect;
  EffectTripleChase tripleChaseEffect;
  EffectGradientMeteor gradientMeteorEffect;
  EffectScanningPulse scanningPulseEffect;
  EffectTrigRibbon trigRibbonEffect;
  EffectLavaFlow lavaFlowEffect;
  EffectPolarIce polarIceEffect;
  EffectStellarTwinkle stellarTwinkleEffect;
  EffectRandomColorPop randomColorPopEffect;
  EffectBouncingPhysics bouncingPhysicsEffect;
  EffectAudioPulse audioPulseEffect;

  Impl(CoreState &stateRef, LedDriver &driverRef)
      : state(stateRef),
        driver(driverRef),
        fixedEffect(stateRef, driverRef),
        gradientEffect(stateRef, driverRef),
        blinkFixedEffect(stateRef, driverRef),
        blinkGradientEffect(stateRef, driverRef),
        diagnosticEffect(stateRef, driverRef),
        breathFixedEffect(stateRef, driverRef),
        breathGradientEffect(stateRef, driverRef),
        tripleChaseEffect(stateRef, driverRef),
        gradientMeteorEffect(stateRef, driverRef),
        scanningPulseEffect(stateRef, driverRef),
        trigRibbonEffect(stateRef, driverRef),
        lavaFlowEffect(stateRef, driverRef),
        polarIceEffect(stateRef, driverRef),
        stellarTwinkleEffect(stateRef, driverRef),
        randomColorPopEffect(stateRef, driverRef),
        bouncingPhysicsEffect(stateRef, driverRef),
        audioPulseEffect(stateRef, driverRef) {
    effects[0] = &fixedEffect;
    effects[1] = &gradientEffect;
    effects[2] = &blinkFixedEffect;
    effects[3] = &blinkGradientEffect;
    effects[4] = &diagnosticEffect;
    effects[5] = &breathFixedEffect;
    effects[6] = &breathGradientEffect;
    effects[7] = &tripleChaseEffect;
    effects[8] = &gradientMeteorEffect;
    effects[9] = &scanningPulseEffect;
    effects[10] = &trigRibbonEffect;
    effects[11] = &lavaFlowEffect;
    effects[12] = &polarIceEffect;
    effects[13] = &stellarTwinkleEffect;
    effects[14] = &randomColorPopEffect;
    effects[15] = &bouncingPhysicsEffect;
    effects[16] = &audioPulseEffect;
  }
};

EffectManager::EffectManager(CoreState &state, LedDriver &driver)
    : impl_(new Impl(state, driver)) {}

EffectManager::~EffectManager() {
  delete impl_;
}

void EffectManager::begin() {
  impl_->fixedEffect.begin();
}

void EffectManager::renderFrame() {
  if (!impl_->state.lock()) {
    return;
  }

  if (!impl_->driver.isInitialized()) {
    impl_->driver.begin();
  }

  if (!impl_->state.power) {
    impl_->driver.clear();
    impl_->driver.show();
    impl_->state.unlock();
    return;
  }

  EffectEngine &activeEffect = resolveActiveEffect();

  // Ciclo de vida: detectar cambio de efecto y notificar onActivate/onDeactivate.
  if (impl_->state.effectId != impl_->lastEffectId) {
    if (impl_->lastEffectId != 255) {
      for (size_t i = 0; i < Impl::kEffectCount; ++i) {
        if (impl_->effects[i] && impl_->effects[i]->supports(impl_->lastEffectId)) {
          impl_->effects[i]->onDeactivate();
          break;
        }
      }
    }
    // Limpiar el buffer antes de activar el nuevo efecto para que no queden
    // pixeles residuales del efecto anterior.  NO llamar show() aqui: hacer
    // un Show() DMA y luego escribir en el mismo buffer dentro de
    // activeEffect.renderFrame() (mas abajo, en el mismo ciclo) produce una
    // carrera de datos que corrompe la trama y enciende pixeles aleatorios.
    // El buffer limpio se enviara al strip en el show() del primer frame del
    // nuevo efecto.
    impl_->driver.clear();

    // Activar log de diagnostico para el primer frame del nuevo efecto.
    impl_->driver.scheduleShowLog();

    activeEffect.onActivate();
    impl_->lastEffectId = impl_->state.effectId;
  }

  activeEffect.renderFrame();
  impl_->state.unlock();
}

EffectEngine &EffectManager::resolveActiveEffect() {
  for (size_t i = 0; i < Impl::kEffectCount; ++i) {
    if (impl_->effects[i] != nullptr && impl_->effects[i]->supports(impl_->state.effectId)) {
      return *impl_->effects[i];
    }
  }

  return impl_->fixedEffect;
}