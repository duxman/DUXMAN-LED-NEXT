/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectRegistry.h
 * Last commit: a67d822 - 2026-04-12
 */

#pragma once

#include <Arduino.h>

struct EffectDescriptor {
  uint8_t id;
  const char *key;
  const char *label;
  const char *description;
  bool usesSpeed;
  bool usesAudio;
};

namespace EffectRegistry {
constexpr uint8_t kEffectFixed = 0;
constexpr uint8_t kEffectGradient = 1;
constexpr uint8_t kEffectDiagnostic = 2;
constexpr uint8_t kEffectBlinkFixed = 3;
constexpr uint8_t kEffectBlinkGradient = 4;
constexpr uint8_t kEffectBreathFixed = 5;
constexpr uint8_t kEffectBreathGradient = 6;
constexpr uint8_t kEffectTripleChase = 7;
constexpr uint8_t kEffectGradientMeteor = 8;
constexpr uint8_t kEffectScanningPulse = 9;
constexpr uint8_t kEffectTrigRibbon = 10;
constexpr uint8_t kEffectLavaFlow = 11;
constexpr uint8_t kEffectPolarIce = 12;
constexpr uint8_t kEffectStellarTwinkle = 13;
constexpr uint8_t kEffectRandomColorPop = 14;
constexpr uint8_t kEffectBouncingPhysics = 15;
constexpr uint8_t kEffectAudioPulse      = 16;

const EffectDescriptor *all();
size_t count();
const EffectDescriptor &defaultEffect();
const EffectDescriptor *findById(uint8_t effectId);
const EffectDescriptor *findByKey(const String &effectKey);
const char *keyFor(uint8_t effectId);
const char *labelFor(uint8_t effectId);
bool usesAudio(uint8_t effectId);
uint8_t parseId(const String &value, uint8_t fallback);
String toJsonArray();
String buildHtmlOptions(uint8_t selectedEffectId);
} // namespace EffectRegistry