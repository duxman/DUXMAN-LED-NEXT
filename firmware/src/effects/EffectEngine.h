#pragma once

#include <Arduino.h>

#include "core/CoreState.h"
#include "drivers/LedDriver.h"

class EffectEngine {
public:
  EffectEngine(CoreState &state, LedDriver &driver);
  virtual ~EffectEngine() = default;

  virtual void begin();
  virtual bool supports(uint8_t effectId) const = 0;
  virtual void renderFrame() = 0;

protected:
  CoreState &state() const {
    return state_;
  }

  LedDriver &driver() const {
    return driver_;
  }

  static uint32_t scaleColor(uint32_t color, uint8_t brightness);
  static uint32_t gradientColor(uint32_t colorA, uint32_t colorB, uint32_t colorC,
                                uint16_t pixelIndex, uint16_t pixelCount);
  static uint16_t resolveSectionSize(uint16_t ledCount, uint8_t sectionCount);
  static unsigned long effectIntervalMs(uint8_t speedScale);

private:
  CoreState &state_;
  LedDriver &driver_;
};
