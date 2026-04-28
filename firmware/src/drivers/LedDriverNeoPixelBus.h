/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/drivers/LedDriverNeoPixelBus.h
 * Last commit: ec3d96f - 2026-04-28
 */

#pragma once

#include "drivers/LedDriver.h"

class LedDriverNeoPixelBus final : public LedDriver {
public:
  ~LedDriverNeoPixelBus() override;

  void begin() override;
  void show() override;
  const char *backendName() const override;
  bool isInitialized() const override;
  void setOutputColor(uint8_t outputIndex, uint32_t color) override;
  void setPixelColor(uint8_t outputIndex, uint16_t pixelIndex, uint32_t color) override;

private:
  bool initialized_ = false;
  void *outputs_[kMaxLedOutputs] = {nullptr, nullptr, nullptr, nullptr};
};