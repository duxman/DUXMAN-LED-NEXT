/*
 * duxman-led next
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/SyncService.cpp
 */

#include "services/SyncService.h"

#include <ArduinoJson.h>

SyncService::SyncService(SyncConfig &syncConfig) : syncConfig_(syncConfig) {}

void SyncService::begin() {
  stats_ = Stats{};
  stats_.lastTickAtMs = millis();
}

void SyncService::tick(uint32_t nowMs) {
  stats_.lastTickAtMs = nowMs;
  if (stats_.lastPacketAtMs == 0) {
    stats_.sourceAlive = false;
    return;
  }

  const uint32_t elapsedMs = nowMs - stats_.lastPacketAtMs;
  stats_.sourceAlive = elapsedMs <= syncConfig_.sourceTimeoutMs;
}

const SyncConfig &SyncService::config() const {
  return syncConfig_;
}

bool SyncService::setMode(const String &mode, String *error) {
  String payload = "{\"sync\":{\"mode\":\"";
  payload += mode;
  payload += "\"}}";

  bool changed = false;
  if (!applyConfigPatch(payload, &changed, error)) {
    return false;
  }
  return changed;
}

bool SyncService::applyConfigPatch(const String &payload, bool *changed, String *error) {
  const bool patchChanged = syncConfig_.applyPatchJson(payload, error);
  if (changed != nullptr) {
    *changed = patchChanged;
  }
  return error == nullptr || error->isEmpty();
}

void SyncService::touchPacket(const String &sourceIp, bool dropped, uint32_t nowMs) {
  const uint32_t effectiveNow = nowMs == 0 ? millis() : nowMs;
  stats_.lastPacketAtMs = effectiveNow;
  if (dropped) {
    stats_.packetsDropped++;
  } else {
    stats_.packetsReceived++;
  }
  if (!sourceIp.isEmpty()) {
    stats_.sourceIp = sourceIp;
  }
  stats_.sourceAlive = true;
}

SyncService::Stats SyncService::statsSnapshot() const {
  return stats_;
}

String SyncService::buildStateJson() const {
  JsonDocument doc;
  JsonObject sync = doc["sync"].to<JsonObject>();
  sync["mode"] = syncConfig_.mode;
  sync["role"] = syncConfig_.role;
  sync["inputProtocol"] = syncConfig_.inputProtocol;
  sync["ddpPort"] = syncConfig_.ddpPort;
  sync["e131UniverseStart"] = syncConfig_.e131UniverseStart;
  sync["e131UniverseCount"] = syncConfig_.e131UniverseCount;
  sync["udpSyncPort"] = syncConfig_.udpSyncPort;
  sync["groupMask"] = syncConfig_.groupMask;
  sync["sourceTimeoutMs"] = syncConfig_.sourceTimeoutMs;
  sync["clockSmoothing"] = syncConfig_.clockSmoothing;

  JsonObject stats = sync["stats"].to<JsonObject>();
  stats["packetsReceived"] = stats_.packetsReceived;
  stats["packetsDropped"] = stats_.packetsDropped;
  stats["lastPacketAtMs"] = stats_.lastPacketAtMs;
  stats["lastTickAtMs"] = stats_.lastTickAtMs;
  stats["sourceAlive"] = stats_.sourceAlive;
  stats["sourceIp"] = stats_.sourceIp;

  String out;
  serializeJson(doc, out);
  return out;
}
