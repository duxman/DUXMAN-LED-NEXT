#include "services/EffectPersistenceService.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace {
constexpr const char *kEffectPersistencePath = "/effects-persistence.json";

uint32_t parseHexColor(const char *value, uint32_t fallback) {
  if (value == nullptr) {
    return fallback;
  }

  String normalized = value;
  normalized.trim();
  if (normalized.startsWith("#")) {
    normalized.remove(0, 1);
  }
  if (normalized.length() != 6) {
    return fallback;
  }

  for (size_t i = 0; i < normalized.length(); ++i) {
    if (!isxdigit(normalized[i])) {
      return fallback;
    }
  }

  return strtoul(normalized.c_str(), nullptr, 16) & 0xFFFFFFUL;
}

uint32_t parseColorValue(JsonVariantConst value, uint32_t fallback) {
  if (value.isNull()) {
    return fallback;
  }

  if (value.is<const char *>()) {
    return parseHexColor(value.as<const char *>(), fallback);
  }

  if (value.is<String>()) {
    return parseHexColor(value.as<String>().c_str(), fallback);
  }

  if (value.is<uint32_t>()) {
    return value.as<uint32_t>() & 0xFFFFFFUL;
  }

  if (value.is<int>()) {
    const long numeric = value.as<long>();
    if (numeric < 0) {
      return fallback;
    }
    return static_cast<uint32_t>(numeric) & 0xFFFFFFUL;
  }

  return fallback;
}
} // namespace

EffectPersistenceService::EffectPersistenceService(CoreState &state) : state_(state) {}

void EffectPersistenceService::begin() {
  String error;
  if (!load(&error) && !error.isEmpty()) {
    Serial.print("[effects] load failed: ");
    Serial.println(error);
  }
}

bool EffectPersistenceService::applyStartupEffect() {
  if (entryCount_ > 0) {
    startSequence(millis());
    return true;
  }

  if (!hasStartupEffect_) {
    return false;
  }

  applySnapshot(startupEffect_);
  return true;
}

void EffectPersistenceService::handle(unsigned long nowMs) {
  if (!sequenceRunning_ || entryCount_ == 0 || activeSequenceIndex_ >= entryCount_) {
    return;
  }

  const SavedEffectEntry &entry = entries_[activeSequenceIndex_];
  const unsigned long durationMs = static_cast<unsigned long>(entry.durationSec) * 1000UL;
  if (durationMs == 0 || nowMs - activeSequenceStartedAtMs_ < durationMs) {
    return;
  }

  const uint8_t nextIndex = static_cast<uint8_t>((activeSequenceIndex_ + 1) % entryCount_);
  applySequenceEntry(nextIndex, nowMs);
}

bool EffectPersistenceService::saveStartupFromCurrent(String *error) {
  hasStartupEffect_ = true;
  startupEffect_ = snapshotFromState(state_);
  return save(error);
}

bool EffectPersistenceService::clearStartup(String *error) {
  hasStartupEffect_ = false;
  startupEffect_ = SavedEffectConfig();
  return save(error);
}

bool EffectPersistenceService::addCurrentToSequence(uint16_t durationSec, uint16_t *createdId,
                                                   String *error) {
  if (entryCount_ >= kMaxEntries) {
    if (error != nullptr) {
      *error = "sequence_full";
    }
    return false;
  }

  SavedEffectEntry &entry = entries_[entryCount_++];
  entry.id = nextEntryId_++;
  entry.durationSec = constrain(durationSec, static_cast<uint16_t>(1), static_cast<uint16_t>(3600));
  entry.config = snapshotFromState(state_);

  if (!save(error)) {
    entryCount_--;
    return false;
  }

  if (createdId != nullptr) {
    *createdId = entry.id;
  }

  if (!sequenceRunning_ && entryCount_ == 1) {
    startSequence(millis());
  }
  return true;
}

bool EffectPersistenceService::deleteSequenceEntry(uint16_t entryId, String *error) {
  int foundIndex = -1;
  for (uint8_t i = 0; i < entryCount_; ++i) {
    if (entries_[i].id == entryId) {
      foundIndex = i;
      break;
    }
  }

  if (foundIndex < 0) {
    if (error != nullptr) {
      *error = "entry_not_found";
    }
    return false;
  }

  for (uint8_t i = static_cast<uint8_t>(foundIndex); i + 1 < entryCount_; ++i) {
    entries_[i] = entries_[i + 1];
  }
  if (entryCount_ > 0) {
    entryCount_--;
    entries_[entryCount_] = SavedEffectEntry();
  }

  if (entryCount_ == 0) {
    stopSequence();
  } else if (activeSequenceIndex_ >= entryCount_) {
    applySequenceEntry(0, millis());
  }

  return save(error);
}

String EffectPersistenceService::toJson() const {
  JsonDocument doc;
  JsonObject effects = doc["effects"].to<JsonObject>();
  effects["hasStartupEffect"] = hasStartupEffect_;
  effects["nextEntryId"] = nextEntryId_;
  effects["sequenceRunning"] = sequenceRunning_;
  effects["activeSequenceIndex"] = sequenceRunning_ ? activeSequenceIndex_ : -1;
  effects["activeSequenceEntryId"] =
      (sequenceRunning_ && activeSequenceIndex_ < entryCount_) ? entries_[activeSequenceIndex_].id : 0;

  if (hasStartupEffect_) {
    JsonObject startup = effects["startupEffect"].to<JsonObject>();
    writeConfigJson(startup, startupEffect_);
  }

  JsonArray sequence = effects["sequence"].to<JsonArray>();
  for (uint8_t i = 0; i < entryCount_; ++i) {
    JsonObject item = sequence.add<JsonObject>();
    writeEntryJson(item, entries_[i]);
  }

  String out;
  serializeJsonPretty(doc, out);
  return out;
}

bool EffectPersistenceService::load(String *error) {
  if (!LittleFS.exists(kEffectPersistencePath)) {
    if (error != nullptr) {
      error->clear();
    }
    return save(error);
  }

  File file = LittleFS.open(kEffectPersistencePath, "r");
  if (!file) {
    if (error != nullptr) {
      *error = "open_read_failed";
    }
    return false;
  }

  const String raw = file.readString();
  file.close();
  if (raw.isEmpty()) {
    if (error != nullptr) {
      *error = "empty_file";
    }
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, raw)) {
    if (error != nullptr) {
      *error = "invalid_json";
    }
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst effects = root["effects"].isNull() ? root : root["effects"].as<JsonObjectConst>();

  hasStartupEffect_ = effects["hasStartupEffect"] | false;
  nextEntryId_ = max<uint16_t>(1, effects["nextEntryId"] | 1);
  startupEffect_ = SavedEffectConfig();
  entryCount_ = 0;

  if (hasStartupEffect_) {
    JsonObjectConst startup = effects["startupEffect"].as<JsonObjectConst>();
    if (startup.isNull() || !readConfigJson(startup, startupEffect_)) {
      hasStartupEffect_ = false;
    }
  }

  JsonArrayConst sequence = effects["sequence"].as<JsonArrayConst>();
  if (!sequence.isNull()) {
    for (JsonObjectConst item : sequence) {
      if (entryCount_ >= kMaxEntries) {
        break;
      }
      SavedEffectEntry &entry = entries_[entryCount_];
      entry = SavedEffectEntry();
      entry.id = item["id"] | nextEntryId_;
      entry.durationSec = constrain(item["durationSec"] | 30, 1, 3600);
      JsonObjectConst config = item["config"].as<JsonObjectConst>();
      if (config.isNull() || !readConfigJson(config, entry.config)) {
        continue;
      }
      nextEntryId_ = max<uint16_t>(nextEntryId_, static_cast<uint16_t>(entry.id + 1));
      entryCount_++;
    }
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

bool EffectPersistenceService::save(String *error) const {
  File file = LittleFS.open(kEffectPersistencePath, "w");
  if (!file) {
    if (error != nullptr) {
      *error = "open_write_failed";
    }
    return false;
  }

  const String content = toJson();
  const size_t written = file.print(content);
  file.close();

  if (written == 0) {
    if (error != nullptr) {
      *error = "write_failed";
    }
    return false;
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

void EffectPersistenceService::startSequence(unsigned long nowMs) {
  if (entryCount_ == 0) {
    stopSequence();
    return;
  }

  sequenceRunning_ = true;
  applySequenceEntry(0, nowMs);
}

void EffectPersistenceService::stopSequence() {
  sequenceRunning_ = false;
  activeSequenceIndex_ = 0;
  activeSequenceStartedAtMs_ = 0;
}

void EffectPersistenceService::applySequenceEntry(uint8_t index, unsigned long nowMs) {
  if (entryCount_ == 0 || index >= entryCount_) {
    stopSequence();
    return;
  }

  activeSequenceIndex_ = index;
  activeSequenceStartedAtMs_ = nowMs;
  sequenceRunning_ = true;
  applySnapshot(entries_[activeSequenceIndex_].config);
}

EffectPersistenceService::SavedEffectConfig
EffectPersistenceService::snapshotFromState(const CoreState &state) {
  SavedEffectConfig snapshot;
  snapshot.power = state.power;
  snapshot.brightness = state.brightness;
  snapshot.effectId = state.effectId;
  snapshot.sectionCount = state.sectionCount;
  snapshot.effectSpeed = state.effectSpeed;
  snapshot.effectLevel = state.effectLevel;
  for (uint8_t i = 0; i < 3; ++i) {
    snapshot.primaryColors[i] = state.primaryColors[i];
  }
  snapshot.backgroundColor = state.backgroundColor;
  return snapshot;
}

void EffectPersistenceService::applySnapshot(const SavedEffectConfig &config) const {
  state_.power = config.power;
  state_.brightness = config.brightness;
  state_.effectId = config.effectId;
  state_.sectionCount = config.sectionCount;
  state_.effectSpeed = config.effectSpeed;
  state_.effectLevel = config.effectLevel;
  for (uint8_t i = 0; i < 3; ++i) {
    state_.primaryColors[i] = config.primaryColors[i];
  }
  state_.backgroundColor = config.backgroundColor;
}

void EffectPersistenceService::writeConfigJson(JsonObject target,
                                               const SavedEffectConfig &config) {
  target["power"] = config.power;
  target["brightness"] = config.brightness;
  target["effectId"] = config.effectId;
  target["effect"] = CoreState::effectName(config.effectId);
  target["effectLabel"] = CoreState::effectLabel(config.effectId);
  target["sectionCount"] = config.sectionCount;
  target["effectSpeed"] = config.effectSpeed;
  target["effectLevel"] = config.effectLevel;
  JsonArray colors = target["primaryColors"].to<JsonArray>();
  for (uint8_t i = 0; i < 3; ++i) {
    colors.add(formatHexColor(config.primaryColors[i]));
  }
  target["backgroundColor"] = formatHexColor(config.backgroundColor);
}

void EffectPersistenceService::writeEntryJson(JsonObject target,
                                              const SavedEffectEntry &entry) {
  target["id"] = entry.id;
  target["durationSec"] = entry.durationSec;
  JsonObject config = target["config"].to<JsonObject>();
  writeConfigJson(config, entry.config);
}

bool EffectPersistenceService::readConfigJson(JsonObjectConst source,
                                              SavedEffectConfig &config) {
  config.power = source["power"] | true;
  config.brightness = constrain(source["brightness"] | 128, 0, 255);
  config.effectId = source["effectId"] | static_cast<uint8_t>(EffectRegistry::kEffectFixed);
  config.sectionCount = constrain(source["sectionCount"] | 3, 1, 10);
  config.effectSpeed = constrain(source["effectSpeed"] | 10, 1, 100);
  config.effectLevel = constrain(source["effectLevel"] | 5, 1, 10);
  config.backgroundColor = parseColorValue(source["backgroundColor"], config.backgroundColor);

  JsonArrayConst colors = source["primaryColors"].as<JsonArrayConst>();
  if (colors.isNull()) {
    return false;
  }

  for (uint8_t i = 0; i < 3; ++i) {
    config.primaryColors[i] = i < colors.size()
                                  ? parseColorValue(colors[i], config.primaryColors[i])
                                  : config.primaryColors[i];
  }

  return true;
}

String EffectPersistenceService::formatHexColor(uint32_t color) {
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%06lX", static_cast<unsigned long>(color & 0xFFFFFFUL));
  return String("#") + buffer;
}