#include "effects/EffectFixed.h"

bool EffectFixed::supports(uint8_t effectId) const {
  return effectId == CoreState::kEffectFixed;
}

void EffectFixed::renderFrame() {
  CoreState &currentState = state();
  LedDriver &ledDriver = driver();
  const uint32_t scaledBackground = scaleColor(currentState.backgroundColor, currentState.brightness);

  for (uint8_t outputIndex = 0; outputIndex < ledDriver.outputCount(); ++outputIndex) 
  {
    const LedDriverOutputConfig &output = ledDriver.outputConfig(outputIndex);
    if (!output.enabled) 
    {
      continue;
    }

    if (!ledDriver.supportsPerPixelColor(outputIndex) || output.ledCount <= 1) 
    {
      ledDriver.setOutputColor(outputIndex,
                               scaleColor(currentState.primaryColors[outputIndex % 3],
                                          currentState.brightness));
      continue;
    }

    const uint16_t sectionSize = resolveSectionSize(output.ledCount, currentState.sectionCount);
    for (uint16_t pixelIndex = 0; pixelIndex < output.ledCount; ++pixelIndex) 
    {
      uint32_t color = scaledBackground;
      const uint16_t sectionIndex = pixelIndex / sectionSize;
      if (sectionIndex < currentState.sectionCount) 
      {
        color = scaleColor(currentState.primaryColors[sectionIndex % 3], currentState.brightness);
      }
      ledDriver.setPixelColor(outputIndex, pixelIndex, color);
    }
  }
  ledDriver.show();
}