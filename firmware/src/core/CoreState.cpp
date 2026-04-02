#include "core/CoreState.h"

#include <ArduinoJson.h>

namespace {
uint32_t parseHexColor(const char *value, uint32_t fallback) {
  if (value == nullptr) {
    return fallback;
  }

  String normalized = value;
  normalized.trim();
  if (normalized.startsWith("#")) {
    normalized.remove(0, 1);
  }
  if (normalized.length() != 6) {
    return fallback;
  }

  for (size_t i = 0; i < normalized.length(); ++i) {
    if (!isxdigit(normalized[i])) {
      return fallback;
    }
  }

  return strtoul(normalized.c_str(), nullptr, 16) & 0xFFFFFFUL;
}

uint32_t parseColorValue(JsonVariantConst value, uint32_t fallback) {
  if (value.isNull()) {
    return fallback;
  }

  if (value.is<const char *>()) {
    return parseHexColor(value.as<const char *>(), fallback);
  }

  if (value.is<String>()) {
    return parseHexColor(value.as<String>().c_str(), fallback);
  }

  if (value.is<uint32_t>()) {
    return value.as<uint32_t>() & 0xFFFFFFUL;
  }

  if (value.is<int>()) {
    const long numeric = value.as<long>();
    if (numeric < 0) {
      return fallback;
    }
    return static_cast<uint32_t>(numeric) & 0xFFFFFFUL;
  }

  return fallback;
}

String formatHexColor(uint32_t color) {
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%06lX", static_cast<unsigned long>(color & 0xFFFFFFUL));
  return String("#") + buffer;
}

uint8_t parseEffectId(JsonVariantConst value, uint8_t fallback) {
  if (value.isNull()) {
    return fallback;
  }

  if (value.is<const char *>()) {
    return EffectRegistry::parseId(String(value.as<const char *>()), fallback);
  }

  if (value.is<String>()) {
    return EffectRegistry::parseId(value.as<String>(), fallback);
  }

  if (value.is<int>()) {
    const int effectId = constrain(value.as<int>(), 0, 255);
    const EffectDescriptor *effect = EffectRegistry::findById(static_cast<uint8_t>(effectId));
    return effect != nullptr ? effect->id : fallback;
  }

  return fallback;
}
} // namespace

CoreState CoreState::defaults() {
  return CoreState{};
}

const char *CoreState::effectName(uint8_t effectId) {
  return EffectRegistry::keyFor(effectId);
}

const char *CoreState::effectLabel(uint8_t effectId) {
  return EffectRegistry::labelFor(effectId);
}

String CoreState::toJson() const {
  JsonDocument doc;
  doc["power"] = power;
  doc["brightness"] = brightness;
  doc["effectId"] = effectId;
  doc["effect"] = effectName(effectId);
  doc["effectLabel"] = effectLabel(effectId);
  doc["sectionCount"] = sectionCount;
  doc["availableEffects"] = serialized(EffectRegistry::toJsonArray());

  JsonArray colors = doc["primaryColors"].to<JsonArray>();
  for (uint8_t i = 0; i < 3; ++i) {
    colors.add(formatHexColor(primaryColors[i]));
  }

  doc["backgroundColor"] = formatHexColor(backgroundColor);

  String json;
  serializeJson(doc, json);
  return json;
}

bool CoreState::applyPatchJson(const String &payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    return false;
  }

  const JsonObjectConst root = doc.as<JsonObjectConst>();

  CoreState next = *this;

  if (!root["power"].isNull()) {
    next.power = root["power"].as<bool>();
  }

  if (!root["brightness"].isNull()) {
    next.brightness = static_cast<uint8_t>(constrain(root["brightness"].as<int>(), 0, 255));
  }

  if (!root["effectId"].isNull()) {
    next.effectId = parseEffectId(root["effectId"], next.effectId);
  }

  if (!root["effect"].isNull()) {
    next.effectId = parseEffectId(root["effect"], next.effectId);
  }

  if (!root["sectionCount"].isNull()) {
    next.sectionCount = static_cast<uint8_t>(constrain(root["sectionCount"].as<int>(), 1, 255));
  }

  if (root["primaryColors"].is<JsonArrayConst>()) {
    JsonArrayConst colors = root["primaryColors"].as<JsonArrayConst>();
    for (uint8_t i = 0; i < 3 && i < colors.size(); ++i) {
      next.primaryColors[i] = parseColorValue(colors[i], next.primaryColors[i]);
    }
  }

  next.backgroundColor = parseColorValue(root["backgroundColor"], next.backgroundColor);

  bool changed = next.power != power || next.brightness != brightness ||
                 next.effectId != effectId || next.sectionCount != sectionCount ||
                 next.backgroundColor != backgroundColor;
  for (uint8_t i = 0; i < 3 && !changed; ++i) {
    if (next.primaryColors[i] != primaryColors[i]) {
      changed = true;
    }
  }

  *this = next;

  return changed;
}
