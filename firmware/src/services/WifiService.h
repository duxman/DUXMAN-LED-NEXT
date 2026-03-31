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

  bool configureSta();
  bool configureAp();
  void updateApAvailabilityPolicy();
  String buildApSsid() const;
};