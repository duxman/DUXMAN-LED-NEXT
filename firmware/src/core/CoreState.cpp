#include "core/CoreState.h"

namespace {
int extractIntField(const String &json, const char *key, int fallback) {
  const String token = String("\"") + key + "\":";
  const int start = json.indexOf(token);
  if (start < 0) {
    return fallback;
  }

  int valueStart = start + token.length();
  while (valueStart < json.length() && (json[valueStart] == ' ' || json[valueStart] == '\t')) {
    valueStart++;
  }

  int valueEnd = valueStart;
  while (valueEnd < json.length() && (isDigit(json[valueEnd]) || json[valueEnd] == '-')) {
    valueEnd++;
  }

  if (valueEnd == valueStart) {
    return fallback;
  }

  return json.substring(valueStart, valueEnd).toInt();
}

bool extractBoolField(const String &json, const char *key, bool fallback) {
  const String token = String("\"") + key + "\":";
  const int start = json.indexOf(token);
  if (start < 0) {
    return fallback;
  }

  int valueStart = start + token.length();
  while (valueStart < json.length() && (json[valueStart] == ' ' || json[valueStart] == '\t')) {
    valueStart++;
  }

  if (json.startsWith("true", valueStart)) {
    return true;
  }
  if (json.startsWith("false", valueStart)) {
    return false;
  }

  const int asInt = extractIntField(json, key, fallback ? 1 : 0);
  return asInt != 0;
}
} // namespace

CoreState CoreState::defaults() {
  return CoreState{};
}

String CoreState::toJson() const {
  String json = "{";
  json += "\"power\":";
  json += power ? "true" : "false";
  json += ",\"brightness\":";
  json += String(brightness);
  json += ",\"effectId\":";
  json += String(effectId);
  json += "}";
  return json;
}

bool CoreState::applyPatchJson(const String &payload) {
  const bool newPower = extractBoolField(payload, "power", power);
  const int newBrightness = extractIntField(payload, "brightness", brightness);
  const int newEffectId = extractIntField(payload, "effectId", effectId);

  const bool changed = (newPower != power) || (newBrightness != brightness) || (newEffectId != effectId);

  power = newPower;
  brightness = static_cast<uint8_t>(constrain(newBrightness, 0, 255));
  effectId = static_cast<uint8_t>(constrain(newEffectId, 0, 255));

  return changed;
}
