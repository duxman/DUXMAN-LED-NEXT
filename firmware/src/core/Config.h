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

struct DebugConfig {
  bool enabled = false;
  uint32_t heartbeatMs = 5000;
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

struct GpioConfig {
  LedOutput outputs[kMaxLedOutputs];
  uint8_t outputCount = 0;

  static GpioConfig defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload, String *error = nullptr);
  bool validate(String *error = nullptr) const;
};

struct NetworkConfig {
  NetworkWifiConfig wifi;
  NetworkIpConfig ap;
  NetworkIpConfig sta;
  NetworkDnsConfig dns;
  NetworkTimeConfig time;
  DebugConfig debug;

  static NetworkConfig defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload, String *error = nullptr);
  bool validate(String *error = nullptr) const;
};