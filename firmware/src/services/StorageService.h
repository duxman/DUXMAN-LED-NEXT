#pragma once

#include "core/CoreState.h"
#include "core/Config.h"
#include "core/ReleaseInfo.h"

class StorageService {
public:
  StorageService(CoreState &state, NetworkConfig &networkConfig, ReleaseInfo &releaseInfo);

  void begin();
  bool save();
  bool load();
  bool saveNetworkConfig();
  bool loadNetworkConfig();
  bool saveReleaseInfo();
  bool loadReleaseInfo();

private:
  CoreState &state_;
  NetworkConfig &networkConfig_;
  ReleaseInfo &releaseInfo_;

  bool saveState();
  bool loadState();
  bool writeFile(const char *path, const String &content);
  bool readFile(const char *path, String &content);
};
