/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/drivers/LedDriverFastLed.h
 * Last commit: 38e7ffb - 2026-04-02
 */

#pragma once

#include "drivers/LedDriver.h"

class LedDriverFastLed final : public LedDriver {
public:
  void begin() override;
  void show() override;
  const char *backendName() const override;
  void setOutputColor(uint8_t outputIndex, uint32_t color) override;
  void setPixelColor(uint8_t outputIndex, uint16_t pixelIndex, uint32_t color) override;

private:
  uint8_t activeOutputIndex_ = 0xFF;
  uint16_t activeLedCount_ = 0;
  bool initialized_ = false;
};