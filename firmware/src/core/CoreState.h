#pragma once

#include <Arduino.h>

struct CoreState {
  bool power = true;
  uint8_t brightness = 128;
  uint8_t effectId = 0;

  static CoreState defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload);
};
