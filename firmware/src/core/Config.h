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

// Configuración de depuración (heartbeat serial, nivel de log).
struct DebugConfig {
  bool enabled = false;
  uint32_t heartbeatMs = 5000;

  static DebugConfig defaults();
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

// Configuración de salidas LED (pines, tipo, orden de color).
struct GpioConfig {
  LedOutput outputs[kMaxLedOutputs];
  uint8_t outputCount = 0;

  static GpioConfig defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload, String *error = nullptr);
  bool validate(String *error = nullptr) const;
};