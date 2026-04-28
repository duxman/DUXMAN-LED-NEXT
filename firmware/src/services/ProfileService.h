/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/ProfileService.h
 * Last commit: ec3d96f - 2026-04-28
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "core/Config.h"
#include "drivers/LedDriver.h"
#include "services/PersistenceSchedulerService.h"
#include "services/StorageService.h"

constexpr uint8_t kMaxUserProfiles = 8;
constexpr uint8_t kMaxBuiltInProfiles = 4;

// AppProfile: snapshot completo de toda la configuración del dispositivo.
// Los perfiles permiten guardar y restaurar network + gpio + microphone + debug.
struct AppProfile {
  String id;
  String name;
  String description;
  bool readOnly = false;
  bool builtIn  = false;
  NetworkConfig   network;
  GpioConfig      gpio;
  MicrophoneConfig microphone;
  DebugConfig      debug;
};

class ProfileService {
public:
  ProfileService(NetworkConfig &networkConfig, GpioConfig &gpioConfig,
                 MicrophoneConfig &microphoneConfig, DebugConfig &debugConfig,
                 StorageService &storageService,
                 PersistenceSchedulerService &persistenceSchedulerService,
                 LedDriver &ledDriver);

  void begin();
  void processPendingPersistence();

  // Aplica el perfil por defecto al arrancar.
  bool applyStartupProfile(String *appliedId = nullptr, String *error = nullptr);
  // Recarga la config activa en el driver LED.
  void applyActiveConfig();
  // Toma la configuración activa y la guarda como nuevo perfil DEFAULT.
  bool syncDefaultProfileFromActiveConfig(String *error = nullptr);

  // --- API de perfiles ---

  // Devuelve la lista de perfiles con metadata (sin configuración completa para ahorrar RAM).
  String listProfilesJson() const;

  // Devuelve la configuración completa de un perfil por id.
  String getProfileConfigJson(const String &id) const;

  // Crea o actualiza un perfil de usuario a partir de JSON.
  // El JSON puede contener la config completa: { id, name, description, network, gpio, microphone, debug }
  bool saveProfileFromJson(const String &payload, String *response = nullptr,
                           String *error = nullptr);

  // Clona un perfil existente con nuevo id/name.
  bool cloneProfileFromJson(const String &payload, String *response = nullptr,
                            String *error = nullptr);

  // Aplica un perfil por id (carga su config y reconfigura el dispositivo).
  bool applyProfileFromJson(const String &payload, String *response = nullptr,
                            String *error = nullptr);

  // Marca un perfil como el que arranca por defecto.
  bool setDefaultProfileFromJson(const String &payload, String *response = nullptr,
                                 String *error = nullptr);

  // Elimina un perfil de usuario.
  bool deleteProfileFromJson(const String &payload, String *response = nullptr,
                             String *error = nullptr);

  static bool   isValidProfileId(const String &id);

private:
  NetworkConfig    &networkConfig_;
  GpioConfig       &gpioConfig_;
  MicrophoneConfig &microphoneConfig_;
  DebugConfig      &debugConfig_;
  StorageService   &storageService_;
  PersistenceSchedulerService &persistenceSchedulerService_;
  LedDriver        &ledDriver_;

  AppProfile builtInProfiles_[kMaxBuiltInProfiles];
  AppProfile userProfiles_[kMaxUserProfiles];
  uint8_t    builtInProfileCount_ = 0;
  uint8_t    userProfileCount_    = 0;
  uint8_t    pendingPersistenceFlags_ = 0;
  String     defaultProfileId_;

  enum PendingPersistenceFlags : uint8_t {
    kPendingUserProfiles  = 1 << 0,
    kPendingDefaultProfile = 1 << 1,
  };

  void requestSaveUserProfiles();
  void requestSaveDefaultProfileId();

  void initializeBuiltInProfiles();
  void refreshDefaultProfile();
  bool loadUserProfiles();
  bool saveUserProfiles() const;
  bool loadDefaultProfileId();
  bool saveDefaultProfileId() const;

  const AppProfile *findProfileById(const String &id) const;
  AppProfile       *findUserProfileById(const String &id);
  int               findUserProfileIndexById(const String &id) const;

  bool upsertUserProfile(const AppProfile &profile, String *error = nullptr);
  bool applyProfileById(const String &id, bool setAsDefault, String *response = nullptr,
                        String *error = nullptr);

  // Serializa la sección de metadata (sin config completa) para el listado.
  void appendProfileMetaJson(JsonArray target) const;
  // Serializa un perfil completo en un objeto JSON.
  void serializeProfileFull(JsonObject target, const AppProfile &profile) const;
  // Serializa sólo metadata.
  void serializeProfileMeta(JsonObject target, const AppProfile &profile) const;

  // Convierte la config activa en un AppProfile snapshot.
  AppProfile snapshotActiveConfig(const String &id, const String &name,
                                   const String &description) const;

  // Aplica la config de un AppProfile a las referencias activas.
  void applyProfileConfig(const AppProfile &profile);

  static bool   readFile(const char *path, String &content);
  static bool   writeFile(const char *path, const String &content);
};
