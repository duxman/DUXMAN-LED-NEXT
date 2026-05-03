/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/core/Config.h
 * Last commit: 2c35a63 - 2026-04-28
 */

#pragma once

#include <Arduino.h>

struct NetworkIpConfig {
  String mode = "dhcp";
  String address;
  String gateway;
  String subnet;
  String primaryDns;
  String secondaryDns;
};

struct NetworkWifiConfig {
  String mode = "ap";
  String apAvailability = "untilStaConnected";
  String ssid;
  String password;
};

struct NetworkDnsConfig {
  String hostname = "duxman-led";
};

struct NetworkTimeConfig {
  bool syncOnBoot = true;
  String ntpServer = "europe.pool.ntp.org";
};

// Configuración de red (WiFi, IP, DNS, NTP).
// Debug y Microphone son secciones independientes al mismo nivel.
struct NetworkConfig {
  NetworkWifiConfig wifi;
  NetworkIpConfig ap;
  NetworkIpConfig sta;
  NetworkDnsConfig dns;
  NetworkTimeConfig time;

  static NetworkConfig defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload, String *error = nullptr);
  bool validate(String *error = nullptr) const;
};

// Configuración general (idioma, región, opciones de debug).
struct GeneralConfig {
  // Language and localization
  String language = "en";              // "en", "es", "fr", etc.
  String regionCode = "US";            // "US", "ES", "FR", etc.
  
  // Debug and logging options
  bool debugEnabled = false;
  uint32_t heartbeatMs = 5000;

  static GeneralConfig defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload, String *error = nullptr);
  bool validate(String *error = nullptr) const;
};

// Configuracion de sincronizacion (entrada externa y cluster maestro-esclavo).
struct SyncConfig {
  String mode = "off";          // off | local_effects | ledfx_realtime | cluster_sync
  String role = "client";       // client | server (legacy aliases: slave | master)
  String inputProtocol = "ddp"; // ddp | e131
  uint16_t ddpPort = 4048;
  uint16_t e131UniverseStart = 1;
  uint8_t e131UniverseCount = 1;
  uint16_t udpSyncPort = 21324;
  uint8_t groupMask = 1;
  uint32_t sourceTimeoutMs = 1500;
  String clockSmoothing = "soft"; // off | soft

  static SyncConfig defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload, String *error = nullptr);
  bool validate(String *error = nullptr) const;
};

struct MicrophoneI2sPins {
  int8_t bclk = -1;
  int8_t ws = -1;
  int8_t din = -1;
};

// Configuración del micrófono I2S.
struct MicrophoneConfig {
  bool enabled = false;
  String source = "generic_i2c";
  String profileId = "DEFAULT";
  uint16_t sampleRate = 16000;
  uint16_t fftSize = 512;
  uint8_t gainPercent = 100;
  uint8_t noiseFloorPercent = 8;
  uint8_t noiseGateKnee = 35;
  uint8_t agcResponsePercent = 100;
  MicrophoneI2sPins pins;

  static MicrophoneConfig defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload, String *error = nullptr);
  bool validate(String *error = nullptr) const;
};

// ── Tipos de LED soportados ─────────────────────────────────────
// digital  — LED digital simple (no direccionable, on/off)
// ws2812b  — WS2812B (3 cables, más común)
// ws2811   — WS2811 (3 cables, 12V habitual)
// ws2813   — WS2813 (3 cables, señal backup)
// ws2815   — WS2815 (3 cables, 12V)
// sk6812   — SK6812 (RGBW, 4 canales)
// tm1814   — TM1814 (RGBW/12V según variante)
//
// ── Ordenes de color / color fijo ───────────────────────────────
// Direccionables: RGB, GRB, BRG, RBG, GBR, BGR (3ch), RGBW, GRBW (4ch)
// Digital:        R, G, B, W (color físico del LED)

constexpr uint8_t kMaxLedOutputs = 4;

struct LedOutput {
  uint8_t id = 0;
  int8_t pin = -1;
  uint16_t ledCount = 1;
  String ledType = "ws2812b";
  String colorOrder = "GRB";
};

struct PowerConfig {
  // ── Power limiting ─────────────────────────────────────────────
  bool powerLimitEnabled = false;
  uint16_t maxTotalCurrentmA = 5000;
  uint8_t milliAmpsPerLedBase = 60;
  
  // ── Voltage sag correction (V = Vcc - I*R) ──────────────────────
  bool voltageSagCorrectionEnabled = false;
  float cableResistanceOhms = 0.1f;
  float supplyVoltageNominal = 5.0f;
  float minAcceptableVoltage = 4.5f;
  
  // ── Thermal throttling ─────────────────────────────────────────
  bool thermalThrottlingEnabled = false;
  int8_t temperatureSensorPin = -1;
  int16_t tempThrottleStartC = 50;
  int16_t tempThrottleMaxC = 70;
  
  // ── Smart dimming ─────────────────────────────────────────────
  bool smartDimmingEnabled = false;
  bool preserveBlueFrequency = true;
  uint8_t priorityMode = 0;
};

// Configuración de salidas LED (pines, tipo, orden de color).
struct GpioConfig {
  LedOutput outputs[kMaxLedOutputs];
  uint8_t outputCount = 0;
  PowerConfig power;

  static GpioConfig defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload, String *error = nullptr);
  bool validate(String *error = nullptr) const;
};