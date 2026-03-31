#include "services/WifiService.h"

#include <WiFi.h>

#include <lwip/ip4_addr.h>

namespace {
IPAddress parseIpOrDefault(const String &raw, const IPAddress &fallback) {
  IPAddress ip;
  if (!ip.fromString(raw)) {
    return fallback;
  }
  return ip;
}

const char *wifiStatusToString(const wl_status_t status) {
  switch (status) {
  case WL_NO_SHIELD:
    return "NO_SHIELD";
  case WL_IDLE_STATUS:
    return "IDLE";
  case WL_NO_SSID_AVAIL:
    return "NO_SSID";
  case WL_SCAN_COMPLETED:
    return "SCAN_COMPLETED";
  case WL_CONNECTED:
    return "CONNECTED";
  case WL_CONNECT_FAILED:
    return "CONNECT_FAILED";
  case WL_CONNECTION_LOST:
    return "CONNECTION_LOST";
  case WL_DISCONNECTED:
    return "DISCONNECTED";
  default:
    return "UNKNOWN";
  }
}

String normalizedWifiMode(String mode) {
  mode.trim();
  mode.toLowerCase();
  if (mode == "ap-sta") {
    return "ap_sta";
  }
  return mode;
}

void printIpDetails(const char *label, const IPAddress &ip, const IPAddress &gateway,
                    const IPAddress &subnet, const IPAddress &dns1, const IPAddress &dns2) {
  Serial.print("[wifi] ");
  Serial.print(label);
  Serial.print(" ip=");
  Serial.print(ip);
  Serial.print(" gw=");
  Serial.print(gateway);
  Serial.print(" mask=");
  Serial.print(subnet);
  Serial.print(" dns1=");
  Serial.print(dns1);
  Serial.print(" dns2=");
  Serial.println(dns2);
}
} // namespace

WifiService::WifiService(NetworkConfig &networkConfig) : networkConfig_(networkConfig) {}

bool WifiService::begin() {
  const bool ok = applyConfig();

  Serial.println("[wifi] startup summary");
  Serial.print("[wifi] mode=");
  Serial.print(networkConfig_.wifi.mode);
  Serial.print(" apAvailability=");
  Serial.println(networkConfig_.wifi.apAvailability);
  Serial.print("[wifi] hostname=");
  Serial.println(networkConfig_.dns.hostname);

  if (networkConfig_.wifi.mode == "sta" || networkConfig_.wifi.mode == "ap_sta") {
    Serial.print("[wifi] target STA ssid=");
    Serial.println(networkConfig_.wifi.ssid);
    Serial.print("[wifi] target STA ipMode=");
    Serial.println(networkConfig_.sta.mode);
    if (networkConfig_.sta.mode == "static") {
      Serial.print("[wifi] target STA static ip=");
      Serial.print(networkConfig_.sta.address);
      Serial.print(" gw=");
      Serial.print(networkConfig_.sta.gateway);
      Serial.print(" mask=");
      Serial.print(networkConfig_.sta.subnet);
      Serial.print(" dns1=");
      Serial.print(networkConfig_.sta.primaryDns);
      Serial.print(" dns2=");
      Serial.println(networkConfig_.sta.secondaryDns);
    }
  }

  if (networkConfig_.wifi.mode == "ap" || networkConfig_.wifi.mode == "ap_sta") {
    Serial.print("[wifi] AP ssid=");
    Serial.println(buildApSsid());
    Serial.print("[wifi] AP ipMode=");
    Serial.println(networkConfig_.ap.mode);
    if (networkConfig_.ap.mode == "static") {
      Serial.print("[wifi] AP static ip=");
      Serial.print(networkConfig_.ap.address);
      Serial.print(" gw=");
      Serial.print(networkConfig_.ap.gateway);
      Serial.print(" mask=");
      Serial.println(networkConfig_.ap.subnet);
    }
  }

  return ok;
}

bool WifiService::applyConfig() {
  networkConfig_.wifi.mode = normalizedWifiMode(networkConfig_.wifi.mode);
  const String mode = networkConfig_.wifi.mode;

  if (networkConfig_.debug.enabled) {
    Serial.print("[wifi][debug] applyConfig mode=");
    Serial.println(mode);
  }

  if (!networkConfig_.dns.hostname.isEmpty()) {
    WiFi.setHostname(networkConfig_.dns.hostname.c_str());
    if (networkConfig_.debug.enabled) {
      Serial.print("[wifi][debug] hostname=");
      Serial.println(networkConfig_.dns.hostname);
    }
  }

  if (mode == "ap") {
    WiFi.mode(WIFI_MODE_AP);
    staEnabled_ = false;
    apEnabled_ = configureAp();
    return apEnabled_;
  }

  if (mode == "sta") {
    WiFi.mode(WIFI_MODE_STA);
    apEnabled_ = false;
    staEnabled_ = configureSta();
    return staEnabled_;
  }

  if (mode == "ap_sta") {
    WiFi.mode(WIFI_MODE_APSTA);
    staEnabled_ = configureSta();
    apEnabled_ = configureAp();
    updateApAvailabilityPolicy();
    return staEnabled_ || apEnabled_;
  }

  Serial.println("[wifi] invalid mode; fallback to AP");
  WiFi.mode(WIFI_MODE_AP);
  staEnabled_ = false;
  apEnabled_ = configureAp();
  return apEnabled_;
}

void WifiService::handle() {
  if (staEnabled_) {
    static wl_status_t lastStatus = WL_IDLE_STATUS;
    static bool initialized = false;

    const wl_status_t currentStatus = WiFi.status();
    if (!initialized || currentStatus != lastStatus) {
      initialized = true;
      lastStatus = currentStatus;
      Serial.print("[wifi] STA status=");
      Serial.println(wifiStatusToString(currentStatus));

      if (currentStatus == WL_CONNECTED) {
        Serial.print("[wifi] STA connected ssid=");
        Serial.println(WiFi.SSID());
        printIpDetails("STA", WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), WiFi.dnsIP(0),
                       WiFi.dnsIP(1));
      }

      if (networkConfig_.debug.enabled && currentStatus == WL_CONNECTED) {
        Serial.print("[wifi][debug] STA IP=");
        Serial.println(WiFi.localIP());
      }
    }
  }

  if (networkConfig_.wifi.mode != "ap_sta") {
    return;
  }
  updateApAvailabilityPolicy();
}

bool WifiService::configureSta() {
  const String ssid = networkConfig_.wifi.ssid;
  const String password = networkConfig_.wifi.password;
  if (ssid.isEmpty()) {
    Serial.println("[wifi] STA SSID empty; STA not started");
    return false;
  }

  if (networkConfig_.sta.mode == "static") {
    const IPAddress ip = parseIpOrDefault(networkConfig_.sta.address, IPAddress(192, 168, 1, 50));
    const IPAddress gateway = parseIpOrDefault(networkConfig_.sta.gateway, IPAddress(192, 168, 1, 1));
    const IPAddress subnet = parseIpOrDefault(networkConfig_.sta.subnet, IPAddress(255, 255, 255, 0));
    const IPAddress dns1 = parseIpOrDefault(networkConfig_.sta.primaryDns, IPAddress(8, 8, 8, 8));
    const IPAddress dns2 = parseIpOrDefault(networkConfig_.sta.secondaryDns, IPAddress(1, 1, 1, 1));
    if (!WiFi.config(ip, gateway, subnet, dns1, dns2)) {
      Serial.println("[wifi] STA static IP config failed");
      return false;
    }

    if (networkConfig_.debug.enabled) {
      Serial.print("[wifi][debug] STA static ip=");
      Serial.print(ip);
      Serial.print(" gw=");
      Serial.print(gateway);
      Serial.print(" mask=");
      Serial.print(subnet);
      Serial.print(" dns1=");
      Serial.print(dns1);
      Serial.print(" dns2=");
      Serial.println(dns2);
    }
  }

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("[wifi] STA connecting to ");
  Serial.println(ssid);
  if (networkConfig_.debug.enabled) {
    Serial.print("[wifi][debug] STA password length=");
    Serial.println(password.length());
  }
  return true;
}

bool WifiService::configureAp() {
  if (networkConfig_.ap.mode == "static") {
    const IPAddress ip = parseIpOrDefault(networkConfig_.ap.address, IPAddress(192, 168, 4, 1));
    const IPAddress gateway = parseIpOrDefault(networkConfig_.ap.gateway, IPAddress(192, 168, 4, 1));
    const IPAddress subnet = parseIpOrDefault(networkConfig_.ap.subnet, IPAddress(255, 255, 255, 0));

    if (!WiFi.softAPConfig(ip, gateway, subnet)) {
      Serial.println("[wifi] AP static IP config failed");
      return false;
    }

    if (networkConfig_.debug.enabled) {
      Serial.print("[wifi][debug] AP static ip=");
      Serial.print(ip);
      Serial.print(" gw=");
      Serial.print(gateway);
      Serial.print(" mask=");
      Serial.println(subnet);
    }
  }

  const String apSsid = buildApSsid();
  const bool ok = WiFi.softAP(apSsid.c_str());
  if (ok) {
    Serial.print("[wifi] AP started: ");
    Serial.println(apSsid);
    if (networkConfig_.debug.enabled) {
      Serial.print("[wifi][debug] AP IP=");
      Serial.println(WiFi.softAPIP());
    }
  } else {
    Serial.println("[wifi] AP start failed");
  }
  return ok;
}

void WifiService::updateApAvailabilityPolicy() {
  if (networkConfig_.wifi.apAvailability == "always") {
    if (!apEnabled_) {
      WiFi.mode(WIFI_MODE_APSTA);
      apEnabled_ = configureAp();
      if (staEnabled_) {
        WiFi.begin(networkConfig_.wifi.ssid.c_str(), networkConfig_.wifi.password.c_str());
      }
    }
    return;
  }

  if (networkConfig_.wifi.apAvailability != "untilStaConnected") {
    return;
  }

  const wl_status_t staStatus = WiFi.status();
  const bool staConnected = (staStatus == WL_CONNECTED);

  if (staConnected && apEnabled_) {
    Serial.println("[wifi] STA connected, disabling AP per policy");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_MODE_STA);
    apEnabled_ = false;
    staEnabled_ = true;
    return;
  }

  if (!staConnected && !apEnabled_) {
    Serial.println("[wifi] STA disconnected, enabling AP per policy");
    WiFi.mode(WIFI_MODE_APSTA);
    apEnabled_ = configureAp();
    if (staEnabled_) {
      WiFi.begin(networkConfig_.wifi.ssid.c_str(), networkConfig_.wifi.password.c_str());
    }
  }
}

String WifiService::buildApSsid() const {
  String base = networkConfig_.dns.hostname;
  if (base.isEmpty()) {
    base = "duxman-led";
  }
  return base + "-ap";
}