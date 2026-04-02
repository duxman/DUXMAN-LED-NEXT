#pragma once

#include "core/CoreState.h"
#include "core/Config.h"

class StorageService {
public:
  StorageService(CoreState &state, NetworkConfig &networkConfig,
                 GpioConfig &gpioConfig);

  void begin();
  bool save();
  bool saveState();
  bool load();
  bool saveNetworkConfig();
  bool loadNetworkConfig();
  bool saveGpioConfig();
  bool loadGpioConfig();

private:
  CoreState &state_;
  NetworkConfig &networkConfig_;
  GpioConfig &gpioConfig_;

  bool loadState();
  bool writeFile(const char *path, const String &content);
  bool readFile(const char *path, String &content);
};
