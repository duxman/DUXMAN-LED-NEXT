#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "core/CoreState.h"
#include "core/Config.h"
#include "core/ReleaseInfo.h"
#include "services/StorageService.h"
#include "services/WifiService.h"

class ApiService {
public:
  ApiService(CoreState &state, NetworkConfig &networkConfig, GpioConfig &gpioConfig,
             ReleaseInfo &releaseInfo, StorageService &storageService, WifiService &wifiService);

  void begin();
  void handle();

private:
  CoreState &state_;
  NetworkConfig &networkConfig_;
  GpioConfig &gpioConfig_;
  ReleaseInfo &releaseInfo_;
  StorageService &storageService_;
  WifiService &wifiService_;
  String commandBuffer_;
  WebServer httpServer_;

  void processCommand(const String &command);
  void setupHttpRoutes();
  void handleHttpStateRoute();
  void handleHttpNetworkRoute();
  void handleHttpGpioRoute();
  void handleHttpDebugRoute();
  void handleHttpConfigAllRoute();
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
  String buildApiReleaseHtml() const;
  String buildVersionHtml() const;
  String buildNetworkConfigHtml() const;
  String buildGpioConfigHtml() const;
  String buildDebugConfigHtml() const;
  String buildManualConfigHtml() const;
};
