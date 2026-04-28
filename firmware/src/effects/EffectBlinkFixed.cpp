/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectBlinkFixed.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

#include "effects/EffectBlinkFixed.h"

namespace {
uint16_t resolveSectionSizeValue(uint16_t ledCount, uint8_t sectionCount) {
  const uint8_t safeSections = max<uint8_t>(1, sectionCount);
  return max<uint16_t>(1, static_cast<uint16_t>((ledCount + safeSections - 1) / safeSections));
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
  const uint32_t scaledBackground = scaleColor(currentState.backgroundColor, currentState.brightness);
  if (!visible_) {
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
    ledDriver.show();
    return;
  }

  for (uint8_t outputIndex = 0; outputIndex < ledDriver.outputCount(); ++outputIndex) {
    const LedDriverOutputConfig &output = ledDriver.outputConfig(outputIndex);
    if (!output.enabled) {
      continue;
    }

    if (!ledDriver.supportsPerPixelColor(outputIndex) || output.ledCount <= 1) {
      ledDriver.setOutputColor(outputIndex,
                               scaleColor(currentState.primaryColors[outputIndex % 3],
                                          currentState.brightness));
      continue;
    }

    for (uint16_t pixelIndex = 0; pixelIndex < output.ledCount; ++pixelIndex) {
      ledDriver.setPixelColor(outputIndex, pixelIndex, scaledBackground);
    }

    const uint16_t sectionSize = resolveSectionSizeValue(output.ledCount, currentState.sectionCount);
    for (uint8_t sectionIndex = 0; sectionIndex < currentState.sectionCount; ++sectionIndex) {
      const uint16_t sectionStart = static_cast<uint16_t>(sectionIndex) * sectionSize;
      if (sectionStart >= output.ledCount) {
        break;
      }

      const uint16_t sectionEnd = min<uint16_t>(output.ledCount, sectionStart + sectionSize);
      const uint32_t sectionColor = scaleColor(currentState.primaryColors[sectionIndex % 3],
                                               currentState.brightness);
      for (uint16_t pixelIndex = sectionStart; pixelIndex < sectionEnd; ++pixelIndex) {
        ledDriver.setPixelColor(outputIndex, pixelIndex, sectionColor);
      }
    }
  }

  ledDriver.show();
}
