/*
 * duxman-led next
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/SyncService.cpp
 */

#include "services/SyncService.h"

#include <ArduinoJson.h>
#include <cstring>
#include <WiFi.h>

#include "effects/EffectEngine.h"

namespace {
String normalizeSyncRole(const String &value) {
  if (value == "master" || value == "server") {
    return "server";
  }
  return "client";
}

bool isDigitalOutput(const LedOutput &output) {
  return output.ledType == "digital";
}

uint16_t outputLogicalPixels(const LedOutput &output) {
  if (output.pin < 0 || output.ledCount == 0) {
    return 0;
  }
  return isDigitalOutput(output) ? 1 : output.ledCount;
}

uint16_t clampUniverseCount(uint8_t configuredCount) {
  if (configuredCount == 0) {
    return 1;
  }
  return configuredCount > 16 ? 16 : configuredCount;
}
}

SyncService::SyncService(SyncConfig &syncConfig, GpioConfig &gpioConfig)
    : syncConfig_(syncConfig), gpioConfig_(gpioConfig) {}

void SyncService::begin() {
  stats_ = Stats{};
  stats_.lastTickAtMs = millis();
  if (frameMutex_ == nullptr) {
    frameMutex_ = xSemaphoreCreateMutex();
  }
  resetE131Assembly();
  ensureFrameBuffers();
}

void SyncService::tick(uint32_t nowMs, CoreState &coreState) {
  stats_.lastTickAtMs = nowMs;
  ensureFrameBuffers();
  ensureDdpListener();
  ensureE131Listener();
  ensureClusterListener();
  pollDdp(nowMs);
  pollE131(nowMs);
  pollCluster(nowMs, coreState);
  publishClusterState(nowMs, coreState.snapshot());
  maybeCommitE131Frame(nowMs);

  if (!shouldReceiveCluster()) {
    EffectEngine::resetSynchronizedClock();
    stats_.clockOffsetMs = 0;
    stats_.lastSyncEpochMs = 0;
  }

  if (stats_.lastPacketAtMs == 0) {
    stats_.sourceAlive = false;
    timeoutLatched_ = false;
    updateDerivedHealth(nowMs);
    return;
  }

  const uint32_t elapsedMs = nowMs - stats_.lastPacketAtMs;
  stats_.sourceAlive = elapsedMs <= syncConfig_.sourceTimeoutMs;
  if (!stats_.sourceAlive && !timeoutLatched_) {
    stats_.timeoutEvents++;
    timeoutLatched_ = true;
  }
  if (stats_.sourceAlive) {
    timeoutLatched_ = false;
  }
  if (!stats_.sourceAlive && shouldReceiveCluster()) {
    EffectEngine::resetSynchronizedClock();
    stats_.clockOffsetMs = 0;
  }

  updateDerivedHealth(nowMs);
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
  if (!dropped && stats_.lastPacketAtMs != 0 && effectiveNow >= stats_.lastPacketAtMs) {
    stats_.lastInterPacketMs = effectiveNow - stats_.lastPacketAtMs;
    if (stats_.lastInterPacketMs > 0) {
      const uint32_t fpsTenths = 10000UL / stats_.lastInterPacketMs;
      if (stats_.inputFpsTenths == 0) {
        stats_.inputFpsTenths = static_cast<uint16_t>(fpsTenths);
      } else {
        stats_.inputFpsTenths = static_cast<uint16_t>((stats_.inputFpsTenths * 7UL + fpsTenths) / 8UL);
      }
    }
  }
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
  timeoutLatched_ = false;
}

SyncService::Stats SyncService::statsSnapshot() const {
  return stats_;
}

bool SyncService::isEnabled() const {
  return syncConfig_.mode != "off";
}

bool SyncService::isConnected() const {
  return isEnabled() && normalizedRole() == "client" && stats_.sourceAlive;
}

String SyncService::normalizedRole() const {
  return normalizeSyncRole(syncConfig_.role);
}

bool SyncService::renderExternalFrame(LedDriver &ledDriver, const CoreState &stateSnapshot) {
  if (!shouldRenderExternal()) {
    return false;
  }

  if (frameMutex_ == nullptr || xSemaphoreTake(frameMutex_, pdMS_TO_TICKS(2)) != pdTRUE) {
    return false;
  }

  if (frontBuffer_ == nullptr || frontBufferLength_ == 0) {
    xSemaphoreGive(frameMutex_);
    return false;
  }

  if (!stateSnapshot.power) {
    ledDriver.clear();
    xSemaphoreGive(frameMutex_);
    ledDriver.show();
    return true;
  }

  ledDriver.clear();

  size_t byteOffset = 0;
  for (uint8_t outputIndex = 0; outputIndex < ledDriver.outputCount(); ++outputIndex) {
    const LedDriverOutputConfig &output = ledDriver.outputConfig(outputIndex);
    if (!output.enabled) {
      continue;
    }

    const uint16_t logicalPixels = output.isDigital ? 1 : output.ledCount;
    if (ledDriver.supportsPerPixelColor(outputIndex)) {
      for (uint16_t pixelIndex = 0; pixelIndex < logicalPixels; ++pixelIndex) {
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
        if (byteOffset + 2 < frontBufferLength_) {
          red = frontBuffer_[byteOffset];
          green = frontBuffer_[byteOffset + 1];
          blue = frontBuffer_[byteOffset + 2];
        }
        ledDriver.setPixelColor(outputIndex, pixelIndex, scaleRgb(red, green, blue, stateSnapshot.brightness));
        byteOffset += 3;
      }
      continue;
    }

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    if (byteOffset + 2 < frontBufferLength_) {
      red = frontBuffer_[byteOffset];
      green = frontBuffer_[byteOffset + 1];
      blue = frontBuffer_[byteOffset + 2];
    }
    ledDriver.setOutputColor(outputIndex, scaleRgb(red, green, blue, stateSnapshot.brightness));
    byteOffset += 3;
  }

  xSemaphoreGive(frameMutex_);
  ledDriver.show();
  return true;
}

String SyncService::buildStateJson() const {
  JsonDocument doc;
  JsonObject sync = doc["sync"].to<JsonObject>();
  sync["mode"] = syncConfig_.mode;
  sync["enabled"] = isEnabled();
  sync["connected"] = isConnected();
  sync["role"] = normalizedRole();
  sync["inputProtocol"] = syncConfig_.inputProtocol;
  sync["ddpPort"] = syncConfig_.ddpPort;
  sync["e131UniverseStart"] = syncConfig_.e131UniverseStart;
  sync["e131UniverseCount"] = syncConfig_.e131UniverseCount;
  sync["udpSyncPort"] = syncConfig_.udpSyncPort;
  sync["groupMask"] = syncConfig_.groupMask;
  sync["sourceTimeoutMs"] = syncConfig_.sourceTimeoutMs;
  sync["clockSmoothing"] = syncConfig_.clockSmoothing;
  sync["activeProtocol"] = stats_.activeProtocol;
  sync["degraded"] = stats_.degraded;

  JsonObject stats = sync["stats"].to<JsonObject>();
  stats["packetsReceived"] = stats_.packetsReceived;
  stats["packetsDropped"] = stats_.packetsDropped;
  stats["timeoutEvents"] = stats_.timeoutEvents;
  stats["pollSaturationEvents"] = stats_.pollSaturationEvents;
  stats["lastSyncEpochMs"] = stats_.lastSyncEpochMs;
  stats["lastSequence"] = stats_.lastSequence;
  stats["framesApplied"] = stats_.framesApplied;
  stats["lastPacketAtMs"] = stats_.lastPacketAtMs;
  stats["lastFrameAtMs"] = stats_.lastFrameAtMs;
  stats["lastTickAtMs"] = stats_.lastTickAtMs;
  stats["lastInterPacketMs"] = stats_.lastInterPacketMs;
  stats["inputFps"] = static_cast<float>(stats_.inputFpsTenths) / 10.0f;
  stats["frameBytes"] = stats_.frameBytes;
  stats["syncStateSent"] = stats_.syncStateSent;
  stats["syncStateApplied"] = stats_.syncStateApplied;
  stats["clockOffsetMs"] = stats_.clockOffsetMs;
  stats["degraded"] = stats_.degraded;
  stats["activeProtocol"] = stats_.activeProtocol;
  stats["sourceAlive"] = stats_.sourceAlive;
  stats["sourceIp"] = stats_.sourceIp;

  String out;
  serializeJson(doc, out);
  return out;
}

String SyncService::buildConnectionJson() const {
  JsonDocument doc;
  JsonObject sync = doc["sync"].to<JsonObject>();
  sync["enabled"] = isEnabled();
  sync["connected"] = isConnected();
  sync["role"] = normalizedRole();
  sync["mode"] = syncConfig_.mode;
  sync["sourceAlive"] = stats_.sourceAlive;
  sync["sourceIp"] = stats_.sourceIp;
  sync["activeProtocol"] = stats_.activeProtocol;
  sync["degraded"] = stats_.degraded;
  sync["lastSyncEpochMs"] = stats_.lastSyncEpochMs;
  sync["packetsReceived"] = stats_.packetsReceived;
  sync["packetsDropped"] = stats_.packetsDropped;
  sync["timeoutEvents"] = stats_.timeoutEvents;
  sync["pollSaturationEvents"] = stats_.pollSaturationEvents;
  sync["lastSequence"] = stats_.lastSequence;
  sync["framesApplied"] = stats_.framesApplied;
  sync["syncStateSent"] = stats_.syncStateSent;
  sync["syncStateApplied"] = stats_.syncStateApplied;
  sync["clockOffsetMs"] = stats_.clockOffsetMs;
  sync["inputFps"] = static_cast<float>(stats_.inputFpsTenths) / 10.0f;
  sync["lastPacketAtMs"] = stats_.lastPacketAtMs;
  sync["lastFrameAtMs"] = stats_.lastFrameAtMs;

  String out;
  serializeJson(doc, out);
  return out;
}

void SyncService::ensureFrameBuffers() {
  const size_t requiredBytes = totalFrameBytes();
  if (requiredBytes == frameBufferSize_) {
    return;
  }

  stopListeners();

  if (frontBuffer_ != nullptr) {
    delete[] frontBuffer_;
    frontBuffer_ = nullptr;
  }
  if (stagingBuffer_ != nullptr) {
    delete[] stagingBuffer_;
    stagingBuffer_ = nullptr;
  }

  frameBufferSize_ = requiredBytes;
  frontBufferLength_ = 0;
  stagingBufferLength_ = 0;
  stagingHasSequence_ = false;
  stagingHasData_ = false;
  resetE131Assembly();

  if (frameBufferSize_ == 0) {
    return;
  }

  frontBuffer_ = new uint8_t[frameBufferSize_];
  stagingBuffer_ = new uint8_t[frameBufferSize_];
  if (frontBuffer_ == nullptr || stagingBuffer_ == nullptr) {
    delete[] frontBuffer_;
    delete[] stagingBuffer_;
    frontBuffer_ = nullptr;
    stagingBuffer_ = nullptr;
    frameBufferSize_ = 0;
    return;
  }

  memset(frontBuffer_, 0, frameBufferSize_);
  memset(stagingBuffer_, 0, frameBufferSize_);
}

void SyncService::resetStagingFrame() {
  if (stagingBuffer_ != nullptr && frameBufferSize_ > 0) {
    memset(stagingBuffer_, 0, frameBufferSize_);
  }
  stagingBufferLength_ = 0;
  stagingHasData_ = false;
}

void SyncService::resetE131Assembly() {
  e131UniverseMask_ = 0;
  e131CycleStartedAtMs_ = 0;
  e131ExpectedMask_ = 0;

  if (!shouldListenForE131()) {
    return;
  }

  const uint16_t universeCount = clampUniverseCount(syncConfig_.e131UniverseCount);
  if (universeCount >= 32) {
    e131ExpectedMask_ = 0xFFFFFFFFUL;
    return;
  }
  e131ExpectedMask_ = (1UL << universeCount) - 1UL;
}

void SyncService::updateDerivedHealth(uint32_t nowMs) {
  const uint32_t sincePacketMs = stats_.lastPacketAtMs == 0 ? 0 : (nowMs - stats_.lastPacketAtMs);
  const bool stale = stats_.lastPacketAtMs != 0 && sincePacketMs > (syncConfig_.sourceTimeoutMs * 2UL);
  const bool packetLossObserved = stats_.packetsDropped > 0;
  const bool pollSaturated = stats_.pollSaturationEvents > 0;
  const bool largeClockOffset = abs(stats_.clockOffsetMs) > 250;
  stats_.degraded = stale || packetLossObserved || pollSaturated || largeClockOffset;
}

void SyncService::notePollSaturation() {
  stats_.pollSaturationEvents++;
}

void SyncService::stopListeners() {
  stopDdpListener();
  stopE131Listener();
  stopClusterListener();
}

void SyncService::stopDdpListener() {
  if (activeDdpPort_ != 0) {
    ddpUdp_.stop();
    activeDdpPort_ = 0;
  }
}

void SyncService::stopE131Listener() {
  if (e131Listening_) {
    e131Udp_.stop();
    e131Listening_ = false;
  }
  resetE131Assembly();
}

void SyncService::stopClusterListener() {
  if (clusterListening_) {
    clusterUdp_.stop();
    clusterListening_ = false;
  }
}

void SyncService::ensureDdpListener() {
  if (!shouldListenForDdp()) {
    stopDdpListener();
    return;
  }

  if (activeDdpPort_ == syncConfig_.ddpPort) {
    return;
  }

  stopDdpListener();
  if (ddpUdp_.begin(syncConfig_.ddpPort)) {
    activeDdpPort_ = syncConfig_.ddpPort;
  }
}

void SyncService::ensureE131Listener() {
  if (!shouldListenForE131()) {
    stopE131Listener();
    return;
  }

  if (e131Listening_) {
    return;
  }

  if (e131Udp_.begin(kE131Port)) {
    e131Listening_ = true;
    resetE131Assembly();
  }
}

void SyncService::ensureClusterListener() {
  if (!shouldReceiveCluster()) {
    stopClusterListener();
    return;
  }

  if (clusterListening_) {
    return;
  }

  if (clusterUdp_.begin(syncConfig_.udpSyncPort)) {
    clusterListening_ = true;
  }
}

void SyncService::pollDdp(uint32_t nowMs) {
  if (activeDdpPort_ == 0 || frameBufferSize_ == 0) {
    return;
  }

  uint8_t packetBuffer[kMaxDdpPacketLength];
  uint8_t packetsProcessed = 0;
  while (packetsProcessed < 8) {
    const int packetLength = ddpUdp_.parsePacket();
    if (packetLength <= 0) {
      break;
    }

    const int bytesToRead = min(packetLength, static_cast<int>(sizeof(packetBuffer)));
    const int bytesRead = ddpUdp_.read(packetBuffer, bytesToRead);
    const IPAddress remoteIp = ddpUdp_.remoteIP();

    if (bytesRead <= 0 || packetLength > static_cast<int>(sizeof(packetBuffer))) {
      touchPacket(ipToString(remoteIp), true, nowMs);
    } else {
      parseDdpPacket(packetBuffer, static_cast<size_t>(bytesRead), remoteIp, nowMs);
    }
    packetsProcessed++;
  }
  if (packetsProcessed == 8 && ddpUdp_.parsePacket() > 0) {
    notePollSaturation();
  }
}

void SyncService::pollE131(uint32_t nowMs) {
  if (!e131Listening_ || frameBufferSize_ == 0) {
    return;
  }

  uint8_t packetBuffer[kMaxE131PacketLength];
  uint8_t packetsProcessed = 0;
  while (packetsProcessed < 8) {
    const int packetLength = e131Udp_.parsePacket();
    if (packetLength <= 0) {
      break;
    }

    const int bytesToRead = min(packetLength, static_cast<int>(sizeof(packetBuffer)));
    const int bytesRead = e131Udp_.read(packetBuffer, bytesToRead);
    const IPAddress remoteIp = e131Udp_.remoteIP();

    if (bytesRead <= 0 || packetLength > static_cast<int>(sizeof(packetBuffer))) {
      touchPacket(ipToString(remoteIp), true, nowMs);
    } else {
      parseE131Packet(packetBuffer, static_cast<size_t>(bytesRead), remoteIp, nowMs);
    }
    packetsProcessed++;
  }
  if (packetsProcessed == 8 && e131Udp_.parsePacket() > 0) {
    notePollSaturation();
  }
}

void SyncService::pollCluster(uint32_t nowMs, CoreState &coreState) {
  if (!clusterListening_) {
    return;
  }

  uint8_t packetBuffer[768];
  uint8_t packetsProcessed = 0;
  while (packetsProcessed < 4) {
    const int packetLength = clusterUdp_.parsePacket();
    if (packetLength <= 0) {
      break;
    }

    const int bytesToRead = min(packetLength, static_cast<int>(sizeof(packetBuffer)));
    const int bytesRead = clusterUdp_.read(packetBuffer, bytesToRead);
    const IPAddress remoteIp = clusterUdp_.remoteIP();

    if (bytesRead <= 0 || packetLength > static_cast<int>(sizeof(packetBuffer))) {
      touchPacket(ipToString(remoteIp), true, nowMs);
    } else {
      parseClusterPacket(packetBuffer, static_cast<size_t>(bytesRead), remoteIp, nowMs, coreState);
    }
    packetsProcessed++;
  }
  if (packetsProcessed == 4 && clusterUdp_.parsePacket() > 0) {
    notePollSaturation();
  }
}

void SyncService::publishClusterState(uint32_t nowMs, const CoreState &coreState) {
  if (!shouldPublishCluster() || syncConfig_.udpSyncPort == 0) {
    return;
  }

  if (clusterLastSentAtMs_ != 0 && nowMs - clusterLastSentAtMs_ < kClusterSyncIntervalMs) {
    return;
  }

  JsonDocument doc;
  doc["v"] = 1;
  doc["sequence"] = ++clusterSequenceTx_;
  doc["syncEpochMs"] = nowMs;
  doc["phaseOffset"] = 0.0f;
  doc["groupMask"] = syncConfig_.groupMask;

  JsonObject stateObj = doc["state"].to<JsonObject>();
  stateObj["power"] = coreState.power;
  stateObj["brightness"] = coreState.brightness;
  stateObj["effectId"] = coreState.effectId;
  stateObj["sectionCount"] = coreState.sectionCount;
  stateObj["effectSpeed"] = coreState.effectSpeed;
  stateObj["effectLevel"] = coreState.effectLevel;
  stateObj["paletteId"] = coreState.paletteId;
  JsonArray colors = stateObj["primaryColors"].to<JsonArray>();
  for (uint8_t index = 0; index < 3; ++index) {
    char colorHex[10];
    snprintf(colorHex, sizeof(colorHex), "#%06lX",
             static_cast<unsigned long>(coreState.primaryColors[index] & 0xFFFFFFUL));
    colors.add(colorHex);
  }
  char backgroundColor[10];
  snprintf(backgroundColor, sizeof(backgroundColor), "#%06lX", static_cast<unsigned long>(coreState.backgroundColor & 0xFFFFFFUL));
  stateObj["backgroundColor"] = backgroundColor;

  String payload;
  serializeJson(doc, payload);

  bool sent = false;
  if (WiFi.status() == WL_CONNECTED) {
    const IPAddress target = broadcastAddressFor(WiFi.localIP(), WiFi.subnetMask());
    sent = clusterUdp_.beginPacket(target, syncConfig_.udpSyncPort) == 1;
    if (sent) {
      clusterUdp_.write(reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length());
      sent = clusterUdp_.endPacket() == 1;
    }
  }

  const IPAddress apIp = WiFi.softAPIP();
  if (!(apIp[0] == 0 && apIp[1] == 0 && apIp[2] == 0 && apIp[3] == 0)) {
    const IPAddress target = broadcastAddressFor(apIp, WiFi.softAPSubnetMask());
    bool apSent = clusterUdp_.beginPacket(target, syncConfig_.udpSyncPort) == 1;
    if (apSent) {
      clusterUdp_.write(reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length());
      apSent = clusterUdp_.endPacket() == 1;
    }
    sent = sent || apSent;
  }

  if (sent) {
    clusterLastSentAtMs_ = nowMs;
    stats_.syncStateSent++;
    stats_.lastSequence = clusterSequenceTx_;
    stats_.activeProtocol = "cluster_sync";
  }
}

void SyncService::maybeCommitE131Frame(uint32_t nowMs) {
  if (!shouldListenForE131() || !stagingHasData_ || e131CycleStartedAtMs_ == 0) {
    return;
  }

  if (e131ExpectedMask_ != 0 && (e131UniverseMask_ & e131ExpectedMask_) == e131ExpectedMask_) {
    if (commitStagingFrame(nowMs)) {
      e131UniverseMask_ = 0;
      e131CycleStartedAtMs_ = nowMs;
    }
    return;
  }

  if (nowMs - e131CycleStartedAtMs_ >= 25) {
    if (commitStagingFrame(nowMs)) {
      e131UniverseMask_ = 0;
      e131CycleStartedAtMs_ = nowMs;
    }
  }
}

bool SyncService::shouldListenForDdp() const {
  return syncConfig_.mode == "ledfx_realtime" && normalizedRole() == "client" && syncConfig_.inputProtocol == "ddp" &&
         syncConfig_.ddpPort > 0 && frameBufferSize_ > 0;
}

bool SyncService::shouldListenForE131() const {
  return syncConfig_.mode == "ledfx_realtime" && normalizedRole() == "client" && syncConfig_.inputProtocol == "e131" &&
         frameBufferSize_ > 0;
}

bool SyncService::shouldReceiveCluster() const {
  return syncConfig_.mode == "cluster_sync" && normalizedRole() == "client" && syncConfig_.udpSyncPort > 0;
}

bool SyncService::shouldPublishCluster() const {
  return syncConfig_.mode == "cluster_sync" && normalizedRole() == "server" && syncConfig_.udpSyncPort > 0;
}

bool SyncService::shouldRenderExternal() const {
  return (shouldListenForDdp() || shouldListenForE131()) && stats_.sourceAlive && frontBuffer_ != nullptr &&
         frontBufferLength_ > 0;
}

bool SyncService::parseClusterPacket(const uint8_t *packet, size_t packetLength, const IPAddress &remoteIp, uint32_t nowMs,
                                    CoreState &coreState) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, packet, packetLength);
  if (error) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  const uint8_t version = doc["v"] | 0;
  const uint32_t sequence = doc["sequence"] | 0UL;
  const uint32_t syncEpochMs = doc["syncEpochMs"] | 0UL;
  const float phaseOffset = doc["phaseOffset"] | 0.0f;
  const uint8_t groupMask = doc["groupMask"] | 0;
  JsonObjectConst stateObj = doc["state"].as<JsonObjectConst>();
  if (version != 1 || sequence == 0 || stateObj.isNull()) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  if (groupMask != 0 && (groupMask & syncConfig_.groupMask) == 0) {
    return false;
  }

  const bool sourceExpired = stats_.lastPacketAtMs == 0 || (nowMs - stats_.lastPacketAtMs) > syncConfig_.sourceTimeoutMs;
  if (!sourceExpired && sequence <= clusterLastSequenceRx_) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  String statePayload;
  serializeJson(stateObj, statePayload);
  coreState.applyPatchJson(statePayload);

  if (syncEpochMs != 0) {
    const bool smoothClock = syncConfig_.clockSmoothing != "off";
    EffectEngine::updateSynchronizedClock(nowMs, syncEpochMs, phaseOffset, smoothClock);
    stats_.lastSyncEpochMs = syncEpochMs;
    stats_.clockOffsetMs = static_cast<int32_t>(syncEpochMs) - static_cast<int32_t>(nowMs) +
                           static_cast<int32_t>(phaseOffset * 1000.0f);
  }

  clusterLastSequenceRx_ = sequence;
  stats_.lastSequence = sequence;
  stats_.syncStateApplied++;
  stats_.activeProtocol = "cluster_sync";
  touchPacket(ipToString(remoteIp), false, nowMs);
  return true;
}

bool SyncService::parseDdpPacket(const uint8_t *packet, size_t packetLength, const IPAddress &remoteIp, uint32_t nowMs) {
  if (packet == nullptr || packetLength < kDdpHeaderLength) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  const uint8_t flags = packet[0];
  if ((flags & kDdpVersionMask) != kDdpVersion1) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  if ((flags & kDdpReplyFlag) != 0 || (flags & kDdpQueryFlag) != 0 || (flags & kDdpStorageFlag) != 0) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  const size_t headerLength = (flags & kDdpTimeFlag) != 0 ? kDdpTimecodeHeaderLength : kDdpHeaderLength;
  if (packetLength < headerLength) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  const uint8_t sequence = packet[1] & 0x0F;
  const uint8_t dataType = packet[2];
  const uint32_t dataOffset = readBigEndian32(packet + 4);
  const uint16_t dataLength = readBigEndian16(packet + 8);

  if (packetLength < headerLength + dataLength || dataType != kDdpDatatypeRgb8) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  if (dataLength > 0) {
    if (stagingBuffer_ == nullptr || dataOffset >= frameBufferSize_) {
      touchPacket(ipToString(remoteIp), true, nowMs);
      return false;
    }

    if (!stagingHasSequence_ || (sequence != 0 && sequence != stagingSequence_) || dataOffset == 0) {
      resetStagingFrame();
      stagingSequence_ = sequence;
      stagingHasSequence_ = true;
    }

    const size_t availableBytes = frameBufferSize_ - dataOffset;
    const size_t copyLength = min(static_cast<size_t>(dataLength), availableBytes);
    memcpy(stagingBuffer_ + dataOffset, packet + headerLength, copyLength);
    stagingBufferLength_ = max(stagingBufferLength_, static_cast<size_t>(dataOffset) + copyLength);
    stagingHasData_ = true;

    if (copyLength != dataLength) {
      touchPacket(ipToString(remoteIp), true, nowMs);
      return false;
    }
  }

  touchPacket(ipToString(remoteIp), false, nowMs);
  stats_.activeProtocol = "ddp";
  if ((flags & kDdpPushFlag) != 0) {
    return commitStagingFrame(nowMs);
  }
  return true;
}

bool SyncService::parseE131Packet(const uint8_t *packet, size_t packetLength, const IPAddress &remoteIp, uint32_t nowMs) {
  // Offsets for E1.31 Data Packet (sACN) with DMP payload.
  static constexpr size_t kFramingVectorOffset = 40;
  static constexpr size_t kUniverseOffset = 113;
  static constexpr size_t kPropertyValueCountOffset = 123;
  static constexpr size_t kPropertyValuesOffset = 125;
  static constexpr uint32_t kDataPacketVector = 0x00000002;

  if (packet == nullptr || packetLength < (kPropertyValuesOffset + 1)) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  const uint32_t framingVector = readBigEndian32(packet + kFramingVectorOffset);
  if (framingVector != kDataPacketVector) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  const uint16_t universe = readBigEndian16(packet + kUniverseOffset);
  const uint16_t universeStart = syncConfig_.e131UniverseStart;
  const uint16_t universeCount = clampUniverseCount(syncConfig_.e131UniverseCount);
  if (universe < universeStart || universe >= static_cast<uint16_t>(universeStart + universeCount)) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  const uint16_t propertyValueCount = readBigEndian16(packet + kPropertyValueCountOffset);
  if (propertyValueCount == 0 || packetLength < (kPropertyValuesOffset + propertyValueCount)) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  const uint8_t startCode = packet[kPropertyValuesOffset];
  if (startCode != 0) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  if (stagingBuffer_ == nullptr) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  const size_t dmxDataLength = propertyValueCount - 1;
  const size_t universeIndex = static_cast<size_t>(universe - universeStart);
  const size_t frameOffset = universeIndex * 510;
  if (frameOffset >= frameBufferSize_) {
    touchPacket(ipToString(remoteIp), true, nowMs);
    return false;
  }

  if (e131CycleStartedAtMs_ == 0 || universeIndex == 0) {
    resetStagingFrame();
    e131UniverseMask_ = 0;
    e131CycleStartedAtMs_ = nowMs;
  }

  const size_t availableBytes = frameBufferSize_ - frameOffset;
  const size_t copyLength = min(dmxDataLength, min(static_cast<size_t>(510), availableBytes));
  memcpy(stagingBuffer_ + frameOffset, packet + kPropertyValuesOffset + 1, copyLength);
  stagingBufferLength_ = max(stagingBufferLength_, frameOffset + copyLength);
  stagingHasData_ = true;

  e131UniverseMask_ |= (1UL << universeIndex);
  stats_.activeProtocol = "e131";
  touchPacket(ipToString(remoteIp), false, nowMs);

  if (universeCount == 1) {
    if (commitStagingFrame(nowMs)) {
      e131UniverseMask_ = 0;
      e131CycleStartedAtMs_ = nowMs;
      return true;
    }
  }

  return true;
}

bool SyncService::commitStagingFrame(uint32_t nowMs) {
  if (!stagingHasData_ || frameMutex_ == nullptr || xSemaphoreTake(frameMutex_, pdMS_TO_TICKS(2)) != pdTRUE) {
    return false;
  }

  uint8_t *previousFront = frontBuffer_;
  frontBuffer_ = stagingBuffer_;
  stagingBuffer_ = previousFront;
  frontBufferLength_ = stagingBufferLength_;

  stagingBufferLength_ = 0;
  stagingHasData_ = false;

  stats_.framesApplied++;
  stats_.lastFrameAtMs = nowMs;
  stats_.frameBytes = frontBufferLength_;

  xSemaphoreGive(frameMutex_);
  return true;
}

size_t SyncService::totalFrameBytes() const {
  size_t totalPixels = 0;
  const uint8_t cappedCount = gpioConfig_.outputCount > kMaxLedOutputs ? kMaxLedOutputs : gpioConfig_.outputCount;
  for (uint8_t index = 0; index < cappedCount; ++index) {
    totalPixels += outputLogicalPixels(gpioConfig_.outputs[index]);
  }
  return totalPixels * 3;
}

uint32_t SyncService::readBigEndian32(const uint8_t *data) {
  return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

uint16_t SyncService::readBigEndian16(const uint8_t *data) {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]));
}

String SyncService::ipToString(const IPAddress &ip) {
  return ip.toString();
}

IPAddress SyncService::broadcastAddressFor(const IPAddress &ip, const IPAddress &mask) {
  IPAddress out;
  for (uint8_t index = 0; index < 4; ++index) {
    out[index] = (ip[index] & mask[index]) | static_cast<uint8_t>(~mask[index]);
  }
  return out;
}

uint32_t SyncService::scaleRgb(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness) {
  if (brightness < 255) {
    red = static_cast<uint8_t>((static_cast<uint16_t>(red) * brightness) / 255U);
    green = static_cast<uint8_t>((static_cast<uint16_t>(green) * brightness) / 255U);
    blue = static_cast<uint8_t>((static_cast<uint16_t>(blue) * brightness) / 255U);
  }

  return (static_cast<uint32_t>(red) << 16) | (static_cast<uint32_t>(green) << 8) | static_cast<uint32_t>(blue);
}
