/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/api/ApiService.h
 * Last commit: 47b0156 - 2026-04-28
 */

#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "core/CoreState.h"
#include "core/Config.h"
#include "core/HardwareInfo.h"
#include "core/ReleaseInfo.h"
#include "services/EffectPersistenceService.h"
#include "services/PersistenceSchedulerService.h"
#include "services/ProfileService.h"
#include "services/StorageService.h"
#include "services/UserPaletteService.h"
#include "services/WatchdogService.h"
#include "services/WifiService.h"

class ApiService {
public:
  ApiService(CoreState &state, NetworkConfig &networkConfig, GpioConfig &gpioConfig,
             MicrophoneConfig &microphoneConfig, DebugConfig &debugConfig,
             StorageService &storageService, WifiService &wifiService,
             PersistenceSchedulerService &persistenceSchedulerService,
             EffectPersistenceService &effectPersistenceService,
             ProfileService &profileService, UserPaletteService &userPaletteService,
             WatchdogService &watchdogService);

  void begin();
  void handle();

private:
  CoreState &state_;
  NetworkConfig &networkConfig_;
  GpioConfig &gpioConfig_;
  MicrophoneConfig &microphoneConfig_;
  DebugConfig &debugConfig_;
  StorageService &storageService_;
  WifiService &wifiService_;
  PersistenceSchedulerService &persistenceSchedulerService_;
  EffectPersistenceService &effectPersistenceService_;
  ProfileService &profileService_;
  UserPaletteService &userPaletteService_;
  WatchdogService &watchdogService_;
  String commandBuffer_;
  WebServer httpServer_;

  void processCommand(const String &command);
  void setupHttpRoutes();
  void handleHttpStateRoute();
  void handleHttpNetworkRoute();
  void handleHttpMicrophoneRoute();
  void handleHttpGpioRoute();
  void handleHttpDebugRoute();
  void handleHttpDiagRoute();
  void handleHttpConfigAllRoute();
  void handleHttpHardwareRoute();
  // Rutas de perfiles (cobertura completa de configuración)
  void handleHttpProfilesRoute();
  void handleHttpProfilesSaveRoute();
  void handleHttpProfilesCloneRoute();
  void handleHttpProfilesApplyRoute();
  void handleHttpProfilesDefaultRoute();
  void handleHttpProfilesDeleteRoute();
  void handleHttpProfilesGetRoute();
  // Rutas de efectos y paletas
  void handleHttpEffectsRoute();
  void handleHttpEffectsStartupRoute();
  void handleHttpEffectsSequenceAddRoute();
  void handleHttpEffectsSequenceDeleteRoute();
  void handleHttpPalettesRoute();
  void handleHttpPalettesApplyRoute();
  void handleHttpPalettesSaveRoute();
  void handleHttpPalettesDeleteRoute();
  void handleHttpRestartRoute();
  String buildFullConfigJson() const;
  String buildOpenApiJson() const;
  String buildCommonCss() const;
  String buildNavHtml() const;
  String buildHomeHtml() const;
  String buildConfigIndexHtml() const;
  String buildDocsHtml() const;
  String buildApiStateHtml() const;
  String buildApiConfigNetworkHtml() const;
  String buildApiConfigMicrophoneHtml() const;
  String buildApiConfigGpioHtml() const;
  String buildApiConfigDebugHtml() const;
  String buildApiConfigAllHtml() const;
  String buildApiHardwareHtml() const;
  String buildApiProfilesHtml() const;
  String buildApiReleaseHtml() const;
  String buildVersionHtml() const;
  String buildNetworkConfigHtml() const;
  String buildMicrophoneConfigHtml() const;
  String buildGpioConfigHtml() const;
  String buildProfilesConfigHtml() const;
  String buildDebugConfigHtml() const;
  String buildManualConfigHtml() const;
  String buildPalettesConfigHtml() const;
};
