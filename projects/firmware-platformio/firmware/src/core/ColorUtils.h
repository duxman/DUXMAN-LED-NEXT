/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/core/ColorUtils.h
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace ColorUtils {

inline uint32_t parseHexColor(const char *value, uint32_t fallback) {
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

inline uint32_t parseColorValue(JsonVariantConst value, uint32_t fallback) {
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

inline String formatHexColor(uint32_t color) {
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%06lX", static_cast<unsigned long>(color & 0xFFFFFFUL));
  return String("#") + buffer;
}

} // namespace ColorUtils
