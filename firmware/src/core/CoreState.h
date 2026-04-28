/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/core/CoreState.h
 * Last commit: ec3d96f - 2026-04-28
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "core/PaletteRegistry.h"
#include "effects/EffectRegistry.h"

struct CoreState {
  static constexpr uint8_t kEffectFixed = EffectRegistry::kEffectFixed;
  static constexpr uint8_t kEffectGradient = EffectRegistry::kEffectGradient;
  static constexpr uint8_t kEffectBlinkFixed = EffectRegistry::kEffectBlinkFixed;
  static constexpr uint8_t kEffectBlinkGradient = EffectRegistry::kEffectBlinkGradient;
  static constexpr uint8_t kEffectDiagnostic = EffectRegistry::kEffectDiagnostic;
  static constexpr uint8_t kEffectBreathFixed = EffectRegistry::kEffectBreathFixed;
  static constexpr uint8_t kEffectBreathGradient = EffectRegistry::kEffectBreathGradient;
  static constexpr uint8_t kEffectTripleChase = EffectRegistry::kEffectTripleChase;
  static constexpr uint8_t kEffectGradientMeteor = EffectRegistry::kEffectGradientMeteor;
  static constexpr uint8_t kEffectScanningPulse = EffectRegistry::kEffectScanningPulse;
  static constexpr uint8_t kEffectTrigRibbon = EffectRegistry::kEffectTrigRibbon;
  static constexpr uint8_t kEffectLavaFlow = EffectRegistry::kEffectLavaFlow;
  static constexpr uint8_t kEffectPolarIce = EffectRegistry::kEffectPolarIce;
  static constexpr uint8_t kEffectStellarTwinkle = EffectRegistry::kEffectStellarTwinkle;
  static constexpr uint8_t kEffectRandomColorPop = EffectRegistry::kEffectRandomColorPop;
  static constexpr uint8_t kEffectBouncingPhysics = EffectRegistry::kEffectBouncingPhysics;
  static constexpr uint8_t kEffectAudioPulse       = EffectRegistry::kEffectAudioPulse;

  bool power = true;
  uint8_t brightness = 128;
  uint8_t effectId = kEffectFixed;
  uint8_t sectionCount = 3;
  uint8_t effectSpeed = 10;
  uint8_t effectLevel = 5;
  int16_t paletteId = 0; // -1 = manual, >=0 = palette predefinida
  uint32_t primaryColors[3] = {0xFF4D00, 0xFFD400, 0x00B8D9};
  uint32_t backgroundColor = 0x000000;
  bool reactiveToAudio = false;
  uint8_t audioLevel = 0;
  bool beatDetected = false;      // Pulso: true durante ~50ms al detectar un beat de audio.
  uint8_t audioPeakHold = 0;     // Nivel pico con decaimiento lento (P6).

  static CoreState defaults();
  static void setMutex(SemaphoreHandle_t mutex);
  static const char *effectName(uint8_t effectId);
  static const char *effectLabel(uint8_t effectId);
  bool lock(TickType_t timeout = portMAX_DELAY) const;
  void unlock() const;
  CoreState snapshot() const;
  String toJson() const;
  bool applyPatchJson(const String &payload);
};
