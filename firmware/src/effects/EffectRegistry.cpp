#include "effects/EffectRegistry.h"

#include <ArduinoJson.h>

namespace {
constexpr EffectDescriptor kEffects[] = {
    {EffectRegistry::kEffectFixed, "fixed", "Color fijo",
     "Alterna los 3 colores por secciones fijas."},
    {EffectRegistry::kEffectGradient, "gradient", "Degradado fijo",
     "Genera un degradado estatico con los 3 colores dentro de cada seccion."},
  {EffectRegistry::kEffectDiagnostic, "diagnostic", "Diagnostico",
   "Activa solo la primera salida en rojo para aislar la linea principal."},
};
}

namespace EffectRegistry {
const EffectDescriptor *all() {
  return kEffects;
}

size_t count() {
  return sizeof(kEffects) / sizeof(kEffects[0]);
}

const EffectDescriptor &defaultEffect() {
  return kEffects[0];
}

const EffectDescriptor *findById(uint8_t effectId) {
  for (size_t i = 0; i < count(); ++i) {
    if (kEffects[i].id == effectId) {
      return &kEffects[i];
    }
  }
  return nullptr;
}

const EffectDescriptor *findByKey(const String &effectKey) {
  for (size_t i = 0; i < count(); ++i) {
    if (effectKey == kEffects[i].key) {
      return &kEffects[i];
    }
  }

  if (effectKey == "fijo" || effectKey == "color_fijo") {
    return findById(kEffectFixed);
  }
  if (effectKey == "degradado" || effectKey == "degradado_fijo") {
    return findById(kEffectGradient);
  }
  if (effectKey == "diagnostic" || effectKey == "diagnostico") {
    return findById(kEffectDiagnostic);
  }

  return nullptr;
}

const char *keyFor(uint8_t effectId) {
  const EffectDescriptor *effect = findById(effectId);
  return effect != nullptr ? effect->key : defaultEffect().key;
}

const char *labelFor(uint8_t effectId) {
  const EffectDescriptor *effect = findById(effectId);
  return effect != nullptr ? effect->label : defaultEffect().label;
}

uint8_t parseId(const String &value, uint8_t fallback) {
  if (value.isEmpty()) {
    return fallback;
  }

  bool isNumeric = true;
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isDigit(value[i])) {
      isNumeric = false;
      break;
    }
  }

  if (isNumeric) {
    const EffectDescriptor *effect = findById(static_cast<uint8_t>(value.toInt()));
    return effect != nullptr ? effect->id : fallback;
  }

  const EffectDescriptor *effect = findByKey(value);
  return effect != nullptr ? effect->id : fallback;
}

String toJsonArray() {
  JsonDocument doc;
  JsonArray effects = doc.to<JsonArray>();
  for (size_t i = 0; i < count(); ++i) {
    JsonObject item = effects.add<JsonObject>();
    item["id"] = kEffects[i].id;
    item["key"] = kEffects[i].key;
    item["label"] = kEffects[i].label;
    item["description"] = kEffects[i].description;
  }

  String json;
  serializeJson(doc, json);
  return json;
}

String buildHtmlOptions(uint8_t selectedEffectId) {
  String html;
  for (size_t i = 0; i < count(); ++i) {
    html += "<option value='";
    html += kEffects[i].key;
    html += "'";
    if (kEffects[i].id == selectedEffectId) {
      html += " selected";
    }
    html += ">";
    html += kEffects[i].label;
    html += "</option>";
  }
  return html;
}
} // namespace EffectRegistry