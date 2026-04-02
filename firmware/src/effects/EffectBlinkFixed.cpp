#include "effects/EffectBlinkFixed.h"

namespace {
uint32_t scaleBackgroundColor(uint32_t color, uint8_t brightness) {
  const uint32_t red = ((color >> 16) & 0xFF) * brightness / 255;
  const uint32_t green = ((color >> 8) & 0xFF) * brightness / 255;
  const uint32_t blue = (color & 0xFF) * brightness / 255;
  return (red << 16) | (green << 8) | blue;
}

uint16_t resolveSectionSizeValue(uint16_t ledCount, uint8_t sectionCount) {
  const uint8_t safeSections = max<uint8_t>(1, sectionCount);
  return max<uint16_t>(1, static_cast<uint16_t>((ledCount + safeSections - 1) / safeSections));
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

void renderFixedSections(LedDriver &ledDriver, const CoreState &state) {
  const uint32_t scaledBackground = scaleBackgroundColor(state.backgroundColor, state.brightness);

  for (uint8_t outputIndex = 0; outputIndex < ledDriver.outputCount(); ++outputIndex) {
    const LedDriverOutputConfig &output = ledDriver.outputConfig(outputIndex);
    if (!output.enabled) {
      continue;
    }

    if (!ledDriver.supportsPerPixelColor(outputIndex) || output.ledCount <= 1) {
      ledDriver.setOutputColor(outputIndex,
                               scaleBackgroundColor(state.primaryColors[outputIndex % 3],
                                                    state.brightness));
      continue;
    }

    for (uint16_t pixelIndex = 0; pixelIndex < output.ledCount; ++pixelIndex) {
      ledDriver.setPixelColor(outputIndex, pixelIndex, scaledBackground);
    }

    const uint16_t sectionSize = resolveSectionSizeValue(output.ledCount, state.sectionCount);
    for (uint8_t sectionIndex = 0; sectionIndex < state.sectionCount; ++sectionIndex) {
      const uint16_t sectionStart = static_cast<uint16_t>(sectionIndex) * sectionSize;
      if (sectionStart >= output.ledCount) {
        break;
      }

      const uint16_t sectionEnd = min<uint16_t>(output.ledCount, sectionStart + sectionSize);
      const uint32_t sectionColor = scaleBackgroundColor(state.primaryColors[sectionIndex % 3],
                                                         state.brightness);
      for (uint16_t pixelIndex = sectionStart; pixelIndex < sectionEnd; ++pixelIndex) {
        ledDriver.setPixelColor(outputIndex, pixelIndex, sectionColor);
      }
    }
  }
}
} // namespace

bool EffectBlinkFixed::supports(uint8_t effectId) const {
  return effectId == CoreState::kEffectBlinkFixed;
}

void EffectBlinkFixed::renderFrame() {
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

  renderFixedSections(ledDriver, currentState);
  ledDriver.show();
}
