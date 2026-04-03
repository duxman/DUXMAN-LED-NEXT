#pragma once

#include <Arduino.h>

#include "core/Config.h"

class WifiService {
public:
  explicit WifiService(NetworkConfig &networkConfig);

  bool begin();
  bool applyConfig();
  void handle();

private:
  NetworkConfig &networkConfig_;
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