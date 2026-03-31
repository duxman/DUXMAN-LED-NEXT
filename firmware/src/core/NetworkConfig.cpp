#include "core/NetworkConfig.h"

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
} // namespace

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