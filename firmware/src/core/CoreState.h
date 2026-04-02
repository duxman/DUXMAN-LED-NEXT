#pragma once

#include <Arduino.h>

#include "effects/EffectRegistry.h"

struct CoreState {
  static constexpr uint8_t kEffectFixed = EffectRegistry::kEffectFixed;
  static constexpr uint8_t kEffectGradient = EffectRegistry::kEffectGradient;
  static constexpr uint8_t kEffectBlinkFixed = EffectRegistry::kEffectBlinkFixed;
  static constexpr uint8_t kEffectBlinkGradient = EffectRegistry::kEffectBlinkGradient;
  static constexpr uint8_t kEffectDiagnostic = EffectRegistry::kEffectDiagnostic;

  bool power = true;
  uint8_t brightness = 128;
  uint8_t effectId = kEffectFixed;
  uint8_t sectionCount = 3;
  uint8_t effectSpeed = 10;
  uint32_t primaryColors[3] = {0xFF4D00, 0xFFD400, 0x00B8D9};
  uint32_t backgroundColor = 0x000000;

  static CoreState defaults();
  static const char *effectName(uint8_t effectId);
  static const char *effectLabel(uint8_t effectId);
  String toJson() const;
  bool applyPatchJson(const String &payload);
};
