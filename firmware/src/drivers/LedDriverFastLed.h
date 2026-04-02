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