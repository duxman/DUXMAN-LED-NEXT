/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/core/PaletteRegistry.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

#include "core/PaletteRegistry.h"

#include <ArduinoJson.h>

namespace {
constexpr ColorPaletteDescriptor kPalettes[] = {
    {0, "ember_citrus_cyan", "Ember Citrus Cyan", "high-contrast", 0xFF4D00, 0xFFD400, 0x00B8D9,
     "Calida y energica con acento frio para buen contraste."},
    {1, "sunset_drive", "Sunset Drive", "warm", 0xFF5E5B, 0xFFA552, 0xFFD166,
     "Naranja rojizo y dorado para ambientes acogedores."},
    {2, "ocean_neon", "Ocean Neon", "neon", 0x00E5FF, 0x00FFA3, 0x0A84FF,
     "Fria y brillante para setup moderno."},
    {3, "aurora_soft", "Aurora Soft", "pastel", 0xA3E1DC, 0xB5B9FF, 0xFFC6D9,
     "Pastel equilibrada para brillo medio/bajo."},
    {4, "vaporwave", "Vaporwave", "party", 0xFF3CAC, 0x784BA0, 0x2B86C5,
     "Magenta y azul intenso para show nocturno."},
    {5, "forest_pop", "Forest Pop", "cold", 0x00A86B, 0x6BD66B, 0xB8F2E6,
     "Verdes limpios con alto detalle en transiciones."},
    {6, "arctic_pulse", "Arctic Pulse", "cold", 0xB8F3FF, 0x66D9FF, 0x1C8DFF,
     "Azules frios con punto de luz elevado."},
    {7, "golden_hour", "Golden Hour", "warm", 0xFF7F11, 0xFFB627, 0xFFE17D,
     "Tonos de tarde con excelente lectura en tiras RGB."},
    {8, "synthwave", "Synthwave", "party", 0xF72585, 0x7209B7, 0x3A0CA3,
     "Escena de alto impacto para musica y beats."},
    {9, "mint_lavender", "Mint Lavender", "pastel", 0xB8F2E6, 0xCDB4DB, 0xA9DEF9,
     "Suave y elegante para ambientacion continua."},
    {10, "lava_ice", "Lava Ice", "high-contrast", 0xFF3D00, 0x00B4D8, 0xF9C80E,
     "Combinacion extrema calido-frio para dinamicos."},
    {11, "club_rgb", "Club RGB", "neon", 0xFF006E, 0x3A86FF, 0x8AC926,
     "Trio saturado para fiesta y audio reactivo."},
};
} // namespace

namespace PaletteRegistry {
const ColorPaletteDescriptor *all() {
  return kPalettes;
}

size_t count() {
  return sizeof(kPalettes) / sizeof(kPalettes[0]);
}

const ColorPaletteDescriptor *findById(int16_t paletteId) {
  for (size_t i = 0; i < count(); ++i) {
    if (kPalettes[i].id == paletteId) {
      return &kPalettes[i];
    }
  }
  return nullptr;
}

const ColorPaletteDescriptor *findByKey(const String &paletteKey) {
  for (size_t i = 0; i < count(); ++i) {
    if (paletteKey == kPalettes[i].key) {
      return &kPalettes[i];
    }
  }
  return nullptr;
}

int16_t parseId(const String &value, int16_t fallback) {
  if (value.isEmpty()) {
    return fallback;
  }

  bool isNumeric = true;
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (!(isDigit(c) || (i == 0 && c == '-'))) {
      isNumeric = false;
      break;
    }
  }

  if (isNumeric) {
    const int16_t paletteId = static_cast<int16_t>(value.toInt());
    if (paletteId == kManualPalette) {
      return kManualPalette;
    }
    const ColorPaletteDescriptor *palette = findById(paletteId);
    return palette != nullptr ? palette->id : fallback;
  }

  const ColorPaletteDescriptor *palette = findByKey(value);
  return palette != nullptr ? palette->id : fallback;
}

const char *keyFor(int16_t paletteId) {
  const ColorPaletteDescriptor *palette = findById(paletteId);
  return palette != nullptr ? palette->key : "manual";
}

const char *labelFor(int16_t paletteId) {
  const ColorPaletteDescriptor *palette = findById(paletteId);
  return palette != nullptr ? palette->label : "Manual";
}

const char *styleFor(int16_t paletteId) {
  const ColorPaletteDescriptor *palette = findById(paletteId);
  return palette != nullptr ? palette->style : "custom";
}

bool applyToColors(int16_t paletteId, uint32_t (&primaryColors)[3]) {
  const ColorPaletteDescriptor *palette = findById(paletteId);
  if (palette == nullptr) {
    return false;
  }

  primaryColors[0] = palette->color1;
  primaryColors[1] = palette->color2;
  primaryColors[2] = palette->color3;
  return true;
}

String toJsonArray() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (size_t i = 0; i < count(); ++i) {
    JsonObject item = arr.add<JsonObject>();
    item["id"] = kPalettes[i].id;
    item["key"] = kPalettes[i].key;
    item["label"] = kPalettes[i].label;
    item["style"] = kPalettes[i].style;
    item["description"] = kPalettes[i].description;

    JsonArray colors = item["primaryColors"].to<JsonArray>();
    char c1[8];
    char c2[8];
    char c3[8];
    snprintf(c1, sizeof(c1), "%06lX", static_cast<unsigned long>(kPalettes[i].color1 & 0xFFFFFFUL));
    snprintf(c2, sizeof(c2), "%06lX", static_cast<unsigned long>(kPalettes[i].color2 & 0xFFFFFFUL));
    snprintf(c3, sizeof(c3), "%06lX", static_cast<unsigned long>(kPalettes[i].color3 & 0xFFFFFFUL));
    colors.add(String("#") + c1);
    colors.add(String("#") + c2);
    colors.add(String("#") + c3);
  }

  String out;
  serializeJson(doc, out);
  return out;
}
} // namespace PaletteRegistry
