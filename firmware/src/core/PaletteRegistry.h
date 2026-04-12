#pragma once

#include <Arduino.h>

struct ColorPaletteDescriptor {
  int16_t id;
  const char *key;
  const char *label;
  const char *style;
  uint32_t color1;
  uint32_t color2;
  uint32_t color3;
  const char *description;
};

namespace PaletteRegistry {
constexpr int16_t kManualPalette = -1;

const ColorPaletteDescriptor *all();
size_t count();
const ColorPaletteDescriptor *findById(int16_t paletteId);
const ColorPaletteDescriptor *findByKey(const String &paletteKey);
int16_t parseId(const String &value, int16_t fallback);
const char *keyFor(int16_t paletteId);
const char *labelFor(int16_t paletteId);
const char *styleFor(int16_t paletteId);
bool applyToColors(int16_t paletteId, uint32_t (&primaryColors)[3]);
String toJsonArray();
} // namespace PaletteRegistry
