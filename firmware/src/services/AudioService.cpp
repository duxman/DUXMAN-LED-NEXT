#include "services/AudioService.h"

#include <cmath>

AudioService::AudioService(NetworkConfig &networkConfig, CoreState &coreState)
    : networkConfig_(networkConfig), coreState_(coreState) {}

AudioService::~AudioService() {
  end();
}

bool AudioService::begin() {
  if (isActive_) {
    return true;
  }

  // Validar configuración de micrófono
  if (!networkConfig_.microphone.enabled) {
    hasError_ = true;
    Serial.println("[audio] begin skipped: microphone.enabled=false");
    return false;
  }

  if (networkConfig_.microphone.source != "generic_i2c") {
    hasError_ = true;
    Serial.print("[audio] begin failed: unsupported source=");
    Serial.println(networkConfig_.microphone.source);
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
    Serial.print(networkConfig_.microphone.gainPercent);
    Serial.print(" noiseFloor=");
    Serial.print(networkConfig_.microphone.noiseFloorPercent);
    Serial.print(" pins=");
    Serial.print(networkConfig_.microphone.pins.bclk);
    Serial.print('/');
    Serial.print(networkConfig_.microphone.pins.ws);
    Serial.print('/');
    Serial.println(networkConfig_.microphone.pins.din);
  }

  if (!isActive_) {
    return;
  }

  // Procesar audio cada 50ms (20 Hz)
  if (nowMs - lastProcessMs_ < 50) {
    return;
  }

  lastProcessMs_ = nowMs;
  processAudioBuffer();
}

bool AudioService::initializeI2S() {
  // Configurar I2S en modo master RX para generar clocks al micro MEMS.
  i2s_config_t i2sConfig = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = networkConfig_.microphone.sampleRate,
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
  pinConfig.bck_io_num = networkConfig_.microphone.pins.bclk;
  pinConfig.ws_io_num = networkConfig_.microphone.pins.ws;
  pinConfig.data_out_num = I2S_PIN_NO_CHANGE;
  pinConfig.data_in_num = networkConfig_.microphone.pins.din;
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
  Serial.print(networkConfig_.microphone.sampleRate);
  Serial.print(" Hz, bclk=");
  Serial.print(networkConfig_.microphone.pins.bclk);
  Serial.print(" ws=");
  Serial.print(networkConfig_.microphone.pins.ws);
  Serial.print(" din=");
  Serial.println(networkConfig_.microphone.pins.din);
  Serial.println("[audio] I2S mode=MASTER_RX channels=RIGHT_LEFT bits=16");

  return true;
}

void AudioService::shutdownI2S() {
  if (i2sPort_ == I2S_NUM_0 || i2sPort_ == I2S_NUM_1) {
    i2s_driver_uninstall(i2sPort_);
  }
}

void AudioService::processAudioBuffer() {
  if (!isActive_) {
    return;
  }

  // Leer datos I2S
  size_t bytesRead = 0;
  esp_err_t err =
      i2s_read(i2sPort_, (void *)audioBuffer_, kAudioBufferSize * sizeof(int16_t), &bytesRead, 10);
  lastBytesRead_ = bytesRead;
  lastReadErr_ = static_cast<int>(err);

  if (err != ESP_OK || bytesRead == 0) {
    // Error o sin datos disponibles, mantener nivel anterior
    return;
  }

  // Convertir bytes leídos a número de muestras
  size_t samplesRead = bytesRead / sizeof(int16_t);
  if (samplesRead == 0) {
    return;
  }

  // Calcular RMS y nivel pico
  uint32_t rms = calculateRMS(audioBuffer_, samplesRead);

  // Normalizar RMS a 0-255
  // RMS de int16 puede ser hasta ~32768 en pico teórico, pero usualmente 0-8000 en audio normal
  // Aplicar gain configurado
  float gainMultiplier = networkConfig_.microphone.gainPercent / 100.0f;
  uint32_t scaledRms = static_cast<uint32_t>(rms * gainMultiplier);

  // Aplicar noise floor configurable antes del mapeo final.
  const uint32_t noiseFloor = static_cast<uint32_t>(networkConfig_.microphone.noiseFloorPercent) * 20U;
  if (scaledRms <= noiseFloor) {
    scaledRms = 0;
  } else {
    scaledRms -= noiseFloor;
  }

  // Mapeo más sensible para micros MEMS típicos:
  // valores bajos ya generan respuesta visible sin necesidad de gritar.
  audioLevel_ = constrain(static_cast<uint8_t>(scaledRms / 3), 0, 255);

  // Guardar pico máximo en esta ventana
  peakLevel_ = scaledRms;

  // Publicar nivel para efectos reactivos. Si no se puede tomar el mutex,
  // hacemos escritura directa para evitar que audioLevel quede congelado.
  if (coreState_.lock(pdMS_TO_TICKS(1))) {
    coreState_.audioLevel = audioLevel_;
    coreState_.unlock();
  } else {
    coreState_.audioLevel = audioLevel_;
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
