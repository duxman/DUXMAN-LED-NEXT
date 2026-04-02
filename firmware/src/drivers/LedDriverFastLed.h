#pragma once

#include "drivers/LedDriver.h"

class LedDriverFastLed final : public LedDriver {
public:
  void begin() override;
  void show() override;
  const char *backendName() const override;

private:
  uint8_t activeOutputIndex_ = 0xFF;
  uint16_t activeLedCount_ = 0;
  bool initialized_ = false;
};