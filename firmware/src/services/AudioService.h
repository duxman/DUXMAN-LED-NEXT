/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/AudioService.h
 * Last commit: 2c35a63 - 2026-04-28
 */

#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

#include "core/Config.h"
#include "core/CoreState.h"

/**
 * AudioService: Captura audio desde entrada I2S configurada en MicrophoneConfig
 * y proporciona métricas de audio (nivel RMS, picos) a los efectos reactivos.
 *
 * Uso:
 *   AudioService audioService(microphoneConfig, coreState);
 *   audioService.begin();
 *   ...
 *   audioService.handle(); // Llamar regularmente en el ciclo de control
 *   
 *   // En un efecto reactivo:
 *   uint8_t audioLevel = audioService.getAudioLevel(); // 0-255
 *   uint32_t peak = audioService.getPeakLevel();
 */
class AudioService {
public:
  AudioService(MicrophoneConfig &microphoneConfig, CoreState &coreState, DebugConfig &debugConfig);
  virtual ~AudioService();

  /**
   * Inicializa I2S y comienza captura de audio.
   * @return true si la inicialización fue exitosa, false en caso contrario.
   */
  bool begin();

  /**
   * Detiene la captura de audio y libera recursos I2S.
   */
  void end();

  /**
   * Procesa buffers de audio y actualiza métricas.
   * Debe llamarse regularmente (ej. cada 10ms).
   * @param nowMs timestamp actual del sistema.
   */
  void handle(unsigned long nowMs);

  /**
   * Obtiene el nivel de audio actual normalizado a 0-255.
   * Basado en RMS de la ventana de audio más reciente.
   */
  uint8_t getAudioLevel() const {
    return audioLevel_;
  }

  /**
   * Obtiene el pico de audio más reciente (valor máximo en la ventana).
   */
  uint32_t getPeakLevel() const {
    return peakLevel_;
  }

  /**
   * Indica si el servicio está activo y capturando.
   */
  bool isActive() const {
    return isActive_;
  }

  /**
   * Indica si hay error en la captura.
   */
  bool hasError() const {
    return hasError_;
  }

private:
  MicrophoneConfig &microphoneConfig_;
  CoreState &coreState_;
  DebugConfig &debugConfig_;

  i2s_port_t i2sPort_ = I2S_NUM_0;
  bool isActive_ = false;
  bool hasError_ = false;

  // Métricas de audio
  uint8_t audioLevel_ = 0;
  uint32_t peakLevel_ = 0;
  unsigned long lastProcessMs_ = 0;
  unsigned long lastLogMs_ = 0;
  size_t lastBytesRead_ = 0;
  int lastReadErr_ = 0;

  // P1: Envolvente attack/decay — mantiene el nivel suavizado en float.
  float smoothedLevel_ = 0.0f;
  // P2: Beat detection — cooldown para evitar beats dobles.
  unsigned long beatCooldownMs_ = 0;
  // P6: Peak hold con decaimiento temporal.
  uint8_t peakHold_ = 0;
  unsigned long peakHoldDecayMs_ = 0;
  // AGC: baseline del nivel ambiente, actualizado lentamente para calibración automática.
  float ambientBaseline_ = 0.0f;

  // Buffer de audio para lectura
  static constexpr size_t kAudioBufferSize = 256;
  static constexpr unsigned long kAudioLogIntervalMs = 1000;

  // P1: Attack rápido para capturar golpes; decay lento para suavizado visual.
  static constexpr float kAttackFactor = 0.80f;
  static constexpr float kDecayFactor  = 0.05f;
  // P2: Beat = spike instantáneo ≥160% del nivel suavizado, encima del umbral mínimo.
  static constexpr float         kBeatSpikeRatio      = 1.60f;
  static constexpr uint8_t       kBeatMinThreshold    = 30;
  static constexpr unsigned long kBeatCooldownMs      = 150;
  // P6: Peak hold 1.2 s; luego descenso de 3 puntos cada 30 ms.
  static constexpr unsigned long kPeakHoldDurationMs  = 1200;
  static constexpr uint8_t       kPeakDecayStep       = 3;
  static constexpr unsigned long kPeakDecayStepMs     = 30;

  // AGC: sqrt(32767/sqrt(2)) ≈ 152 = RMS máximo teórico de int16_t.
  // El AGC calibra el nivel ambiente en ~5 s y mapea todo lo que lo supera a 0-255.
  static constexpr float kSqrtMaxRms        = 152.0f;
  static constexpr float kAgcBaselineFactor = 0.002f;  // tau rápido si diff>40, lento si no
  static constexpr float kAgcMinHeadroom    = 10.0f;   // evita divón entre casi-cero
  // Noise gate: señales post-AGC por debajo de este umbral se suprimen a 0.
  // Elimina la fluctuación del ruido ambiente residual que queda tras el AGC.
  static constexpr float kNoiseGateKnee     = 35.0f;

  int16_t audioBuffer_[kAudioBufferSize] = {0};

  bool initializeI2S();
  void shutdownI2S();
  void processAudioBuffer(unsigned long nowMs);
  uint32_t calculateRMS(const int16_t *buffer, size_t length);
};
