/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectDiagnostic.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

#include "effects/visual-only/EffectDiagnostic.h"

bool EffectDiagnostic::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectDiagnostic;
}

void EffectDiagnostic::renderFrame() {
  CoreState &currentState = state();
  LedDriver &ledDriver = driver();
  const uint32_t activeColor = scaleColor(0xFF0000UL, currentState.brightness);

  for (uint8_t outputIndex = 0; outputIndex < ledDriver.outputCount(); ++outputIndex) {
    const LedDriverOutputConfig &output = ledDriver.outputConfig(outputIndex);
    if (!output.enabled) {
      continue;
    }

    if (outputIndex == 0) {
      if (!ledDriver.supportsPerPixelColor(outputIndex) || output.ledCount <= 1) {
        ledDriver.setOutputColor(outputIndex, activeColor);
        continue;
      }

      for (uint16_t pixelIndex = 0; pixelIndex < output.ledCount; ++pixelIndex) {
        ledDriver.setPixelColor(outputIndex, pixelIndex, activeColor);
      }
      continue;
    }

    if (!ledDriver.supportsPerPixelColor(outputIndex) || output.ledCount <= 1) {
      ledDriver.setOutputColor(outputIndex, 0);
      continue;
    }

    for (uint16_t pixelIndex = 0; pixelIndex < output.ledCount; ++pixelIndex) {
      ledDriver.setPixelColor(outputIndex, pixelIndex, 0);
    }
  }
  ledDriver.show();
}