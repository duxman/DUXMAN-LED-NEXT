/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/drivers/LedDriverDigital.h
 * Last commit: 2c35a63 - 2026-04-28
 */

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