/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/WifiService.h
 * Last commit: 2c35a63 - 2026-04-28
 */

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