/*
 * duxman-led next
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/SyncService.h
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <WiFiUdp.h>

#include "core/Config.h"
#include "core/CoreState.h"
#include "drivers/LedDriver.h"

class SyncService {
public:
  struct Stats {
    uint32_t packetsReceived = 0;
    uint32_t packetsDropped = 0;
    uint32_t lastPacketAtMs = 0;
    uint32_t lastTickAtMs = 0;
    uint32_t timeoutEvents = 0;
    uint32_t pollSaturationEvents = 0;
    uint32_t lastSyncEpochMs = 0;
    uint32_t lastSequence = 0;
    uint32_t framesApplied = 0;
    uint32_t lastFrameAtMs = 0;
    uint32_t frameBytes = 0;
    uint32_t syncStateSent = 0;
    uint32_t syncStateApplied = 0;
    uint32_t lastInterPacketMs = 0;
    uint16_t inputFpsTenths = 0;
    int32_t clockOffsetMs = 0;
    bool degraded = false;
    bool sourceAlive = false;
    String activeProtocol;
    String sourceIp;
  };

  SyncService(SyncConfig &syncConfig, GpioConfig &gpioConfig);

  void begin();
  void tick(uint32_t nowMs, CoreState &coreState);

  const SyncConfig &config() const;

  bool setMode(const String &mode, String *error = nullptr);
  bool applyConfigPatch(const String &payload, bool *changed, String *error = nullptr);

  void touchPacket(const String &sourceIp = String(), bool dropped = false, uint32_t nowMs = 0);

  Stats statsSnapshot() const;
  bool isEnabled() const;
  bool isConnected() const;
  String normalizedRole() const;
  bool renderExternalFrame(LedDriver &ledDriver, const CoreState &stateSnapshot);
  String buildStateJson() const;
  String buildConnectionJson() const;

private:
  static constexpr uint8_t kDdpVersionMask = 0xC0;
  static constexpr uint8_t kDdpVersion1 = 0x40;
  static constexpr uint8_t kDdpPushFlag = 0x01;
  static constexpr uint8_t kDdpQueryFlag = 0x02;
  static constexpr uint8_t kDdpReplyFlag = 0x04;
  static constexpr uint8_t kDdpStorageFlag = 0x08;
  static constexpr uint8_t kDdpTimeFlag = 0x10;
  static constexpr uint8_t kDdpDatatypeRgb8 = 0x0B;
  static constexpr size_t kDdpHeaderLength = 10;
  static constexpr size_t kDdpTimecodeHeaderLength = 14;
  static constexpr size_t kMaxDdpPacketLength = 1500;
  static constexpr uint16_t kE131Port = 5568;
  static constexpr size_t kMaxE131PacketLength = 1500;
  static constexpr uint32_t kClusterSyncIntervalMs = 100;

  void ensureFrameBuffers();
  void resetStagingFrame();
  void resetE131Assembly();
  void updateDerivedHealth(uint32_t nowMs);
  void notePollSaturation();
  void stopListeners();
  void stopDdpListener();
  void stopE131Listener();
  void stopClusterListener();
  void ensureDdpListener();
  void ensureE131Listener();
  void ensureClusterListener();
  void pollDdp(uint32_t nowMs);
  void pollE131(uint32_t nowMs);
  void pollCluster(uint32_t nowMs, CoreState &coreState);
  void publishClusterState(uint32_t nowMs, const CoreState &coreState);
  void maybeCommitE131Frame(uint32_t nowMs);
  bool shouldListenForDdp() const;
  bool shouldListenForE131() const;
  bool shouldReceiveCluster() const;
  bool shouldPublishCluster() const;
  bool shouldRenderExternal() const;
  bool parseDdpPacket(const uint8_t *packet, size_t packetLength, const IPAddress &remoteIp, uint32_t nowMs);
  bool parseE131Packet(const uint8_t *packet, size_t packetLength, const IPAddress &remoteIp, uint32_t nowMs);
  bool parseClusterPacket(const uint8_t *packet, size_t packetLength, const IPAddress &remoteIp, uint32_t nowMs,
                          CoreState &coreState);
  bool commitStagingFrame(uint32_t nowMs);
  size_t totalFrameBytes() const;
  static uint32_t readBigEndian32(const uint8_t *data);
  static uint16_t readBigEndian16(const uint8_t *data);
  static String ipToString(const IPAddress &ip);
  static IPAddress broadcastAddressFor(const IPAddress &ip, const IPAddress &mask);
  static uint32_t scaleRgb(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness);

  SyncConfig &syncConfig_;
  GpioConfig &gpioConfig_;
  Stats stats_;
  WiFiUDP ddpUdp_;
  WiFiUDP e131Udp_;
  WiFiUDP clusterUdp_;
  SemaphoreHandle_t frameMutex_ = nullptr;
  uint8_t *frontBuffer_ = nullptr;
  uint8_t *stagingBuffer_ = nullptr;
  size_t frameBufferSize_ = 0;
  size_t frontBufferLength_ = 0;
  size_t stagingBufferLength_ = 0;
  uint8_t stagingSequence_ = 0;
  bool stagingHasSequence_ = false;
  bool stagingHasData_ = false;
  uint16_t activeDdpPort_ = 0;
  bool e131Listening_ = false;
  uint32_t e131UniverseMask_ = 0;
  uint32_t e131ExpectedMask_ = 0;
  uint32_t e131CycleStartedAtMs_ = 0;
  bool clusterListening_ = false;
  uint32_t clusterSequenceTx_ = 0;
  uint32_t clusterLastSequenceRx_ = 0;
  uint32_t clusterLastSentAtMs_ = 0;
  bool timeoutLatched_ = false;
};
