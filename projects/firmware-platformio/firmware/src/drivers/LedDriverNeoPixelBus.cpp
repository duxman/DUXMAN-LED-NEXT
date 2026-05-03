/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/drivers/LedDriverNeoPixelBus.cpp
 * Last commit: 2c35a63 - 2026-04-28
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
  // Returns true when the bus is idle (no DMA transfer in progress).
  virtual bool canShow() const { return true; }

  // Diagnostico: numero de pixeles no negros en el ultimo frame enviado.
  uint16_t litCount() const { return litCount_; }
  uint16_t totalCount() const { return totalCount_; }
  uint32_t firstLitColor() const { return firstLitColor_; }
  uint16_t firstLitIndex() const { return firstLitIndex_; }

protected:
  void trackFill(uint32_t color, uint16_t count) {
    totalCount_ = count;
    if (color != 0) {
      litCount_ = count;
      firstLitColor_ = color;
      firstLitIndex_ = 0;
    } else {
      litCount_ = 0;
      firstLitColor_ = 0;
      firstLitIndex_ = 0xFFFF;
    }
  }

  void trackPixel(uint16_t index, uint32_t color) {
    if (color != 0 && litCount_ == 0) {
      firstLitColor_ = color;
      firstLitIndex_ = index;
    }
    if (color != 0) ++litCount_;
  }

  void resetTrack(uint16_t count) {
    totalCount_ = count;
    litCount_ = 0;
    firstLitColor_ = 0;
    firstLitIndex_ = 0xFFFF;
  }

private:
  uint16_t litCount_ = 0;
  uint16_t totalCount_ = 0;
  uint32_t firstLitColor_ = 0;
  uint16_t firstLitIndex_ = 0xFFFF;
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
  // busCount >= ledCount: el bus se crea con busCount pixeles para poder
  // apagar LEDs fisicos que superan ledCount (p.ej. tras cambio de config).
  NeoOutputRgb(uint16_t ledCount, int8_t pin, uint16_t busCount)
      : bus_(busCount, pin), ledCount_(ledCount), busCount_(busCount) {}

  void begin() override {
    bus_.Begin();
    bus_.ClearTo(RgbColor(0, 0, 0));
    // Do NOT call Show() here: starting an async RMT transfer while holding the
    // CoreState mutex causes a race where the render task can call Show() again
    // before the transfer completes, producing a frame-stitch on the LED strip
    // that makes extra pixels light up.  The render task will issue the first
    // proper Show() as soon as it reacquires the mutex.
  }

  bool canShow() const override { return bus_.CanShow(); }

  void fillColor(uint32_t color) override {
    if (color == 0) {
      // Apaga TODOS los pixeles del bus (incluyendo los que superan ledCount_)
      // para limpiar LEDs fisicos que quedaron encendidos de configs anteriores.
      trackFill(0, busCount_);
      bus_.ClearTo(RgbColor(0, 0, 0));
    } else {
      trackFill(color, ledCount_);
      const RgbColor rgb = rgbFromColor(color);
      for (uint16_t i = 0; i < ledCount_; ++i) {
        bus_.SetPixelColor(i, rgb);
      }
    }
  }

  void setPixelColor(uint16_t pixelIndex, uint32_t color) override {
    if (pixelIndex >= ledCount_) {
      return;
    }
    if (pixelIndex == 0) resetTrack(ledCount_);
    trackPixel(pixelIndex, color);
    bus_.SetPixelColor(pixelIndex, rgbFromColor(color));
  }

  void show() override {
    bus_.Show();
  }

private:
  NeoPixelBus<TFeature, TMethod> bus_;
  uint16_t ledCount_;
  uint16_t busCount_;
};

template <typename TFeature, typename TMethod>
class NeoOutputRgbw final : public NeoOutputBase {
public:
  NeoOutputRgbw(uint16_t ledCount, int8_t pin, uint16_t busCount)
      : bus_(busCount, pin), ledCount_(ledCount), busCount_(busCount) {}

  void begin() override {
    bus_.Begin();
    bus_.ClearTo(RgbwColor(0, 0, 0, 0));
    // Same reasoning as NeoOutputRgb: omit Show() here to avoid async RMT
    // race against the render task.
  }

  bool canShow() const override { return bus_.CanShow(); }

  void fillColor(uint32_t color) override {
    if (color == 0) {
      trackFill(0, busCount_);
      bus_.ClearTo(RgbwColor(0, 0, 0, 0));
    } else {
      trackFill(color, ledCount_);
      const RgbwColor rgbw = rgbwFromColor(color);
      for (uint16_t i = 0; i < ledCount_; ++i) {
        bus_.SetPixelColor(i, rgbw);
      }
    }
  }

  void setPixelColor(uint16_t pixelIndex, uint32_t color) override {
    if (pixelIndex >= ledCount_) {
      return;
    }
    if (pixelIndex == 0) resetTrack(ledCount_);
    trackPixel(pixelIndex, color);
    bus_.SetPixelColor(pixelIndex, rgbwFromColor(color));
  }

  void show() override {
    bus_.Show();
  }

private:
  NeoPixelBus<TFeature, TMethod> bus_;
  uint16_t ledCount_;
  uint16_t busCount_;
};

template <typename TFeature, typename TMethod>
NeoOutputBase *createNeoOutputRgbByMethod(uint16_t ledCount, int8_t pin, uint16_t busCount) {
  return new NeoOutputRgb<TFeature, TMethod>(ledCount, pin, busCount);
}

template <typename TFeature, typename TMethod>
NeoOutputBase *createNeoOutputRgbwByMethod(uint16_t ledCount, int8_t pin, uint16_t busCount) {
  return new NeoOutputRgbw<TFeature, TMethod>(ledCount, pin, busCount);
}

template <typename TFeature>
NeoOutputBase *createNeoOutputRgbByChannel(uint8_t outputIndex, uint16_t ledCount, int8_t pin, uint16_t busCount) {
  switch (outputIndex) {
    case 0:
      return createNeoOutputRgbByMethod<TFeature, NeoEsp32Rmt0800KbpsMethod>(ledCount, pin, busCount);
    case 1:
      return createNeoOutputRgbByMethod<TFeature, NeoEsp32Rmt1800KbpsMethod>(ledCount, pin, busCount);
#if !defined(CONFIG_IDF_TARGET_ESP32C3)
    case 2:
      return createNeoOutputRgbByMethod<TFeature, NeoEsp32Rmt2800KbpsMethod>(ledCount, pin, busCount);
    case 3:
      return createNeoOutputRgbByMethod<TFeature, NeoEsp32Rmt3800KbpsMethod>(ledCount, pin, busCount);
#endif
    default:
      return nullptr;
  }
}

template <typename TFeature>
NeoOutputBase *createNeoOutputRgbwByChannel(uint8_t outputIndex, uint16_t ledCount, int8_t pin, uint16_t busCount) {
  switch (outputIndex) {
    case 0:
      return createNeoOutputRgbwByMethod<TFeature, NeoEsp32Rmt0800KbpsMethod>(ledCount, pin, busCount);
    case 1:
      return createNeoOutputRgbwByMethod<TFeature, NeoEsp32Rmt1800KbpsMethod>(ledCount, pin, busCount);
#if !defined(CONFIG_IDF_TARGET_ESP32C3)
    case 2:
      return createNeoOutputRgbwByMethod<TFeature, NeoEsp32Rmt2800KbpsMethod>(ledCount, pin, busCount);
    case 3:
      return createNeoOutputRgbwByMethod<TFeature, NeoEsp32Rmt3800KbpsMethod>(ledCount, pin, busCount);
#endif
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

NeoOutputBase *createNeoOutput(uint8_t outputIndex, const LedDriverOutputConfig &output, uint16_t busCount) {
  if (!output.enabled || output.isDigital || !isAddressableOutputType(output.ledType)) {
    return nullptr;
  }

  if (output.isRgbw) {
    switch (output.colorOrder) {
      case LedDriverColorOrder::RGBW:
        return createNeoOutputRgbwByChannel<NeoRgbwFeature>(outputIndex, output.ledCount, output.pin, busCount);
      case LedDriverColorOrder::GRBW:
      default:
        return createNeoOutputRgbwByChannel<NeoGrbwFeature>(outputIndex, output.ledCount, output.pin, busCount);
    }
  }

  switch (output.colorOrder) {
    case LedDriverColorOrder::RGB:
      return createNeoOutputRgbByChannel<NeoRgbFeature>(outputIndex, output.ledCount, output.pin, busCount);
    case LedDriverColorOrder::BRG:
      return createNeoOutputRgbByChannel<NeoBrgFeature>(outputIndex, output.ledCount, output.pin, busCount);
    case LedDriverColorOrder::RBG:
      return createNeoOutputRgbByChannel<NeoRbgFeature>(outputIndex, output.ledCount, output.pin, busCount);
    case LedDriverColorOrder::GBR:
      return createNeoOutputRgbByChannel<NeoGbrFeature>(outputIndex, output.ledCount, output.pin, busCount);
    case LedDriverColorOrder::BGR:
      return createNeoOutputRgbByChannel<NeoBgrFeature>(outputIndex, output.ledCount, output.pin, busCount);
    case LedDriverColorOrder::GRB:
    default:
      return createNeoOutputRgbByChannel<NeoGrbFeature>(outputIndex, output.ledCount, output.pin, busCount);
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
  // Before destroying old output objects wait for any in-progress async RMT
  // transfer to finish.  Destroying a NeoPixelBus mid-transfer abruptly stops
  // the data line; the strip then interprets the next Show() as a continuation
  // (no reset pulse), which causes extra pixels to light up.
  for (uint8_t i = 0; i < kMaxLedOutputs; ++i) {
    if (outputs_[i] == nullptr) continue;
    const uint32_t startUs = micros();
    while (!asNeoOutput(outputs_[i])->canShow()) {
      if (static_cast<uint32_t>(micros() - startUs) > 50000u) break; // 50 ms safety cap
      delayMicroseconds(10);
    }
  }

  for (uint8_t i = 0; i < kMaxLedOutputs; ++i) {
    destroyNeoOutput(outputs_[i]);
    outputs_[i] = nullptr;
  }

  for (uint8_t i = 0; i < outputCount(); ++i) {
    const LedDriverOutputConfig &output = outputConfig(i);
    // busCount = max(configured ledCount, peak ever seen for this channel).
    // El bus se crea con busCount pixeles so fillColor(0) puede apagar LEDs
    // fisicos que superan ledCount (p.ej. tras reducir la configuracion).
    const uint16_t busCount = (output.ledCount > peakLedCounts_[i]) ? output.ledCount : peakLedCounts_[i];
    peakLedCounts_[i] = busCount;
    outputs_[i] = createNeoOutput(i, output, busCount);
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
  const bool doLog = logNextShow_;
  if (doLog) logNextShow_ = false;

  for (uint8_t i = 0; i < outputCount(); ++i) {
    if (outputs_[i] == nullptr) {
      continue;
    }

    NeoOutputBase *out = asNeoOutput(outputs_[i]);
    out->show();

    if (doLog) {
      const LedDriverOutputConfig &cfg = outputConfig(i);
      Serial.print("[led-show] out=");
      Serial.print(i);
      Serial.print(" total=");
      Serial.print(out->totalCount());
      Serial.print(" lit=");
      Serial.print(out->litCount());
      if (out->litCount() > 0) {
        Serial.print(" first_idx=");
        Serial.print(out->firstLitIndex());
        Serial.print(" first_color=0x");
        Serial.print(out->firstLitColor(), HEX);
      }
      Serial.print(" cfg.ledCount=");
      Serial.print(cfg.ledCount);
      Serial.print(" pin=");
      Serial.println(cfg.pin);
    }
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
  const uint32_t limitedColor = applyPowerLimit(color);

#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
  if (outputIndex >= outputCount() || outputs_[outputIndex] == nullptr) {
    return;
  }

  asNeoOutput(outputs_[outputIndex])->fillColor(limitedColor);
#else
  (void)outputIndex;
  (void)color;
#endif
}

void LedDriverNeoPixelBus::setPixelColor(uint8_t outputIndex, uint16_t pixelIndex, uint32_t color) {
  LedDriver::setOutputColor(outputIndex, color);
  const uint32_t limitedColor = applyPowerLimit(color);

#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
  if (outputIndex >= outputCount() || outputs_[outputIndex] == nullptr) {
    return;
  }

  asNeoOutput(outputs_[outputIndex])->setPixelColor(pixelIndex, limitedColor);
#else
  (void)outputIndex;
  (void)pixelIndex;
  (void)color;
#endif
}
