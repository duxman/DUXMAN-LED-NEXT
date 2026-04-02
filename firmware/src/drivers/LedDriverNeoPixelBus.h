#pragma once

#include "drivers/LedDriver.h"

class LedDriverNeoPixelBus final : public LedDriver {
public:
  ~LedDriverNeoPixelBus() override;

  void begin() override;
  void show() override;
  const char *backendName() const override;

private:
  bool initialized_ = false;
  void *outputs_[kMaxLedOutputs] = {nullptr, nullptr, nullptr, nullptr};
};