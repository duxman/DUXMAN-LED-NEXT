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
         type == "apa102" || type == "tm1814";
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
} // namespace

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
  config.debug.enabled = false;
  config.debug.heartbeatMs = 5000;
  return config;
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

  JsonObject debug = doc["debug"].to<JsonObject>();
  debug["enabled"] = this->debug.enabled;
  debug["heartbeatMs"] = this->debug.heartbeatMs;

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
  }

  if (!isValidHostname(dns.hostname)) {
    if (error != nullptr) {
      *error = "invalid_dns_hostname";
    }
    return false;
  }

  if (debug.heartbeatMs < kHeartbeatMinMs || debug.heartbeatMs > kHeartbeatMaxMs) {
    if (error != nullptr) {
      *error = "invalid_debug_heartbeat_ms";
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

  JsonObjectConst debugObj = root["debug"].as<JsonObjectConst>();
  if (!debugObj.isNull()) {
    setBoolIfPresent(debugObj, "enabled", candidate.debug.enabled);
    setUIntIfPresent(debugObj, "heartbeatMs", candidate.debug.heartbeatMs);
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