#include "services/AudioService.h"

#include <cmath>

AudioService::AudioService(MicrophoneConfig &microphoneConfig, CoreState &coreState)
    : microphoneConfig_(microphoneConfig), coreState_(coreState) {}

AudioService::~AudioService() {
  end();
}

bool AudioService::begin() {
  if (isActive_) {
    return true;
  }

  // Validar configuración de micrófono
  if (!microphoneConfig_.enabled) {
    hasError_ = true;
    Serial.println("[audio] begin skipped: microphone.enabled=false");
    return false;
  }

  if (microphoneConfig_.source != "generic_i2c") {
    hasError_ = true;
    Serial.print("[audio] begin failed: unsupported source=");
    Serial.println(microphoneConfig_.source);
    return false;
  }

  // Inicializar I2S
  if (!initializeI2S()) {
    hasError_ = true;
    isActive_ = false;
    return false;
  }

  isActive_ = true;
  hasError_ = false;
  lastProcessMs_ = millis();
  lastLogMs_ = 0;

  Serial.println("[audio] AudioService initialized successfully");
  return true;
}

void AudioService::end() {
  if (!isActive_) {
    return;
  }

  shutdownI2S();
  isActive_ = false;
  hasError_ = true;

  Serial.println("[audio] AudioService stopped");
}

void AudioService::handle(unsigned long nowMs) {
  if (nowMs - lastLogMs_ >= kAudioLogIntervalMs) {
    lastLogMs_ = nowMs;
    Serial.print("[audio.dbg] active=");
    Serial.print(isActive_ ? "1" : "0");
    Serial.print(" error=");
    Serial.print(hasError_ ? "1" : "0");
    Serial.print(" level=");
    Serial.print(audioLevel_);
    Serial.print(" peak=");
    Serial.print(peakLevel_);
    Serial.print(" bytes=");
    Serial.print(lastBytesRead_);
    Serial.print(" readErr=");
    Serial.print(lastReadErr_);
    Serial.print(" reactive=");
    Serial.print(coreState_.reactiveToAudio ? "1" : "0");
    Serial.print(" effectId=");
    Serial.print(coreState_.effectId);
    Serial.print(" gain=");
    Serial.print(microphoneConfig_.gainPercent);
    Serial.print(" noiseFloor=");
    Serial.print(microphoneConfig_.noiseFloorPercent);
    Serial.print(" baseline=");
    Serial.print(static_cast<int>(ambientBaseline_));
    Serial.print(" pins=");
    Serial.print(microphoneConfig_.pins.bclk);
    Serial.print('/');
    Serial.print(microphoneConfig_.pins.ws);
    Serial.print('/');
    Serial.println(microphoneConfig_.pins.din);
  }

  if (!isActive_) {
    return;
  }

  // Procesar audio cada 50ms (20 Hz)
  if (nowMs - lastProcessMs_ < 50) {
    return;
  }

  lastProcessMs_ = nowMs;
  processAudioBuffer(nowMs);
}

bool AudioService::initializeI2S() {
  // Configurar I2S en modo master RX para generar clocks al micro MEMS.
  i2s_config_t i2sConfig = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = microphoneConfig_.sampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S_MSB,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };

  i2s_pin_config_t pinConfig;
  memset(&pinConfig, 0, sizeof(pinConfig));
  pinConfig.bck_io_num = microphoneConfig_.pins.bclk;
  pinConfig.ws_io_num = microphoneConfig_.pins.ws;
  pinConfig.data_out_num = I2S_PIN_NO_CHANGE;
  pinConfig.data_in_num = microphoneConfig_.pins.din;
  pinConfig.mck_io_num = I2S_PIN_NO_CHANGE;

  esp_err_t err = i2s_driver_install(i2sPort_, &i2sConfig, 0, NULL);
  if (err != ESP_OK) {
    Serial.print("[audio] i2s_driver_install failed: ");
    Serial.println(err);
    return false;
  }

  err = i2s_set_pin(i2sPort_, &pinConfig);
  if (err != ESP_OK) {
    Serial.print("[audio] i2s_set_pin failed: ");
    Serial.println(err);
    i2s_driver_uninstall(i2sPort_);
    return false;
  }

  i2s_zero_dma_buffer(i2sPort_);

  Serial.print("[audio] I2S initialized: sampleRate=");
  Serial.print(microphoneConfig_.sampleRate);
  Serial.print(" Hz, bclk=");
  Serial.print(microphoneConfig_.pins.bclk);
  Serial.print(" ws=");
  Serial.print(microphoneConfig_.pins.ws);
  Serial.print(" din=");
  Serial.println(microphoneConfig_.pins.din);
  Serial.println("[audio] I2S mode=MASTER_RX channels=RIGHT_LEFT bits=16");

  return true;
}

void AudioService::shutdownI2S() {
  if (i2sPort_ == I2S_NUM_0 || i2sPort_ == I2S_NUM_1) {
    i2s_driver_uninstall(i2sPort_);
  }
}

void AudioService::processAudioBuffer(unsigned long nowMs) {
  if (!isActive_) {
    return;
  }

  // P2: Limpiar flag de beat del ciclo anterior para que sea un pulso limpio.
  coreState_.beatDetected = false;

  // Leer datos I2S.
  size_t bytesRead = 0;
  esp_err_t err =
      i2s_read(i2sPort_, (void *)audioBuffer_, kAudioBufferSize * sizeof(int16_t), &bytesRead, 10);
  lastBytesRead_ = bytesRead;
  lastReadErr_ = static_cast<int>(err);

  if (err != ESP_OK || bytesRead == 0) {
    // Sin datos: dejar caer el nivel suavizado para que los efectos reflejen el silencio.
    smoothedLevel_ += (0.0f - smoothedLevel_) * kDecayFactor;
    audioLevel_ = static_cast<uint8_t>(smoothedLevel_);
    return;
  }

  size_t samplesRead = bytesRead / sizeof(int16_t);
  if (samplesRead == 0) {
    return;
  }

  // Calcular RMS, aplicar ganancia y filtro de ruido estático configurable.
  uint32_t rms = calculateRMS(audioBuffer_, samplesRead);
  const float gainMultiplier = microphoneConfig_.gainPercent / 100.0f;
  uint32_t scaledRms = static_cast<uint32_t>(rms * gainMultiplier);

  const uint32_t noiseFloor =
      static_cast<uint32_t>(microphoneConfig_.noiseFloorPercent) * 20U;
  if (scaledRms <= noiseFloor) {
    scaledRms = 0;
  } else {
    scaledRms -= noiseFloor;
  }
  peakLevel_ = scaledRms;

  // Normalización con raíz cuadrada: escalado perceptual mejor que lineal.
  // kSqrtMaxRms = sqrt(23169) ≈ 152 (RMS máximo teórico de señal int16_t a plena escala).
  const float sqrtRaw = sqrtf(static_cast<float>(scaledRms));
  const float rawF    = (sqrtRaw / kSqrtMaxRms) * 255.0f;
  const float rawFClamped = rawF < 0.0f ? 0.0f : (rawF > 255.0f ? 255.0f : rawF);

  // AGC: seguimiento asimétrico del nivel ambiente.
  // Sube lento para evitar que transientes eleven la baseline permanentemente.
  // Baja MUY lento para que la baseline nunca siga una caída de señal real.
  // Primeros 8 s: convergencia rápida para calibrar el entorno en el arranque.
  const float agcDiff  = rawFClamped - ambientBaseline_;
  const bool  isWarmup = (nowMs < 8000UL);
  float agcFactor;
  if (agcDiff > 0.0f) {
    // Baseline sube: rápido en warmup, lento en estado estacionario.
    agcFactor = isWarmup ? kAgcBaselineFactor * 12.0f : kAgcBaselineFactor;
  } else {
    // Baseline baja: siempre muy lento para no exponer ruido como señal.
    agcFactor = kAgcBaselineFactor * 0.15f;
  }
  ambientBaseline_ += agcDiff * agcFactor;
  if (ambientBaseline_ < 0.0f)   ambientBaseline_ = 0.0f;
  if (ambientBaseline_ > 250.0f) ambientBaseline_ = 250.0f;

  // Señal activa = lo que supera el baseline, re-escalada al headroom disponible.
  const float headroom      = 255.0f - ambientBaseline_;
  const float aboveBaseline = rawFClamped - ambientBaseline_;
  float agcSignal = 0.0f;
  if (headroom > kAgcMinHeadroom && aboveBaseline > 0.0f) {
    agcSignal = (aboveBaseline / headroom) * 255.0f;
    if (agcSignal > 255.0f) agcSignal = 255.0f;
  }

  // Noise gate suave: elimina la fluctuación del ruido ambiente residual post-AGC.
  // Señal < kNoiseGateKnee → 0; por encima → rampa lineal hasta 255.
  if (agcSignal <= kNoiseGateKnee) {
    agcSignal = 0.0f;
  } else {
    agcSignal = (agcSignal - kNoiseGateKnee) * (255.0f / (255.0f - kNoiseGateKnee));
    if (agcSignal > 255.0f) agcSignal = 255.0f;
  }

  // P2: Detección de beat — spike instantáneo ≥160% sobre el nivel suavizado actual.
  if (agcSignal >= smoothedLevel_ * kBeatSpikeRatio &&
      agcSignal >= static_cast<float>(kBeatMinThreshold) &&
      nowMs >= beatCooldownMs_) {
    coreState_.beatDetected = true;
    beatCooldownMs_ = nowMs + kBeatCooldownMs;
  }

  // P1: Envolvente attack/decay — ataque rápido, caída lenta.
  if (agcSignal > smoothedLevel_) {
    smoothedLevel_ += (agcSignal - smoothedLevel_) * kAttackFactor;
  } else {
    smoothedLevel_ += (agcSignal - smoothedLevel_) * kDecayFactor;
  }
  audioLevel_ = static_cast<uint8_t>(constrain(static_cast<int>(smoothedLevel_), 0, 255));

  // P6: Peak hold con decaimiento temporal.
  if (audioLevel_ >= peakHold_) {
    peakHold_        = audioLevel_;
    peakHoldDecayMs_ = nowMs + kPeakHoldDurationMs;
  } else if (nowMs >= peakHoldDecayMs_ && peakHold_ > 0) {
    peakHold_        = (peakHold_ > kPeakDecayStep) ? (peakHold_ - kPeakDecayStep) : 0;
    peakHoldDecayMs_ = nowMs + kPeakDecayStepMs;
  }

  // Publicar métricas al CoreState compartido.
  if (coreState_.lock(pdMS_TO_TICKS(1))) {
    coreState_.audioLevel    = audioLevel_;
    coreState_.audioPeakHold = peakHold_;
    coreState_.unlock();
  } else {
    coreState_.audioLevel    = audioLevel_;
    coreState_.audioPeakHold = peakHold_;
  }
}

uint32_t AudioService::calculateRMS(const int16_t *buffer, size_t length) {
  if (length == 0) {
    return 0;
  }

  uint64_t sum = 0;
  for (size_t i = 0; i < length; ++i) {
    int32_t sample = static_cast<int32_t>(buffer[i]);
    sum += sample * sample;
  }

  // RMS = sqrt(sum / length)
  float rms = sqrtf(static_cast<float>(sum) / static_cast<float>(length));
  return static_cast<uint32_t>(rms);
}
