#include <Arduino.h>
#include <LittleFS.h>

#include "services/StorageService.h"

namespace {
constexpr const char *kStatePath = "/state.json";
constexpr const char *kNetworkConfigPath = "/device-config.json";
constexpr const char *kGpioConfigPath = "/gpio-config.json";
} // namespace

StorageService::StorageService(CoreState &state, NetworkConfig &networkConfig,
                               GpioConfig &gpioConfig)
  : state_(state), networkConfig_(networkConfig), gpioConfig_(gpioConfig) {}

void StorageService::begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[storage] LittleFS mount failed");
    state_ = CoreState::defaults();
    networkConfig_ = NetworkConfig::defaults();
    gpioConfig_ = GpioConfig::defaults();
    return;
  }

  load();
}

bool StorageService::save() {
  const bool stateSaved = saveState();
  const bool networkSaved = saveNetworkConfig();
  const bool gpioSaved = saveGpioConfig();
  return stateSaved && networkSaved && gpioSaved;
}

bool StorageService::load() {
  const bool stateLoaded = loadState();
  const bool networkLoaded = loadNetworkConfig();
  const bool gpioLoaded = loadGpioConfig();
  return stateLoaded && networkLoaded && gpioLoaded;
}

bool StorageService::saveNetworkConfig() {
  return writeFile(kNetworkConfigPath, networkConfig_.toJson());
}

bool StorageService::saveGpioConfig() {
  return writeFile(kGpioConfigPath, gpioConfig_.toJson());
}

bool StorageService::loadGpioConfig() {
  String raw;
  if (!readFile(kGpioConfigPath, raw)) {
    gpioConfig_ = GpioConfig::defaults();
    return saveGpioConfig();
  }

  GpioConfig loaded = GpioConfig::defaults();
  String error;
  loaded.applyPatchJson(raw, &error);
  if (!error.isEmpty()) {
    Serial.print("[storage] invalid gpio config: ");
    Serial.println(error);
    gpioConfig_ = GpioConfig::defaults();
    return saveGpioConfig();
  }

  gpioConfig_ = loaded;
  return true;
}

bool StorageService::loadNetworkConfig() {
  String raw;
  if (!readFile(kNetworkConfigPath, raw)) {
    networkConfig_ = NetworkConfig::defaults();
    return saveNetworkConfig();
  }

  NetworkConfig loaded = NetworkConfig::defaults();
  String error;
  loaded.applyPatchJson(raw, &error);
  if (!error.isEmpty()) {
    Serial.print("[storage] invalid network config: ");
    Serial.println(error);
    networkConfig_ = NetworkConfig::defaults();
    return saveNetworkConfig();
  }

  networkConfig_ = loaded;
  return true;
}

bool StorageService::saveState() {
  return writeFile(kStatePath, state_.toJson());
}

bool StorageService::loadState() {
  String raw;
  if (!readFile(kStatePath, raw)) {
    state_ = CoreState::defaults();
    return saveState();
  }

  state_ = CoreState::defaults();
  state_.applyPatchJson(raw);
  return true;
}

bool StorageService::writeFile(const char *path, const String &content) {
  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.print("[storage] open for write failed: ");
    Serial.println(path);
    return false;
  }

  const size_t bytes = file.print(content);
  file.close();
  if (bytes == 0) {
    Serial.print("[storage] write failed: ");
    Serial.println(path);
    return false;
  }

  return true;
}

bool StorageService::readFile(const char *path, String &content) {
  if (!LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.print("[storage] open for read failed: ");
    Serial.println(path);
    return false;
  }

  content = file.readString();
  file.close();
  return !content.isEmpty();
}
