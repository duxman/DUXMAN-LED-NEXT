/*
 * duxman-led next
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/SyncService.h
 */

#pragma once

#include <Arduino.h>

#include "core/Config.h"

class SyncService {
public:
  struct Stats {
    uint32_t packetsReceived = 0;
    uint32_t packetsDropped = 0;
    uint32_t lastPacketAtMs = 0;
    uint32_t lastTickAtMs = 0;
    bool sourceAlive = false;
    String sourceIp;
  };

  explicit SyncService(SyncConfig &syncConfig);

  void begin();
  void tick(uint32_t nowMs);

  const SyncConfig &config() const;

  bool setMode(const String &mode, String *error = nullptr);
  bool applyConfigPatch(const String &payload, bool *changed, String *error = nullptr);

  void touchPacket(const String &sourceIp = String(), bool dropped = false, uint32_t nowMs = 0);

  Stats statsSnapshot() const;
  String buildStateJson() const;

private:
  SyncConfig &syncConfig_;
  Stats stats_;
};
