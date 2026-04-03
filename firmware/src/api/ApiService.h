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
#include "services/WatchdogService.h"
#include "services/WifiService.h"

class ApiService {
public:
  ApiService(CoreState &state, NetworkConfig &networkConfig, GpioConfig &gpioConfig,
             StorageService &storageService, WifiService &wifiService,
             PersistenceSchedulerService &persistenceSchedulerService,
             EffectPersistenceService &effectPersistenceService,
             ProfileService &profileService, WatchdogService &watchdogService);

  void begin();
  void handle();

private:
  CoreState &state_;
  NetworkConfig &networkConfig_;
  GpioConfig &gpioConfig_;
  StorageService &storageService_;
  WifiService &wifiService_;
  PersistenceSchedulerService &persistenceSchedulerService_;
  EffectPersistenceService &effectPersistenceService_;
  ProfileService &profileService_;
  WatchdogService &watchdogService_;
  String commandBuffer_;
  WebServer httpServer_;

  void processCommand(const String &command);
  void setupHttpRoutes();
  void handleHttpStateRoute();
  void handleHttpNetworkRoute();
  void handleHttpGpioRoute();
  void handleHttpDebugRoute();
  void handleHttpDiagRoute();
  void handleHttpConfigAllRoute();
  void handleHttpHardwareRoute();
  void handleHttpGpioProfilesRoute();
  void handleHttpGpioProfilesSaveRoute();
  void handleHttpGpioProfilesApplyRoute();
  void handleHttpGpioProfilesDefaultRoute();
  void handleHttpGpioProfilesDeleteRoute();
  void handleHttpEffectsRoute();
  void handleHttpEffectsStartupRoute();
  void handleHttpEffectsSequenceAddRoute();
  void handleHttpEffectsSequenceDeleteRoute();
  void handleHttpRestartRoute();
  String buildFullConfigJson() const;
  String buildOpenApiJson() const;
  String buildHomeHtml() const;
  String buildConfigIndexHtml() const;
  String buildDocsHtml() const;
  String buildApiStateHtml() const;
  String buildApiConfigNetworkHtml() const;
  String buildApiConfigGpioHtml() const;
  String buildApiConfigDebugHtml() const;
  String buildApiConfigAllHtml() const;
  String buildApiHardwareHtml() const;
  String buildApiProfilesHtml() const;
  String buildApiReleaseHtml() const;
  String buildVersionHtml() const;
  String buildNetworkConfigHtml() const;
  String buildGpioConfigHtml() const;
  String buildProfilesConfigHtml() const;
  String buildDebugConfigHtml() const;
  String buildManualConfigHtml() const;
};
