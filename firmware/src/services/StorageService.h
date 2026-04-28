/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/StorageService.h
 * Last commit: 47b0156 - 2026-04-28
 */

#pragma once

#include "core/CoreState.h"
#include "core/Config.h"

// StorageService gestiona la persistencia del estado y la configuración.
// Toda la configuración (network, gpio, microphone, debug) se guarda en un único
// fichero /config.json. El estado de ejecución se guarda en /state.json.
class StorageService {
public:
  StorageService(CoreState &state, NetworkConfig &networkConfig,
                 GpioConfig &gpioConfig, MicrophoneConfig &microphoneConfig,
                 DebugConfig &debugConfig);

  void begin();
  bool save();
  bool saveState();
  bool load();
  bool saveConfig();   // Guarda config unificada en /config.json
  bool loadConfig();   // Carga config unificada desde /config.json

private:
  CoreState &state_;
  NetworkConfig &networkConfig_;
  GpioConfig &gpioConfig_;
  MicrophoneConfig &microphoneConfig_;
  DebugConfig &debugConfig_;

  bool loadState();
  bool migrateFromLegacyFiles();  // Migración desde device-config.json + gpio-config.json
  bool writeFile(const char *path, const String &content);
  bool readFile(const char *path, String &content);
};
