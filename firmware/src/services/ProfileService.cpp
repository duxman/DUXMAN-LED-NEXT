#include "services/ProfileService.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
constexpr const char *kProfilesPath = "/gpio-profiles.json";
constexpr const char *kStartupProfilePath = "/startup-profile.json";
constexpr const char *kDefaultProfileId = "DEFAULT";
constexpr const char *kDefaultProfileName = "DEFAULT";
constexpr const char *kDefaultProfileDescription =
  "Configuracion GPIO activa del dispositivo. Se actualiza automaticamente al guardar GPIO o aplicar un profile.";
constexpr const char *kGledoptoProfileId = "gledopto_gl_c_017wl_d";
constexpr const char *kGledoptoProfileName = "Gledopto GL-C-017WL-D";
constexpr const char *kGledoptoProfileDescription =
    "Preset LED para este controlador. Incluye GPIO 16, 4 y 2. GPIO 1 queda fuera por ser TX y no se activa por defecto.";

bool jsonBoolOrDefault(JsonVariantConst value, bool fallback = false) {
  if (value.isNull()) {
    return fallback;
  }

  if (value.is<bool>()) {
    return value.as<bool>();
  }

  if (value.is<int>() || value.is<long>()) {
    return value.as<long>() != 0;
  }

  if (value.is<const char *>()) {
    const String parsed = value.as<String>();
    return parsed == "true" || parsed == "1";
  }

  return fallback;
}

String normalizeGpioPayload(const JsonObjectConst &source) {
  JsonDocument doc;
  if (!source["gpio"].isNull()) {
    doc["gpio"] = source["gpio"];
  } else {
    doc["gpio"] = source;
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}
} // namespace

ProfileService::ProfileService(GpioConfig &gpioConfig,
                               StorageService &storageService,
                               LedDriver &ledDriver)
  : gpioConfig_(gpioConfig), storageService_(storageService),
    ledDriver_(ledDriver) {}

void ProfileService::begin() {
  initializeBuiltInProfiles();
  loadUserProfiles();
  initializeBuiltInProfiles();
  loadDefaultProfileId();
  refreshDefaultProfile();
}

bool ProfileService::applyStartupProfile(String *appliedId, String *error) {
  if (appliedId != nullptr) {
    appliedId->clear();
  }

  if (defaultProfileId_.isEmpty()) {
    if (error != nullptr) {
      error->clear();
    }
    return false;
  }

  const GpioProfile *profile = findProfileById(defaultProfileId_);
  if (profile == nullptr) {
    if (error != nullptr) {
      *error = "default_profile_not_found";
    }
    return false;
  }

  const bool changed = gpioConfig_.toJson() != profile->gpio.toJson();
  gpioConfig_ = profile->gpio;

  if (!storageService_.saveGpioConfig()) {
    if (error != nullptr) {
      *error = "persistence_failed";
    }
    return false;
  }

  if (appliedId != nullptr) {
    *appliedId = profile->id;
  }
  if (error != nullptr) {
    error->clear();
  }
  return changed;
}

void ProfileService::applyActiveConfig() {
  ledDriver_.configure(gpioConfig_);
  ledDriver_.begin();
}

bool ProfileService::syncDefaultProfileFromActiveConfig(String *error) {
  refreshDefaultProfile();
  defaultProfileId_ = kDefaultProfileId;
  if (!saveDefaultProfileId()) {
    if (error != nullptr) {
      *error = "persistence_failed";
    }
    return false;
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

String ProfileService::listProfilesJson() const {
  JsonDocument doc;
  JsonObject profiles = doc["profiles"].to<JsonObject>();
  profiles["defaultProfileId"] = defaultProfileId_;
  profiles["activeProfileId"] = detectActiveProfileId();
  profiles["buildProfile"] = kDefaultProfileId;
  JsonArray items = profiles["items"].to<JsonArray>();
  appendProfileJson(items);

  String out;
  serializeJsonPretty(doc, out);
  return out;
}

bool ProfileService::saveProfileFromJson(const String &payload, String *response,
                                         String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    if (error != nullptr) {
      *error = "invalid_json";
    }
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst profileObj = root["profile"].isNull()
                                 ? root
                                 : root["profile"].as<JsonObjectConst>();

  const String id = profileObj["id"].isNull() ? "" : profileObj["id"].as<String>();
  if (!isValidProfileId(id)) {
    if (error != nullptr) {
      *error = "invalid_profile_id";
    }
    return false;
  }

  if (findProfileById(id) != nullptr && findUserProfileById(id) == nullptr) {
    if (error != nullptr) {
      *error = "readonly_profile";
    }
    return false;
  }

  JsonObjectConst gpioObj = profileObj["gpio"].isNull()
                              ? root["gpio"].as<JsonObjectConst>()
                              : profileObj["gpio"].as<JsonObjectConst>();
  if (gpioObj.isNull()) {
    if (error != nullptr) {
      *error = "missing_gpio";
    }
    return false;
  }

  GpioConfig candidate = GpioConfig::defaults();
  String validationError;
  candidate.applyPatchJson(normalizeGpioPayload(gpioObj), &validationError);
  if (!validationError.isEmpty()) {
    if (error != nullptr) {
      *error = validationError;
    }
    return false;
  }

  GpioProfile profile;
  profile.id = id;
  profile.name = profileObj["name"].isNull() ? id : profileObj["name"].as<String>();
  profile.description = profileObj["description"].isNull()
                          ? ""
                          : profileObj["description"].as<String>();
  profile.readOnly = false;
  profile.builtIn = false;
  profile.gpio = candidate;

  if (!upsertUserProfile(profile, error) || !saveUserProfiles()) {
    if (error != nullptr && error->isEmpty()) {
      *error = "persistence_failed";
    }
    return false;
  }

  const bool applyNow = jsonBoolOrDefault(profileObj["applyNow"]);
  const bool setDefault = jsonBoolOrDefault(profileObj["autoApplyOnBoot"]) ||
                          jsonBoolOrDefault(profileObj["setDefault"]);
  const bool activateAsCurrent = applyNow || setDefault;

  bool applied = false;
  if (activateAsCurrent) {
    gpioConfig_ = candidate;
    if (!storageService_.saveGpioConfig()) {
      if (error != nullptr) {
        *error = "persistence_failed";
      }
      return false;
    }
    if (!syncDefaultProfileFromActiveConfig(error)) {
      return false;
    }
    applyActiveConfig();
    applied = true;
  }

  const bool defaultUpdated = activateAsCurrent;

  if (response != nullptr) {
    JsonDocument responseDoc;
    responseDoc["saved"] = true;
    responseDoc["applied"] = applied;
    responseDoc["defaultUpdated"] = defaultUpdated;
    JsonObject profileJson = responseDoc["profile"].to<JsonObject>();
    serializeProfile(profileJson, profile);
    String out;
    serializeJson(responseDoc, out);
    *response = out;
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

bool ProfileService::applyProfileFromJson(const String &payload, String *response,
                                          String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    if (error != nullptr) {
      *error = "invalid_json";
    }
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst request = root["profile"].isNull()
                              ? root
                              : root["profile"].as<JsonObjectConst>();
  const String id = request["id"].isNull() ? "" : request["id"].as<String>();

  return applyProfileById(id, true, response, error);
}

bool ProfileService::setDefaultProfileFromJson(const String &payload,
                                               String *response,
                                               String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    if (error != nullptr) {
      *error = "invalid_json";
    }
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst request = root["profile"].isNull()
                              ? root
                              : root["profile"].as<JsonObjectConst>();
  const String id = request["id"].isNull() ? "" : request["id"].as<String>();
  if (!applyProfileById(id, true, response, error)) {
    return false;
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

bool ProfileService::deleteProfileFromJson(const String &payload, String *response,
                                           String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    if (error != nullptr) {
      *error = "invalid_json";
    }
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst request = root["profile"].isNull()
                              ? root
                              : root["profile"].as<JsonObjectConst>();
  const String id = request["id"].isNull() ? "" : request["id"].as<String>();
  const int index = findUserProfileIndexById(id);
  if (index < 0) {
    const GpioProfile *profile = findProfileById(id);
    if (profile != nullptr && profile->builtIn) {
      if (!markBuiltInProfileDeleted(id, error)) {
        return false;
      }

      if (response != nullptr) {
        JsonDocument responseDoc;
        responseDoc["deleted"] = true;
        responseDoc["id"] = id;
        responseDoc["defaultCleared"] = false;
        String out;
        serializeJson(responseDoc, out);
        *response = out;
      }

      if (error != nullptr) {
        error->clear();
      }
      return true;
    }
    if (error != nullptr) {
      *error = "profile_not_found";
    }
    return false;
  }

  for (int i = index; i < static_cast<int>(userProfileCount_) - 1; ++i) {
    userProfiles_[i] = userProfiles_[i + 1];
  }
  if (userProfileCount_ > 0) {
    userProfiles_[userProfileCount_ - 1] = GpioProfile();
    --userProfileCount_;
  }

  if (!saveUserProfiles()) {
    if (error != nullptr) {
      *error = "persistence_failed";
    }
    return false;
  }

  bool defaultCleared = false;
  if (defaultProfileId_ == id) {
    defaultProfileId_.clear();
    if (!saveDefaultProfileId()) {
      if (error != nullptr) {
        *error = "persistence_failed";
      }
      return false;
    }
    defaultCleared = true;
  }

  if (response != nullptr) {
    JsonDocument responseDoc;
    responseDoc["deleted"] = true;
    responseDoc["id"] = id;
    responseDoc["defaultCleared"] = defaultCleared;
    String out;
    serializeJson(responseDoc, out);
    *response = out;
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

void ProfileService::initializeBuiltInProfiles() {
  builtInProfileCount_ = 0;

  GpioProfile buildProfile;
  buildProfile.id = kDefaultProfileId;
  buildProfile.name = kDefaultProfileName;
  buildProfile.description = kDefaultProfileDescription;
  buildProfile.readOnly = true;
  buildProfile.builtIn = true;
  buildProfile.gpio = gpioConfig_;
  builtInProfiles_[builtInProfileCount_++] = buildProfile;

    if (!isBuiltInProfileDeleted(kGledoptoProfileId) &&
      buildProfile.id != kGledoptoProfileId &&
      builtInProfileCount_ < kMaxBuiltInGpioProfiles) {
    GpioProfile gledoptoProfile;
    gledoptoProfile.id = kGledoptoProfileId;
    gledoptoProfile.name = kGledoptoProfileName;
    gledoptoProfile.description = kGledoptoProfileDescription;
    gledoptoProfile.readOnly = true;
    gledoptoProfile.builtIn = true;
    gledoptoProfile.gpio = createGledoptoProfileConfig();
    builtInProfiles_[builtInProfileCount_++] = gledoptoProfile;
  }
}

void ProfileService::refreshDefaultProfile() {
  if (builtInProfileCount_ == 0) {
    initializeBuiltInProfiles();
  }

  builtInProfiles_[0].id = kDefaultProfileId;
  builtInProfiles_[0].name = kDefaultProfileName;
  builtInProfiles_[0].description = kDefaultProfileDescription;
  builtInProfiles_[0].readOnly = true;
  builtInProfiles_[0].builtIn = true;
  builtInProfiles_[0].gpio = gpioConfig_;
}

bool ProfileService::loadUserProfiles() {
  for (uint8_t i = 0; i < kMaxDeletedBuiltInGpioProfiles; ++i) {
    deletedBuiltInProfileIds_[i].clear();
  }
  deletedBuiltInProfileCount_ = 0;

  for (uint8_t i = 0; i < kMaxUserGpioProfiles; ++i) {
    userProfiles_[i] = GpioProfile();
  }
  userProfileCount_ = 0;

  String raw;
  if (!readFile(kProfilesPath, raw)) {
    return true;
  }

  JsonDocument doc;
  if (deserializeJson(doc, raw)) {
    return false;
  }

  JsonArrayConst items = doc["profiles"].as<JsonArrayConst>();
  if (items.isNull()) {
    return false;
  }

  JsonArrayConst deletedBuiltIns = doc["deletedBuiltIns"].as<JsonArrayConst>();
  if (!deletedBuiltIns.isNull()) {
    for (JsonVariantConst item : deletedBuiltIns) {
      if (deletedBuiltInProfileCount_ >= kMaxDeletedBuiltInGpioProfiles) {
        break;
      }
      const String id = item.isNull() ? "" : item.as<String>();
      if (id.isEmpty() || id == kDefaultProfileId) {
        continue;
      }
      bool exists = false;
      for (uint8_t i = 0; i < deletedBuiltInProfileCount_; ++i) {
        if (deletedBuiltInProfileIds_[i] == id) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        deletedBuiltInProfileIds_[deletedBuiltInProfileCount_++] = id;
      }
    }
  }

  for (JsonObjectConst item : items) {
    if (userProfileCount_ >= kMaxUserGpioProfiles) {
      break;
    }

    const String id = item["id"].isNull() ? "" : item["id"].as<String>();
    if (!isValidProfileId(id) || findProfileById(id) != nullptr) {
      continue;
    }

    JsonObjectConst gpioObj = item["gpio"].as<JsonObjectConst>();
    if (gpioObj.isNull()) {
      continue;
    }

    GpioConfig gpio = GpioConfig::defaults();
    String validationError;
    gpio.applyPatchJson(normalizeGpioPayload(gpioObj), &validationError);
    if (!validationError.isEmpty()) {
      continue;
    }

    GpioProfile &profile = userProfiles_[userProfileCount_++];
    profile.id = id;
    profile.name = item["name"].isNull() ? id : item["name"].as<String>();
    profile.description = item["description"].isNull() ? "" : item["description"].as<String>();
    profile.readOnly = false;
    profile.builtIn = false;
    profile.gpio = gpio;
  }

  return true;
}

bool ProfileService::saveUserProfiles() const {
  JsonDocument doc;
  JsonArray items = doc["profiles"].to<JsonArray>();
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    JsonObject item = items.add<JsonObject>();
    serializeProfile(item, userProfiles_[i]);
  }

  JsonArray deletedBuiltIns = doc["deletedBuiltIns"].to<JsonArray>();
  for (uint8_t i = 0; i < deletedBuiltInProfileCount_; ++i) {
    deletedBuiltIns.add(deletedBuiltInProfileIds_[i]);
  }

  String out;
  serializeJsonPretty(doc, out);
  return writeFile(kProfilesPath, out);
}

bool ProfileService::loadDefaultProfileId() {
  defaultProfileId_ = kDefaultProfileId;

  String raw;
  if (!readFile(kStartupProfilePath, raw)) {
    return saveDefaultProfileId();
  }

  JsonDocument doc;
  if (deserializeJson(doc, raw)) {
    return false;
  }

  const String id = doc["defaultProfileId"].isNull()
                      ? ""
                      : doc["defaultProfileId"].as<String>();
  if (id != kDefaultProfileId) {
    defaultProfileId_ = kDefaultProfileId;
    return saveDefaultProfileId();
  }

  return true;
}

bool ProfileService::saveDefaultProfileId() const {
  JsonDocument doc;
  doc["defaultProfileId"] = defaultProfileId_;
  String out;
  serializeJsonPretty(doc, out);
  return writeFile(kStartupProfilePath, out);
}

const GpioProfile *ProfileService::findProfileById(const String &id) const {
  if (id.isEmpty()) {
    return nullptr;
  }

  for (uint8_t i = 0; i < builtInProfileCount_; ++i) {
    if (builtInProfiles_[i].id == id) {
      return &builtInProfiles_[i];
    }
  }

  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    if (userProfiles_[i].id == id) {
      return &userProfiles_[i];
    }
  }

  return nullptr;
}

GpioProfile *ProfileService::findUserProfileById(const String &id) {
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    if (userProfiles_[i].id == id) {
      return &userProfiles_[i];
    }
  }
  return nullptr;
}

int ProfileService::findUserProfileIndexById(const String &id) const {
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    if (userProfiles_[i].id == id) {
      return i;
    }
  }
  return -1;
}

int ProfileService::findDeletedBuiltInProfileIndexById(const String &id) const {
  for (uint8_t i = 0; i < deletedBuiltInProfileCount_; ++i) {
    if (deletedBuiltInProfileIds_[i] == id) {
      return i;
    }
  }
  return -1;
}

String ProfileService::detectActiveProfileId() const {
  const String activeJson = gpioConfig_.toJson();
  for (uint8_t i = 0; i < builtInProfileCount_; ++i) {
    if (builtInProfiles_[i].gpio.toJson() == activeJson) {
      return builtInProfiles_[i].id;
    }
  }
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    if (userProfiles_[i].gpio.toJson() == activeJson) {
      return userProfiles_[i].id;
    }
  }
  return "";
}

bool ProfileService::upsertUserProfile(const GpioProfile &profile, String *error) {
  GpioProfile *existing = findUserProfileById(profile.id);
  if (existing != nullptr) {
    *existing = profile;
    return true;
  }

  if (userProfileCount_ >= kMaxUserGpioProfiles) {
    if (error != nullptr) {
      *error = "profile_limit_reached";
    }
    return false;
  }

  userProfiles_[userProfileCount_++] = profile;
  return true;
}

bool ProfileService::markBuiltInProfileDeleted(const String &id, String *error) {
  if (id.isEmpty()) {
    if (error != nullptr) {
      *error = "profile_not_found";
    }
    return false;
  }

  if (id == kDefaultProfileId) {
    if (error != nullptr) {
      *error = "cannot_delete_default";
    }
    return false;
  }

  if (findDeletedBuiltInProfileIndexById(id) >= 0) {
    if (error != nullptr) {
      error->clear();
    }
    return true;
  }

  if (deletedBuiltInProfileCount_ >= kMaxDeletedBuiltInGpioProfiles) {
    if (error != nullptr) {
      *error = "profile_limit_reached";
    }
    return false;
  }

  deletedBuiltInProfileIds_[deletedBuiltInProfileCount_++] = id;
  initializeBuiltInProfiles();
  if (!saveUserProfiles()) {
    if (error != nullptr) {
      *error = "persistence_failed";
    }
    return false;
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

bool ProfileService::isBuiltInProfileDeleted(const String &id) const {
  return findDeletedBuiltInProfileIndexById(id) >= 0;
}

bool ProfileService::setDefaultProfileId(const String &id, String *error) {
  (void)id;
  if (!syncDefaultProfileFromActiveConfig(error)) {
    return false;
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

bool ProfileService::applyProfileById(const String &id, bool setAsDefault,
                                      String *response, String *error) {
  (void)setAsDefault;
  const GpioProfile *profile = findProfileById(id);
  if (profile == nullptr) {
    if (error != nullptr) {
      *error = "profile_not_found";
    }
    return false;
  }

  gpioConfig_ = profile->gpio;
  if (!storageService_.saveGpioConfig()) {
    if (error != nullptr) {
      *error = "persistence_failed";
    }
    return false;
  }

  if (!syncDefaultProfileFromActiveConfig(error)) {
    return false;
  }

  applyActiveConfig();

  if (response != nullptr) {
    JsonDocument responseDoc;
    responseDoc["applied"] = true;
    responseDoc["defaultUpdated"] = true;
    responseDoc["defaultProfileId"] = kDefaultProfileId;
    JsonObject profileJson = responseDoc["profile"].to<JsonObject>();
    serializeProfile(profileJson, *profile);
    String out;
    serializeJson(responseDoc, out);
    *response = out;
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

void ProfileService::appendProfileJson(JsonArray target) const {
  const String activeProfileId = detectActiveProfileId();
  for (uint8_t i = 0; i < builtInProfileCount_; ++i) {
    JsonObject item = target.add<JsonObject>();
    serializeProfile(item, builtInProfiles_[i]);
    item["isDefault"] = builtInProfiles_[i].id == defaultProfileId_;
    item["isActive"] = builtInProfiles_[i].id == activeProfileId;
  }
  for (uint8_t i = 0; i < userProfileCount_; ++i) {
    JsonObject item = target.add<JsonObject>();
    serializeProfile(item, userProfiles_[i]);
    item["isDefault"] = userProfiles_[i].id == defaultProfileId_;
    item["isActive"] = userProfiles_[i].id == activeProfileId;
  }
}

void ProfileService::serializeProfile(JsonObject target,
                                      const GpioProfile &profile) const {
  target["id"] = profile.id;
  target["name"] = profile.name;
  target["description"] = profile.description;
  target["readOnly"] = profile.readOnly;
  target["builtIn"] = profile.builtIn;

  JsonDocument gpioDoc;
  deserializeJson(gpioDoc, profile.gpio.toJson());
  target["gpio"] = gpioDoc["gpio"];
}

GpioConfig ProfileService::createGledoptoProfileConfig() {
  GpioConfig config;
  config.outputCount = 3;

  config.outputs[0].id = 0;
  config.outputs[0].pin = 16;
  config.outputs[0].ledCount = 60;
  config.outputs[0].ledType = "ws2812b";
  config.outputs[0].colorOrder = "GRB";

  config.outputs[1].id = 1;
  config.outputs[1].pin = 4;
  config.outputs[1].ledCount = 60;
  config.outputs[1].ledType = "ws2812b";
  config.outputs[1].colorOrder = "GRB";

  config.outputs[2].id = 2;
  config.outputs[2].pin = 2;
  config.outputs[2].ledCount = 60;
  config.outputs[2].ledType = "ws2812b";
  config.outputs[2].colorOrder = "GRB";

  return config;
}

bool ProfileService::isValidProfileId(const String &id) {
  if (id.isEmpty() || id.length() > 48) {
    return false;
  }

  for (size_t i = 0; i < id.length(); ++i) {
    const char ch = id[i];
    const bool isLowerAlpha = ch >= 'a' && ch <= 'z';
    const bool isDigitChar = ch >= '0' && ch <= '9';
    const bool isSeparator = ch == '-' || ch == '_';
    if (!isLowerAlpha && !isDigitChar && !isSeparator) {
      return false;
    }
  }

  return true;
}

bool ProfileService::readFile(const char *path, String &content) {
  if (!LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  content = file.readString();
  file.close();
  return !content.isEmpty();
}

bool ProfileService::writeFile(const char *path, const String &content) {
  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }

  const size_t bytes = file.print(content);
  file.close();
  return bytes > 0;
}