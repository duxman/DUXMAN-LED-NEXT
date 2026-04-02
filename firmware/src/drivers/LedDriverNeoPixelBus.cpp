#include "drivers/LedDriverNeoPixelBus.h"

#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
#include <NeoPixelBus.h>

namespace {
class NeoOutputBase {
public:
  virtual ~NeoOutputBase() = default;
  virtual void begin() = 0;
  virtual void fill(uint8_t level) = 0;
  virtual void show() = 0;
};

template <typename TFeature>
class NeoOutputRgb final : public NeoOutputBase {
public:
  NeoOutputRgb(uint16_t ledCount, int8_t pin) : bus_(ledCount, pin), ledCount_(ledCount) {}

  void begin() override {
    bus_.Begin();
    bus_.ClearTo(RgbColor(0, 0, 0));
    bus_.Show();
  }

  void fill(uint8_t level) override {
    const RgbColor color(level, level, level);
    for (uint16_t i = 0; i < ledCount_; ++i) {
      bus_.SetPixelColor(i, color);
    }
  }

  void show() override {
    bus_.Show();
  }

private:
  NeoPixelBus<TFeature, NeoEsp32Rmt0800KbpsMethod> bus_;
  uint16_t ledCount_;
};

template <typename TFeature>
class NeoOutputRgbw final : public NeoOutputBase {
public:
  NeoOutputRgbw(uint16_t ledCount, int8_t pin) : bus_(ledCount, pin), ledCount_(ledCount) {}

  void begin() override {
    bus_.Begin();
    bus_.ClearTo(RgbwColor(0, 0, 0, 0));
    bus_.Show();
  }

  void fill(uint8_t level) override {
    const RgbwColor color(0, 0, 0, level);
    for (uint16_t i = 0; i < ledCount_; ++i) {
      bus_.SetPixelColor(i, color);
    }
  }

  void show() override {
    bus_.Show();
  }

private:
  NeoPixelBus<TFeature, NeoEsp32Rmt0800KbpsMethod> bus_;
  uint16_t ledCount_;
};

NeoOutputBase *asNeoOutput(void *output) {
  return static_cast<NeoOutputBase *>(output);
}

void destroyNeoOutput(void *output) {
  delete asNeoOutput(output);
}

NeoOutputBase *createNeoOutput(const LedDriverOutputConfig &output) {
  if (!output.enabled || output.isDigital || !LedDriver::isAddressableType(output.ledType)) {
    return nullptr;
  }

  if (output.isRgbw) {
    switch (output.colorOrder) {
      case LedDriverColorOrder::RGBW:
        return new NeoOutputRgbw<NeoRgbwFeature>(output.ledCount, output.pin);
      case LedDriverColorOrder::GRBW:
      default:
        return new NeoOutputRgbw<NeoGrbwFeature>(output.ledCount, output.pin);
    }
  }

  switch (output.colorOrder) {
    case LedDriverColorOrder::RGB:
      return new NeoOutputRgb<NeoRgbFeature>(output.ledCount, output.pin);
    case LedDriverColorOrder::BRG:
      return new NeoOutputRgb<NeoBrgFeature>(output.ledCount, output.pin);
    case LedDriverColorOrder::RBG:
      return new NeoOutputRgb<NeoRbgFeature>(output.ledCount, output.pin);
    case LedDriverColorOrder::GBR:
      return new NeoOutputRgb<NeoGbrFeature>(output.ledCount, output.pin);
    case LedDriverColorOrder::BGR:
      return new NeoOutputRgb<NeoBgrFeature>(output.ledCount, output.pin);
    case LedDriverColorOrder::GRB:
    default:
      return new NeoOutputRgb<NeoGrbFeature>(output.ledCount, output.pin);
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
    outputs_[i] = createNeoOutput(output);
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

    NeoOutputBase *output = asNeoOutput(outputs_[i]);
    output->fill(outputLevel(i));
    output->show();
  }
#endif
}

const char *LedDriverNeoPixelBus::backendName() const {
  return "neopixelbus";
}