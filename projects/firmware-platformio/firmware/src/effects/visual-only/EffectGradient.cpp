/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectGradient.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

#include "effects/visual-only/EffectGradient.h"

bool EffectGradient::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectGradient;
}

void EffectGradient::renderFrame() {
  CoreState &currentState = state();
  LedDriver &ledDriver = driver();
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