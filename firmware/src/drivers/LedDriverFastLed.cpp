#include "drivers/LedDriverFastLed.h"

#include "core/BuildProfile.h"

#if DUX_LED_BACKEND == DUX_LED_BACKEND_FASTLED
#include <FastLED.h>

namespace {
CLEDController *gFastLedController = nullptr;
CRGB *gFastLedPixels = nullptr;

template <template <uint8_t, EOrder> class TChipset>
CLEDController *addFastLedController(EOrder order, CRGB *pixels, uint16_t ledCount) {
  switch (order) {
    case RGB:
      return &FastLED.addLeds<TChipset, DUX_LED_PIN, RGB>(pixels, ledCount);
    case GRB:
      return &FastLED.addLeds<TChipset, DUX_LED_PIN, GRB>(pixels, ledCount);
    case BRG:
      return &FastLED.addLeds<TChipset, DUX_LED_PIN, BRG>(pixels, ledCount);
    case RBG:
      return &FastLED.addLeds<TChipset, DUX_LED_PIN, RBG>(pixels, ledCount);
    case GBR:
      return &FastLED.addLeds<TChipset, DUX_LED_PIN, GBR>(pixels, ledCount);
    case BGR:
      return &FastLED.addLeds<TChipset, DUX_LED_PIN, BGR>(pixels, ledCount);
    default:
      return &FastLED.addLeds<TChipset, DUX_LED_PIN, GRB>(pixels, ledCount);
  }
}

EOrder resolveFastLedOrder(LedDriverColorOrder colorOrder) {
  switch (colorOrder) {
    case LedDriverColorOrder::RGB:
    case LedDriverColorOrder::RGBW:
      return RGB;
    case LedDriverColorOrder::GRB:
    case LedDriverColorOrder::GRBW:
      return GRB;
    case LedDriverColorOrder::BRG:
      return BRG;
    case LedDriverColorOrder::RBG:
      return RBG;
    case LedDriverColorOrder::GBR:
      return GBR;
    case LedDriverColorOrder::BGR:
      return BGR;
    default:
      return GRB;
  }
}

CLEDController *createFastLedController(const LedDriverOutputConfig &output, CRGB *pixels) {
  const EOrder order = resolveFastLedOrder(output.colorOrder);

  switch (output.ledType) {
    case LedDriverType::Ws2811:
      return addFastLedController<WS2811>(order, pixels, output.ledCount);
    case LedDriverType::Ws2813:
      return addFastLedController<WS2813>(order, pixels, output.ledCount);
    case LedDriverType::Ws2815:
      return addFastLedController<WS2815>(order, pixels, output.ledCount);
    case LedDriverType::Sk6812:
      return addFastLedController<SK6812>(order, pixels, output.ledCount);
    case LedDriverType::Tm1814:
      return addFastLedController<TM1812>(order, pixels, output.ledCount);
    case LedDriverType::Ws2812b:
    default:
      return addFastLedController<WS2812B>(order, pixels, output.ledCount);
  }
}
} // namespace
#endif

void LedDriverFastLed::begin() {
  level_ = 0;
  activeOutputIndex_ = 0xFF;
  activeLedCount_ = 0;

#if DUX_LED_BACKEND == DUX_LED_BACKEND_FASTLED
  delete[] gFastLedPixels;
  gFastLedPixels = nullptr;
  gFastLedController = nullptr;

  for (uint8_t i = 0; i < outputCount(); ++i) {
    const LedDriverOutputConfig &output = outputConfig(i);
    if (!output.enabled || output.isDigital || output.pin != BuildProfile::kLedPin) {
      continue;
    }

    gFastLedPixels = new CRGB[output.ledCount];
    if (gFastLedPixels == nullptr) {
      break;
    }

    gFastLedController = createFastLedController(output, gFastLedPixels);
    activeOutputIndex_ = i;
    activeLedCount_ = output.ledCount;
    FastLED.clear(true);
    initialized_ = gFastLedController != nullptr;
    break;
  }
#else
  initialized_ = false;
#endif
}

void LedDriverFastLed::show() {
  if (!initialized_) {
    return;
  }

#if DUX_LED_BACKEND == DUX_LED_BACKEND_FASTLED
  if (gFastLedPixels == nullptr || activeOutputIndex_ >= outputCount()) {
    return;
  }

  fill_solid(gFastLedPixels, activeLedCount_, CRGB(outputLevel(activeOutputIndex_), outputLevel(activeOutputIndex_), outputLevel(activeOutputIndex_)));
  FastLED.show();
#endif
}

const char *LedDriverFastLed::backendName() const {
  return "fastled";
}