#pragma once

#include <Arduino.h>

#include "core/Config.h"

enum class LedDriverType : uint8_t {
  Digital,
  Ws2812b,
  Ws2811,
  Ws2813,
  Ws2815,
  Sk6812,
  Tm1814,
  Unknown,
};

enum class LedDriverColorOrder : uint8_t {
  RGB,
  GRB,
  BRG,
  RBG,
  GBR,
  BGR,
  RGBW,
  GRBW,
  R,
  G,
  B,
  W,
  Unknown,
};

struct LedDriverOutputConfig {
  bool enabled = false;
  uint8_t id = 0;
  int8_t pin = -1;
  uint16_t ledCount = 0;
  LedDriverType ledType = LedDriverType::Unknown;
  LedDriverColorOrder colorOrder = LedDriverColorOrder::Unknown;
  bool isDigital = false;
  bool isRgbw = false;
};

class LedDriver {
public:
  virtual ~LedDriver() = default;

  void configure(const GpioConfig &config);
  virtual void begin() = 0;
  virtual void show() = 0;
  virtual const char *backendName() const = 0;
  virtual bool isInitialized() const {
    return true;
  }

  virtual void setAll(uint8_t level);
  virtual void setAllColor(uint32_t color);
  virtual void setOutputLevel(uint8_t outputIndex, uint8_t level);
  virtual void setOutputColor(uint8_t outputIndex, uint32_t color);
  virtual void setPixelColor(uint8_t outputIndex, uint16_t pixelIndex, uint32_t color);
  virtual bool supportsPerPixelColor(uint8_t outputIndex) const;
  virtual uint32_t debugLastFillColor(uint8_t outputIndex) const {
    (void)outputIndex;
    return 0;
  }
  virtual uint32_t debugSamplePixelColor(uint8_t outputIndex, uint8_t sampleIndex) const {
    (void)outputIndex;
    (void)sampleIndex;
    return 0;
  }

  virtual void clear() {
    setAllColor(0);
  }

  uint8_t outputCount() const {
    return outputCount_;
  }

  const LedDriverOutputConfig &outputConfig(uint8_t outputIndex) const;

protected:
  const LedDriverOutputConfig *outputs() const {
    return outputs_;
  }

  uint8_t outputLevel(uint8_t outputIndex) const;
  static uint8_t colorLevel(uint32_t color);
  static bool isAddressableType(LedDriverType ledType);

  uint8_t level_ = 0;

private:
  LedDriverOutputConfig outputs_[kMaxLedOutputs];
  uint8_t outputLevels_[kMaxLedOutputs] = {0, 0, 0, 0};
  uint8_t outputCount_ = 0;
};
