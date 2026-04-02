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
  uint32_t debugLastFillColor(uint8_t outputIndex) const override;
  uint32_t debugSamplePixelColor(uint8_t outputIndex, uint8_t sampleIndex) const override;

private:
  bool initialized_ = false;
  void *outputs_[kMaxLedOutputs] = {nullptr, nullptr, nullptr, nullptr};
  uint32_t lastFillColors_[kMaxLedOutputs] = {0, 0, 0, 0};
  uint32_t samplePixelColors_[kMaxLedOutputs][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
};