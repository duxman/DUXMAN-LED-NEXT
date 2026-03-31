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

struct DebugConfig {
  bool enabled = false;
  uint32_t heartbeatMs = 5000;
};

struct NetworkConfig {
  NetworkWifiConfig wifi;
  NetworkIpConfig ap;
  NetworkIpConfig sta;
  NetworkDnsConfig dns;
  DebugConfig debug;

  static NetworkConfig defaults();
  String toJson() const;
  bool applyPatchJson(const String &payload, String *error = nullptr);
  bool validate(String *error = nullptr) const;
};