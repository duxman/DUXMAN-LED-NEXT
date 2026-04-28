/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/core/Config.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

#include "core/Config.h"
#include "core/BuildProfile.h"

#include <ArduinoJson.h>

namespace {
constexpr const char *kModeAp = "ap";
constexpr const char *kModeSta = "sta";
constexpr const char *kModeApSta = "ap_sta";

constexpr const char *kApAlways = "always";
constexpr const char *kApUntilStaConnected = "untilStaConnected";

constexpr const char *kIpDhcp = "dhcp";
constexpr const char *kIpStatic = "static";
constexpr uint32_t kHeartbeatMinMs = 0;
constexpr uint32_t kHeartbeatMaxMs = 600000;
constexpr uint16_t kMicSampleRateMin = 8000;
constexpr uint16_t kMicSampleRateMax = 48000;
constexpr uint8_t kMicGainMin = 1;
constexpr uint8_t kMicGainMax = 200;
constexpr uint8_t kMicNoiseFloorMax = 100;
constexpr const char *kMicSourceGeneric = "generic_i2c";
constexpr const char *kMicSourceLegacyI2s = "i2s";
constexpr const char *kMicProfileGledopto = "gledopto_gl_c_017wl_d";
constexpr int8_t kMicGledoptoSckPin = 21;
constexpr int8_t kMicGledoptoWsPin = 5;
constexpr int8_t kMicGledoptoSdPin = 26;

bool isOneOf(const String &value, const char *a, const char *b, const char *c = nullptr) {
  if (value == a || value == b) {
    return true;
  }
  return c != nullptr && value == c;
}

bool isValidIpv4(const String &ip) {
  if (ip.isEmpty()) {
    return false;
  }

  int parts = 0;
  int start = 0;
  while (start <= ip.length()) {
    const int dot = ip.indexOf('.', start);
    const int end = (dot < 0) ? ip.length() : dot;
    if (end == start) {
      return false;
    }

    int value = 0;
    for (int i = start; i < end; ++i) {
      const char ch = ip[i];
      if (!isDigit(ch)) {
        return false;
      }
      value = (value * 10) + (ch - '0');
      if (value > 255) {
        return false;
      }
    }

    parts++;
    if (dot < 0) {
      break;
    }
    start = dot + 1;
  }

  return parts == 4;
}

bool isValidHostname(const String &hostname) {
  if (hostname.isEmpty() || hostname.length() > 63) {
    return false;
  }

  if (hostname[0] == '-' || hostname[hostname.length() - 1] == '-') {
    return false;
  }

  for (size_t i = 0; i < hostname.length(); ++i) {
    const char ch = hostname[i];
    const bool ok = isAlphaNumeric(ch) || ch == '-';
    if (!ok) {
      return false;
    }
  }

  return true;
}

void setIfPresent(const JsonObjectConst &source, const char *key, String &target) {
  if (!source[key].isNull()) {
    target = source[key].as<String>();
  }
}

void setBoolIfPresent(const JsonObjectConst &source, const char *key, bool &target) {
  JsonVariantConst value = source[key];
  if (value.isNull()) {
    return;
  }

  if (value.is<bool>()) {
    target = value.as<bool>();
    return;
  }

  if (value.is<int>() || value.is<long>()) {
    target = value.as<long>() != 0;
  }
}

void setUIntIfPresent(const JsonObjectConst &source, const char *key, uint32_t &target) {
  JsonVariantConst value = source[key];
  if (value.isNull()) {
    return;
  }

  if (value.is<unsigned long>()) {
    target = value.as<unsigned long>();
    return;
  }

  if (value.is<long>()) {
    const long parsed = value.as<long>();
    if (parsed >= 0) {
      target = static_cast<uint32_t>(parsed);
    }
  }
}

void setUInt8IfPresent(const JsonObjectConst &source, const char *key, uint8_t &target) {
  JsonVariantConst value = source[key];
  if (value.isNull()) {
    return;
  }

  if (value.is<unsigned int>() || value.is<int>()) {
    const int parsed = value.as<int>();
    if (parsed >= 0 && parsed <= 255) {
      target = static_cast<uint8_t>(parsed);
    }
  }
}

void patchIpConfig(const JsonObjectConst &source, NetworkIpConfig &target) {
  setIfPresent(source, "mode", target.mode);
  setIfPresent(source, "address", target.address);
  setIfPresent(source, "gateway", target.gateway);
  setIfPresent(source, "subnet", target.subnet);
  setIfPresent(source, "primaryDns", target.primaryDns);
  setIfPresent(source, "secondaryDns", target.secondaryDns);
}

void setInt8IfPresent(const JsonObjectConst &source, const char *key, int8_t &target) {
  JsonVariantConst value = source[key];
  if (value.isNull()) {
    return;
  }
  if (value.is<int>()) {
    const int parsed = value.as<int>();
    if (parsed >= -1 && parsed <= 127) {
      target = static_cast<int8_t>(parsed);
    }
  }
}

void setUInt16IfPresent(const JsonObjectConst &source, const char *key, uint16_t &target) {
  JsonVariantConst value = source[key];
  if (value.isNull()) {
    return;
  }
  if (value.is<int>() || value.is<unsigned int>()) {
    const int parsed = value.as<int>();
    if (parsed >= 0 && parsed <= 65535) {
      target = static_cast<uint16_t>(parsed);
    }
  }
}

constexpr int kLedCountMin = 1;
constexpr int kLedCountMax = 1500;
constexpr int kGpioPinMin = -1;
constexpr int kGpioPinMax = 48;

bool isValidLedType(const String &type) {
  return type == "digital" || type == "ws2812b" || type == "ws2811" ||
         type == "ws2813" || type == "ws2815" || type == "sk6812" ||
         type == "tm1814";
}

bool isDigitalLedType(const String &type) {
  return type == "digital";
}

bool isValidDigitalColor(const String &color) {
  return color == "R" || color == "G" || color == "B" || color == "W";
}

bool isValidColorOrder(const String &order) {
  return order == "GRB" || order == "RGB" || order == "BRG" ||
         order == "RBG" || order == "GBR" || order == "BGR" ||
         order == "RGBW" || order == "GRBW";
}

// Input-only GPIOs en ESP32 clásico (no usar para salidas)
bool isInputOnlyGpio(int8_t pin) {
  return pin == 34 || pin == 35 || pin == 36 || pin == 39;
}

bool isValidMicFftSize(uint16_t size) {
  return size == 256 || size == 512 || size == 1024 || size == 2048;
}

bool isValidMicProfileId(const String &profileId) {
  return profileId == "DEFAULT" || profileId == kMicProfileGledopto;
}

bool areDistinctPins(int8_t a, int8_t b, int8_t c) {
  if (a >= 0 && b >= 0 && a == b) {
    return false;
  }
  if (a >= 0 && c >= 0 && a == c) {
    return false;
  }
  if (b >= 0 && c >= 0 && b == c) {
    return false;
  }
  return true;
}

bool isGledoptoReservedMicPin(int8_t pin) {
  return pin == 1 || pin == 2 || pin == 4 || pin == 16;
}

void applyGledoptoMicDefaults(MicrophoneConfig &cfg) {
  cfg.source = kMicSourceGeneric;
  cfg.profileId = kMicProfileGledopto;
  cfg.pins.bclk = kMicGledoptoSckPin;
  cfg.pins.ws = kMicGledoptoWsPin;
  cfg.pins.din = kMicGledoptoSdPin;
}
} // namespace

// ── MicrophoneConfig ───────────────────────────────────────────

MicrophoneConfig MicrophoneConfig::defaults() {
  MicrophoneConfig config;
  config.enabled = false;
  config.source = kMicSourceGeneric;
  config.profileId = kMicProfileGledopto;
  config.sampleRate = 16000;
  config.fftSize = 512;
  config.gainPercent = 100;
  config.noiseFloorPercent = 8;
  config.pins.bclk = kMicGledoptoSckPin;
  config.pins.ws = kMicGledoptoWsPin;
  config.pins.din = kMicGledoptoSdPin;
  return config;
}

String MicrophoneConfig::toJson() const {
  JsonDocument doc;
  JsonObject mic = doc["microphone"].to<JsonObject>();
  mic["enabled"] = enabled;
  mic["source"] = source;
  mic["profileId"] = profileId;
  mic["sampleRate"] = sampleRate;
  mic["fftSize"] = fftSize;
  mic["gainPercent"] = gainPercent;
  mic["noiseFloorPercent"] = noiseFloorPercent;

  JsonObject pins = mic["pins"].to<JsonObject>();
  pins["bclk"] = this->pins.bclk;
  pins["ws"] = this->pins.ws;
  pins["din"] = this->pins.din;

  String out;
  serializeJson(doc, out);
  return out;
}

bool MicrophoneConfig::validate(String *error) const {
  if (source != kMicSourceGeneric && source != kMicSourceLegacyI2s) {
    if (error != nullptr) {
      *error = "invalid_microphone_source";
    }
    return false;
  }

  if (!isValidMicProfileId(profileId)) {
    if (error != nullptr) {
      *error = "invalid_microphone_profile_id";
    }
    return false;
  }

  if (sampleRate < kMicSampleRateMin || sampleRate > kMicSampleRateMax) {
    if (error != nullptr) {
      *error = "invalid_microphone_sample_rate";
    }
    return false;
  }

  if (!isValidMicFftSize(fftSize)) {
    if (error != nullptr) {
      *error = "invalid_microphone_fft_size";
    }
    return false;
  }

  if (gainPercent < kMicGainMin || gainPercent > kMicGainMax) {
    if (error != nullptr) {
      *error = "invalid_microphone_gain";
    }
    return false;
  }

  if (noiseFloorPercent > kMicNoiseFloorMax) {
    if (error != nullptr) {
      *error = "invalid_microphone_noise_floor";
    }
    return false;
  }

  if (pins.bclk < kGpioPinMin || pins.bclk > kGpioPinMax ||
      pins.ws < kGpioPinMin || pins.ws > kGpioPinMax ||
      pins.din < kGpioPinMin || pins.din > kGpioPinMax) {
    if (error != nullptr) {
      *error = "invalid_microphone_pin_range";
    }
    return false;
  }

  if (!areDistinctPins(pins.bclk, pins.ws, pins.din)) {
    if (error != nullptr) {
      *error = "duplicate_microphone_pins";
    }
    return false;
  }

  if (enabled) {
    if (pins.bclk < 0 || pins.ws < 0 || pins.din < 0) {
      if (error != nullptr) {
        *error = "missing_microphone_pins";
      }
      return false;
    }

    if (isInputOnlyGpio(pins.bclk) || isInputOnlyGpio(pins.ws)) {
      if (error != nullptr) {
        *error = "invalid_microphone_clock_pin";
      }
      return false;
    }
  }

  if (profileId == kMicProfileGledopto) {
    if (isGledoptoReservedMicPin(pins.bclk) || isGledoptoReservedMicPin(pins.ws) || isGledoptoReservedMicPin(pins.din)) {
      if (error != nullptr) {
        *error = "microphone_pin_reserved_by_profile";
      }
      return false;
    }
  }

  return true;
}

bool MicrophoneConfig::applyPatchJson(const String &payload, String *error) {
  JsonDocument doc;
  const DeserializationError parseResult = deserializeJson(doc, payload);
  if (parseResult) {
    if (error != nullptr) {
      *error = "invalid_json";
    }
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst micObj = root["microphone"].isNull() ? root : root["microphone"].as<JsonObjectConst>();

  MicrophoneConfig candidate = *this;

  setBoolIfPresent(micObj, "enabled", candidate.enabled);
  setIfPresent(micObj, "source", candidate.source);
  setIfPresent(micObj, "profileId", candidate.profileId);
  setUInt16IfPresent(micObj, "sampleRate", candidate.sampleRate);
  setUInt16IfPresent(micObj, "fftSize", candidate.fftSize);
  setUInt8IfPresent(micObj, "gainPercent", candidate.gainPercent);
  setUInt8IfPresent(micObj, "noiseFloorPercent", candidate.noiseFloorPercent);

  JsonObjectConst pinsObj = micObj["pins"].as<JsonObjectConst>();
  if (!pinsObj.isNull()) {
    setInt8IfPresent(pinsObj, "bclk", candidate.pins.bclk);
    setInt8IfPresent(pinsObj, "ws", candidate.pins.ws);
    setInt8IfPresent(pinsObj, "din", candidate.pins.din);
  }

  if (candidate.source == kMicSourceLegacyI2s) {
    candidate.source = kMicSourceGeneric;
  }

  if (candidate.profileId == kMicProfileGledopto) {
    applyGledoptoMicDefaults(candidate);
  }

  String validationError;
  if (!candidate.validate(&validationError)) {
    if (error != nullptr) {
      *error = validationError;
    }
    return false;
  }

  const bool changed = (candidate.toJson() != this->toJson());
  *this = candidate;
  if (error != nullptr) {
    error->clear();
  }
  return changed;
}

// ── GpioConfig ──────────────────────────────────────────────────

GpioConfig GpioConfig::defaults() {
  GpioConfig config;
  config.outputCount = 1;
  LedOutput &out = config.outputs[0];
  out.id = 0;
  out.pin = static_cast<int8_t>(BuildProfile::kLedPin);
  out.ledCount = static_cast<uint16_t>(BuildProfile::kLedCount);
  out.ledType = "ws2812b";
  out.colorOrder = "GRB";
  return config;
}

String GpioConfig::toJson() const {
  JsonDocument doc;
  JsonObject gpio = doc["gpio"].to<JsonObject>();
  JsonArray arr = gpio["outputs"].to<JsonArray>();

  for (uint8_t i = 0; i < outputCount; ++i) {
    JsonObject item = arr.add<JsonObject>();
    item["id"] = outputs[i].id;
    item["pin"] = outputs[i].pin;
    item["ledCount"] = outputs[i].ledCount;
    item["ledType"] = outputs[i].ledType;
    item["colorOrder"] = outputs[i].colorOrder;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

bool GpioConfig::validate(String *error) const {
  if (outputCount > kMaxLedOutputs) {
    if (error) *error = "too_many_outputs";
    return false;
  }

  for (uint8_t i = 0; i < outputCount; ++i) {
    const LedOutput &o = outputs[i];

    if (o.pin < kGpioPinMin || o.pin > kGpioPinMax) {
      if (error) *error = "invalid_pin_output_" + String(i);
      return false;
    }
    if (o.pin >= 0 && isInputOnlyGpio(o.pin)) {
      if (error) *error = "pin_input_only_output_" + String(i);
      return false;
    }
    if (o.ledCount < kLedCountMin || o.ledCount > kLedCountMax) {
      if (error) *error = "invalid_led_count_output_" + String(i);
      return false;
    }
    if (!isValidLedType(o.ledType)) {
      if (error) *error = "invalid_led_type_output_" + String(i);
      return false;
    }
    if (isDigitalLedType(o.ledType)) {
      if (!isValidDigitalColor(o.colorOrder)) {
        if (error) *error = "invalid_digital_color_output_" + String(i);
        return false;
      }
      if (o.ledCount != 1) {
        if (error) *error = "digital_led_count_must_be_1_output_" + String(i);
        return false;
      }
    } else {
      if (!isValidColorOrder(o.colorOrder)) {
        if (error) *error = "invalid_color_order_output_" + String(i);
        return false;
      }
    }

    // Comprobar pines duplicados (solo si pin >= 0)
    for (uint8_t j = 0; j < i; ++j) {
      if (o.pin >= 0 && outputs[j].pin == o.pin) {
        if (error) *error = "duplicate_pin_outputs_" + String(j) + "_" + String(i);
        return false;
      }
    }
  }

  return true;
}

bool GpioConfig::applyPatchJson(const String &payload, String *error) {
  JsonDocument doc;
  const DeserializationError parseResult = deserializeJson(doc, payload);
  if (parseResult) {
    if (error) *error = "invalid_json";
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst gpioObj = root["gpio"].isNull() ? root : root["gpio"].as<JsonObjectConst>();

  GpioConfig candidate = *this;

  JsonArrayConst arr = gpioObj["outputs"].as<JsonArrayConst>();
  if (!arr.isNull()) {
    const uint8_t count = (arr.size() > kMaxLedOutputs) ? kMaxLedOutputs : static_cast<uint8_t>(arr.size());
    candidate.outputCount = count;

    for (uint8_t i = 0; i < count; ++i) {
      JsonObjectConst item = arr[i].as<JsonObjectConst>();
      if (item.isNull()) continue;

      LedOutput &o = candidate.outputs[i];
      o.id = i;
      setInt8IfPresent(item, "pin", o.pin);
      setUInt16IfPresent(item, "ledCount", o.ledCount);
      setIfPresent(item, "ledType", o.ledType);
      setIfPresent(item, "colorOrder", o.colorOrder);
    }
  }

  String validationError;
  if (!candidate.validate(&validationError)) {
    if (error) *error = validationError;
    return false;
  }

  const bool changed = (candidate.toJson() != this->toJson());
  *this = candidate;
  if (error) error->clear();
  return changed;
}

// ── NetworkConfig ───────────────────────────────────────────────

NetworkConfig NetworkConfig::defaults() {
  NetworkConfig config;
  config.wifi.mode = kModeAp;
  config.wifi.apAvailability = kApAlways;
  config.wifi.ssid = "";
  config.wifi.password = "";

  config.ap.mode = kIpStatic;
  config.ap.address = "192.168.4.1";
  config.ap.gateway = "192.168.4.1";
  config.ap.subnet = "255.255.255.0";
  config.ap.primaryDns = "";
  config.ap.secondaryDns = "";

  config.sta.mode = kIpDhcp;
  config.sta.address = "192.168.1.50";
  config.sta.gateway = "192.168.1.1";
  config.sta.subnet = "255.255.255.0";
  config.sta.primaryDns = "8.8.8.8";
  config.sta.secondaryDns = "1.1.1.1";

  config.dns.hostname = "duxman-led";
  config.time.syncOnBoot = true;
  config.time.ntpServer = "europe.pool.ntp.org";
  return config;
}

// ── DebugConfig ─────────────────────────────────────────────────

DebugConfig DebugConfig::defaults() {
  DebugConfig config;
  config.enabled = false;
  config.heartbeatMs = 5000;
  return config;
}

String DebugConfig::toJson() const {
  JsonDocument doc;
  JsonObject debug = doc["debug"].to<JsonObject>();
  debug["enabled"] = enabled;
  debug["heartbeatMs"] = heartbeatMs;
  String out;
  serializeJson(doc, out);
  return out;
}

bool DebugConfig::validate(String *error) const {
  if (heartbeatMs < kHeartbeatMinMs || heartbeatMs > kHeartbeatMaxMs) {
    if (error != nullptr) {
      *error = "invalid_debug_heartbeat_ms";
    }
    return false;
  }
  return true;
}

bool DebugConfig::applyPatchJson(const String &payload, String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    if (error != nullptr) *error = "invalid_json";
    return false;
  }
  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst debugObj = root["debug"].isNull() ? root : root["debug"].as<JsonObjectConst>();

  DebugConfig candidate = *this;
  setBoolIfPresent(debugObj, "enabled", candidate.enabled);
  setUIntIfPresent(debugObj, "heartbeatMs", candidate.heartbeatMs);

  String validationError;
  if (!candidate.validate(&validationError)) {
    if (error != nullptr) *error = validationError;
    return false;
  }

  const bool changed = (candidate.toJson() != this->toJson());
  *this = candidate;
  if (error != nullptr) error->clear();
  return changed;
}

String NetworkConfig::toJson() const {
  JsonDocument doc;

  JsonObject network = doc["network"].to<JsonObject>();
  JsonObject wifiObj = network["wifi"].to<JsonObject>();
  wifiObj["mode"] = this->wifi.mode;
  wifiObj["apAvailability"] = this->wifi.apAvailability;

  JsonObject connection = wifiObj["connection"].to<JsonObject>();
  connection["ssid"] = this->wifi.ssid;
  connection["password"] = this->wifi.password;

  JsonObject ip = network["ip"].to<JsonObject>();
  JsonObject ap = ip["ap"].to<JsonObject>();
  ap["mode"] = this->ap.mode;
  ap["address"] = this->ap.address;
  ap["gateway"] = this->ap.gateway;
  ap["subnet"] = this->ap.subnet;

  JsonObject sta = ip["sta"].to<JsonObject>();
  sta["mode"] = this->sta.mode;
  sta["address"] = this->sta.address;
  sta["gateway"] = this->sta.gateway;
  sta["subnet"] = this->sta.subnet;
  sta["primaryDns"] = this->sta.primaryDns;
  sta["secondaryDns"] = this->sta.secondaryDns;

  JsonObject dns = network["dns"].to<JsonObject>();
  dns["hostname"] = this->dns.hostname;

  JsonObject time = network["time"].to<JsonObject>();
  time["syncOnBoot"] = this->time.syncOnBoot;
  time["ntpServer"] = this->time.ntpServer;

  String out;
  serializeJson(doc, out);
  return out;
}

bool NetworkConfig::validate(String *error) const {
  if (!isOneOf(wifi.mode, kModeAp, kModeSta, kModeApSta)) {
    if (error != nullptr) {
      *error = "invalid_wifi_mode";
    }
    return false;
  }

  if (!isOneOf(wifi.apAvailability, kApAlways, kApUntilStaConnected)) {
    if (error != nullptr) {
      *error = "invalid_ap_availability";
    }
    return false;
  }

  if (!isOneOf(ap.mode, kIpDhcp, kIpStatic)) {
    if (error != nullptr) {
      *error = "invalid_ap_ip_mode";
    }
    return false;
  }

  if (!isOneOf(sta.mode, kIpDhcp, kIpStatic)) {
    if (error != nullptr) {
      *error = "invalid_sta_ip_mode";
    }
    return false;
  }

  if (ap.mode == kIpStatic) {
    if (!isValidIpv4(ap.address) || !isValidIpv4(ap.gateway) || !isValidIpv4(ap.subnet)) {
      if (error != nullptr) {
        *error = "invalid_ap_static_ip";
      }
      return false;
    }
  }

  if (sta.mode == kIpStatic) {
    if (!isValidIpv4(sta.address) || !isValidIpv4(sta.gateway) || !isValidIpv4(sta.subnet)) {
      if (error != nullptr) {
        *error = "invalid_sta_static_ip";
      }
      return false;
    }
  }

  if (!sta.primaryDns.isEmpty() && !isValidIpv4(sta.primaryDns)) {
    if (error != nullptr) {
      *error = "invalid_sta_primary_dns";
    }
    return false;
  }

  if (!sta.secondaryDns.isEmpty() && !isValidIpv4(sta.secondaryDns)) {
    if (error != nullptr) {
      *error = "invalid_sta_secondary_dns";
    }
    return false;
  }

  if (!isValidHostname(dns.hostname)) {
    if (error != nullptr) {
      *error = "invalid_dns_hostname";
    }
    return false;
  }

  if (time.ntpServer.isEmpty() || time.ntpServer.length() > 128) {
    if (error != nullptr) {
      *error = "invalid_ntp_server";
    }
    return false;
  }

  return true;
}

bool NetworkConfig::applyPatchJson(const String &payload, String *error) {
  JsonDocument doc;
  const DeserializationError parseResult = deserializeJson(doc, payload);
  if (parseResult) {
    if (error != nullptr) {
      *error = "invalid_json";
    }
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst network = root["network"].isNull() ? root : root["network"].as<JsonObjectConst>();

  NetworkConfig candidate = *this;

  JsonObjectConst wifiObj = network["wifi"].as<JsonObjectConst>();
  if (!wifiObj.isNull()) {
    setIfPresent(wifiObj, "mode", candidate.wifi.mode);
    setIfPresent(wifiObj, "apAvailability", candidate.wifi.apAvailability);

    JsonObjectConst connectionObj = wifiObj["connection"].as<JsonObjectConst>();
    if (!connectionObj.isNull()) {
      setIfPresent(connectionObj, "ssid", candidate.wifi.ssid);
      setIfPresent(connectionObj, "password", candidate.wifi.password);
    }
  }

  JsonObjectConst ipObj = network["ip"].as<JsonObjectConst>();
  if (!ipObj.isNull()) {
    JsonObjectConst apObj = ipObj["ap"].as<JsonObjectConst>();
    if (!apObj.isNull()) {
      patchIpConfig(apObj, candidate.ap);
    }

    JsonObjectConst staObj = ipObj["sta"].as<JsonObjectConst>();
    if (!staObj.isNull()) {
      patchIpConfig(staObj, candidate.sta);
    }
  }

  JsonObjectConst dnsObj = network["dns"].as<JsonObjectConst>();
  if (!dnsObj.isNull()) {
    setIfPresent(dnsObj, "hostname", candidate.dns.hostname);
  }

  JsonObjectConst timeObj = network["time"].as<JsonObjectConst>();
  if (!timeObj.isNull()) {
    setBoolIfPresent(timeObj, "syncOnBoot", candidate.time.syncOnBoot);
    setIfPresent(timeObj, "ntpServer", candidate.time.ntpServer);
  }

  String validationError;
  if (!candidate.validate(&validationError)) {
    if (error != nullptr) {
      *error = validationError;
    }
    return false;
  }

  const bool changed = (candidate.toJson() != this->toJson());
  *this = candidate;
  if (error != nullptr) {
    error->clear();
  }
  return changed;
}