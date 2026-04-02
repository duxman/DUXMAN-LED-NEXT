#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "core/Config.h"
#include "drivers/LedDriver.h"
#include "services/StorageService.h"

constexpr uint8_t kMaxUserGpioProfiles = 8;
constexpr uint8_t kMaxBuiltInGpioProfiles = 2;

struct GpioProfile {
  String id;
  String name;
  String description;
  bool readOnly = false;
  bool builtIn = false;
  GpioConfig gpio;
};

class ProfileService {
public:
  ProfileService(GpioConfig &gpioConfig, StorageService &storageService,
                 LedDriver &ledDriver);

  void begin();
  bool applyStartupProfile(String *appliedId = nullptr, String *error = nullptr);
  void applyActiveConfig();

  String listProfilesJson() const;
  bool saveProfileFromJson(const String &payload, String *response = nullptr,
                           String *error = nullptr);
  bool applyProfileFromJson(const String &payload, String *response = nullptr,
                            String *error = nullptr);
  bool setDefaultProfileFromJson(const String &payload, String *response = nullptr,
                                 String *error = nullptr);
  bool deleteProfileFromJson(const String &payload, String *response = nullptr,
                             String *error = nullptr);

private:
  GpioConfig &gpioConfig_;
  StorageService &storageService_;
  LedDriver &ledDriver_;
  GpioProfile builtInProfiles_[kMaxBuiltInGpioProfiles];
  GpioProfile userProfiles_[kMaxUserGpioProfiles];
  uint8_t builtInProfileCount_ = 0;
  uint8_t userProfileCount_ = 0;
  String defaultProfileId_;

  void initializeBuiltInProfiles();
  bool loadUserProfiles();
  bool saveUserProfiles() const;
  bool loadDefaultProfileId();
  bool saveDefaultProfileId() const;
  const GpioProfile *findProfileById(const String &id) const;
  GpioProfile *findUserProfileById(const String &id);
  int findUserProfileIndexById(const String &id) const;
  String detectActiveProfileId() const;
  bool upsertUserProfile(const GpioProfile &profile, String *error = nullptr);
  bool setDefaultProfileId(const String &id, String *error = nullptr);
  bool applyProfileById(const String &id, bool setAsDefault, String *response = nullptr,
                        String *error = nullptr);
  void appendProfileJson(JsonArray target) const;
  void serializeProfile(JsonObject target, const GpioProfile &profile) const;
  static GpioConfig createGledoptoProfileConfig();
  static bool isValidProfileId(const String &id);
  static bool readFile(const char *path, String &content);
  static bool writeFile(const char *path, const String &content);
};