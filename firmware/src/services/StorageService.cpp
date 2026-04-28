/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/StorageService.cpp
 * Last commit: 47b0156 - 2026-04-28
 */

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "services/StorageService.h"

namespace {
constexpr const char *kStatePath = "/state.json";
// Fichero de configuración unificada (network + gpio + microphone + debug)
constexpr const char *kConfigPath = "/config.json";
// Paths legacy para migración automática
constexpr const char *kLegacyNetworkPath = "/device-config.json";
constexpr const char *kLegacyGpioPath    = "/gpio-config.json";
} // namespace

StorageService::StorageService(CoreState &state, NetworkConfig &networkConfig,
                               GpioConfig &gpioConfig, MicrophoneConfig &microphoneConfig,
                               DebugConfig &debugConfig)
  : state_(state), networkConfig_(networkConfig), gpioConfig_(gpioConfig),
    microphoneConfig_(microphoneConfig), debugConfig_(debugConfig) {}

void StorageService::begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[storage] LittleFS mount failed");
    state_          = CoreState::defaults();
    networkConfig_  = NetworkConfig::defaults();
    gpioConfig_     = GpioConfig::defaults();
    microphoneConfig_ = MicrophoneConfig::defaults();
    debugConfig_    = DebugConfig::defaults();
    return;
  }

  load();
}

bool StorageService::save() {
  return saveState() && saveConfig();
}

bool StorageService::load() {
  return loadState() && loadConfig();
}

bool StorageService::saveConfig() {
  // Construye el JSON unificado con todas las secciones de configuración
  JsonDocument doc;

  // network
  JsonDocument netDoc;
  deserializeJson(netDoc, networkConfig_.toJson());
  doc["network"] = netDoc["network"];

  // gpio
  JsonDocument gpioDoc;
  deserializeJson(gpioDoc, gpioConfig_.toJson());
  doc["gpio"] = gpioDoc["gpio"];

  // microphone
  JsonDocument micDoc;
  deserializeJson(micDoc, microphoneConfig_.toJson());
  doc["microphone"] = micDoc["microphone"];

  // debug
  JsonDocument debugDoc;
  deserializeJson(debugDoc, debugConfig_.toJson());
  doc["debug"] = debugDoc["debug"];

  String out;
  serializeJsonPretty(doc, out);
  return writeFile(kConfigPath, out);
}

bool StorageService::loadConfig() {
  // Si no existe config.json, intentar migrar desde ficheros legacy
  if (!LittleFS.exists(kConfigPath)) {
    if (!migrateFromLegacyFiles()) {
      // Sin config previa: usar valores por defecto y persistirlos
      networkConfig_    = NetworkConfig::defaults();
      gpioConfig_       = GpioConfig::defaults();
      microphoneConfig_ = MicrophoneConfig::defaults();
      debugConfig_      = DebugConfig::defaults();
      return saveConfig();
    }
    return true;
  }

  String raw;
  if (!readFile(kConfigPath, raw)) {
    networkConfig_    = NetworkConfig::defaults();
    gpioConfig_       = GpioConfig::defaults();
    microphoneConfig_ = MicrophoneConfig::defaults();
    debugConfig_      = DebugConfig::defaults();
    return saveConfig();
  }

  JsonDocument doc;
  if (deserializeJson(doc, raw)) {
    Serial.println("[storage] config.json parse error, using defaults");
    networkConfig_    = NetworkConfig::defaults();
    gpioConfig_       = GpioConfig::defaults();
    microphoneConfig_ = MicrophoneConfig::defaults();
    debugConfig_      = DebugConfig::defaults();
    return saveConfig();
  }

  // network
  if (!doc["network"].isNull()) {
    String netPayload;
    { JsonDocument d; d["network"] = doc["network"]; serializeJson(d, netPayload); }
    String err;
    NetworkConfig candidate = NetworkConfig::defaults();
    candidate.applyPatchJson(netPayload, &err);
    if (err.isEmpty()) {
      networkConfig_ = candidate;
    } else {
      Serial.print("[storage] config.json network error: "); Serial.println(err);
    }
  }

  // gpio
  if (!doc["gpio"].isNull()) {
    String gpioPayload;
    { JsonDocument d; d["gpio"] = doc["gpio"]; serializeJson(d, gpioPayload); }
    String err;
    GpioConfig candidate = GpioConfig::defaults();
    candidate.applyPatchJson(gpioPayload, &err);
    if (err.isEmpty()) {
      gpioConfig_ = candidate;
    } else {
      Serial.print("[storage] config.json gpio error: "); Serial.println(err);
    }
  }

  // microphone
  if (!doc["microphone"].isNull()) {
    String micPayload;
    { JsonDocument d; d["microphone"] = doc["microphone"]; serializeJson(d, micPayload); }
    String err;
    MicrophoneConfig candidate = MicrophoneConfig::defaults();
    candidate.applyPatchJson(micPayload, &err);
    if (err.isEmpty()) {
      microphoneConfig_ = candidate;
    } else {
      Serial.print("[storage] config.json microphone error: "); Serial.println(err);
    }
  }

  // debug
  if (!doc["debug"].isNull()) {
    String dbgPayload;
    { JsonDocument d; d["debug"] = doc["debug"]; serializeJson(d, dbgPayload); }
    String err;
    DebugConfig candidate = DebugConfig::defaults();
    candidate.applyPatchJson(dbgPayload, &err);
    if (err.isEmpty()) {
      debugConfig_ = candidate;
    } else {
      Serial.print("[storage] config.json debug error: "); Serial.println(err);
    }
  }

  return true;
}

bool StorageService::migrateFromLegacyFiles() {
  bool anyFound = false;

  // Migrar device-config.json (contenía network + debug + microphone)
  if (LittleFS.exists(kLegacyNetworkPath)) {
    String raw;
    if (readFile(kLegacyNetworkPath, raw)) {
      JsonDocument doc;
      if (!deserializeJson(doc, raw)) {
        // network
        if (!doc["network"].isNull()) {
          String p; JsonDocument d; d["network"] = doc["network"]; serializeJson(d, p);
          String err; networkConfig_.applyPatchJson(p, &err);
        }
        // debug (estaba embebido en network legacy)
        if (!doc["debug"].isNull()) {
          String p; JsonDocument d; d["debug"] = doc["debug"]; serializeJson(d, p);
          String err; debugConfig_.applyPatchJson(p, &err);
        }
        // microphone (estaba embebido en network legacy)
        if (!doc["microphone"].isNull()) {
          String p; JsonDocument d; d["microphone"] = doc["microphone"]; serializeJson(d, p);
          String err; microphoneConfig_.applyPatchJson(p, &err);
        }
        anyFound = true;
        Serial.println("[storage] migrated device-config.json → config.json");
      }
    }
  }

  // Migrar gpio-config.json
  if (LittleFS.exists(kLegacyGpioPath)) {
    String raw;
    if (readFile(kLegacyGpioPath, raw)) {
      JsonDocument doc;
      if (!deserializeJson(doc, raw)) {
        if (!doc["gpio"].isNull()) {
          String p; JsonDocument d; d["gpio"] = doc["gpio"]; serializeJson(d, p);
          String err; gpioConfig_.applyPatchJson(p, &err);
        }
        anyFound = true;
        Serial.println("[storage] migrated gpio-config.json → config.json");
      }
    }
  }

  if (anyFound) {
    saveConfig();
    // Eliminar ficheros legacy para evitar remigraciones
    LittleFS.remove(kLegacyNetworkPath);
    LittleFS.remove(kLegacyGpioPath);
  }

  return anyFound;
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


