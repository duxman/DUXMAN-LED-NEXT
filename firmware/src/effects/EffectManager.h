/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectManager.h
 * Last commit: 2c35a63 - 2026-04-28
 */

#pragma once

#include "core/CoreState.h"
#include "drivers/LedDriver.h"
#include "effects/EffectEngine.h"

class EffectManager {
public:
  EffectManager(CoreState &state, LedDriver &driver);
  ~EffectManager();

  void begin();
  void renderFrame();

private:
  struct Impl;
  Impl *impl_ = nullptr;

  EffectEngine &resolveActiveEffect();
};