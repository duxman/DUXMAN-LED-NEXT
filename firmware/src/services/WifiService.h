#pragma once

#include <Arduino.h>

#include "core/Config.h"

class WifiService {
public:
  WifiService(NetworkConfig &networkConfig, DebugConfig &debugConfig);

  bool begin();
  bool applyConfig();
  void handle();

private:
  NetworkConfig &networkConfig_;
  DebugConfig &debugConfig_;
  bool apEnabled_ = false;
  bool staEnabled_ = false;
  bool staConnected_ = false;
  bool dnsApplied_ = false;
  bool ntpSynced_ = false;

  bool configureSta();
  bool configureAp();
  bool applyStaDnsOverrides();
  bool syncTimeFromNtp();
  void updateApAvailabilityPolicy();
  String buildApSsid() const;
};