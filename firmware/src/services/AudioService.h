#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

#include "core/Config.h"
#include "core/CoreState.h"

/**
 * AudioService: Captura audio desde entrada I2S configurada en NetworkConfig.microphone
 * y proporciona métricas de audio (nivel RMS, picos) a los efectos reactivos.
 *
 * Uso:
 *   AudioService audioService(networkConfig, coreState);
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
  AudioService(NetworkConfig &networkConfig, CoreState &coreState);
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
  NetworkConfig &networkConfig_;
  CoreState &coreState_;

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

  // Buffer de audio para lectura
  static constexpr size_t kAudioBufferSize = 256;
  static constexpr unsigned long kAudioLogIntervalMs = 1000;
  int16_t audioBuffer_[kAudioBufferSize] = {0};

  bool initializeI2S();
  void shutdownI2S();
  void processAudioBuffer();
  uint32_t calculateRMS(const int16_t *buffer, size_t length);
};
