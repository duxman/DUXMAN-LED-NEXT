#pragma once

#include "drivers/LedDriver.h"

class LedDriverDigital final : public LedDriver {
public:
  void begin() override;
  void show() override;
  const char *backendName() const override;

private:
  bool initialized_ = false;
};