#include "effects/EffectBlinkGradient.h"

namespace {
uint32_t scaleBackgroundColor(uint32_t color, uint8_t brightness) {
  const uint32_t red = ((color >> 16) & 0xFF) * brightness / 255;
  const uint32_t green = ((color >> 8) & 0xFF) * brightness / 255;
  const uint32_t blue = (color & 0xFF) * brightness / 255;
  return (red << 16) | (green << 8) | blue;
}

void renderBackground(LedDriver &ledDriver, uint32_t backgroundColor, uint8_t brightness) {
  const uint32_t scaledBackground = scaleBackgroundColor(backgroundColor, brightness);
  for (uint8_t outputIndex = 0; outputIndex < ledDriver.outputCount(); ++outputIndex) {
    const LedDriverOutputConfig &output = ledDriver.outputConfig(outputIndex);
    if (!output.enabled) {
      continue;
    }

    if (!ledDriver.supportsPerPixelColor(outputIndex) || output.ledCount <= 1) {
      ledDriver.setOutputColor(outputIndex, scaledBackground);
      continue;
    }

    for (uint16_t pixelIndex = 0; pixelIndex < output.ledCount; ++pixelIndex) {
      ledDriver.setPixelColor(outputIndex, pixelIndex, scaledBackground);
    }
  }
}
} // namespace

bool EffectBlinkGradient::supports(uint8_t effectId) const {
  return effectId == CoreState::kEffectBlinkGradient;
}

void EffectBlinkGradient::renderFrame() {
  const unsigned long now = millis();
  if (lastToggleAtMs_ == 0) {
    lastToggleAtMs_ = now;
  } else if (now - lastToggleAtMs_ >= effectIntervalMs(state().effectSpeed)) {
    visible_ = !visible_;
    lastToggleAtMs_ = now;
  }

  CoreState &currentState = state();
  LedDriver &ledDriver = driver();
  if (!visible_) {
    renderBackground(ledDriver, currentState.backgroundColor, currentState.brightness);
    ledDriver.show();
    return;
  }

  const uint32_t scaledBackground = scaleColor(currentState.backgroundColor, currentState.brightness);
  for (uint8_t outputIndex = 0; outputIndex < ledDriver.outputCount(); ++outputIndex) {
    const LedDriverOutputConfig &output = ledDriver.outputConfig(outputIndex);
    if (!output.enabled) {
      continue;
    }

    if (!ledDriver.supportsPerPixelColor(outputIndex) || output.ledCount <= 1) {
      const uint32_t flatColor = gradientColor(currentState.primaryColors[0],
                                               currentState.primaryColors[1],
                                               currentState.primaryColors[2],
                                               outputIndex % 3, 3);
      ledDriver.setOutputColor(outputIndex, scaleColor(flatColor, currentState.brightness));
      continue;
    }

    const uint16_t sectionSize = resolveSectionSize(output.ledCount, currentState.sectionCount);
    for (uint16_t pixelIndex = 0; pixelIndex < output.ledCount; ++pixelIndex) {
      uint32_t color = scaledBackground;
      const uint16_t sectionIndex = pixelIndex / sectionSize;
      if (sectionIndex < currentState.sectionCount) {
        const uint16_t sectionStart = sectionIndex * sectionSize;
        const uint16_t sectionEnd = min<uint16_t>(output.ledCount, sectionStart + sectionSize);
        const uint16_t sectionLength = max<uint16_t>(1, sectionEnd - sectionStart);
        color = scaleColor(
            gradientColor(currentState.primaryColors[0], currentState.primaryColors[1],
                          currentState.primaryColors[2], pixelIndex - sectionStart, sectionLength),
            currentState.brightness);
      }
      ledDriver.setPixelColor(outputIndex, pixelIndex, color);
    }
  }

  ledDriver.show();
}
