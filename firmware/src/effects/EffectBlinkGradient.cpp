/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectBlinkGradient.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

#include "effects/EffectBlinkGradient.h"

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
