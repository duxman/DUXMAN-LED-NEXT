#include "effects/EffectEngine.h"

namespace {
uint8_t blendChannel(uint8_t start, uint8_t end, uint16_t step, uint16_t maxStep) {
  if (maxStep == 0) {
    return start;
  }
  const int delta = static_cast<int>(end) - static_cast<int>(start);
  return static_cast<uint8_t>(start + (delta * step) / maxStep);
}
} // namespace

EffectEngine::EffectEngine(CoreState &state, LedDriver &driver) : state_(state), driver_(driver) {}

void EffectEngine::begin() {
  driver_.begin();
}

uint32_t EffectEngine::scaleColor(uint32_t color, uint8_t brightness) {
  const uint32_t red = ((color >> 16) & 0xFF) * brightness / 255;
  const uint32_t green = ((color >> 8) & 0xFF) * brightness / 255;
  const uint32_t blue = (color & 0xFF) * brightness / 255;
  return (red << 16) | (green << 8) | blue;
}

namespace {
uint32_t blendColor(uint32_t start, uint32_t end, uint16_t step, uint16_t maxStep) {
  const uint8_t startRed = static_cast<uint8_t>((start >> 16) & 0xFF);
  const uint8_t startGreen = static_cast<uint8_t>((start >> 8) & 0xFF);
  const uint8_t startBlue = static_cast<uint8_t>(start & 0xFF);
  const uint8_t endRed = static_cast<uint8_t>((end >> 16) & 0xFF);
  const uint8_t endGreen = static_cast<uint8_t>((end >> 8) & 0xFF);
  const uint8_t endBlue = static_cast<uint8_t>(end & 0xFF);

  return (static_cast<uint32_t>(blendChannel(startRed, endRed, step, maxStep)) << 16) |
         (static_cast<uint32_t>(blendChannel(startGreen, endGreen, step, maxStep)) << 8) |
         static_cast<uint32_t>(blendChannel(startBlue, endBlue, step, maxStep));
}
} // namespace

uint32_t EffectEngine::gradientColor(uint32_t colorA, uint32_t colorB, uint32_t colorC,
                                     uint16_t pixelIndex, uint16_t pixelCount) {
  if (pixelCount <= 1) {
    return colorA;
  }

  const uint32_t position = static_cast<uint32_t>(pixelIndex) * 512UL /
                            static_cast<uint32_t>(pixelCount - 1);
  if (position <= 256UL) {
    return blendColor(colorA, colorB, static_cast<uint16_t>(position), 256);
  }

  return blendColor(colorB, colorC, static_cast<uint16_t>(position - 256UL), 256);
}

uint16_t EffectEngine::resolveSectionSize(uint16_t ledCount, uint8_t sectionCount) {
  const uint8_t safeSections = max<uint8_t>(1, sectionCount);
  return max<uint16_t>(1, static_cast<uint16_t>((ledCount + safeSections - 1) / safeSections));
}
