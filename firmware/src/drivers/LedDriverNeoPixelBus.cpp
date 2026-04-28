/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/drivers/LedDriverNeoPixelBus.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

#include "drivers/LedDriverNeoPixelBus.h"

#include "core/BuildProfile.h"

#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
#include <NeoPixelBus.h>

namespace {
class NeoOutputBase {
public:
  virtual ~NeoOutputBase() = default;
  virtual void begin() = 0;
  virtual void fillColor(uint32_t color) = 0;
  virtual void setPixelColor(uint16_t pixelIndex, uint32_t color) = 0;
  virtual void show() = 0;
};

RgbColor rgbFromColor(uint32_t color) {
  return RgbColor(static_cast<uint8_t>((color >> 16) & 0xFF),
                  static_cast<uint8_t>((color >> 8) & 0xFF),
                  static_cast<uint8_t>(color & 0xFF));
}

RgbwColor rgbwFromColor(uint32_t color) {
  return RgbwColor(static_cast<uint8_t>((color >> 16) & 0xFF),
                   static_cast<uint8_t>((color >> 8) & 0xFF),
                   static_cast<uint8_t>(color & 0xFF), 0);
}

bool isAddressableOutputType(LedDriverType ledType) {
  return ledType == LedDriverType::Ws2812b || ledType == LedDriverType::Ws2811 ||
         ledType == LedDriverType::Ws2813 || ledType == LedDriverType::Ws2815 ||
         ledType == LedDriverType::Sk6812 || ledType == LedDriverType::Tm1814;
}

template <typename TFeature, typename TMethod>
class NeoOutputRgb final : public NeoOutputBase {
public:
  NeoOutputRgb(uint16_t ledCount, int8_t pin) : bus_(ledCount, pin), ledCount_(ledCount) {}

  void begin() override {
    bus_.Begin();
    bus_.ClearTo(RgbColor(0, 0, 0));
    bus_.Show();
  }

  void fillColor(uint32_t color) override {
    const RgbColor rgb = rgbFromColor(color);
    for (uint16_t i = 0; i < ledCount_; ++i) {
      bus_.SetPixelColor(i, rgb);
    }
  }

  void setPixelColor(uint16_t pixelIndex, uint32_t color) override {
    if (pixelIndex >= ledCount_) {
      return;
    }
    bus_.SetPixelColor(pixelIndex, rgbFromColor(color));
  }

  void show() override {
    bus_.Show();
  }

private:
  NeoPixelBus<TFeature, TMethod> bus_;
  uint16_t ledCount_;
};

template <typename TFeature, typename TMethod>
class NeoOutputRgbw final : public NeoOutputBase {
public:
  NeoOutputRgbw(uint16_t ledCount, int8_t pin) : bus_(ledCount, pin), ledCount_(ledCount) {}

  void begin() override {
    bus_.Begin();
    bus_.ClearTo(RgbwColor(0, 0, 0, 0));
    bus_.Show();
  }

  void fillColor(uint32_t color) override {
    const RgbwColor rgbw = rgbwFromColor(color);
    for (uint16_t i = 0; i < ledCount_; ++i) {
      bus_.SetPixelColor(i, rgbw);
    }
  }

  void setPixelColor(uint16_t pixelIndex, uint32_t color) override {
    if (pixelIndex >= ledCount_) {
      return;
    }
    bus_.SetPixelColor(pixelIndex, rgbwFromColor(color));
  }

  void show() override {
    bus_.Show();
  }

private:
  NeoPixelBus<TFeature, TMethod> bus_;
  uint16_t ledCount_;
};

template <typename TFeature, typename TMethod>
NeoOutputBase *createNeoOutputRgbByMethod(uint16_t ledCount, int8_t pin) {
  return new NeoOutputRgb<TFeature, TMethod>(ledCount, pin);
}

template <typename TFeature, typename TMethod>
NeoOutputBase *createNeoOutputRgbwByMethod(uint16_t ledCount, int8_t pin) {
  return new NeoOutputRgbw<TFeature, TMethod>(ledCount, pin);
}

template <typename TFeature>
NeoOutputBase *createNeoOutputRgbByChannel(uint8_t outputIndex, uint16_t ledCount, int8_t pin) {
  switch (outputIndex) {
    case 0:
      return createNeoOutputRgbByMethod<TFeature, NeoEsp32Rmt0800KbpsMethod>(ledCount, pin);
    case 1:
      return createNeoOutputRgbByMethod<TFeature, NeoEsp32Rmt1800KbpsMethod>(ledCount, pin);
    case 2:
      return createNeoOutputRgbByMethod<TFeature, NeoEsp32Rmt2800KbpsMethod>(ledCount, pin);
    case 3:
      return createNeoOutputRgbByMethod<TFeature, NeoEsp32Rmt3800KbpsMethod>(ledCount, pin);
    default:
      return nullptr;
  }
}

template <typename TFeature>
NeoOutputBase *createNeoOutputRgbwByChannel(uint8_t outputIndex, uint16_t ledCount, int8_t pin) {
  switch (outputIndex) {
    case 0:
      return createNeoOutputRgbwByMethod<TFeature, NeoEsp32Rmt0800KbpsMethod>(ledCount, pin);
    case 1:
      return createNeoOutputRgbwByMethod<TFeature, NeoEsp32Rmt1800KbpsMethod>(ledCount, pin);
    case 2:
      return createNeoOutputRgbwByMethod<TFeature, NeoEsp32Rmt2800KbpsMethod>(ledCount, pin);
    case 3:
      return createNeoOutputRgbwByMethod<TFeature, NeoEsp32Rmt3800KbpsMethod>(ledCount, pin);
    default:
      return nullptr;
  }
}

NeoOutputBase *asNeoOutput(void *output) {
  return static_cast<NeoOutputBase *>(output);
}

void destroyNeoOutput(void *output) {
  delete asNeoOutput(output);
}

NeoOutputBase *createNeoOutput(uint8_t outputIndex, const LedDriverOutputConfig &output) {
  if (!output.enabled || output.isDigital || !isAddressableOutputType(output.ledType)) {
    return nullptr;
  }

  if (output.isRgbw) {
    switch (output.colorOrder) {
      case LedDriverColorOrder::RGBW:
        return createNeoOutputRgbwByChannel<NeoRgbwFeature>(outputIndex, output.ledCount, output.pin);
      case LedDriverColorOrder::GRBW:
      default:
        return createNeoOutputRgbwByChannel<NeoGrbwFeature>(outputIndex, output.ledCount, output.pin);
    }
  }

  switch (output.colorOrder) {
    case LedDriverColorOrder::RGB:
      return createNeoOutputRgbByChannel<NeoRgbFeature>(outputIndex, output.ledCount, output.pin);
    case LedDriverColorOrder::BRG:
      return createNeoOutputRgbByChannel<NeoBrgFeature>(outputIndex, output.ledCount, output.pin);
    case LedDriverColorOrder::RBG:
      return createNeoOutputRgbByChannel<NeoRbgFeature>(outputIndex, output.ledCount, output.pin);
    case LedDriverColorOrder::GBR:
      return createNeoOutputRgbByChannel<NeoGbrFeature>(outputIndex, output.ledCount, output.pin);
    case LedDriverColorOrder::BGR:
      return createNeoOutputRgbByChannel<NeoBgrFeature>(outputIndex, output.ledCount, output.pin);
    case LedDriverColorOrder::GRB:
    default:
      return createNeoOutputRgbByChannel<NeoGrbFeature>(outputIndex, output.ledCount, output.pin);
  }
}
} // namespace
#endif

LedDriverNeoPixelBus::~LedDriverNeoPixelBus() {
#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
  for (uint8_t i = 0; i < kMaxLedOutputs; ++i) {
    destroyNeoOutput(outputs_[i]);
    outputs_[i] = nullptr;
  }
#endif
}

void LedDriverNeoPixelBus::begin() {
  level_ = 0;
  initialized_ = false;

#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
  for (uint8_t i = 0; i < kMaxLedOutputs; ++i) {
    destroyNeoOutput(outputs_[i]);
    outputs_[i] = nullptr;
  }

  for (uint8_t i = 0; i < outputCount(); ++i) {
    const LedDriverOutputConfig &output = outputConfig(i);
    outputs_[i] = createNeoOutput(i, output);
    if (outputs_[i] == nullptr) {
      continue;
    }

    asNeoOutput(outputs_[i])->begin();
    initialized_ = true;
  }
#else
  initialized_ = false;
#endif
}

void LedDriverNeoPixelBus::show() {
  if (!initialized_) {
    return;
  }

#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
  for (uint8_t i = 0; i < outputCount(); ++i) {
    if (outputs_[i] == nullptr) {
      continue;
    }

    asNeoOutput(outputs_[i])->show();
  }
#endif
}

const char *LedDriverNeoPixelBus::backendName() const {
  return "neopixelbus";
}

bool LedDriverNeoPixelBus::isInitialized() const {
  return initialized_;
}

void LedDriverNeoPixelBus::setOutputColor(uint8_t outputIndex, uint32_t color) {
  LedDriver::setOutputColor(outputIndex, color);

#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
  if (outputIndex >= outputCount() || outputs_[outputIndex] == nullptr) {
    return;
  }

  asNeoOutput(outputs_[outputIndex])->fillColor(color);
#else
  (void)outputIndex;
  (void)color;
#endif
}

void LedDriverNeoPixelBus::setPixelColor(uint8_t outputIndex, uint16_t pixelIndex, uint32_t color) {
  LedDriver::setOutputColor(outputIndex, color);

#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
  if (outputIndex >= outputCount() || outputs_[outputIndex] == nullptr) {
    return;
  }

  asNeoOutput(outputs_[outputIndex])->setPixelColor(pixelIndex, color);
#else
  (void)outputIndex;
  (void)pixelIndex;
  (void)color;
#endif
}
