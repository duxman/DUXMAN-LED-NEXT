#pragma once

#include <Arduino.h>

#include "core/CoreState.h"
#include "drivers/LedDriver.h"

class EffectEngine {
public:
  EffectEngine(CoreState &state, LedDriver &driver);

  void begin();
  void renderFrame();

private:
  CoreState &state_;
  LedDriver &driver_;
};
