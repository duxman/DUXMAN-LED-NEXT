#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "core/CoreState.h"

class EffectPersistenceService {
public:
  static constexpr uint8_t kMaxEntries = 16;

  explicit EffectPersistenceService(CoreState &state);

  void begin();
  bool applyStartupEffect();
  void handle(unsigned long nowMs);
  bool saveStartupFromCurrent(String *error = nullptr);
  bool clearStartup(String *error = nullptr);
  bool addCurrentToSequence(uint16_t durationSec, uint16_t *createdId = nullptr,
                            String *error = nullptr);
  bool deleteSequenceEntry(uint16_t entryId, String *error = nullptr);
  String toJson() const;

private:
  struct SavedEffectConfig {
    bool power = true;
    uint8_t brightness = 128;
    uint8_t effectId = CoreState::kEffectFixed;
    uint8_t sectionCount = 3;
    uint8_t effectSpeed = 10;
    uint8_t effectLevel = 5;
    uint32_t primaryColors[3] = {0xFF4D00, 0xFFD400, 0x00B8D9};
    uint32_t backgroundColor = 0x000000;
    bool reactiveToAudio = false;
  };

  struct SavedEffectEntry {
    uint16_t id = 0;
    uint16_t durationSec = 30;
    SavedEffectConfig config;
  };

  CoreState &state_;
  bool hasStartupEffect_ = false;
  SavedEffectConfig startupEffect_;
  uint16_t nextEntryId_ = 1;
  uint8_t entryCount_ = 0;
  SavedEffectEntry entries_[kMaxEntries];
  bool sequenceRunning_ = false;
  uint8_t activeSequenceIndex_ = 0;
  unsigned long activeSequenceStartedAtMs_ = 0;

  bool load(String *error = nullptr);
  bool save(String *error = nullptr) const;
  void startSequence(unsigned long nowMs);
  void stopSequence();
  void applySequenceEntry(uint8_t index, unsigned long nowMs);
  static SavedEffectConfig snapshotFromState(const CoreState &state);
  void applySnapshot(const SavedEffectConfig &config) const;
  static void writeConfigJson(JsonObject target, const SavedEffectConfig &config);
  static void writeEntryJson(JsonObject target, const SavedEffectEntry &entry);
  static bool readConfigJson(JsonObjectConst source, SavedEffectConfig &config);
  static String formatHexColor(uint32_t color);
};