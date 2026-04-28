/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/drivers/LedDriverDigital.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

#include "drivers/LedDriverDigital.h"

void LedDriverDigital::begin() {
  level_ = 0;
  initialized_ = true;

  for (uint8_t i = 0; i < outputCount(); ++i) {
    const LedDriverOutputConfig &output = outputConfig(i);
    if (!output.enabled || !output.isDigital) {
      continue;
    }

    pinMode(output.pin, OUTPUT);
    digitalWrite(output.pin, LOW);
  }
}

void LedDriverDigital::show() {
  if (!initialized_) {
    return;
  }

  for (uint8_t i = 0; i < outputCount(); ++i) {
    const LedDriverOutputConfig &output = outputConfig(i);
    if (!output.enabled || !output.isDigital) {
      continue;
    }

    digitalWrite(output.pin, outputLevel(i) > 0 ? HIGH : LOW);
  }
}

const char *LedDriverDigital::backendName() const {
  return "digital";
}