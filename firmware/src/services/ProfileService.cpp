/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/ProfileService.cpp
 * Last commit: 1ce6ea9 - 2026-04-28
 */

#include "services/ProfileService.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
constexpr const char *kProfilesPath      = "/profiles.json";
constexpr const char *kStartupProfilePath = "/startup-profile.json";
constexpr const char *kDefaultProfileId  = "default";
constexpr const char *kDefaultProfileName = "Default";
constexpr const char *kDefaultProfileDesc =
  "Perfil por defecto del dispositivo. Se actualiza al pulsar Aplicar al perfil por defecto.";
constexpr const char *kGledoptoProfileId  = "gledopto_gl_c_017wl_d";
constexpr const char *kGledoptoProfileName = "Gledopto GL-C-017WL-D";
constexpr const char *kGledoptoProfileDesc =
  "Preset para el controlador Gledopto GL-C-017WL-D. 3 salidas LED + microfono I2S.";

GpioConfig makeGledoptoGpio() {
  GpioConfig config;
  config.outputCount = 3;

  config.outputs[0].id = 0;  config.outputs[0].pin = 16; config.outputs[0].ledCount = 60;
  config.outputs[0].ledType = "ws2812b"; config.outputs[0].colorOrder = "GRB";

  config.outputs[1].id = 1;  config.outputs[1].pin = 4;  config.outputs[1].ledCount = 60;
  config.outputs[1].ledType = "ws2812b"; config.outputs[1].colorOrder = "GRB";

  config.outputs[2].id = 2;  config.outputs[2].pin = 2;  config.outputs[2].ledCount = 60;
  config.outputs[2].ledType = "ws2812b"; config.outputs[2].colorOrder = "GRB";

  return config;
}

MicrophoneConfig makeGledoptoMicrophone() {
  MicrophoneConfig mic = MicrophoneConfig::defaults();
  mic.enabled           = true;
  mic.source            = "generic_i2c";
  mic.profileId         = kGledoptoProfileId;
  mic.sampleRate        = 16000;
  mic.fftSize           = 512;
  mic.gainPercent       = 100;
  mic.noiseFloorPercent = 8;
  mic.pins.bclk         = 21;
  mic.pins.ws           = 5;
  mic.pins.din          = 26;
  return mic;
}

bool profileFromJson(const JsonObjectConst &obj, AppProfile &out, String *error) {
  const String id = obj["id"].isNull() ? "" : obj["id"].as<String>();
  if (!ProfileService::isValidProfileId(id)) {
    if (error) *error = "invalid_profile_id";
    return false;
  }
  out.id          = id;
  out.name        = obj["name"].isNull() ? id : obj["name"].as<String>();
  out.description = obj["description"].isNull() ? "" : obj["description"].as<String>();
  out.readOnly    = false;
  out.builtIn     = false;

  if (!obj["network"].isNull()) {
    String p; { JsonDocument d; d["network"] = obj["network"]; serializeJson(d, p); }
    String err; out.network.applyPatchJson(p, &err);
    if (!err.isEmpty()) { if (error) *error = "network_" + err; return false; }
  }
  if (!obj["gpio"].isNull()) {
    String p; { JsonDocument d; d["gpio"] = obj["gpio"]; serializeJson(d, p); }
    String err; out.gpio.applyPatchJson(p, &err);
    if (!err.isEmpty()) { if (error) *error = "gpio_" + err; return false; }
  }
  if (!obj["microphone"].isNull()) {
    String p; { JsonDocument d; d["microphone"] = obj["microphone"]; serializeJson(d, p); }
    String err; out.microphone.applyPatchJson(p, &err);
    if (!err.isEmpty()) { if (error) *error = "microphone_" + err; return false; }
  }
  if (!obj["debug"].isNull()) {
    String p; { JsonDocument d; d["debug"] = obj["debug"]; serializeJson(d, p); }
    String err; out.debug.applyPatchJson(p, &err);
    if (!err.isEmpty()) { if (error) *error = "debug_" + err; return false; }
  }
  return true;
}
} // namespace

ProfileService::ProfileService(NetworkConfig &networkConfig, GpioConfig &gpioConfig,
                               MicrophoneConfig &microphoneConfig, DebugConfig &debugConfig,
                               StorageService &storageService,
                               PersistenceSchedulerService &persistenceSchedulerService,
                               LedDriver &ledDriver,
                               CoreState &coreState)
  : networkConfig_(networkConfig), gpioConfig_(gpioConfig),
    microphoneConfig_(microphoneConfig), debugConfig_(debugConfig),
    storageService_(storageService),
    persistenceSchedulerService_(persistenceSchedulerService),
    ledDriver_(ledDriver), coreState_(coreState) {}

void ProfileService::begin() {
  initializeBuiltInProfiles();
  loadUserProfiles();
  initializeBuiltInProfiles();
  loadDefaultProfileId();
  refreshDefaultProfile();
}

void ProfileService::requestSaveUserProfiles() {
  pendingPersistenceFlags_ |= kPendingUserProfiles;
}

void ProfileService::requestSaveDefaultProfileId() {
  pendingPersistenceFlags_ |= kPendingDefaultProfile;
}

void ProfileService::processPendingPersistence() {
  const uint8_t pending = pendingPersistenceFlags_;
  if (pending == 0) return;
  pendingPersistenceFlags_ = 0;

  if ((pending & kPendingUserProfiles) != 0 && !saveUserProfiles()) {
    Serial.println("[profiles][async] saveUserProfiles failed");
  }
  if ((pending & kPendingDefaultProfile) != 0 && !saveDefaultProfileId()) {
    Serial.println("[profiles][async] saveDefaultProfileId failed");
  }
}

bool ProfileService::applyStartupProfile(String *appliedId, String *error) {
  if (appliedId) appliedId->clear();
  if (defaultProfileId_.isEmpty()) { if (error) error->clear(); return false; }

  const AppProfile *profile = findProfileById(defaultProfileId_);
  if (profile == nullptr) {
    if (error) *error = "default_profile_not_found";
    return false;
  }

  applyProfileConfig(*profile);
  persistenceSchedulerService_.requestSaveConfig();
  if (appliedId) *appliedId = profile->id;
  if (error) error->clear();
  return true;
}

void ProfileService::applyActiveConfig() {
  // IMPORTANTE: tomar el mutex de CoreState antes de reconfigurar el driver.
  // El render task (core 1) mantiene este mutex durante renderFrame();
  // sin esta proteccion hay una carrera de datos sobre los objetos NeoPixelBus
  // que causa pixeles aleatorios y congela la animacion.
  coreState_.lock();
  ledDriver_.configure(gpioConfig_);
  ledDriver_.begin();
  coreState_.unlock();
}

bool ProfileService::syncDefaultProfileFromActiveConfig(String *error) {
  refreshDefaultProfile();
  defaultProfileId_ = kDefaultProfileId;
  requestSaveDefaultProfileId();
  if (error) error->clear();
  return true;
}

String ProfileService::listProfilesJson() const {
  JsonDocument doc;
  JsonObject root = doc["profiles"].to<JsonObject>();
  root["defaultId"] = defaultProfileId_;
  JsonArray items = root["items"].to<JsonArray>();
  appendProfileMetaJson(items);
  String out;
  serializeJsonPretty(doc, out);
  return out;
}

String ProfileService::getProfileConfigJson(const String &id) const {
  const AppProfile *profile = findProfileById(id);
  if (profile == nullptr) return "{\"error\":\"profile_not_found\"}";
  JsonDocument doc;
  JsonObject obj = doc["profile"].to<JsonObject>();
  serializeProfileFull(obj, *profile);
  String out;
  serializeJsonPretty(doc, out);
  return out;
}

bool ProfileService::saveProfileFromJson(const String &payload, String *response, String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) { if (error) *error = "invalid_json"; return false; }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst profileObj = root["profile"].isNull() ? root : root["profile"].as<JsonObjectConst>();

  const String id = profileObj["id"].isNull() ? "" : profileObj["id"].as<String>();
  AppProfile profile = snapshotActiveConfig(id, id, "");

  String parseError;
  if (!profileFromJson(profileObj, profile, &parseError)) {
    if (error) *error = parseError;
    return false;
  }

  const AppProfile *existing = findProfileById(profile.id);
  if (existing != nullptr && findUserProfileById(profile.id) == nullptr) {
    if (error) *error = "readonly_profile";
    return false;
  }

  if (!upsertUserProfile(profile, error)) return false;
  requestSaveUserProfiles();

  if (response) {
    JsonDocument resp;
    resp["saved"] = true;
    JsonObject obj = resp["profile"].to<JsonObject>();
    serializeProfileMeta(obj, profile);
    String out; serializeJson(resp, out);
    *response = out;
  }
  if (error) error->clear();
  return true;
}

bool ProfileService::cloneProfileFromJson(const String &payload, String *response, String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) { if (error) *error = "invalid_json"; return false; }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  const String sourceId = root["sourceId"].isNull() ? "" : root["sourceId"].as<String>();
  const String newId    = root["newId"].isNull()    ? "" : root["newId"].as<String>();
  const String newName  = root["newName"].isNull()  ? newId : root["newName"].as<String>();

  if (!isValidProfileId(newId)) { if (error) *error = "invalid_new_profile_id"; return false; }

  const AppProfile *source = findProfileById(sourceId);
  if (source == nullptr) { if (error) *error = "source_profile_not_found"; return false; }
  if (findProfileById(newId) != nullptr) { if (error) *error = "profile_id_already_exists"; return false; }

  AppProfile clone = *source;
  clone.id = newId; clone.name = newName;
  clone.readOnly = false; clone.builtIn = false;

  if (!upsertUserProfile(clone, error)) return false;
  requestSaveUserProfiles();

  if (response) {
    JsonDocument resp;
    resp["cloned"] = true;
    JsonObject obj = resp["profile"].to<JsonObject>();
    serializeProfileMeta(obj, clone);
    String out; serializeJson(resp, out);
    *response = out;
  }
  if (error) error->clear();
  return true;
}

bool ProfileService::applyProfileFromJson(const String &payload, String *response, String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) { if (error) *error = "invalid_json"; return false; }
  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst req  = root["profile"].isNull() ? root : root["profile"].as<JsonObjectConst>();
  const String id = req["id"].isNull() ? "" : req["id"].as<String>();
  return applyProfileById(id, true, response, error);
}

bool ProfileService::setDefaultProfileFromJson(const String &payload, String *response, String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) { if (error) *error = "invalid_json"; return false; }
  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst req  = root["profile"].isNull() ? root : root["profile"].as<JsonObjectConst>();
  const String id = req["id"].isNull() ? "" : req["id"].as<String>();

  if (findProfileById(id) == nullptr) { if (error) *error = "profile_not_found"; return false; }

  defaultProfileId_ = id;
  requestSaveDefaultProfileId();

  if (response) {
    JsonDocument resp;
    resp["defaultUpdated"] = true;
    resp["defaultId"] = defaultProfileId_;
    String out; serializeJson(resp, out);
    *response = out;
  }
  if (error) error->clear();
  return true;
}

bool ProfileService::deleteProfileFromJson(const String &payload, String *response, String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) { if (error) *error = "invalid_json"; return false; }
  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst req  = root["profile"].isNull() ? root : root["profile"].as<JsonObjectConst>();
  const String id = req["id"].isNull() ? "" : req["id"].as<String>();

  if (id == kDefaultProfileId) { if (error) *error = "cannot_delete_builtin"; return false; }

  const int index = findUserProfileIndexById(id);
  if (index < 0) {
    const AppProfile *p = findProfileById(id);
    if (p != nullptr && p->builtIn) { if (error) *error = "cannot_delete_builtin"; return false; }
    if (error) *error = "profile_not_found";
    return false;
  }

  for (int i = index; i < static_cast<int>(userProfileCount_) - 1; ++i) {
    userProfiles_[i] = userProfiles_[i + 1];
  }
  if (userProfileCount_ > 0) {
    userProfiles_[userProfileCount_ - 1] = AppProfile();
    --userProfileCount_;
  }
  requestSaveUserProfiles();

  bool defaultCleared = false;
  if (defaultProfileId_ == id) {
    defaultProfileId_ = kDefaultProfileId;
    requestSaveDefaultProfileId();
    defaultCleared = true;
  }

  if (response) {
    JsonDocument resp;
    resp["deleted"] = true; resp["id"] = id; resp["defaultCleared"] = defaultCleared;
    String out; serializeJson(resp, out);
    *response = out;
  }
  if (error) error->clear();
  return true;
}

void ProfileService::initializeBuiltInProfiles() {
  builtInProfileCount_ = 0;

  AppProfile def = snapshotActiveConfig(kDefaultProfileId, kDefaultProfileName, kDefaultProfileDesc);
  def.readOnly = true; def.builtIn = true;
  builtInProfiles_[builtInProfileCount_++] = def;

  if (builtInProfileCount_ < kMaxBuiltInProfiles) {
    AppProfile gled;
    gled.id = kGledoptoProfileId; gled.name = kGledoptoProfileName;
    gled.description = kGledoptoProfileDesc;
    gled.readOnly = true; gled.builtIn = true;
    gled.network    = NetworkConfig::defaults();
    gled.gpio       = makeGledoptoGpio();
    gled.microphone = makeGledoptoMicrophone();
    gled.debug      = DebugConfig::defaults();
    builtInProfiles_[builtInProfileCount_++] = gled;
  }
}

void ProfileService::refreshDefaultProfile() {
  if (builtInProfileCount_ == 0) initializeBuiltInProfiles();
  builtInProfiles_[0] = snapshotActiveConfig(kDefaultProfileId, kDefaultProfileName, kDefaultProfileDesc);
  builtInProfiles_[0].readOnly = true;
  builtInProfiles_[0].builtIn  = true;
}

bool ProfileService::loadUserProfiles() {
  for (uint8_t i = 0; i < kMaxUserProfiles; ++i) userProfiles_[i] = AppProfile();
  userProfileCount_ = 0;

  String raw;
  if (!readFile(kProfilesPath, raw)) return true;

  JsonDocument doc;
  if (deserializeJson(doc, raw)) return false;

  JsonArrayConst items = doc["profiles"].as<JsonArrayConst>();
  if (items.isNull()) return false;

  for (JsonObjectConst item : items) {
    if (userProfileCount_ >= kMaxUserProfiles) break;
    const String id = item["id"].isNull() ? "" : item["id"].as<String>();
    if (!isValidProfileId(id) || findProfileById(id) != nullptr) continue;

    AppProfile profile = snapshotActiveConfig(id, id, "");
    String err;
    if (!profileFromJson(item, profile, &err)) {
      Serial.print("[profiles] skipping '"); Serial.print(id); Serial.print("': "); Serial.println(err);
      continue;
    }
    userProfiles_[userProfileCount_++] = profile;
  }
  return true;
}

bool ProfileService::saveUserProfiles() const {
  JsonDocument doc;
  JsonArray items = doc["profiles"].to<JsonArray>();
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    JsonObject item = items.add<JsonObject>();
    serializeProfileFull(item, userProfiles_[i]);
  }
  String out;
  serializeJsonPretty(doc, out);
  return writeFile(kProfilesPath, out);
}

bool ProfileService::loadDefaultProfileId() {
  defaultProfileId_ = kDefaultProfileId;
  String raw;
  if (!readFile(kStartupProfilePath, raw)) return saveDefaultProfileId();
  JsonDocument doc;
  if (deserializeJson(doc, raw)) return false;
  const String id = doc["defaultId"].isNull() ? "" : doc["defaultId"].as<String>();
  if (id.isEmpty() || findProfileById(id) == nullptr) {
    defaultProfileId_ = kDefaultProfileId;
    return saveDefaultProfileId();
  }
  defaultProfileId_ = id;
  return true;
}

bool ProfileService::saveDefaultProfileId() const {
  JsonDocument doc;
  doc["defaultId"] = defaultProfileId_;
  String out;
  serializeJsonPretty(doc, out);
  return writeFile(kStartupProfilePath, out);
}

const AppProfile *ProfileService::findProfileById(const String &id) const {
  if (id.isEmpty()) return nullptr;
  for (uint8_t i = 0; i < builtInProfileCount_; ++i) {
    if (builtInProfiles_[i].id == id) return &builtInProfiles_[i];
  }
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    if (userProfiles_[i].id == id) return &userProfiles_[i];
  }
  return nullptr;
}

AppProfile *ProfileService::findUserProfileById(const String &id) {
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    if (userProfiles_[i].id == id) return &userProfiles_[i];
  }
  return nullptr;
}

int ProfileService::findUserProfileIndexById(const String &id) const {
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    if (userProfiles_[i].id == id) return static_cast<int>(i);
  }
  return -1;
}

bool ProfileService::upsertUserProfile(const AppProfile &profile, String *error) {
  AppProfile *existing = findUserProfileById(profile.id);
  if (existing != nullptr) { *existing = profile; return true; }
  if (userProfileCount_ >= kMaxUserProfiles) {
    if (error) *error = "profile_limit_reached";
    return false;
  }
  userProfiles_[userProfileCount_++] = profile;
  return true;
}

bool ProfileService::applyProfileById(const String &id, bool setAsDefault,
                                       String *response, String *error) {
  const AppProfile *profile = findProfileById(id);
  if (profile == nullptr) { if (error) *error = "profile_not_found"; return false; }

  // Capture the current GPIO config before updating so we can detect whether
  // the LED driver actually needs to be reinitialised.  Calling begin() when
  // nothing changed causes an unnecessary RMT teardown/reinit that can produce
  // extra pixels on the strip (frame-stitch artefact).
  const String gpioJsonBefore = gpioConfig_.toJson();

  applyProfileConfig(*profile);
  persistenceSchedulerService_.requestSaveConfig();
  refreshDefaultProfile();

  if (setAsDefault) {
    defaultProfileId_ = id;
    requestSaveDefaultProfileId();
  }

  const bool gpioChanged = (gpioConfig_.toJson() != gpioJsonBefore);
  if (gpioChanged) {
    applyActiveConfig();
  }

  if (response) {
    JsonDocument resp;
    resp["applied"] = true;
    resp["defaultUpdated"] = setAsDefault;
    resp["defaultId"] = defaultProfileId_;
    JsonObject obj = resp["profile"].to<JsonObject>();
    serializeProfileMeta(obj, *profile);
    String out; serializeJson(resp, out);
    *response = out;
  }
  if (error) error->clear();
  return true;
}

AppProfile ProfileService::snapshotActiveConfig(const String &id, const String &name,
                                                 const String &description) const {
  AppProfile p;
  p.id = id; p.name = name; p.description = description;
  p.network    = networkConfig_;
  p.gpio       = gpioConfig_;
  p.microphone = microphoneConfig_;
  p.debug      = debugConfig_;
  return p;
}

void ProfileService::applyProfileConfig(const AppProfile &profile) {
  networkConfig_    = profile.network;
  gpioConfig_       = profile.gpio;
  microphoneConfig_ = profile.microphone;
  debugConfig_      = profile.debug;
}

void ProfileService::appendProfileMetaJson(JsonArray target) const {
  for (uint8_t i = 0; i < builtInProfileCount_; ++i) {
    JsonObject item = target.add<JsonObject>();
    serializeProfileMeta(item, builtInProfiles_[i]);
    item["isDefault"] = builtInProfiles_[i].id == defaultProfileId_;
  }
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    JsonObject item = target.add<JsonObject>();
    serializeProfileMeta(item, userProfiles_[i]);
    item["isDefault"] = userProfiles_[i].id == defaultProfileId_;
  }
}

void ProfileService::serializeProfileMeta(JsonObject target, const AppProfile &profile) const {
  target["id"]          = profile.id;
  target["name"]        = profile.name;
  target["description"] = profile.description;
  target["readOnly"]    = profile.readOnly;
  target["builtIn"]     = profile.builtIn;
}

void ProfileService::serializeProfileFull(JsonObject target, const AppProfile &profile) const {
  serializeProfileMeta(target, profile);
  { JsonDocument d; deserializeJson(d, profile.network.toJson());    target["network"]    = d["network"]; }
  { JsonDocument d; deserializeJson(d, profile.gpio.toJson());       target["gpio"]       = d["gpio"]; }
  { JsonDocument d; deserializeJson(d, profile.microphone.toJson()); target["microphone"] = d["microphone"]; }
  { JsonDocument d; deserializeJson(d, profile.debug.toJson());      target["debug"]      = d["debug"]; }
}

bool ProfileService::isValidProfileId(const String &id) {
  if (id.isEmpty() || id.length() > 48) return false;
  for (size_t i = 0; i < id.length(); ++i) {
    const char ch = id[i];
    if (!(ch >= 'a' && ch <= 'z') && !(ch >= '0' && ch <= '9') && ch != '-' && ch != '_') return false;
  }
  return true;
}

bool ProfileService::readFile(const char *path, String &content) {
  if (!LittleFS.exists(path)) return false;
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  content = file.readString();
  file.close();
  return !content.isEmpty();
}

bool ProfileService::writeFile(const char *path, const String &content) {
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  const size_t bytes = file.print(content);
  file.close();
  return bytes > 0;
}