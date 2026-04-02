#pragma once

#include <Arduino.h>

struct EffectDescriptor {
  uint8_t id;
  const char *key;
  const char *label;
  const char *description;
};

namespace EffectRegistry {
constexpr uint8_t kEffectFixed = 0;
constexpr uint8_t kEffectGradient = 1;
constexpr uint8_t kEffectDiagnostic = 2;

const EffectDescriptor *all();
size_t count();
const EffectDescriptor &defaultEffect();
const EffectDescriptor *findById(uint8_t effectId);
const EffectDescriptor *findByKey(const String &effectKey);
const char *keyFor(uint8_t effectId);
const char *labelFor(uint8_t effectId);
uint8_t parseId(const String &value, uint8_t fallback);
String toJsonArray();
String buildHtmlOptions(uint8_t selectedEffectId);
} // namespace EffectRegistry