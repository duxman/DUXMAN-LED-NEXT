/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectEngine.h
 * Last commit: 2c35a63 - 2026-04-28
 */

#pragma once

#include <Arduino.h>

#include "core/CoreState.h"
#include "drivers/LedDriver.h"

class EffectEngine {
public:
  EffectEngine(CoreState &state, LedDriver &driver);
  virtual ~EffectEngine() = default;

  virtual void begin();
  virtual bool supports(uint8_t effectId) const = 0;
  virtual void renderFrame() = 0;

  // Ciclo de vida: llamados por EffectManager al cambiar de efecto activo.
  virtual void onActivate() {}
  virtual void onDeactivate() {}

protected:
  CoreState &state() const {
    return state_;
  }

  LedDriver &driver() const {
    return driver_;
  }

  float reactiveAudio01() const;
  float reactiveGain(float minGain = 0.30f, float maxGain = 1.30f) const;

  uint32_t scaleColor(uint32_t color, uint8_t brightness) const;
  static uint32_t gradientColor(uint32_t colorA, uint32_t colorB, uint32_t colorC,
                                uint16_t pixelIndex, uint16_t pixelCount);
  static uint16_t resolveSectionSize(uint16_t ledCount, uint8_t sectionCount);
  unsigned long effectIntervalMs(uint8_t speedScale) const;
  static float speed01(uint8_t speedScale);
  static float level01(uint8_t levelScale);

  // Helpers matemáticos para efectos dinámicos.
  static float normalizedX(uint16_t pixelIndex, uint16_t pixelCount);
  static float normalizedTimeSec();
  static float clamp01(float v);
  static float smoothstep(float edge0, float edge1, float x);
  static uint32_t lerpColor(uint32_t colorA, uint32_t colorB, float t);
  static uint32_t addColor(uint32_t colorA, uint32_t colorB);
  uint32_t scaleColorFloat(uint32_t color, float gain) const;
  // P4: Interpola entre tres colores primarios según el nivel de audio (0→cA, 50%→cB, max→cC).
  uint32_t audioColorShift(uint32_t cA, uint32_t cB, uint32_t cC) const;
  static uint32_t applyGamma(uint32_t color);

private:
  CoreState &state_;
  LedDriver &driver_;
};
