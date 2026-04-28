/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/api/ApiService.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

﻿#include "api/ApiService.h"
#include "core/BuildProfile.h"
#include "core/PaletteRegistry.h"
#include "services/UserPaletteService.h"

#include "effects/EffectRegistry.h"

#include <ArduinoJson.h>
#include <time.h>

namespace {
String buildBootedAtLabel() {
  const time_t now = time(nullptr);
  if (now < 1700000000) {
    return "Arranque: pendiente de hora";
  }

  const time_t bootAt = now - static_cast<time_t>(millis() / 1000UL);
  struct tm timeInfo;
  if (gmtime_r(&bootAt, &timeInfo) == nullptr) {
    return "Arranque: no disponible";
  }

  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", &timeInfo);
  return String("Arranque: ") + buffer;
}

String buildMicrophoneJson(const MicrophoneConfig &microphoneConfig) {
  return microphoneConfig.toJson();
}
} // namespace

ApiService::ApiService(CoreState &state, NetworkConfig &networkConfig, GpioConfig &gpioConfig,
                       MicrophoneConfig &microphoneConfig, DebugConfig &debugConfig,
                       StorageService &storageService, WifiService &wifiService,
                       PersistenceSchedulerService &persistenceSchedulerService,
                       EffectPersistenceService &effectPersistenceService,
                       ProfileService &profileService, UserPaletteService &userPaletteService,
                       WatchdogService &watchdogService)
    : state_(state), networkConfig_(networkConfig), gpioConfig_(gpioConfig),
      microphoneConfig_(microphoneConfig), debugConfig_(debugConfig),
      storageService_(storageService), wifiService_(wifiService),
      persistenceSchedulerService_(persistenceSchedulerService),
      effectPersistenceService_(effectPersistenceService),
      profileService_(profileService), userPaletteService_(userPaletteService),
      watchdogService_(watchdogService), httpServer_(80) {}

void ApiService::begin() {
  setupHttpRoutes();
  httpServer_.begin();

  Serial.println("[api] ready: GET /api/v1/state | PATCH /api/v1/state {json}");
  Serial.println("[api] ready: GET /api/v1/config/network | PATCH /api/v1/config/network {json}");
  Serial.println("[api] ready: GET /api/v1/config/microphone | PATCH /api/v1/config/microphone {json}");
  Serial.println("[api] ready: GET /api/v1/config/gpio | PATCH /api/v1/config/gpio {json}");
  Serial.println("[api] ready: GET /api/v1/profiles | POST /api/v1/profiles/save|apply|default|delete|clone {json} | GET /api/v1/profiles/get?id=");
  Serial.println("[api] ready: GET /api/v1/config/debug | PATCH /api/v1/config/debug {json}");
  Serial.println("[api] ready: GET /api/v1/config/all | POST /api/v1/config/all {json}");
  Serial.println("[api] ready: GET /api/v1/effects | POST /api/v1/effects/startup/save | POST /api/v1/effects/sequence/add|delete");
  Serial.println("[api] ready: GET /api/v1/palettes | POST /api/v1/palettes/apply {json} | POST /api/v1/palettes/save {json} | POST /api/v1/palettes/delete {json}");
  Serial.println("[api] ready: POST /api/v1/system/restart");
  Serial.println("[api] ready: GET /api/v1/hardware");
  Serial.println("[api] ready: GET /api/v1/release");
  Serial.println("[api] ui: GET / | GET /config | GET /config/network | GET /config/microphone | GET /config/gpio | GET /config/palettes | GET /config/debug | GET /config/manual | GET /api | GET /version");

  // ── IP summary ────────────────────────────────────────────────────────────
  {
    const IPAddress staIp = WiFi.localIP();
    const IPAddress apIp  = WiFi.softAPIP();
    Serial.print("[api] STA ip=");
    Serial.print(staIp.toString());
    Serial.print("  AP ip=");
    Serial.println(apIp.toString());
    if (staIp[0] != 0) {
      Serial.print("[api] UI -> http://");
      Serial.println(staIp.toString());
    } else if (apIp[0] != 0) {
      Serial.print("[api] UI -> http://");
      Serial.println(apIp.toString());
    }
  }
}

void ApiService::handle() {
  httpServer_.handleClient();

  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (!commandBuffer_.isEmpty()) {
        processCommand(commandBuffer_);
        commandBuffer_.clear();
      }
      continue;
    }
    commandBuffer_ += c;
  }
}

void ApiService::processCommand(const String &command) {
  if (command == "GET /api/v1/state") {
    Serial.println(state_.toJson());
    return;
  }

  if (command.startsWith("PATCH /api/v1/state ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    const String payload = command.substring(payloadPos);
    const bool changed = state_.applyPatchJson(payload);
    if (changed) {
      persistenceSchedulerService_.requestSaveState();
    }
    Serial.print("{\"updated\":");
    Serial.print(changed ? "true" : "false");
    Serial.print(",\"state\":");
    Serial.print(state_.toJson());
    Serial.println("}");
    return;
  }

  if (command == "GET /api/v1/config/network") {
    Serial.println(networkConfig_.toJson());
    return;
  }

  if (command == "GET /api/v1/config/microphone") {
    Serial.println(buildMicrophoneJson(microphoneConfig_));
    return;
  }

  if (command == "GET /api/v1/config/debug") {
    Serial.print("{\"debug\":{\"enabled\":");
    Serial.print(debugConfig_.enabled ? "true" : "false");
    Serial.print(",\"heartbeatMs\":");
    Serial.print(debugConfig_.heartbeatMs);
    Serial.println("}}");
    return;
  }

  if (command == "GET /api/v1/release") {
    Serial.println(ReleaseInfo::toJson());
    return;
  }

  if (command == "GET /api/v1/hardware") {
    Serial.println(HardwareInfo::toJson());
    return;
  }

  if (command == "GET /api/v1/effects") {
    Serial.println(effectPersistenceService_.toJson());
    return;
  }

  if (command == "GET /api/v1/palettes") {
    Serial.println(userPaletteService_.listAllJson());
    return;
  }

  if (command.startsWith("POST /api/v1/palettes/save ") ||
      command.startsWith("PATCH /api/v1/palettes/save ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }
    String respBody, errMsg;
    if (!userPaletteService_.saveFromJson(command.substring(payloadPos), &respBody, &errMsg)) {
      Serial.print("{\"error\":\""); Serial.print(errMsg); Serial.println("\"}");
      return;
    }
    Serial.println(respBody);
    return;
  }

  if (command.startsWith("POST /api/v1/palettes/delete ") ||
      command.startsWith("PATCH /api/v1/palettes/delete ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }
    String respBody, errMsg;
    if (!userPaletteService_.deleteFromJson(command.substring(payloadPos), &respBody, &errMsg)) {
      Serial.print("{\"error\":\""); Serial.print(errMsg); Serial.println("\"}");
      return;
    }
    Serial.println(respBody);
    return;
  }

  if (command.startsWith("POST /api/v1/palettes/apply ") ||
      command.startsWith("PATCH /api/v1/palettes/apply ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    const String payload = command.substring(payloadPos);
    JsonDocument input;
    if (deserializeJson(input, payload)) {
      Serial.println("{\"error\":\"invalid_json\"}");
      return;
    }

    JsonDocument patch;
    JsonObject root = patch.to<JsonObject>();
    if (!input["paletteId"].isNull()) {
      root["paletteId"] = input["paletteId"];
    } else if (!input["palette"].isNull()) {
      root["palette"] = input["palette"];
    } else {
      Serial.println("{\"error\":\"missing_palette\"}");
      return;
    }

    String patchPayload;
    serializeJson(patch, patchPayload);
    const bool changed = state_.applyPatchJson(patchPayload);
    if (changed) {
      persistenceSchedulerService_.requestSaveState();
    }

    String response = "{\"updated\":";
    response += changed ? "true" : "false";
    response += ",\"state\":";
    response += state_.toJson();
    response += "}";
    Serial.println(response);
    return;
  }

  if (command == "POST /api/v1/system/restart") {
    Serial.println("{\"restart\":true}");
    Serial.flush();
    delay(150);
    ESP.restart();
    return;
  }

  if (command.startsWith("PATCH /api/v1/config/network ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    const String payload = command.substring(payloadPos);
    String error;
    const bool changed = networkConfig_.applyPatchJson(payload, &error);
    if (!error.isEmpty()) {
      Serial.print("{\"error\":\"");
      Serial.print(error);
      Serial.println("\"}");
      return;
    }

    if (changed) {
      persistenceSchedulerService_.requestSaveConfig();
    }

    if (changed && !wifiService_.applyConfig()) {
      Serial.println("{\"error\":\"wifi_apply_failed\"}");
      return;
    }

    Serial.print("{\"updated\":");
    Serial.print(changed ? "true" : "false");
    Serial.print(",\"network\":");
    Serial.print(networkConfig_.toJson());
    Serial.println("}");
    return;
  }

  if (command.startsWith("PATCH /api/v1/config/microphone ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    String error;
    const bool changed = microphoneConfig_.applyPatchJson(command.substring(payloadPos), &error);
    if (!error.isEmpty()) {
      Serial.print("{\"error\":\"");
      Serial.print(error);
      Serial.println("\"}");
      return;
    }

    if (changed) {
      persistenceSchedulerService_.requestSaveConfig();
    }

    Serial.print("{\"updated\":");
    Serial.print(changed ? "true" : "false");
    Serial.print(",\"microphone\":");
    Serial.print(microphoneConfig_.toJson());
    Serial.println("}");
    return;
  }

  if (command == "GET /api/v1/config/gpio") {
    Serial.println(gpioConfig_.toJson());
    return;
  }

  if (command == "GET /api/v1/profiles") {
    Serial.println(profileService_.listProfilesJson());
    return;
  }

  if (command.startsWith("PATCH /api/v1/config/gpio ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    const String payload = command.substring(payloadPos);
    String error;
    const bool changed = gpioConfig_.applyPatchJson(payload, &error);
    if (!error.isEmpty()) {
      Serial.print("{\"error\":\"");
      Serial.print(error);
      Serial.println("\"}");
      return;
    }

    if (changed) {
      persistenceSchedulerService_.requestSaveConfig();
    }

    if (changed) {
      String syncError;
      if (!profileService_.syncDefaultProfileFromActiveConfig(&syncError)) {
        Serial.print("{\"error\":\"");
        Serial.print(syncError);
        Serial.println("\"}");
        return;
      }
      profileService_.applyActiveConfig();
    }

    Serial.print("{\"updated\":");
    Serial.print(changed ? "true" : "false");
    Serial.print(",\"gpio\":");
    Serial.print(gpioConfig_.toJson());
    Serial.println("}");
    return;
  }

  if (command.startsWith("POST /api/v1/profiles/save ") ||
      command.startsWith("PATCH /api/v1/profiles/save ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    String response;
    String error;
    if (!profileService_.saveProfileFromJson(command.substring(payloadPos), &response, &error)) {
      Serial.print("{\"error\":\"");
      Serial.print(error);
      Serial.println("\"}");
      return;
    }

    Serial.println(response);
    return;
  }

  if (command.startsWith("POST /api/v1/profiles/apply ") ||
      command.startsWith("PATCH /api/v1/profiles/apply ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    String response;
    String error;
    if (!profileService_.applyProfileFromJson(command.substring(payloadPos), &response, &error)) {
      Serial.print("{\"error\":\"");
      Serial.print(error);
      Serial.println("\"}");
      return;
    }

    Serial.println(response);
    return;
  }

  if (command.startsWith("POST /api/v1/profiles/default ") ||
      command.startsWith("PATCH /api/v1/profiles/default ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    String response;
    String error;
    if (!profileService_.setDefaultProfileFromJson(command.substring(payloadPos), &response, &error)) {
      Serial.print("{\"error\":\"");
      Serial.print(error);
      Serial.println("\"}");
      return;
    }

    Serial.println(response);
    return;
  }

  if (command.startsWith("POST /api/v1/profiles/delete ") ||
      command.startsWith("PATCH /api/v1/profiles/delete ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    String response;
    String error;
    if (!profileService_.deleteProfileFromJson(command.substring(payloadPos), &response, &error)) {
      Serial.print("{\"error\":\"");
      Serial.print(error);
      Serial.println("\"}");
      return;
    }

    Serial.println(response);
    return;
  }

  if (command.startsWith("PATCH /api/v1/config/debug ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    String error;
    const bool changed = debugConfig_.applyPatchJson(command.substring(payloadPos), &error);
    if (!error.isEmpty()) {
      Serial.print("{\"error\":\"");
      Serial.print(error);
      Serial.println("\"}");
      return;
    }

    if (changed) {
      persistenceSchedulerService_.requestSaveConfig();
    }

    Serial.print("{\"updated\":");
    Serial.print(changed ? "true" : "false");
    Serial.print(",\"debug\":{\"enabled\":");
    Serial.print(debugConfig_.enabled ? "true" : "false");
    Serial.print(",\"heartbeatMs\":");
    Serial.print(debugConfig_.heartbeatMs);
    Serial.println("}}");
    return;
  }

  if (command == "GET /api/v1/config/all") {
    Serial.println(buildFullConfigJson());
    return;
  }

  if (command.startsWith("POST /api/v1/config/all ")) {
    const int payloadPos = command.indexOf('{');
    if (payloadPos < 0) {
      Serial.println("{\"error\":\"invalid_payload\"}");
      return;
    }

    const String payload = command.substring(payloadPos);
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
      Serial.println("{\"error\":\"invalid_json\"}");
      return;
    }

    JsonObjectConst root = doc.as<JsonObjectConst>();

    // Validar en candidatos antes de aplicar
    NetworkConfig netCandidate = networkConfig_;
    GpioConfig gpioCandidate = gpioConfig_;
    MicrophoneConfig micCandidate = microphoneConfig_;
    DebugConfig debugCandidate = debugConfig_;

    String netPayload;
    { JsonDocument d; d["network"] = root["network"]; serializeJson(d, netPayload); }
    String error;
    netCandidate.applyPatchJson(netPayload, &error);
    if (!error.isEmpty()) {
      Serial.print("{\"error\":\"network_"); Serial.print(error); Serial.println("\"}");
      return;
    }

    String gpioPayload;
    { JsonDocument d; d["gpio"] = root["gpio"]; serializeJson(d, gpioPayload); }
    gpioCandidate.applyPatchJson(gpioPayload, &error);
    if (!error.isEmpty()) {
      Serial.print("{\"error\":\"gpio_"); Serial.print(error); Serial.println("\"}");
      return;
    }

    String micPayload;
    { JsonDocument d; d["microphone"] = root["microphone"]; serializeJson(d, micPayload); }
    micCandidate.applyPatchJson(micPayload, &error);
    if (!error.isEmpty()) {
      Serial.print("{\"error\":\"microphone_"); Serial.print(error); Serial.println("\"}");
      return;
    }

    String debugPayload;
    { JsonDocument d; d["debug"] = root["debug"]; serializeJson(d, debugPayload); }
    debugCandidate.applyPatchJson(debugPayload, &error);
    if (!error.isEmpty()) {
      Serial.print("{\"error\":\"debug_"); Serial.print(error); Serial.println("\"}");
      return;
    }

    networkConfig_ = netCandidate;
    gpioConfig_ = gpioCandidate;
    microphoneConfig_ = micCandidate;
    debugConfig_ = debugCandidate;
    persistenceSchedulerService_.requestSaveConfig();
    profileService_.syncDefaultProfileFromActiveConfig();
    wifiService_.applyConfig();

    Serial.print("{\"imported\":true,\"config\":");
    Serial.print(buildFullConfigJson());
    Serial.println("}");
    return;
  }

  Serial.println("{\"error\":\"unknown_command\"}");
}

void ApiService::setupHttpRoutes() {
  httpServer_.on("/", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildHomeHtml());
  });

  httpServer_.on("/config", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildConfigIndexHtml());
  });

  httpServer_.on("/config/network", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildNetworkConfigHtml());
  });

  httpServer_.on("/config/microphone", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildMicrophoneConfigHtml());
  });

  httpServer_.on("/config/debug", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildDebugConfigHtml());
  });

  httpServer_.on("/config/gpio", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildGpioConfigHtml());
  });

  httpServer_.on("/config/profiles", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildProfilesConfigHtml());
  });

  httpServer_.on("/api", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildDocsHtml());
  });

  httpServer_.on("/api/state", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiStateHtml());
  });

  httpServer_.on("/api/config/network", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiConfigNetworkHtml());
  });

  httpServer_.on("/api/config/microphone", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiConfigMicrophoneHtml());
  });

  httpServer_.on("/api/config/debug", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiConfigDebugHtml());
  });

  httpServer_.on("/api/config/gpio", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiConfigGpioHtml());
  });

  httpServer_.on("/api/release", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiReleaseHtml());
  });

  httpServer_.on("/api/hardware", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiHardwareHtml());
  });

  httpServer_.on("/api/profiles", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiProfilesHtml());
  });

  httpServer_.on("/version", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildVersionHtml());
  });

  httpServer_.on("/api/v1/openapi.json", HTTP_GET, [this]() {
    httpServer_.send(200, "application/json", buildOpenApiJson());
  });

  httpServer_.on("/api/v1/state", HTTP_ANY, [this]() {
    handleHttpStateRoute();
  });

  httpServer_.on("/api/v1/config/network", HTTP_ANY, [this]() {
    handleHttpNetworkRoute();
  });

  httpServer_.on("/api/v1/config/microphone", HTTP_ANY, [this]() {
    handleHttpMicrophoneRoute();
  });

  httpServer_.on("/api/v1/config/gpio", HTTP_ANY, [this]() {
    handleHttpGpioRoute();
  });

  httpServer_.on("/api/v1/profiles", HTTP_ANY, [this]() {
    handleHttpProfilesRoute();
  });

  httpServer_.on("/api/v1/profiles/save", HTTP_ANY, [this]() {
    handleHttpProfilesSaveRoute();
  });

  httpServer_.on("/api/v1/profiles/apply", HTTP_ANY, [this]() {
    handleHttpProfilesApplyRoute();
  });

  httpServer_.on("/api/v1/profiles/default", HTTP_ANY, [this]() {
    handleHttpProfilesDefaultRoute();
  });

  httpServer_.on("/api/v1/profiles/delete", HTTP_ANY, [this]() {
    handleHttpProfilesDeleteRoute();
  });

  httpServer_.on("/api/v1/profiles/clone", HTTP_ANY, [this]() {
    handleHttpProfilesCloneRoute();
  });

  httpServer_.on("/api/v1/profiles/get", HTTP_ANY, [this]() {
    handleHttpProfilesGetRoute();
  });

  httpServer_.on("/api/v1/effects", HTTP_ANY, [this]() {
    handleHttpEffectsRoute();
  });

  httpServer_.on("/api/v1/effects/startup/save", HTTP_ANY, [this]() {
    handleHttpEffectsStartupRoute();
  });

  httpServer_.on("/api/v1/effects/sequence/add", HTTP_ANY, [this]() {
    handleHttpEffectsSequenceAddRoute();
  });

  httpServer_.on("/api/v1/effects/sequence/delete", HTTP_ANY, [this]() {
    handleHttpEffectsSequenceDeleteRoute();
  });

  httpServer_.on("/api/v1/palettes", HTTP_ANY, [this]() {
    handleHttpPalettesRoute();
  });

  httpServer_.on("/api/v1/palettes/apply", HTTP_ANY, [this]() {
    handleHttpPalettesApplyRoute();
  });

  httpServer_.on("/api/v1/palettes/save", HTTP_ANY, [this]() {
    handleHttpPalettesSaveRoute();
  });

  httpServer_.on("/api/v1/palettes/delete", HTTP_ANY, [this]() {
    handleHttpPalettesDeleteRoute();
  });

  httpServer_.on("/config/palettes", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildPalettesConfigHtml());
  });

  httpServer_.on("/api/v1/system/restart", HTTP_ANY, [this]() {
    handleHttpRestartRoute();
  });

  httpServer_.on("/api/v1/config/debug", HTTP_ANY, [this]() {
    handleHttpDebugRoute();
  });

  httpServer_.on("/api/v1/diag", HTTP_GET, [this]() {
    handleHttpDiagRoute();
  });

  httpServer_.on("/config/manual", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildManualConfigHtml());
  });

  httpServer_.on("/api/config/all", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiConfigAllHtml());
  });

  httpServer_.on("/api/v1/config/all", HTTP_ANY, [this]() {
    handleHttpConfigAllRoute();
  });

  httpServer_.on("/api/v1/hardware", HTTP_GET, [this]() {
    handleHttpHardwareRoute();
  });

  httpServer_.on("/api/v1/release", HTTP_GET, [this]() {
    httpServer_.send(200, "application/json", ReleaseInfo::toJson());
  });

  httpServer_.onNotFound([this]() {
    JsonDocument doc;
    doc["error"] = "not_found";
    doc["path"] = httpServer_.uri();
    String out;
    serializeJson(doc, out);
    httpServer_.send(404, "application/json", out);
  });
}

void ApiService::handleHttpStateRoute() {
  const HTTPMethod method = httpServer_.method();

  if (method == HTTP_GET) {
    httpServer_.send(200, "application/json", state_.toJson());
    return;
  }

  if (method == HTTP_PATCH || method == HTTP_POST) {
    const String payload = httpServer_.arg("plain");
    if (payload.isEmpty()) {
      httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
      return;
    }

    const bool changed = state_.applyPatchJson(payload);
    if (changed) {
      persistenceSchedulerService_.requestSaveState();
    }
    String response = "{\"updated\":";
    response += changed ? "true" : "false";
    response += ",\"state\":";
    response += state_.toJson();
    response += "}";
    httpServer_.send(200, "application/json", response);
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpNetworkRoute() {
  const HTTPMethod method = httpServer_.method();

  if (method == HTTP_GET) {
    httpServer_.send(200, "application/json", networkConfig_.toJson());
    return;
  }

  if (method == HTTP_PATCH || method == HTTP_POST) {
    const String payload = httpServer_.arg("plain");
    if (payload.isEmpty()) {
      httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
      return;
    }

    String error;
    const bool changed = networkConfig_.applyPatchJson(payload, &error);
    if (!error.isEmpty()) {
      String response = "{\"error\":\"";
      response += error;
      response += "\"}";
      httpServer_.send(400, "application/json", response);
      return;
    }

    if (changed) {
      persistenceSchedulerService_.requestSaveConfig();
    }

    if (changed && !wifiService_.applyConfig()) {
      httpServer_.send(500, "application/json", "{\"error\":\"wifi_apply_failed\"}");
      return;
    }

    String response = "{\"updated\":";
    response += changed ? "true" : "false";
    response += ",\"network\":";
    response += networkConfig_.toJson();
    response += "}";
    httpServer_.send(200, "application/json", response);
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpMicrophoneRoute() {
  const HTTPMethod method = httpServer_.method();

  if (method == HTTP_GET) {
    httpServer_.send(200, "application/json", buildMicrophoneJson(microphoneConfig_));
    return;
  }

  if (method == HTTP_PATCH || method == HTTP_POST) {
    const String payload = httpServer_.arg("plain");
    if (payload.isEmpty()) {
      httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
      return;
    }

    String error;
    const bool changed = microphoneConfig_.applyPatchJson(payload, &error);
    if (!error.isEmpty()) {
      String response = "{\"error\":\"";
      response += error;
      response += "\"}";
      httpServer_.send(400, "application/json", response);
      return;
    }

    if (changed) {
      persistenceSchedulerService_.requestSaveConfig();
    }

    JsonDocument responseDoc;
    responseDoc["updated"] = changed;
    { JsonDocument d; deserializeJson(d, microphoneConfig_.toJson()); responseDoc["microphone"] = d["microphone"]; }
    String response;
    serializeJson(responseDoc, response);
    httpServer_.send(200, "application/json", response);
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpGpioRoute() {
  const HTTPMethod method = httpServer_.method();

  if (method == HTTP_GET) {
    httpServer_.send(200, "application/json", gpioConfig_.toJson());
    return;
  }

  if (method == HTTP_PATCH || method == HTTP_POST) {
    const String payload = httpServer_.arg("plain");
    if (payload.isEmpty()) {
      httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
      return;
    }

    String error;
    const bool changed = gpioConfig_.applyPatchJson(payload, &error);
    if (!error.isEmpty()) {
      String response = "{\"error\":\"";
      response += error;
      response += "\"}";
      httpServer_.send(400, "application/json", response);
      return;
    }

    if (changed) {
      persistenceSchedulerService_.requestSaveConfig();
    }

    if (changed) {
      String syncError;
      if (!profileService_.syncDefaultProfileFromActiveConfig(&syncError)) {
        String response = "{\"error\":\"";
        response += syncError;
        response += "\"}";
        httpServer_.send(500, "application/json", response);
        return;
      }
      profileService_.applyActiveConfig();
    }

    String response = "{\"updated\":";
    response += changed ? "true" : "false";
    response += ",\"gpio\":";
    response += gpioConfig_.toJson();
    response += "}";
    httpServer_.send(200, "application/json", response);
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpProfilesRoute() {
  if (httpServer_.method() == HTTP_GET) {
    httpServer_.send(200, "application/json", profileService_.listProfilesJson());
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpProfilesSaveRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String payload = httpServer_.arg("plain");
  if (payload.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
    return;
  }

  String response;
  String error;
  if (!profileService_.saveProfileFromJson(payload, &response, &error)) {
    String out = "{\"error\":\"";
    out += error;
    out += "\"}";
    httpServer_.send(400, "application/json", out);
    return;
  }

  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpProfilesApplyRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String payload = httpServer_.arg("plain");
  if (payload.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
    return;
  }

  String response;
  String error;
  if (!profileService_.applyProfileFromJson(payload, &response, &error)) {
    String out = "{\"error\":\"";
    out += error;
    out += "\"}";
    httpServer_.send(400, "application/json", out);
    return;
  }

  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpProfilesDefaultRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String payload = httpServer_.arg("plain");
  if (payload.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
    return;
  }

  String response;
  String error;
  if (!profileService_.setDefaultProfileFromJson(payload, &response, &error)) {
    String out = "{\"error\":\"";
    out += error;
    out += "\"}";
    httpServer_.send(400, "application/json", out);
    return;
  }

  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpProfilesDeleteRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String payload = httpServer_.arg("plain");
  if (payload.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
    return;
  }

  String response;
  String error;
  if (!profileService_.deleteProfileFromJson(payload, &response, &error)) {
    String out = "{\"error\":\"";
    out += error;
    out += "\"}";
    httpServer_.send(400, "application/json", out);
    return;
  }

  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpProfilesCloneRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String payload = httpServer_.arg("plain");
  if (payload.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
    return;
  }

  String response;
  String error;
  if (!profileService_.cloneProfileFromJson(payload, &response, &error)) {
    String out = "{\"error\":\"";
    out += error;
    out += "\"}";
    httpServer_.send(400, "application/json", out);
    return;
  }

  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpProfilesGetRoute() {
  if (httpServer_.method() != HTTP_GET) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String id = httpServer_.arg("id");
  if (id.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"missing_id\"}");
    return;
  }

  const String response = profileService_.getProfileConfigJson(id);
  if (response.isEmpty()) {
    httpServer_.send(404, "application/json", "{\"error\":\"profile_not_found\"}");
    return;
  }

  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpEffectsRoute() {
  if (httpServer_.method() == HTTP_GET) {
    httpServer_.send(200, "application/json", effectPersistenceService_.toJson());
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpEffectsStartupRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  String error;
  if (!effectPersistenceService_.saveStartupFromCurrent(&error)) {
    String response = "{\"error\":\"";
    response += error;
    response += "\"}";
    httpServer_.send(500, "application/json", response);
    return;
  }

  String response = "{\"saved\":true,\"effects\":";
  response += effectPersistenceService_.toJson();
  response += "}";
  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpEffectsSequenceAddRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  uint16_t durationSec = 30;
  const String payload = httpServer_.arg("plain");
  if (!payload.isEmpty()) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
      httpServer_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
      return;
    }
    durationSec = constrain(doc["durationSec"] | 30, 1, 3600);
  }

  uint16_t createdId = 0;
  String error;
  if (!effectPersistenceService_.addCurrentToSequence(durationSec, &createdId, &error)) {
    String response = "{\"error\":\"";
    response += error;
    response += "\"}";
    httpServer_.send(400, "application/json", response);
    return;
  }

  String response = "{\"added\":true,\"id\":";
  response += createdId;
  response += ",\"effects\":";
  response += effectPersistenceService_.toJson();
  response += "}";
  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpEffectsSequenceDeleteRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String payload = httpServer_.arg("plain");
  if (payload.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }

  const uint16_t entryId = doc["id"] | 0;
  if (entryId == 0) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_id\"}");
    return;
  }

  String error;
  if (!effectPersistenceService_.deleteSequenceEntry(entryId, &error)) {
    String response = "{\"error\":\"";
    response += error;
    response += "\"}";
    httpServer_.send(400, "application/json", response);
    return;
  }

  String response = "{\"deleted\":true,\"effects\":";
  response += effectPersistenceService_.toJson();
  response += "}";
  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpPalettesRoute() {
  if (httpServer_.method() == HTTP_GET) {
    httpServer_.send(200, "application/json", userPaletteService_.listAllJson());
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpPalettesApplyRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String payload = httpServer_.arg("plain");
  if (payload.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
    return;
  }

  JsonDocument input;
  if (deserializeJson(input, payload)) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }

  JsonDocument patch;
  JsonObject root = patch.to<JsonObject>();
  if (!input["paletteId"].isNull()) {
    root["paletteId"] = input["paletteId"];
  } else if (!input["palette"].isNull()) {
    root["palette"] = input["palette"];
  } else {
    httpServer_.send(400, "application/json", "{\"error\":\"missing_palette\"}");
    return;
  }

  String patchPayload;
  serializeJson(patch, patchPayload);
  const bool changed = state_.applyPatchJson(patchPayload);
  if (changed) {
    persistenceSchedulerService_.requestSaveState();
  }

  String response = "{\"updated\":";
  response += changed ? "true" : "false";
  response += ",\"state\":";
  response += state_.toJson();
  response += "}";
  httpServer_.send(200, "application/json", response);
}

void ApiService::handleHttpPalettesSaveRoute() {
  if (httpServer_.method() != HTTP_POST) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String payload = httpServer_.arg("plain");
  if (payload.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
    return;
  }

  String response;
  String error;
  const bool ok = userPaletteService_.saveFromJson(payload, &response, &error);
  if (!ok) {
    String errJson = "{\"error\":\"";
    errJson += error;
    errJson += "\"}";
    httpServer_.send(400, "application/json", errJson);
    return;
  }
  httpServer_.send(200, "application/json", response.isEmpty() ? "{\"saved\":true}" : response);
}

void ApiService::handleHttpPalettesDeleteRoute() {
  if (httpServer_.method() != HTTP_POST) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  const String payload = httpServer_.arg("plain");
  if (payload.isEmpty()) {
    httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
    return;
  }

  String response;
  String error;
  const bool ok = userPaletteService_.deleteFromJson(payload, &response, &error);
  if (!ok) {
    String errJson = "{\"error\":\"";
    errJson += error;
    errJson += "\"}";
    httpServer_.send(400, "application/json", errJson);
    return;
  }
  httpServer_.send(200, "application/json", response.isEmpty() ? "{\"deleted\":true}" : response);
}

void ApiService::handleHttpRestartRoute() {
  const HTTPMethod method = httpServer_.method();
  if (method != HTTP_POST && method != HTTP_PATCH) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  httpServer_.send(200, "application/json", "{\"restart\":true}");
  delay(150);
  ESP.restart();
}

void ApiService::handleHttpDebugRoute() {
  const HTTPMethod method = httpServer_.method();

  if (method == HTTP_GET) {
    String response = "{\"debug\":{\"enabled\":";
    response += debugConfig_.enabled ? "true" : "false";
    response += ",\"heartbeatMs\":";
    response += debugConfig_.heartbeatMs;
    response += "}}";
    httpServer_.send(200, "application/json", response);
    return;
  }

  if (method == HTTP_PATCH || method == HTTP_POST) {
    const String payload = httpServer_.arg("plain");
    if (payload.isEmpty()) {
      httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
      return;
    }

    String error;
    const bool changed = debugConfig_.applyPatchJson(payload, &error);
    if (!error.isEmpty()) {
      String response = "{\"error\":\"";
      response += error;
      response += "\"}";
      httpServer_.send(400, "application/json", response);
      return;
    }

    if (changed) {
      persistenceSchedulerService_.requestSaveConfig();
    }

    String response = "{\"updated\":";
    response += changed ? "true" : "false";
    response += ",\"debug\":{\"enabled\":";
    response += debugConfig_.enabled ? "true" : "false";
    response += ",\"heartbeatMs\":";
    response += debugConfig_.heartbeatMs;
    response += "}}";
    httpServer_.send(200, "application/json", response);
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpDiagRoute() {
  // GET /api/v1/diag - System diagnostics (watchdog, uptime, memory)
  if (httpServer_.method() != HTTP_GET) {
    httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
    return;
  }

  JsonDocument doc;
  doc["ok"] = true;

  // Uptime in seconds
  doc["uptime_s"] = millis() / 1000;

  // Free heap memory in bytes
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t totalHeap = ESP.getHeapSize();
  doc["heap_free_bytes"] = freeHeap;
  doc["heap_total_bytes"] = totalHeap;
  doc["heap_percent_free"] = (freeHeap * 100) / totalHeap;

  // Watchdog stats
  const WatchdogService::Stats watchdogStats = watchdogService_.getStats();
  doc["watchdog"]["reset_count"] = watchdogStats.resetCount;
  doc["watchdog"]["control_task_resets"] = watchdogStats.controlTaskResets;
  doc["watchdog"]["render_task_resets"] = watchdogStats.renderTaskResets;
  doc["watchdog"]["last_reset_ms"] = watchdogStats.lastResetTimeMs;

  // Task count (simple, no detailed task info)
  doc["task_count"] = uxTaskGetNumberOfTasks();

  String response;
  serializeJson(doc, response);
  httpServer_.send(200, "application/json", response);
}

String ApiService::buildFullConfigJson() const {
  JsonDocument doc;

  { JsonDocument d; deserializeJson(d, networkConfig_.toJson());    doc["network"]    = d["network"]; }
  { JsonDocument d; deserializeJson(d, gpioConfig_.toJson());       doc["gpio"]       = d["gpio"]; }
  { JsonDocument d; deserializeJson(d, microphoneConfig_.toJson()); doc["microphone"] = d["microphone"]; }
  { JsonDocument d; deserializeJson(d, debugConfig_.toJson());      doc["debug"]      = d["debug"]; }

  String out;
  serializeJsonPretty(doc, out);
  return out;
}

void ApiService::handleHttpConfigAllRoute() {
  const HTTPMethod method = httpServer_.method();

  if (method == HTTP_GET) {
    httpServer_.send(200, "application/json", buildFullConfigJson());
    return;
  }

  if (method == HTTP_POST) {
    const String payload = httpServer_.arg("plain");
    if (payload.isEmpty()) {
      httpServer_.send(400, "application/json", "{\"error\":\"invalid_payload\"}");
      return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
      httpServer_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
      return;
    }

    JsonObjectConst root = doc.as<JsonObjectConst>();

    // Validar todo en candidatos antes de tocar la config real
    NetworkConfig netCandidate = networkConfig_;
    GpioConfig gpioCandidate = gpioConfig_;
    MicrophoneConfig micCandidate = microphoneConfig_;
    DebugConfig debugCandidate = debugConfig_;

    String netPayload;
    { JsonDocument d; d["network"] = root["network"]; serializeJson(d, netPayload); }
    String error;
    netCandidate.applyPatchJson(netPayload, &error);
    if (!error.isEmpty()) {
      String response = "{\"error\":\"network_";
      response += error;
      response += "\"}";
      httpServer_.send(400, "application/json", response);
      return;
    }

    String gpioPayload;
    { JsonDocument d; d["gpio"] = root["gpio"]; serializeJson(d, gpioPayload); }
    gpioCandidate.applyPatchJson(gpioPayload, &error);
    if (!error.isEmpty()) {
      String response = "{\"error\":\"gpio_";
      response += error;
      response += "\"}";
      httpServer_.send(400, "application/json", response);
      return;
    }

    String micPayload;
    { JsonDocument d; d["microphone"] = root["microphone"]; serializeJson(d, micPayload); }
    micCandidate.applyPatchJson(micPayload, &error);
    if (!error.isEmpty()) {
      String response = "{\"error\":\"microphone_";
      response += error;
      response += "\"}";
      httpServer_.send(400, "application/json", response);
      return;
    }

    String debugPayload;
    { JsonDocument d; d["debug"] = root["debug"]; serializeJson(d, debugPayload); }
    debugCandidate.applyPatchJson(debugPayload, &error);
    if (!error.isEmpty()) {
      String response = "{\"error\":\"debug_";
      response += error;
      response += "\"}";
      httpServer_.send(400, "application/json", response);
      return;
    }

    // Todo valido -- aplicar
    networkConfig_ = netCandidate;
    gpioConfig_ = gpioCandidate;
    microphoneConfig_ = micCandidate;
    debugConfig_ = debugCandidate;

    persistenceSchedulerService_.requestSaveConfig();

    String syncError;
    if (!profileService_.syncDefaultProfileFromActiveConfig(&syncError)) {
      String response = "{\"error\":\"";
      response += syncError;
      response += "\"}";
      httpServer_.send(500, "application/json", response);
      return;
    }

    wifiService_.applyConfig();
    profileService_.applyActiveConfig();

    String response = "{\"imported\":true,\"config\":";
    response += buildFullConfigJson();
    response += "}";
    httpServer_.send(200, "application/json", response);
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpHardwareRoute() {
  if (httpServer_.method() == HTTP_GET) {
    httpServer_.send(200, "application/json", HardwareInfo::toJson());
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

String ApiService::buildOpenApiJson() const {
  JsonDocument doc;
  doc["openapi"] = "3.0.0-lite";
  doc["info"]["title"] = "DUXMAN-LED-NEXT API";
  doc["info"]["version"] = "v1";
  doc["info"]["description"] = "Documentacion embebida en firmware para HTTP y comandos Serial.";

  JsonObject servers = doc["servers"][0].to<JsonObject>();
  servers["url"] = "/";

  JsonObject paths = doc["paths"].to<JsonObject>();

  JsonObject statePath = paths["/api/v1/state"].to<JsonObject>();
  statePath["get"]["summary"] = "Obtener estado actual";
  statePath["patch"]["summary"] = "Actualizar estado con JSON parcial";
  statePath["post"]["summary"] = "Alias de PATCH para clientes limitados";

  JsonObject netPath = paths["/api/v1/config/network"].to<JsonObject>();
  netPath["get"]["summary"] = "Obtener configuracion de red";
  netPath["patch"]["summary"] = "Actualizar configuracion de red, DNS STA y NTP con JSON parcial";
  netPath["post"]["summary"] = "Alias de PATCH para clientes limitados";

  JsonObject micPath = paths["/api/v1/config/microphone"].to<JsonObject>();
  micPath["get"]["summary"] = "Obtener configuracion de microfono (I2S)";
  micPath["patch"]["summary"] = "Actualizar configuracion de microfono con JSON parcial";
  micPath["post"]["summary"] = "Alias de PATCH para clientes limitados";

  JsonObject debugPath = paths["/api/v1/config/debug"].to<JsonObject>();
  debugPath["get"]["summary"] = "Obtener configuracion de debug";
  debugPath["patch"]["summary"] = "Actualizar debug (enabled y heartbeatMs)";
  debugPath["post"]["summary"] = "Alias de PATCH para clientes limitados";

  JsonObject gpioPath = paths["/api/v1/config/gpio"].to<JsonObject>();
  gpioPath["get"]["summary"] = "Obtener configuracion GPIO";
  gpioPath["patch"]["summary"] = "Actualizar GPIO (outputs: pin, ledCount, ledType, colorOrder)";
  gpioPath["post"]["summary"] = "Alias de PATCH para clientes limitados";

  JsonObject profilesPath = paths["/api/v1/profiles"].to<JsonObject>();
  profilesPath["get"]["summary"] = "Listar perfiles guardados e integrados";

  JsonObject profilesSavePath = paths["/api/v1/profiles/save"].to<JsonObject>();
  profilesSavePath["post"]["summary"] = "Guardar o actualizar un perfil de usuario";
  profilesSavePath["patch"]["summary"] = "Alias de POST para guardar perfil GPIO";

  JsonObject profilesApplyPath = paths["/api/v1/profiles/apply"].to<JsonObject>();
  profilesApplyPath["post"]["summary"] = "Aplicar un perfil y copiarlo en DEFAULT";
  profilesApplyPath["patch"]["summary"] = "Alias de POST para aplicar perfil GPIO";

  JsonObject profilesDefaultPath = paths["/api/v1/profiles/default"].to<JsonObject>();
  profilesDefaultPath["post"]["summary"] = "Alias de aplicar: copia el perfil elegido en DEFAULT";
  profilesDefaultPath["patch"]["summary"] = "Alias de POST para copiar el perfil en DEFAULT";

  JsonObject profilesDeletePath = paths["/api/v1/profiles/delete"].to<JsonObject>();
  profilesDeletePath["post"]["summary"] = "Eliminar un perfil de usuario";
  profilesDeletePath["patch"]["summary"] = "Alias de POST para eliminar perfil";

  JsonObject profilesClonePath = paths["/api/v1/profiles/clone"].to<JsonObject>();
  profilesClonePath["post"]["summary"] = "Clonar un perfil existente con nuevo id y nombre";

  JsonObject profilesGetPath = paths["/api/v1/profiles/get"].to<JsonObject>();
  profilesGetPath["get"]["summary"] = "Obtener la configuracion completa de un perfil por id";

  JsonObject restartPath = paths["/api/v1/system/restart"].to<JsonObject>();
  restartPath["post"]["summary"] = "Reiniciar la placa";
  restartPath["patch"]["summary"] = "Alias de POST para reiniciar la placa";

  JsonObject palettesPath = paths["/api/v1/palettes"].to<JsonObject>();
  palettesPath["get"]["summary"] = "Listar paletas predefinidas de 3 colores";

  JsonObject palettesApplyPath = paths["/api/v1/palettes/apply"].to<JsonObject>();
  palettesApplyPath["post"]["summary"] = "Aplicar paleta por id o key al estado actual";
  palettesApplyPath["patch"]["summary"] = "Alias de POST para aplicar paleta";

  JsonObject releasePath = paths["/api/v1/release"].to<JsonObject>();
  releasePath["get"]["summary"] = "Obtener metadatos de release";

  JsonObject hardwarePath = paths["/api/v1/hardware"].to<JsonObject>();
  hardwarePath["get"]["summary"] = "Obtener capacidades basicas de hardware detectadas en runtime";

  JsonObject homePath = paths["/"].to<JsonObject>();
  homePath["get"]["summary"] = "Pagina principal";

  JsonObject configPath = paths["/config"].to<JsonObject>();
  configPath["get"]["summary"] = "Indice de configuracion";

  JsonObject configUiPath = paths["/config/network"].to<JsonObject>();
  configUiPath["get"]["summary"] = "UI de configuracion de red";

  JsonObject configMicUiPath = paths["/config/microphone"].to<JsonObject>();
  configMicUiPath["get"]["summary"] = "UI de configuracion de microfono";

  JsonObject configDebugUiPath = paths["/config/debug"].to<JsonObject>();
  configDebugUiPath["get"]["summary"] = "UI de configuracion debug";

  JsonObject configGpioUiPath = paths["/config/gpio"].to<JsonObject>();
  configGpioUiPath["get"]["summary"] = "UI de configuracion GPIO";

  JsonObject configProfilesUiPath = paths["/config/profiles"].to<JsonObject>();
  configProfilesUiPath["get"]["summary"] = "UI de gestion de perfiles GPIO";

  JsonObject apiUiPath = paths["/api"].to<JsonObject>();
  apiUiPath["get"]["summary"] = "Indice de UI API";

  JsonObject apiStateUiPath = paths["/api/state"].to<JsonObject>();
  apiStateUiPath["get"]["summary"] = "UI API para estado";

  JsonObject apiConfigUiPath = paths["/api/config/network"].to<JsonObject>();
  apiConfigUiPath["get"]["summary"] = "UI API para configuracion de red";

  JsonObject apiConfigMicUiPath = paths["/api/config/microphone"].to<JsonObject>();
  apiConfigMicUiPath["get"]["summary"] = "UI API para configuracion de microfono";

  JsonObject apiConfigDebugUiPath = paths["/api/config/debug"].to<JsonObject>();
  apiConfigDebugUiPath["get"]["summary"] = "UI API para configuracion debug";

  JsonObject apiConfigGpioUiPath = paths["/api/config/gpio"].to<JsonObject>();
  apiConfigGpioUiPath["get"]["summary"] = "UI API para configuracion GPIO";

  JsonObject apiProfilesUiPath = paths["/api/profiles"].to<JsonObject>();
  apiProfilesUiPath["get"]["summary"] = "UI API para perfiles";

  JsonObject apiHardwareUiPath = paths["/api/hardware"].to<JsonObject>();
  apiHardwareUiPath["get"]["summary"] = "UI API para hardware runtime";

  JsonObject configAllPath = paths["/api/v1/config/all"].to<JsonObject>();
  configAllPath["get"]["summary"] = "Exportar toda la configuracion como JSON";
  configAllPath["post"]["summary"] = "Importar configuracion completa (valida antes de aplicar)";

  JsonObject configManualUiPath = paths["/config/manual"].to<JsonObject>();
  configManualUiPath["get"]["summary"] = "UI de edicion manual de configuracion";

  JsonObject apiConfigAllUiPath = paths["/api/config/all"].to<JsonObject>();
  apiConfigAllUiPath["get"]["summary"] = "UI API para configuracion completa";

  JsonObject apiReleaseUiPath = paths["/api/release"].to<JsonObject>();
  apiReleaseUiPath["get"]["summary"] = "UI API para release";

  JsonObject versionUiPath = paths["/version"].to<JsonObject>();
  versionUiPath["get"]["summary"] = "UI de informacion de version";

  JsonObject specPath = paths["/api/v1/openapi.json"].to<JsonObject>();
  specPath["get"]["summary"] = "Especificacion OpenAPI-like";

  JsonArray serialCommands = doc["x-serialCommands"].to<JsonArray>();
  serialCommands.add("GET /api/v1/state");
  serialCommands.add("PATCH /api/v1/state {json}");
  serialCommands.add("GET /api/v1/config/network");
  serialCommands.add("PATCH /api/v1/config/network {\"network\":{\"wifi\":{\"mode\":\"sta\",\"connection\":{\"ssid\":\"MiWiFi\",\"password\":\"secreto\"}},\"ip\":{\"sta\":{\"mode\":\"dhcp\",\"primaryDns\":\"8.8.8.8\",\"secondaryDns\":\"1.1.1.1\"}},\"dns\":{\"hostname\":\"duxman-led\"},\"time\":{\"syncOnBoot\":true,\"ntpServer\":\"europe.pool.ntp.org\"}}}");
  serialCommands.add("GET /api/v1/config/microphone");
  serialCommands.add("PATCH /api/v1/config/microphone {\"microphone\":{\"enabled\":false,\"source\":\"generic_i2c\",\"profileId\":\"gledopto_gl_c_017wl_d\",\"sampleRate\":16000,\"fftSize\":512,\"gainPercent\":100,\"noiseFloorPercent\":8,\"pins\":{\"bclk\":21,\"ws\":5,\"din\":26}}}");
  serialCommands.add("GET /api/v1/config/debug");
  serialCommands.add("PATCH /api/v1/config/debug {\"debug\":{\"enabled\":true,\"heartbeatMs\":5000}}");
  serialCommands.add("GET /api/v1/config/gpio");
  serialCommands.add("PATCH /api/v1/config/gpio {\"gpio\":{\"outputs\":[{\"pin\":8,\"ledCount\":60,\"ledType\":\"ws2812b\",\"colorOrder\":\"GRB\"}]}}");
  serialCommands.add("GET /api/v1/profiles/gpio");
  serialCommands.add("POST /api/v1/profiles/gpio/save {\"profile\":{\"id\":\"mi_perfil\",\"name\":\"Mi perfil\",\"gpio\":{\"outputs\":[{\"pin\":8,\"ledCount\":60,\"ledType\":\"ws2812b\",\"colorOrder\":\"GRB\"}]},\"applyNow\":true}} ");
  serialCommands.add("POST /api/v1/profiles/gpio/apply {\"profile\":{\"id\":\"gledopto_gl_c_017wl_d\"}} ");
  serialCommands.add("POST /api/v1/profiles/gpio/default {\"profile\":{\"id\":\"gledopto_gl_c_017wl_d\"}} ");
  serialCommands.add("POST /api/v1/profiles/gpio/delete {\"profile\":{\"id\":\"mi_perfil\"}} ");
  serialCommands.add("GET /api/v1/palettes");
  serialCommands.add("POST /api/v1/palettes/apply {\"palette\":\"sunset_drive\"}");
  serialCommands.add("POST /api/v1/system/restart");
  serialCommands.add("GET /api/v1/config/all");
  serialCommands.add("POST /api/v1/config/all {json_completo}");
  serialCommands.add("GET /api/v1/hardware");
  serialCommands.add("GET /api/v1/release");

  String out;
  serializeJsonPretty(doc, out);
  return out;
}

// ---------------------------------------------------------------------------
// Shared navigation bar — included verbatim in every page builder.
// CSS uses .gen-* prefix to avoid collision with per-page styles.
// ---------------------------------------------------------------------------
String ApiService::buildCommonCss() const {
  return R"CSS(
<style>
  :root{--bg:#0d1b22;--card:#132530;--card2:#0f2028;--line:#1e3a48;--text:#d8edf5;--muted:#7aacbf;--accent:#0fd0e0;--ok:#22d68a;--warn:#f5a623;--danger:#f87171;}
  *{box-sizing:border-box;}
  body{margin:0;min-height:100vh;background:var(--bg);color:var(--text);font-family:Trebuchet MS,Segoe UI,sans-serif;}
  main{padding:20px;}
  h1{margin:0 0 10px;font-size:26px;}
  h2{margin:0 0 10px;font-size:18px;}
  h3{margin:0 0 8px;font-size:14px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);}
  p{margin:0;color:var(--muted);}
  a{color:var(--accent);}
  .link{color:var(--accent);font-size:12px;text-decoration:none;word-break:break-all;}
  .link:hover{text-decoration:underline;}
  /* ── Cards ── */
  .card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;margin-bottom:12px;box-shadow:0 8px 28px rgba(0,0,0,.2);}
  /* ── Buttons ── */
  .btn,button{border:0;border-radius:8px;padding:9px 14px;font-size:14px;font-weight:600;cursor:pointer;color:var(--bg);background:var(--accent);transition:opacity .2s;}
  .btn.alt,button.alt,button[class~=alt]{background:#1a8fa0;color:#fff;}
  .btn.ghost,.btn-ghost{background:rgba(255,255,255,.1);color:var(--text);}
  .btn.danger,.btn-danger,button.danger{background:var(--danger);color:#fff;}
  .btn.sm,.btn-sm{padding:5px 10px;font-size:12px;}
  .btn-primary{background:var(--accent);color:var(--bg);}
  .btn:hover,button:hover{opacity:.85;}
  /* ── Forms ── */
  .field{margin-bottom:10px;}
  label{display:block;font-size:13px;color:var(--muted);margin-bottom:4px;}
  input,select,textarea{width:100%;border:1px solid var(--line);border-radius:8px;padding:8px 10px;font-size:14px;background:var(--card2);color:var(--text);box-sizing:border-box;font-family:inherit;}
  input[type=color]{min-height:40px;padding:3px;cursor:pointer;}
  input[type=range]{padding:0;background:transparent;}
  input[type=checkbox]{width:auto;}
  input[type=file]{font-size:14px;color:var(--text);}
  input:focus,select:focus,textarea:focus{outline:1px solid var(--accent);}
  /* ── Code / Pre ── */
  pre{margin:10px 0;min-height:80px;white-space:pre-wrap;border:1px solid var(--line);border-radius:8px;background:var(--card2);color:var(--text);padding:10px;overflow:auto;font-family:Consolas,Courier New,monospace;font-size:12px;line-height:1.45;}
  code{font-family:Consolas,monospace;background:var(--card2);border-radius:4px;padding:1px 4px;}
  /* ── Tables ── */
  table{width:100%;border-collapse:collapse;font-size:13px;margin-top:8px;}
  th,td{text-align:left;padding:6px 8px;border-bottom:1px solid var(--line);}
  th{color:var(--muted);font-weight:600;}
  /* ── Layout helpers ── */
  .row{display:flex;flex-wrap:wrap;gap:10px;}
  .col{flex:1 1 180px;}
  .actions{display:flex;flex-wrap:wrap;gap:10px;margin-top:10px;}
  .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:14px;}
  /* Version page key-value rows */
  .kv-row{display:flex;justify-content:space-between;align-items:baseline;padding:5px 0;border-bottom:1px solid rgba(255,255,255,.04);}
  .kv-row:last-child{border-bottom:none;}
  .lbl{font-size:13px;color:var(--muted);}
  .val{font-size:14px;font-weight:600;color:var(--text);text-align:right;font-family:Consolas,monospace;}
  .val.accent{color:var(--accent);}.val.ok{color:var(--ok);}.val.warn{color:var(--warn);}
  /* ── Feedback ── */
  .hint{font-size:13px;color:var(--muted);}
  .ok{color:var(--ok);}
  .err{color:var(--danger);}
  .warn{background:rgba(245,166,35,.1);border:1px solid rgba(245,166,35,.3);border-radius:8px;padding:10px;margin-top:8px;font-size:13px;color:var(--warn);}
  .badge{display:inline-block;font-size:11px;padding:2px 8px;border-radius:99px;background:rgba(15,208,224,.15);color:var(--accent);border:1px solid rgba(15,208,224,.3);}
  .badge-sys{background:rgba(34,214,138,.12);color:var(--ok);border:1px solid rgba(34,214,138,.3);}
  .badge-user{background:rgba(15,208,224,.12);color:var(--accent);border:1px solid rgba(15,208,224,.3);}
  .badge.builtin{background:rgba(255,255,255,.08);color:var(--muted);border:1px solid var(--line);}
  .badge.default{background:rgba(15,208,224,.2);color:var(--accent);}
  .loader{text-align:center;color:var(--muted);padding:30px;}
  /* ── Progress bars ── */
  .bar-wrap{background:rgba(255,255,255,.06);border-radius:4px;height:6px;width:100%;margin-top:4px;}
  .bar-fill{height:6px;border-radius:4px;background:linear-gradient(90deg,var(--ok),var(--accent));transition:width .4s;}
  /* ── Connectivity dots ── */
  .dot{width:8px;height:8px;border-radius:50%;display:inline-block;}
  .dot.on{background:var(--ok);}.dot.off{background:#444;}
  .chip-bool{display:inline-flex;align-items:center;gap:4px;font-size:12px;}
  /* ── Hero ── */
  .hero{display:flex;flex-direction:column;align-items:center;text-align:center;margin-bottom:24px;}
  .hero img{max-width:220px;margin-bottom:16px;}
  .hero h1{font-size:26px;margin:0 0 4px;color:var(--accent);}
  .hero .tagline{font-size:14px;color:var(--muted);margin:0;}
  .hero-title{display:flex;align-items:baseline;gap:12px;flex-wrap:wrap;}
  .boot-at{display:inline-flex;align-items:center;padding:2px 8px;border-radius:999px;background:rgba(15,208,224,.1);color:var(--muted);font-size:11px;font-weight:700;letter-spacing:.02em;}
  .section-title{font-size:11px;text-transform:uppercase;letter-spacing:.1em;color:var(--muted);margin:20px 0 8px;}
  /* ── Box/card links (config index, docs) ── */
  a.box{text-decoration:none;color:var(--text);border:1px solid var(--line);border-left:6px solid var(--accent);border-radius:10px;padding:12px;background:var(--card);display:block;transition:transform .14s,box-shadow .14s;}
  a.box:hover{transform:translateY(-2px);box-shadow:0 10px 24px rgba(0,0,0,.3);}
  a.box h2{margin:0 0 4px;font-size:16px;color:var(--accent);}
  a.box p{color:var(--muted);font-size:13px;margin:0;}
  /* ── Home page ── */
  .panel{display:block;}
  .menu{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;width:100%;padding:10px;border:1px solid var(--line);border-radius:14px;background:var(--card);}
  .controls{display:grid;gap:14px;background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;}
  .controls-top{display:grid;grid-template-columns:minmax(280px,1fr) minmax(0,1.35fr);gap:12px;}
  .controls-bottom{display:grid;grid-template-columns:1fr;gap:12px;}
  .power-card,.saved-effects-card{display:grid;gap:14px;padding:14px;border:1px solid var(--line);border-radius:12px;background:var(--card);}
  .power-copy,.saved-effects-copy{display:grid;gap:6px;}
  .power-copy h2,.saved-effects-copy h2{margin:0;font-size:18px;}
  .power-actions{display:grid;gap:10px;}
  .sequence-tools{display:grid;gap:10px;padding:12px;border:1px solid rgba(15,208,224,.16);border-radius:12px;background:rgba(255,255,255,.04);}
  .slider-readout,.effects-status{font-size:12px;font-weight:700;color:var(--accent);}
  .sequence-list{display:grid;gap:10px;}
  .sequence-item{display:grid;gap:8px;padding:12px;border:1px solid var(--line);border-radius:12px;background:var(--card);}
  .sequence-head{display:flex;align-items:flex-start;justify-content:space-between;gap:10px;}
  .sequence-title{display:grid;gap:4px;}
  .sequence-title strong{font-size:14px;}
  .sequence-meta{font-size:12px;color:var(--muted);}
  .sequence-colors{display:flex;gap:8px;flex-wrap:wrap;}
  .sequence-color{width:22px;height:22px;border-radius:999px;border:1px solid rgba(255,255,255,.2);}
  .empty-note{padding:12px;border:1px dashed rgba(15,208,224,.26);border-radius:12px;color:var(--muted);background:rgba(255,255,255,.03);}
  .delete-btn{width:auto;border:none;background:var(--danger);color:#fff;font-weight:700;cursor:pointer;}
  .runtime-card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-top:12px;}
  .runtime-head{display:flex;align-items:flex-start;justify-content:space-between;gap:12px;}
  .runtime-copy{display:grid;gap:6px;}
  .runtime-copy h2{margin:0;display:flex;align-items:center;gap:10px;}
  .runtime-pill{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;background:rgba(15,208,224,.1);color:var(--accent);font-size:11px;font-weight:700;letter-spacing:.04em;text-transform:uppercase;}
  .toggle-btn{width:auto;min-width:140px;border:none;background:var(--accent);color:var(--bg);font-weight:700;cursor:pointer;}
  .runtime-body{margin-top:12px;}
  .runtime-body.hidden{display:none;}
  .control-sections{display:grid;grid-template-columns:1fr;gap:12px;}
  .control-card{display:grid;gap:12px;padding:14px;border:1px solid var(--line);border-radius:12px;background:var(--card);}
  .control-card h2{font-size:18px;margin:0;}
  .switch{display:flex;align-items:center;gap:10px;color:var(--text);font-weight:600;}
  .switch input{width:auto;transform:scale(1.2);}
  .item{display:grid;gap:4px;text-decoration:none;color:var(--text);background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px 16px;transition:transform .14s,box-shadow .14s;}
  .item:hover{transform:translateY(-2px);box-shadow:0 10px 20px rgba(0,0,0,.25);}
  .item h2{margin:0;font-size:16px;}
  .item span{color:var(--muted);font-size:12px;}
  /* ── GPIO ── */
  .output-block{border-top:2px solid var(--line);padding-top:12px;margin-top:12px;}
  .output-block:first-child{border-top:none;padding-top:0;margin-top:0;}
  .output-header{display:flex;align-items:center;justify-content:space-between;}
  .output-header h2{margin:0;}
  .json-card,.json-debug{display:none;}
  body.show-json .json-card,body.show-json .json-debug{display:block;}
  /* ── Profiles ── */
  .profile-grid{display:grid;gap:10px;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));margin-top:10px;}
  .profile-card{border:2px solid var(--line);border-radius:10px;padding:12px;background:var(--card);}
  .profile-card.is-default{border-color:var(--accent);}
  .profile-card .pname{font-size:15px;font-weight:700;margin:0 0 2px;}
  .profile-card .pid{font-size:11px;color:var(--muted);margin:0 0 4px;font-family:Consolas,monospace;}
  .profile-card .pdesc{font-size:12px;color:var(--muted);margin:0 0 8px;min-height:16px;}
  .badges{display:flex;gap:5px;flex-wrap:wrap;margin-bottom:8px;}
  .pactions{display:flex;gap:6px;flex-wrap:wrap;}
  .resp-card{display:none;}
  /* ── Palettes ── */
  .palettes-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(190px,1fr));gap:10px;margin-top:10px;}
  .pal-card{border:1px solid var(--line);border-radius:10px;padding:10px;background:var(--card);transition:box-shadow .15s;}
  .pal-card.editing{border-color:var(--accent);box-shadow:0 0 0 2px rgba(15,208,224,.25);}
  .pal-card h3{font-size:.9rem;margin:0 0 4px;text-transform:none;letter-spacing:0;color:var(--text);}
  .swatches{display:flex;gap:3px;margin:6px 0;}
  .swatch{width:24px;height:16px;border-radius:3px;border:1px solid rgba(255,255,255,.1);}
  .pal-actions{display:flex;gap:5px;flex-wrap:wrap;margin-top:6px;}
  .edit-panel{margin-top:14px;padding:14px;background:var(--card2);border:1px solid var(--line);border-radius:10px;border-left:4px solid var(--accent);}
  .edit-panel h3{color:var(--accent);margin:0 0 10px;font-size:.95rem;text-transform:none;letter-spacing:0;}
  .form-row{display:flex;flex-wrap:wrap;gap:10px;margin-bottom:8px;}
  .form-group{display:flex;flex-direction:column;gap:3px;flex:1;min-width:130px;}
  .form-label{font-size:.76rem;font-weight:600;color:var(--muted);}
  .colors-row{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:8px;}
  .color-slot{display:flex;flex-direction:column;gap:3px;align-items:flex-start;}
  .color-slot .form-label{font-size:.72rem;}
  .color-pair{display:flex;align-items:center;gap:4px;}
  .color-hex{width:72px;font-family:monospace;font-size:.8rem;padding:4px 6px;}
  .form-actions{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:10px;}
  .section-hdr{display:flex;align-items:center;gap:8px;margin-bottom:4px;}
  .no-palettes{color:var(--muted);font-style:italic;font-size:.85rem;}
  .json-pre{background:var(--card2);border:1px solid var(--line);border-radius:6px;padding:8px;font-size:.75rem;white-space:pre-wrap;overflow:auto;max-height:160px;margin:0;font-family:Consolas,monospace;color:var(--text);}
  /* ── File input ── */
  .file-row{display:flex;gap:10px;align-items:center;flex-wrap:wrap;}
  /* ── Responsive ── */
  @media(max-width:760px){
    .menu,.controls-top,.controls-bottom,.control-sections{grid-template-columns:1fr;}
    .row{flex-direction:column;}
    .runtime-head{flex-direction:column;align-items:stretch;}
    .toggle-btn{width:100%;}
  }
</style>
)CSS";
}

String ApiService::buildNavHtml() const {
  return R"HTML(
<style>
  /* ── Outer page box: 90% wide, contains nav + content ── */
  .gen-page-outer{width:90%;max-width:90%;margin:20px auto;border-radius:16px;box-shadow:0 12px 36px rgba(10,30,20,.15);overflow:hidden;background:var(--card,#fff);}
  .gen-page-outer>main{max-width:100%!important;width:100%!important;margin:0!important;box-sizing:border-box;}
  /* ── Nav: flush inside box, no own radius ── */
  .gen-nav{display:flex;align-items:center;background:linear-gradient(135deg,#0a3d4a 0%,#0f6a7a 100%);min-height:52px;border-radius:0;box-shadow:none;padding:0 16px;position:relative;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;margin:0;width:100%;}
  .gen-logo{display:flex;align-items:center;gap:8px;flex-shrink:0;margin-right:auto;text-decoration:none;}
  .gen-logo-text{color:#e8f6f8;font-size:18px;font-weight:600;white-space:nowrap;letter-spacing:.01em;}
  .gen-hamburger{display:none;background:none;border:none;cursor:pointer;grid-template-columns:1fr 1fr;gap:3px;padding:32px;}
  .gen-hamburger span{display:block;background:#e8f6f8;width:8px;height:8px;border-radius:2px;}
  .gen-nav>ul{display:flex;justify-content:center;flex:1;list-style:none;margin:0;padding:0;}
  .gen-nav>ul>li{position:relative;}
  .gen-nav>ul>li>a{display:flex;align-items:center;gap:4px;padding:0 16px;height:52px;color:#e8f6f8;font-size:14px;font-weight:500;text-decoration:none;transition:background .2s;}
  .gen-nav>ul>li>a:hover{background:rgba(255,255,255,.12);color:#fff;}
  .gen-nav>ul>li>ul{display:none;position:absolute;top:100%;left:0;background:#083040;min-width:170px;border-radius:0 0 8px 8px;box-shadow:0 8px 24px rgba(8,48,64,.28);z-index:1000;list-style:none;margin:0;padding:0;}
  .gen-nav>ul>li:hover>ul{display:block;}
  .gen-nav>ul>li>ul>li>a{display:block;padding:10px 18px;color:#b2e4ec;font-size:13px;text-decoration:none;white-space:nowrap;}
  .gen-nav>ul>li>ul>li>a:hover{background:rgba(255,255,255,.1);color:#fff;}
  @media(max-width:768px){
    .gen-hamburger{display:grid;}
    .gen-nav>ul{display:none;position:absolute;top:100%;left:0;right:0;flex-direction:column;background:linear-gradient(135deg,#0a3d4a 0%,#0f6a7a 100%);z-index:9999;border-radius:0;}
    .gen-nav>ul.open{display:flex;}
    .gen-nav>ul>li>ul{display:block;position:static;background:rgba(0,0,0,.18);box-shadow:none;border-radius:0;}
    .gen-page-outer{width:100%;margin:0;border-radius:0;}
  }
</style>
<nav class='gen-nav' id='gen-nav'>
  <a class='gen-logo' href='/'>
    <img src='data:image/png;base64,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' width='32' height='32' alt='DUXMAN icon' style='border-radius:4px;flex-shrink:0;'>
    <span class='gen-logo-text'>DUXMAN-LED-NEXT</span>
  </a>
  <button class='gen-hamburger' id='gen-ham' aria-label='Menu'>
    <span></span><span></span><span></span><span></span>
  </button>
  <ul id='gen-menu'>
    <li><a href='/'>Home</a></li>
    <li>
      <a href='/config'>Config &#9660;</a>
      <ul>
        <li><a href='/config/network'>Network</a></li>
        <li><a href='/config/microphone'>Microphone</a></li>
        <li><a href='/config/gpio'>GPIO</a></li>
        <li><a href='/config/profiles'>Profiles</a></li>
        <li><a href='/config/palettes'>Paletas</a></li>
        <li><a href='/config/debug'>Debug</a></li>
        <li><a href='/config/manual'>Manual JSON</a></li>
      </ul>
    </li>
    <li>
      <a href='/api'>API &#9660;</a>
      <ul>
        <li><a href='/api/state'>State</a></li>
        <li><a href='/api/config/network'>Network</a></li>
        <li><a href='/api/config/microphone'>Microphone</a></li>
        <li><a href='/api/config/gpio'>GPIO</a></li>
        <li><a href='/api/config/debug'>Debug</a></li>
        <li><a href='/api/profiles'>Profiles</a></li>
        <li><a href='/api/hardware'>Hardware</a></li>
        <li><a href='/api/release'>Release</a></li>
        <li><a href='/api/config/all'>Config All</a></li>
      </ul>
    </li>
    <li>
      <a href='#'>Sobre &#9660;</a>
      <ul>
        <li><a href='/version'>Version</a></li>
      </ul>
    </li>
  </ul>
</nav>
<script>
  (function(){
    var ham=document.getElementById('gen-ham');
    var nav=document.getElementById('gen-nav');
    var menu=document.getElementById('gen-menu');
    if(ham){
      ham.addEventListener('click',function(){
        var open=menu.classList.toggle('open');
        nav.classList.toggle('mobile-open',open);
      });
      document.addEventListener('click',function(e){
        if(!nav.contains(e.target)&&menu.classList.contains('open')){
          menu.classList.remove('open');
          nav.classList.remove('mobile-open');
        }
      });
    }
  })();
</script>
)HTML";
}

String ApiService::buildHomeHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN-LED-NEXT __FW_VERSION__</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <section class='hero'>
      <div>
      <div class='hero-title'>
        <h1>DUXMAN-LED-NEXT __FW_VERSION__</h1>
        <span class='boot-at'>__BOOTED_AT__</span>
      </div>
      <p>Control rapido de efectos fijos y navegacion principal.</p>
      </div>


      <div class='panel'>
        <section class='controls'>
          <div class='controls-top'>
            <section class='control-card colors'>
              <div>
                <h2>Seleccion de colores</h2>
                <p>Define el color de fondo y los tres colores de primer plano del efecto.</p>
              </div>

              <div class='row colors'>
                <label>
                  Fondo / off
                  <input id='backgroundColor' type='color' value='#000000'>
                </label>

                <label>
                  Color 1
                  <input id='color0' type='color' value='#ff4d00'>
                </label>
                <label>
                  Color 2
                  <input id='color1' type='color' value='#ffd400'>
                </label>
                <label>
                  Color 3
                  <input id='color2' type='color' value='#00b8d9'>
                </label>
              </div>

              <div class='row'>
                <label>
                  Paleta predefinida
                  <select id='paletteId'>
                    <option value='-1'>Manual (colores libres)</option>
                  </select>
                </label>
                <label>
                  Vista previa de paleta
                  <div id='palettePreview' style='display:flex; gap:8px; margin-top:8px;'>
                    <span class='sequence-color' style='background:#ff4d00;'></span>
                    <span class='sequence-color' style='background:#ffd400;'></span>
                    <span class='sequence-color' style='background:#00b8d9;'></span>
                  </div>
                </label>
              </div>

              <div class='actions'>
                <button class='alt' type='button' onclick='applySelectedPalette(false)'>Aplicar paleta</button>
                <button class='alt' type='button' onclick='applySelectedPalette(true)'>Aplicar y guardar</button>
              </div>
            </section>

            <section class='control-card effect'>
              <div>
                <h2>Seleccion de efecto</h2>
                <p>Elige el render principal, numero de secciones, velocidad y brillo global.</p>
              </div>

              <div class='control-sections'>
                <div class='row'>
                  <label>
                    Efecto
                    <select id='effect'>
                      __EFFECT_OPTIONS__
                    </select>
                  </label>

                  <label>
                    Numero de secciones
                    <input id='sectionCount' type='range' min='1' max='10' value='3'>
                    <span id='sectionCountValue'>3</span>
                  </label>
                </div>

                <label id='speedControl'>
                  Rapidez del efecto
                  <input id='effectSpeed' type='range' min='1' max='100' value='10'>
                  <span id='effectSpeedValue'>10</span>
                  <span id='effectSpeedHint'>Solo se usa en efectos animados.</span>
                </label>

                <label>
                  Nivel del efecto
                  <input id='effectLevel' type='range' min='1' max='10' value='5'>
                  <span id='effectLevelValue'>5</span>
                </label>

                <label style='display:flex; align-items:center; gap:10px; opacity:0.9;'>
                  <input id='reactiveToAudio' type='checkbox' style='width:auto; flex-shrink:0;' disabled>
                  <span id='reactiveToAudioLabel'>Audio del microfono: automatico segun el efecto</span>
                </label>

                <div id='effectModeBanner' class='badge' style='display:inline-flex; align-items:center; gap:8px;'>Tipo de efecto: cargando...</div>

                <label>
                  Brillo global
                  <input id='brightness' type='range' min='0' max='255' value='128'>
                  <span id='brightnessValue'>128</span>
                </label>
              </div>
            </section>
          </div>

          <div class='controls-bottom'>
            <section class='power-card'>
              <div class='power-copy'>
                <h2>Estado y acciones</h2>
                <p>Activa la salida, revisa el estado cargado y aplica cambios rapidamente.</p>
              </div>

              <div>
                <div class='switch'>
                  <input id='power' type='checkbox'>
                  <span>Salida activa</span>
                </div>
                <span class='badge' id='status'>Cargando estado...</span>
              </div>

              <div class='power-actions'>
                <div class='actions'>
                  <button onclick='applyState()'>Aplicar</button>
                  <button class='alt' onclick='loadState()'>Recargar</button>
                  <button class='alt' onclick='saveStartupEffect()'>Guardar arranque</button>
                  <button class='alt' onclick='restartBoard()'>Reiniciar placa</button>
                </div>

                <div class='actions'>
                  <button class='alt' onclick='applyVisualPreset("smooth")'>Preset suave</button>
                  <button class='alt' onclick='applyVisualPreset("show")'>Preset show</button>
                  <button class='alt' onclick='applyVisualPreset("aggressive")'>Preset agresivo</button>
                </div>

                <div class='sequence-tools'>
                  <label>
                    Duracion para nueva entrada
                    <input id='effectDurationSec' type='range' min='1' max='600' value='30'>
                    <span id='effectDurationSecValue' class='slider-readout'>30 s</span>
                  </label>

                  <div class='actions'>
                    <button class='alt' onclick='addCurrentEffectToSequence()'>Anadir a lista</button>
                    <button class='alt' onclick='loadEffectsPersistence()'>Recargar lista</button>
                  </div>
                </div>
              </div>
            </section>

            <section class='saved-effects-card'>
              <div class='saved-effects-copy'>
                <h2>Persistencia de efectos</h2>
                <p>Guarda el efecto de arranque y prepara la secuencia futura con duracion por entrada.</p>
              </div>

              <div>
                <span id='effectsStatus' class='effects-status'>Leyendo persistencia...</span>
              </div>

              <div>
                <strong id='startupEffectSummary'>Sin efecto de arranque guardado.</strong>
              </div>

              <div id='sequenceList' class='sequence-list'>
                <div class='empty-note'>No hay efectos guardados en la secuencia.</div>
              </div>

              <pre id='effectsOut' class='json-debug'>Sin datos aun.</pre>
            </section>
          </div>
        </section>
      </div>

      <section class='runtime-card json-card'>
        <div class='runtime-head'>
          <div class='runtime-copy'>
            <h2>Estado runtime <span class='runtime-pill'>live json</span></h2>
            <p>El efecto se guarda en LittleFS al aplicar cambios.</p>
          </div>
          <button id='toggleRuntime' class='toggle-btn' type='button' aria-expanded='false'>Mostrar runtime</button>
        </div>
        <div id='runtimeBody' class='runtime-body hidden'>
          <pre id='stateOut'>Sin datos aun.</pre>
        </div>
      </section>
    </section>
  </main>
  <script>
    if(localStorage.getItem('dux_show_json')==='1') document.body.classList.add('show-json');

    const out = document.getElementById('stateOut');
    const status = document.getElementById('status');
    const brightness = document.getElementById('brightness');
    const brightnessValue = document.getElementById('brightnessValue');
    const sectionCount = document.getElementById('sectionCount');
    const sectionCountValue = document.getElementById('sectionCountValue');
    const effect = document.getElementById('effect');
    const paletteIdSelect = document.getElementById('paletteId');
    const palettePreview = document.getElementById('palettePreview');
    const effectSpeed = document.getElementById('effectSpeed');
    const effectSpeedValue = document.getElementById('effectSpeedValue');
    const effectSpeedHint = document.getElementById('effectSpeedHint');
    const effectLevel = document.getElementById('effectLevel');
    const effectLevelValue = document.getElementById('effectLevelValue');
    const speedControl = document.getElementById('speedControl');
    const reactiveToAudio = document.getElementById('reactiveToAudio');
    const reactiveToAudioLabel = document.getElementById('reactiveToAudioLabel');
    const runtimeBody = document.getElementById('runtimeBody');
    const toggleRuntime = document.getElementById('toggleRuntime');
    const effectDurationSec = document.getElementById('effectDurationSec');
    const effectDurationSecValue = document.getElementById('effectDurationSecValue');
    const effectsStatus = document.getElementById('effectsStatus');
    const startupEffectSummary = document.getElementById('startupEffectSummary');
    const sequenceList = document.getElementById('sequenceList');
    const effectsOut = document.getElementById('effectsOut');
    const effectModeBanner = document.getElementById('effectModeBanner');
    let effectsState = null;

    function selectedPaletteId() {
      const raw = Number(paletteIdSelect.value);
      return Number.isFinite(raw) && raw >= 0 ? raw : null;
    }

    function createPaletteSwatch(color) {
      return "<span class='sequence-color' style='background:" + color + "' title='" + color + "'></span>";
    }

    function renderPalettePreview(colors) {
      const safe = Array.isArray(colors) && colors.length >= 3
        ? colors
        : ['#ff4d00', '#ffd400', '#00b8d9'];
      palettePreview.innerHTML = createPaletteSwatch(safe[0])
        + createPaletteSwatch(safe[1])
        + createPaletteSwatch(safe[2]);
    }

    function renderPaletteOptionsFromState(state) {
      const palettes = Array.isArray(state.availablePalettes) ? state.availablePalettes : [];
      const currentPaletteId = Number.isFinite(Number(state.paletteId)) ? Number(state.paletteId) : -1;

      const options = ["<option value='-1'>Manual (colores libres)</option>"];
      palettes.forEach((palette) => {
        const pid = Number(palette.id);
        const selected = pid === currentPaletteId ? ' selected' : '';
        const style = palette.style || 'custom';
        const label = palette.label || palette.key || ('Palette ' + pid);
        options.push("<option value='" + pid + "'" + selected + ">" + label + " - " + style + "</option>");
      });
      paletteIdSelect.innerHTML = options.join('');

      if (currentPaletteId < 0) {
        paletteIdSelect.value = '-1';
      }

      renderPalettePreview(state.primaryColors);
    }

    function findSelectedPaletteColors() {
      const selected = selectedPaletteId();
      if (selected === null) {
        return null;
      }

      const stateObj = JSON.parse(out.textContent || '{}');
      const palettes = Array.isArray(stateObj.availablePalettes) ? stateObj.availablePalettes : [];
      const match = palettes.find((p) => Number(p.id) === selected);
      if (!match || !Array.isArray(match.primaryColors) || match.primaryColors.length < 3) {
        return null;
      }
      return match.primaryColors;
    }

    function applyPaletteToInputs(colors) {
      if (!Array.isArray(colors) || colors.length < 3) {
        return;
      }
      document.getElementById('color0').value = colors[0];
      document.getElementById('color1').value = colors[1];
      document.getElementById('color2').value = colors[2];
      renderPalettePreview(colors);
    }

    function effectUsesAudio(config) {
      return !!(config && (config.effectUsesAudio || config.reactiveToAudio));
    }

    function effectTypeText(config) {
      return effectUsesAudio(config) ? 'AUDIO REACTIVO' : 'VISUAL';
    }

    function effectTypeBadgeHtml(config) {
      const isAudio = effectUsesAudio(config);
      const bg = isAudio ? '#3f0d12' : '#0b3d20';
      const fg = isAudio ? '#ffd9de' : '#d8ffe7';
      const label = isAudio ? 'AUDIO' : 'VISUAL';
      return "<span style='display:inline-block; padding:2px 8px; border-radius:999px; background:" + bg + "; color:" + fg + "; font-size:12px; font-weight:700; letter-spacing:0.04em;'>" + label + "</span>";
    }

    function setRuntimeVisible(visible) {
      runtimeBody.classList.toggle('hidden', !visible);
      toggleRuntime.textContent = visible ? 'Ocultar runtime' : 'Mostrar runtime';
      toggleRuntime.setAttribute('aria-expanded', visible ? 'true' : 'false');
    }

    toggleRuntime.addEventListener('click', () => {
      setRuntimeVisible(runtimeBody.classList.contains('hidden'));
    });

    brightness.addEventListener('input', () => {
      brightnessValue.textContent = brightness.value;
    });
    sectionCount.addEventListener('input', () => {
      sectionCountValue.textContent = sectionCount.value;
    });
    effectSpeed.addEventListener('input', () => {
      effectSpeedValue.textContent = effectSpeed.value;
    });
    effectLevel.addEventListener('input', () => {
      effectLevelValue.textContent = effectLevel.value;
    });
    effectDurationSec.addEventListener('input', () => {
      effectDurationSecValue.textContent = effectDurationSec.value + ' s';
    });

    paletteIdSelect.addEventListener('change', () => {
      const colors = findSelectedPaletteColors();
      if (colors) {
        applyPaletteToInputs(colors);
      } else {
        renderPalettePreview([
          document.getElementById('color0').value,
          document.getElementById('color1').value,
          document.getElementById('color2').value,
        ]);
      }
    });

    ['color0', 'color1', 'color2'].forEach((id) => {
      document.getElementById(id).addEventListener('input', () => {
        paletteIdSelect.value = '-1';
        renderPalettePreview([
          document.getElementById('color0').value,
          document.getElementById('color1').value,
          document.getElementById('color2').value,
        ]);
      });
    });

    function selectedEffectUsesAudio() {
      const option = effect.options[effect.selectedIndex];
      return !!(option && option.dataset && option.dataset.audio === '1');
    }

    function updateSpeedControl() {
      effectSpeed.disabled = false;
      speedControl.style.opacity = '1';
      effectSpeedHint.textContent = 'Valor 1..100. Disponible siempre; cada efecto decide si lo usa.';

      const usesAudio = selectedEffectUsesAudio();
      reactiveToAudio.checked = usesAudio;
      reactiveToAudioLabel.textContent = usesAudio
        ? 'Audio del microfono: activado automaticamente para este efecto'
        : 'Audio del microfono: desactivado en efectos visuales';
      effectModeBanner.textContent = 'Tipo de efecto: ' + (usesAudio ? 'AUDIO REACTIVO' : 'VISUAL');
    }

    effect.addEventListener('change', updateSpeedControl);

    function renderState(state) {
      document.getElementById('power').checked = !!state.power;
      document.getElementById('effect').value = state.effect || (state.effectId === 1 ? 'gradient' : 'fixed');
      document.getElementById('brightness').value = state.brightness ?? 128;
      brightnessValue.textContent = document.getElementById('brightness').value;
      document.getElementById('sectionCount').value = state.sectionCount ?? 3;
      sectionCountValue.textContent = document.getElementById('sectionCount').value;
      document.getElementById('effectSpeed').value = state.effectSpeed ?? 10;
      effectSpeedValue.textContent = document.getElementById('effectSpeed').value;
      document.getElementById('effectLevel').value = state.effectLevel ?? 5;
      effectLevelValue.textContent = document.getElementById('effectLevel').value;
      reactiveToAudio.checked = !!(state.effectUsesAudio || state.reactiveToAudio);
      const colors = Array.isArray(state.primaryColors) ? state.primaryColors : ['#ff4d00','#ffd400','#00b8d9'];
      document.getElementById('color0').value = colors[0] || '#ff4d00';
      document.getElementById('color1').value = colors[1] || '#ffd400';
      document.getElementById('color2').value = colors[2] || '#00b8d9';
      document.getElementById('backgroundColor').value = state.backgroundColor || '#000000';
      renderPaletteOptionsFromState(state);
      updateSpeedControl();
      effectModeBanner.textContent = 'Tipo de efecto: ' + effectTypeText(state);
      out.textContent = JSON.stringify(state, null, 2);
      status.textContent = 'Estado cargado';
    }

    function normalizeEffectsPayload(data) {
      if (data && data.effects && data.effects.effects) {
        return data.effects.effects;
      }
      if (data && data.effects) {
        return data.effects;
      }
      return data;
    }

    function createColorSwatch(color) {
      return "<span class='sequence-color' style='background:" + color + "' title='" + color + "'></span>";
    }

    function describeConfig(config) {
      if (!config) {
        return 'Configuracion no disponible';
      }
      const effectName = config.effectLabel || config.effect || ('ID ' + (config.effectId ?? '?'));
      const paletteName = config.paletteLabel || (Number(config.paletteId) >= 0 ? ('palette ' + config.paletteId) : 'manual');
      return effectName + ' | paleta ' + paletteName + ' | secciones ' + (config.sectionCount ?? '?') + ' | velocidad ' + (config.effectSpeed ?? '?') + ' | nivel ' + (config.effectLevel ?? '?') + ' | brillo ' + (config.brightness ?? '?');
    }

    async function applySelectedPalette(saveAfterApply) {
      const paletteId = selectedPaletteId();
      if (paletteId === null) {
        status.textContent = 'Selecciona una paleta predefinida para aplicar.';
        return;
      }

      status.textContent = saveAfterApply ? 'Aplicando paleta y guardando...' : 'Aplicando paleta...';
      const res = await fetch('/api/v1/palettes/apply', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ paletteId })
      });

      const data = await res.json();
      if (data && data.state) {
        renderState(data.state);
      }

      if (saveAfterApply) {
        await saveStartupEffect();
      }

      status.textContent = saveAfterApply ? 'Paleta aplicada y guardada' : 'Paleta aplicada';
    }

    function renderEffectsState(data) {
      effectsState = normalizeEffectsPayload(data) || {};
      effectsOut.textContent = JSON.stringify(effectsState, null, 2);

      if (effectsState.hasStartupEffect && effectsState.startupEffect) {
        startupEffectSummary.innerHTML = 'Arranque: ' + effectTypeBadgeHtml(effectsState.startupEffect) + ' ' + describeConfig(effectsState.startupEffect);
      } else {
        startupEffectSummary.textContent = 'Sin efecto de arranque guardado.';
      }

      const sequence = Array.isArray(effectsState.sequence) ? effectsState.sequence : [];
      if (!sequence.length) {
        sequenceList.innerHTML = "<div class='empty-note'>No hay efectos guardados en la secuencia.</div>";
      } else {
        sequenceList.innerHTML = sequence.map((entry) => {
          const config = entry.config || {};
          const colors = Array.isArray(config.primaryColors) ? config.primaryColors : [];
          return "<article class='sequence-item'>"
            + "<div class='sequence-head'>"
            + "<div class='sequence-title'>"
            + "<strong>#" + entry.id + " - " + effectTypeBadgeHtml(config) + " " + (config.effectLabel || config.effect || 'Efecto') + "</strong>"
            + "<span class='sequence-meta'>Duracion " + (entry.durationSec ?? 0) + " s - " + describeConfig(config) + "</span>"
            + "</div>"
            + "<button class='delete-btn' type='button' onclick='deleteSequenceEntry(" + entry.id + ")'>Eliminar</button>"
            + "</div>"
            + "<div class='sequence-colors'>"
            + colors.map(createColorSwatch).join('')
            + createColorSwatch(config.backgroundColor || '#000000')
            + "</div>"
            + "</article>";
        }).join('');
      }

      effectsStatus.textContent = 'Persistencia cargada';
    }

    async function loadEffectsPersistence() {
      effectsStatus.textContent = 'Leyendo persistencia...';
      const res = await fetch('/api/v1/effects');
      const data = await res.json();
      renderEffectsState(data);
    }

    async function saveStartupEffect() {
      effectsStatus.textContent = 'Guardando arranque...';
      const res = await fetch('/api/v1/effects/startup/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: '{}'
      });
      const data = await res.json();
      renderEffectsState(data);
      effectsStatus.textContent = data && data.saved ? 'Efecto de arranque guardado' : 'No se pudo guardar arranque';
    }

    async function addCurrentEffectToSequence() {
      effectsStatus.textContent = 'Guardando efecto en la lista...';
      const res = await fetch('/api/v1/effects/sequence/add', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ durationSec: Number(effectDurationSec.value) })
      });
      const data = await res.json();
      renderEffectsState(data);
      effectsStatus.textContent = data && data.added ? 'Entrada anadida a la secuencia' : 'No se pudo anadir la entrada';
    }

    async function deleteSequenceEntry(id) {
      effectsStatus.textContent = 'Eliminando entrada...';
      const res = await fetch('/api/v1/effects/sequence/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id })
      });
      const data = await res.json();
      renderEffectsState(data);
      effectsStatus.textContent = data && data.deleted ? 'Entrada eliminada' : 'No se pudo eliminar la entrada';
    }

    async function restartBoard() {
      const confirmed = window.confirm('Se reiniciara la placa inmediatamente. Continuar?');
      if (!confirmed) {
        return;
      }

      status.textContent = 'Enviando reinicio...';
      try {
        await fetch('/api/v1/system/restart', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: '{}'
        });
        status.textContent = 'Reinicio enviado. Esperando reconexion...';
      } catch (_) {
        status.textContent = 'La placa se esta reiniciando...';
      }

      setTimeout(() => {
        loadState().catch(() => {
          status.textContent = 'Esperando a que la placa vuelva a responder...';
        });
      }, 4000);
    }

    function setSliderWithReadout(sliderId, readoutId, value, suffix = '') {
      const slider = document.getElementById(sliderId);
      const readout = document.getElementById(readoutId);
      slider.value = String(value);
      readout.textContent = String(value) + suffix;
    }

    async function applyVisualPreset(presetName) {
      const presets = {
        smooth: { sectionCount: 3, effectSpeed: 24, effectLevel: 3, brightness: 120, backgroundColor: '#020202' },
        show: { sectionCount: 4, effectSpeed: 56, effectLevel: 6, brightness: 170, backgroundColor: '#000000' },
        aggressive: { sectionCount: 6, effectSpeed: 86, effectLevel: 9, brightness: 220, backgroundColor: '#000000' }
      };

      const preset = presets[presetName];
      if (!preset) {
        status.textContent = 'Preset no valido';
        return;
      }

      setSliderWithReadout('sectionCount', 'sectionCountValue', preset.sectionCount);
      setSliderWithReadout('effectSpeed', 'effectSpeedValue', preset.effectSpeed);
      setSliderWithReadout('effectLevel', 'effectLevelValue', preset.effectLevel);
      setSliderWithReadout('brightness', 'brightnessValue', preset.brightness);
      document.getElementById('backgroundColor').value = preset.backgroundColor;
      updateSpeedControl();

      status.textContent = 'Aplicando preset ' + presetName + '...';
      await applyState();
      status.textContent = 'Preset ' + presetName + ' aplicado';
    }

    async function loadState() {
      status.textContent = 'Leyendo estado...';
      const res = await fetch('/api/v1/state');
      const data = await res.json();
      renderState(data);
    }

    async function applyState() {
      status.textContent = 'Aplicando cambios...';
      const payload = {
        power: document.getElementById('power').checked,
        brightness: Number(document.getElementById('brightness').value),
        effect: document.getElementById('effect').value,
        sectionCount: Number(document.getElementById('sectionCount').value),
        effectSpeed: Number(document.getElementById('effectSpeed').value),
        effectLevel: Number(document.getElementById('effectLevel').value),
        reactiveToAudio: selectedEffectUsesAudio(),
        backgroundColor: document.getElementById('backgroundColor').value
      };

      const paletteId = selectedPaletteId();
      if (paletteId !== null) {
        payload.paletteId = paletteId;
      } else {
        payload.primaryColors = [
          document.getElementById('color0').value,
          document.getElementById('color1').value,
          document.getElementById('color2').value
        ];
      }
      const res = await fetch('/api/v1/state', {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      const text = await res.text();
      let data = text;
      try { data = JSON.parse(text); } catch (_) {}
      if (data && data.state) {
        renderState(data.state);
      } else {
        out.textContent = typeof data === 'string' ? data : JSON.stringify(data, null, 2);
      }
      status.textContent = 'Cambios aplicados';
    }

    setRuntimeVisible(window.matchMedia('(min-width: 761px)').matches);
    Promise.all([loadState(), loadEffectsPersistence()]);
  </script>
</div>
</body>
</html>
)HTML";
  const String versionLabel = String(BuildProfile::kFwVersion);
  const String bootedAtLabel = buildBootedAtLabel();
  const CoreState stateSnapshot = state_.snapshot();
  html.replace("__FW_VERSION__", versionLabel);
  html.replace("__BOOTED_AT__", bootedAtLabel);
  html.replace("__EFFECT_OPTIONS__", EffectRegistry::buildHtmlOptions(stateSnapshot.effectId));
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildPalettesConfigHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN - Editor de Paletas</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
<main>
  <div class='card'>
    <div class='section-hdr'>
      <h1>Paletas del Sistema</h1>
      <span class='badge badge-sys'>Sistema</span>
    </div>
    <p>Paletas de fabrica. Solo lectura.</p>
    <div class='palettes-grid' id='systemGrid'><p class='no-palettes'>Cargando...</p></div>
  </div>

  <div class='card'>
    <div class='section-hdr'>
      <h1>Mis Paletas</h1>
      <span class='badge badge-user'>Usuario</span>
      <button class='btn btn-primary btn-sm' style='margin-left:auto' onclick='openNewForm()'>+ Nueva</button>
    </div>
    <p>Paletas propias. Edita o elimina desde cada tarjeta.</p>

    <div class='palettes-grid' id='userGrid'><p class='no-palettes'>Cargando...</p></div>

    <!-- editor inline -->
    <div class='edit-panel' id='editPanel' style='display:none'>
      <h3 id='editTitle'>Nueva Paleta</h3>
      <div class='form-row'>
        <div class='form-group'>
          <span class='form-label'>ID (slug)</span>
          <input type='text' id='inpId' placeholder='mi_paleta' pattern='[a-z0-9_-]+' maxlength='40'>
        </div>
        <div class='form-group'>
          <span class='form-label'>Nombre visual</span>
          <input type='text' id='inpLabel' placeholder='Mi paleta' maxlength='60'>
        </div>
        <div class='form-group' style='min-width:110px;max-width:140px'>
          <span class='form-label'>Estilo</span>
          <select id='inpStyle'>
            <option value='warm'>Warm</option>
            <option value='cold'>Cold</option>
            <option value='neon'>Neon</option>
            <option value='pastel'>Pastel</option>
            <option value='high-contrast'>High Contrast</option>
            <option value='party'>Party</option>
            <option value='custom'>Custom</option>
          </select>
        </div>
      </div>
      <div class='form-group' style='margin-bottom:8px'>
        <span class='form-label'>Descripcion (opcional)</span>
        <input type='text' id='inpDesc' maxlength='80' placeholder=''>
      </div>
      <div class='colors-row'>
        <div class='color-slot'>
          <span class='form-label'>Color 1</span>
          <div class='color-pair'>
            <input type='color' id='c1picker' value='#ff0000' oninput='syncColor(1,"picker")'>
            <input type='text' class='color-hex' id='c1hex' value='#ff0000' maxlength='7' oninput='syncColor(1,"hex")'>
          </div>
        </div>
        <div class='color-slot'>
          <span class='form-label'>Color 2</span>
          <div class='color-pair'>
            <input type='color' id='c2picker' value='#00ff00' oninput='syncColor(2,"picker")'>
            <input type='text' class='color-hex' id='c2hex' value='#00ff00' maxlength='7' oninput='syncColor(2,"hex")'>
          </div>
        </div>
        <div class='color-slot'>
          <span class='form-label'>Color 3</span>
          <div class='color-pair'>
            <input type='color' id='c3picker' value='#0000ff' oninput='syncColor(3,"picker")'>
            <input type='text' class='color-hex' id='c3hex' value='#0000ff' maxlength='7' oninput='syncColor(3,"hex")'>
          </div>
        </div>
      </div>
      <div class='form-actions'>
        <button class='btn btn-primary' onclick='submitPalette()'>Guardar</button>
        <button class='btn btn-ghost' onclick='closePanel()'>Cancelar</button>
        <span id='formStatus' class='hint'></span>
      </div>
    </div>

    <div class='json-debug'>
      <p class='form-label' style='margin-bottom:4px'>Ultima respuesta API</p>
      <pre class='json-pre' id='jsonOut'></pre>
    </div>
  </div>
</main>
<script>
  var editingKey = null;

  // Leer preferencia de localStorage
  if (localStorage.getItem('dux_show_json') === '1') {
    document.body.classList.add('show-json');
  }

  function colorToHex(c) {
    if (!c) return '#000000';
    if (typeof c === 'string') return c.startsWith('#') ? c : '#'+c;
    if (typeof c === 'object' && c.r !== undefined)
      return '#' + [c.r,c.g,c.b].map(function(v){ return v.toString(16).padStart(2,'0'); }).join('');
    return '#000000';
  }
  function syncColor(idx, src) {
    var picker = document.getElementById('c'+idx+'picker');
    var hex = document.getElementById('c'+idx+'hex');
    if (src==='picker') { hex.value = picker.value; }
    else if (/^#[0-9a-fA-F]{6}$/.test(hex.value)) { picker.value = hex.value; }
  }
  function setColors(c1,c2,c3) {
    [[1,c1],[2,c2],[3,c3]].forEach(function(x) {
      var h = colorToHex(x[1]);
      document.getElementById('c'+x[0]+'picker').value = h;
      document.getElementById('c'+x[0]+'hex').value = h;
    });
  }
  function setStatus(msg, isErr) {
    var el = document.getElementById('formStatus');
    el.textContent = msg;
    el.className = isErr ? 'hint err' : 'hint ok';
  }
  function showJson(obj) {
    document.getElementById('jsonOut').textContent = JSON.stringify(obj, null, 2);
  }
  function openNewForm() {
    editingKey = null;
    document.querySelectorAll('.pal-card').forEach(function(c){ c.classList.remove('editing'); });
    document.getElementById('editTitle').textContent = 'Nueva Paleta';
    document.getElementById('inpId').value = '';
    document.getElementById('inpId').readOnly = false;
    document.getElementById('inpLabel').value = '';
    document.getElementById('inpStyle').value = 'custom';
    document.getElementById('inpDesc').value = '';
    setColors('#ff0000','#00ff00','#0000ff');
    setStatus('','');
    document.getElementById('editPanel').style.display = '';
    document.getElementById('editPanel').scrollIntoView({behavior:'smooth', block:'nearest'});
  }
  function closePanel() {
    document.getElementById('editPanel').style.display = 'none';
    document.querySelectorAll('.pal-card').forEach(function(c){ c.classList.remove('editing'); });
    editingKey = null;
  }
  function editPalette(p, cardEl) {
    editingKey = p.key;
    document.querySelectorAll('.pal-card').forEach(function(c){ c.classList.remove('editing'); });
    if (cardEl) cardEl.classList.add('editing');
    document.getElementById('editTitle').textContent = 'Editando: ' + p.label;
    document.getElementById('inpId').value = p.key;
    document.getElementById('inpId').readOnly = true;
    document.getElementById('inpLabel').value = p.label;
    document.getElementById('inpStyle').value = p.style || 'custom';
    document.getElementById('inpDesc').value = p.description || '';
    var pc = p.primaryColors || [];
    setColors(pc[0]||'#ff0000', pc[1]||'#00ff00', pc[2]||'#0000ff');
    setStatus('','');
    document.getElementById('editPanel').style.display = '';
    document.getElementById('editPanel').scrollIntoView({behavior:'smooth', block:'nearest'});
  }
  function applyPalette(key) {
    fetch('/api/v1/palettes/apply', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({palette:key})
    }).then(function(r){return r.json();}).then(function(d){
      showJson(d);
      setStatus('Aplicada: '+key, false);
    }).catch(function(e){setStatus('Error: '+e, true);});
  }
  function deletePalette(key) {
    if (!confirm('Eliminar paleta "'+key+'"?')) return;
    fetch('/api/v1/palettes/delete', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({key:key})
    }).then(function(r){return r.json();}).then(function(d){
      showJson(d);
      if (d.error) { alert('Error: '+d.error); return; }
      if (editingKey === key) closePanel();
      loadPalettes();
    }).catch(function(e){alert('Error: '+e);});
  }
  function submitPalette() {
    var key = document.getElementById('inpId').value.trim();
    if (!key) { setStatus('El ID es obligatorio', true); return; }
    if (!/^[a-z0-9_-]+$/.test(key)) { setStatus('ID solo puede contener a-z, 0-9, _ -', true); return; }
    var label = document.getElementById('inpLabel').value.trim();
    if (!label) { setStatus('El nombre es obligatorio', true); return; }
    var body = {
      key: key, label: label,
      style: document.getElementById('inpStyle').value,
      description: document.getElementById('inpDesc').value.trim(),
      colors: {
        color1: document.getElementById('c1hex').value,
        color2: document.getElementById('c2hex').value,
        color3: document.getElementById('c3hex').value
      }
    };
    fetch('/api/v1/palettes/save', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify(body)
    }).then(function(r){return r.json();}).then(function(d){
      showJson(d);
      if (d.error) { setStatus('Error: '+d.error, true); return; }
      setStatus('Guardada correctamente.', false);
      closePanel();
      loadPalettes();
    }).catch(function(e){setStatus('Error: '+e, true);});
  }
  function makeSwatch(color) {
    var h = colorToHex(color);
    var div = document.createElement('div');
    div.className = 'swatch'; div.style.background = h; div.title = h;
    return div;
  }
  function renderPaletteCard(p, container) {
    var div = document.createElement('div');
    div.className = 'pal-card';
    var pc = p.primaryColors || [];
    var isUser = p.source === 'user';
    var desc = (p.style||'') + (p.description ? ' \u2014 '+p.description : '');
    div.innerHTML = '<h3>' + p.label + '</h3>' +
      (desc ? '<small style="color:var(--muted);font-size:.75rem">' + desc + '</small>' : '');
    var sw = document.createElement('div'); sw.className = 'swatches';
    sw.appendChild(makeSwatch(pc[0]));
    sw.appendChild(makeSwatch(pc[1]));
    sw.appendChild(makeSwatch(pc[2]));
    div.appendChild(sw);
    var act = document.createElement('div'); act.className = 'pal-actions';
    var ab = document.createElement('button');
    ab.className = 'btn btn-primary btn-sm'; ab.textContent = 'Aplicar';
    ab.onclick = function(){ applyPalette(p.key); }; act.appendChild(ab);
    if (isUser) {
      var eb = document.createElement('button');
      eb.className = 'btn btn-ghost btn-sm'; eb.textContent = 'Editar';
      eb.onclick = function(){ editPalette(p, div); }; act.appendChild(eb);
      var db = document.createElement('button');
      db.className = 'btn btn-danger btn-sm'; db.textContent = 'x';
      db.title = 'Eliminar'; db.onclick = function(){ deletePalette(p.key); }; act.appendChild(db);
    }
    div.appendChild(act);
    container.appendChild(div);
  }
  function loadPalettes() {
    fetch('/api/v1/palettes').then(function(r){return r.json();}).then(function(data){
      showJson(data);
      var palettes = data.palettes || [];
      var sysGrid = document.getElementById('systemGrid');
      var userGrid = document.getElementById('userGrid');
      sysGrid.innerHTML = ''; userGrid.innerHTML = '';
      var sysl = palettes.filter(function(p){return p.source==='system';});
      var usrl = palettes.filter(function(p){return p.source==='user';});
      if (!sysl.length) sysGrid.innerHTML = '<p class="no-palettes">Sin paletas del sistema.</p>';
      else sysl.forEach(function(p){ renderPaletteCard(p, sysGrid); });
      if (!usrl.length) userGrid.innerHTML = '<p class="no-palettes">Sin paletas propias aun.</p>';
      else usrl.forEach(function(p){ renderPaletteCard(p, userGrid); });
    }).catch(function(){
      document.getElementById('systemGrid').innerHTML = '<p class="no-palettes">Error al cargar.</p>';
    });
  }
  window.addEventListener('DOMContentLoaded', loadPalettes);
</script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}
String ApiService::buildConfigIndexHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN - Configuracion</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
      <section class='card'>
      <h1>Configuracion</h1>
      <p>Selecciona la seccion de configuracion que quieres editar.</p>
      <div class='grid'>
        <a class='box' href='/config/network'>
          <h2>Network</h2>
          <p>WiFi, AP/STA, hostname, IP y DNS.</p>
        </a>
        <a class='box' href='/config/microphone'>
          <h2>Microfono</h2>
          <p>Configurar microfono generic_i2c (SD/WS/SCK) y perfil.</p>
        </a>
        <a class='box' href='/config/gpio'>
          <h2>GPIO</h2>
          <p>Pin LED, cantidad, tipo de LED y orden de color.</p>
        </a>
        <a class='box' href='/config/profiles'>
          <h2>Profiles</h2>
          <p>Guardar, aplicar y fijar perfiles GPIO por defecto.</p>
        </a>
        <a class='box' href='/config/palettes'>
          <h2>Paletas</h2>
          <p>Crear, editar y guardar paletas de colores personalizadas.</p>
        </a>
        <a class='box' href='/config/debug'>
          <h2>Debug</h2>
          <p>Habilitar debug y ajustar intervalo de heartbeat.</p>
        </a>
        <a class='box' href='/config/manual'>
          <h2>Manual</h2>
          <p>Editar, exportar e importar toda la configuracion JSON.</p>
        </a>
      </div>
    </section>
  </main>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildDocsHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN - API</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API</h1>
      <p>Indice de secciones API. Cada bloque tiene su propia UI para pruebas.</p>
      <div class='grid'>
        <a class='box' href='/api/state'>
          <h2>State</h2>
          <p>GET/PATCH de /api/v1/state.</p>
        </a>
        <a class='box' href='/api/config/network'>
          <h2>Config Network</h2>
          <p>GET/PATCH de /api/v1/config/network.</p>
        </a>
        <a class='box' href='/api/config/microphone'>
          <h2>Config Microphone</h2>
          <p>GET/PATCH de /api/v1/config/microphone.</p>
        </a>
        <a class='box' href='/api/config/gpio'>
          <h2>Config GPIO</h2>
          <p>GET/PATCH de /api/v1/config/gpio.</p>
        </a>
        <a class='box' href='/api/config/debug'>
          <h2>Config Debug</h2>
          <p>GET/PATCH de /api/v1/config/debug.</p>
        </a>
        <a class='box' href='/api/profiles/gpio'>
          <h2>Profiles GPIO</h2>
          <p>GET/POST de /api/v1/profiles/gpio*.</p>
        </a>
        <a class='box' href='/api/hardware'>
          <h2>Hardware</h2>
          <p>GET de /api/v1/hardware.</p>
        </a>
        <a class='box' href='/api/release'>
          <h2>Release</h2>
          <p>GET de /api/v1/release.</p>
        </a>
        <a class='box' href='/api/config/all'>
          <h2>Config All</h2>
          <p>GET/POST de /api/v1/config/all.</p>
        </a>
      </div>
      <a class='spec' href='/api/v1/openapi.json'>Abrir /api/v1/openapi.json</a>
    </div>
  </main>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildApiStateHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN API - State</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API State</h1>
      <button onclick='getState()'>GET /api/v1/state</button>
      <button onclick='patchState()'>PATCH /api/v1/state</button>
      <textarea id='payload'>{"power":true,"brightness":180,"effect":"blink_gradient","sectionCount":6,"effectSpeed":12,"primaryColors":["#FF4D00","#FFD400","#00B8D9"],"backgroundColor":"#050505"}</textarea>
    </div>
    <div class='card'><pre id='out'>Sin llamadas aun.</pre></div>
  </main>
  <script>
    async function callApi(method, body) {
      const options = { method, headers: { 'Content-Type': 'application/json' } };
      if (body) options.body = body;
      const res = await fetch('/api/v1/state', options);
      const text = await res.text();
      let parsed = text;
      try { parsed = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      document.getElementById('out').textContent = method + ' /api/v1/state -> HTTP ' + res.status + '\n\n' + parsed;
    }
    function getState() { callApi('GET'); }
    function patchState() { callApi('PATCH', document.getElementById('payload').value); }
    getState();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildApiConfigNetworkHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN API - Config Network</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API Config Network</h1>
      <button onclick='getCfg()'>GET /api/v1/config/network</button>
      <button onclick='patchCfg()'>PATCH /api/v1/config/network</button>
      <textarea id='payload'>{"network":{"wifi":{"mode":"ap_sta","apAvailability":"always","connection":{"ssid":"MiWiFi","password":"secreto"}},"ip":{"sta":{"mode":"dhcp","primaryDns":"8.8.8.8","secondaryDns":"1.1.1.1"}},"dns":{"hostname":"duxman-led-1"},"time":{"syncOnBoot":true,"ntpServer":"europe.pool.ntp.org"}}}</textarea>
    </div>
    <div class='card'><pre id='out'>Sin llamadas aun.</pre></div>
  </main>
  <script>
    async function callApi(method, body) {
      const options = { method, headers: { 'Content-Type': 'application/json' } };
      if (body) options.body = body;
      const res = await fetch('/api/v1/config/network', options);
      const text = await res.text();
      let parsed = text;
      try { parsed = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      document.getElementById('out').textContent = method + ' /api/v1/config/network -> HTTP ' + res.status + '\n\n' + parsed;
    }
    function getCfg() { callApi('GET'); }
    function patchCfg() { callApi('PATCH', document.getElementById('payload').value); }
    getCfg();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildApiReleaseHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN API - Release</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API Release</h1>
      <button onclick='getRelease()'>GET /api/v1/release</button>
    </div>
    <div class='card'><pre id='out'>Cargando...</pre></div>
  </main>
  <script>
    async function getRelease() {
      const res = await fetch('/api/v1/release');
      const text = await res.text();
      let parsed = text;
      try { parsed = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      document.getElementById('out').textContent = 'GET /api/v1/release -> HTTP ' + res.status + '\n\n' + parsed;
    }
    getRelease();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildNetworkConfigHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN Network Config</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>Configuracion de Red</h1>
      <p class='hint'>Edita WiFi/AP/STA y guarda en el dispositivo. Los cambios se aplican al momento.</p>
      <div class='actions'>
        <button class='btn alt' onclick='loadConfig()'>Recargar</button>
        <button class='btn' onclick='saveConfig()'>Guardar Configuracion</button>
      </div>
      <p id='status' class='hint'>Cargando...</p>
    </div>

    <div class='card'>
      <h2>WiFi</h2>
      <div class='row'>
        <div class='col field'>
          <label for='wifiMode'>Modo</label>
          <select id='wifiMode'>
            <option value='ap'>ap</option>
            <option value='sta'>sta</option>
            <option value='ap_sta'>ap_sta</option>
          </select>
        </div>
        <div class='col field'>
          <label for='apAvailability'>Disponibilidad AP</label>
          <select id='apAvailability'>
            <option value='always'>always</option>
            <option value='untilStaConnected'>untilStaConnected</option>
          </select>
        </div>
      </div>
      <div class='row'>
        <div class='col field'>
          <label for='ssid'>SSID STA</label>
          <input id='ssid' type='text' placeholder='MiWiFi'>
        </div>
        <div class='col field'>
          <label for='password'>Password STA</label>
          <input id='password' type='password' placeholder='********'>
        </div>
      </div>
      <div class='field'>
        <label for='hostname'>Hostname DNS</label>
        <input id='hostname' type='text' placeholder='duxman-led-1'>
      </div>
    </div>

    <div class='card'>
      <h2>Tiempo / NTP</h2>
      <p class='hint'>Al arrancar y al conectar STA, el equipo intentara sincronizar la hora con el servidor NTP configurado.</p>
      <div class='row'>
        <div class='col field'>
          <label for='ntpSyncOnBoot'>Sincronizar en arranque</label>
          <select id='ntpSyncOnBoot'>
            <option value='true'>true</option>
            <option value='false'>false</option>
          </select>
        </div>
        <div class='col field'>
          <label for='ntpServer'>Servidor NTP</label>
          <input id='ntpServer' type='text' placeholder='europe.pool.ntp.org'>
        </div>
      </div>
    </div>

    <div class='card'>
      <h2>IP AP</h2>
      <div class='row'>
        <div class='col field'>
          <label for='apMode'>Modo IP AP</label>
          <select id='apMode'>
            <option value='dhcp'>dhcp</option>
            <option value='static'>static</option>
          </select>
        </div>
        <div class='col field'>
          <label for='apAddress'>IP AP</label>
          <input id='apAddress' type='text' placeholder='192.168.4.1'>
        </div>
      </div>
      <div class='row'>
        <div class='col field'>
          <label for='apGateway'>Gateway AP</label>
          <input id='apGateway' type='text' placeholder='192.168.4.1'>
        </div>
        <div class='col field'>
          <label for='apSubnet'>Subnet AP</label>
          <input id='apSubnet' type='text' placeholder='255.255.255.0'>
        </div>
      </div>
    </div>

    <div class='card'>
      <h2>IP STA</h2>
      <div class='row'>
        <div class='col field'>
          <label for='staMode'>Modo IP STA</label>
          <select id='staMode'>
            <option value='dhcp'>dhcp</option>
            <option value='static'>static</option>
          </select>
        </div>
        <div class='col field'>
          <label for='staAddress'>IP STA</label>
          <input id='staAddress' type='text' placeholder='192.168.1.50'>
        </div>
      </div>
      <div class='row'>
        <div class='col field'>
          <label for='staGateway'>Gateway STA</label>
          <input id='staGateway' type='text' placeholder='192.168.1.1'>
        </div>
        <div class='col field'>
          <label for='staSubnet'>Subnet STA</label>
          <input id='staSubnet' type='text' placeholder='255.255.255.0'>
        </div>
      </div>
      <div class='row'>
        <div class='col field'>
          <label for='staDns1'>DNS 1 STA</label>
          <input id='staDns1' type='text' placeholder='8.8.8.8'>
        </div>
        <div class='col field'>
          <label for='staDns2'>DNS 2 STA</label>
          <input id='staDns2' type='text' placeholder='1.1.1.1'>
        </div>
      </div>
    </div>

    <div class='card json-card'>
      <h2>Payload / Respuesta</h2>
      <h3>Payload Enviado</h3>
      <pre id='payloadOut'>{}</pre>
      <h3>Respuesta API</h3>
      <pre id='resultOut'>Sin llamadas aun.</pre>
    </div>
  </main>

  <script>
    if(localStorage.getItem('dux_show_json')==='1') document.body.classList.add('show-json');

    function byId(id) { return document.getElementById(id); }

    function setStatus(message, isError) {
      const el = byId('status');
      el.textContent = message;
      el.className = isError ? 'hint err' : 'hint ok';
    }

    function setValue(id, value) {
      byId(id).value = value == null ? '' : String(value);
    }

    function fillFromConfig(cfg) {
      const network = cfg && cfg.network ? cfg.network : {};
      const wifi = network.wifi || {};
      const conn = wifi.connection || {};
      const ip = network.ip || {};
      const ap = ip.ap || {};
      const sta = ip.sta || {};
      const dns = network.dns || {};
      const time = network.time || {};

      setValue('wifiMode', wifi.mode || 'ap');
      setValue('apAvailability', wifi.apAvailability || 'always');
      setValue('ssid', conn.ssid || '');
      setValue('password', conn.password || '');
      setValue('hostname', dns.hostname || 'duxman-led');
      setValue('ntpSyncOnBoot', String(time.syncOnBoot !== false));
      setValue('ntpServer', time.ntpServer || 'europe.pool.ntp.org');

      setValue('apMode', ap.mode || 'static');
      setValue('apAddress', ap.address || '');
      setValue('apGateway', ap.gateway || '');
      setValue('apSubnet', ap.subnet || '');

      setValue('staMode', sta.mode || 'dhcp');
      setValue('staAddress', sta.address || '');
      setValue('staGateway', sta.gateway || '');
      setValue('staSubnet', sta.subnet || '');
      setValue('staDns1', sta.primaryDns || '');
      setValue('staDns2', sta.secondaryDns || '');
    }

    function readPayload() {
      return {
        network: {
          wifi: {
            mode: byId('wifiMode').value,
            apAvailability: byId('apAvailability').value,
            connection: {
              ssid: byId('ssid').value,
              password: byId('password').value
            }
          },
          ip: {
            ap: {
              mode: byId('apMode').value,
              address: byId('apAddress').value,
              gateway: byId('apGateway').value,
              subnet: byId('apSubnet').value
            },
            sta: {
              mode: byId('staMode').value,
              address: byId('staAddress').value,
              gateway: byId('staGateway').value,
              subnet: byId('staSubnet').value,
              primaryDns: byId('staDns1').value,
              secondaryDns: byId('staDns2').value
            }
          },
          dns: {
            hostname: byId('hostname').value
          },
          time: {
            syncOnBoot: byId('ntpSyncOnBoot').value === 'true',
            ntpServer: byId('ntpServer').value
          }
        }
      };
    }

    async function loadConfig() {
      setStatus('Leyendo configuracion...', false);
      try {
        const res = await fetch('/api/v1/config/network');
        const text = await res.text();
        const parsed = JSON.parse(text);
        fillFromConfig(parsed);
        byId('resultOut').textContent = JSON.stringify(parsed, null, 2);
        setStatus('Configuracion cargada (HTTP ' + res.status + ').', false);
      } catch (error) {
        byId('resultOut').textContent = String(error);
        setStatus('Error leyendo configuracion.', true);
      }
    }

    async function saveConfig() {
      const payload = readPayload();
      byId('payloadOut').textContent = JSON.stringify(payload, null, 2);
      setStatus('Guardando configuracion...', false);

      try {
        const res = await fetch('/api/v1/config/network', {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        const text = await res.text();
        let pretty = text;
        try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
        byId('resultOut').textContent = pretty;

        if (res.ok) {
          setStatus('Configuracion guardada y aplicada (HTTP ' + res.status + ').', false);
        } else {
          setStatus('Error al guardar (HTTP ' + res.status + ').', true);
        }
      } catch (error) {
        byId('resultOut').textContent = String(error);
        setStatus('Error de red al guardar.', true);
      }
    }

    loadConfig();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildMicrophoneConfigHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN Microphone Config</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>Configuracion de Microfono</h1>
      <p class='hint'>Perfil actual soportado: <strong>generic_i2c</strong> con pines GLEDOPTO (SD=26, WS=5, SCK=21).</p>
      <div class='actions'>
        <button class='btn alt' onclick='loadConfig()'>Recargar</button>
        <button class='btn' onclick='saveConfig()'>Guardar</button>
      </div>
      <p id='status' class='hint'>Cargando...</p>
    </div>

    <div class='card'>
      <div class='field'>
        <label><input id='enabled' type='checkbox'> Microfono habilitado</label>
      </div>
      <div class='row'>
        <div class='col field'>
          <label for='source'>Source</label>
          <select id='source'>
            <option value='generic_i2c'>generic_i2c</option>
          </select>
        </div>
        <div class='col field'>
          <label for='profileId'>Profile</label>
          <select id='profileId'>
            <option value='gledopto_gl_c_017wl_d'>gledopto_gl_c_017wl_d</option>
            <option value='DEFAULT'>DEFAULT</option>
          </select>
        </div>
      </div>
      <div class='row'>
        <div class='col field'>
          <label for='sampleRate'>Sample Rate</label>
          <input id='sampleRate' type='number' min='8000' max='48000' step='1000'>
        </div>
        <div class='col field'>
          <label for='fftSize'>FFT Size</label>
          <select id='fftSize'>
            <option value='256'>256</option>
            <option value='512'>512</option>
            <option value='1024'>1024</option>
            <option value='2048'>2048</option>
          </select>
        </div>
      </div>
      <div class='row'>
        <div class='col field'>
          <label for='gainPercent'>Gain %</label>
          <input id='gainPercent' type='number' min='1' max='200'>
        </div>
        <div class='col field'>
          <label for='noiseFloorPercent'>Noise floor %</label>
          <input id='noiseFloorPercent' type='number' min='0' max='100'>
        </div>
      </div>
    </div>

    <div class='card'>
      <h2>Pines</h2>
      <p class='hint'>SD = DIN (datos), WS = LRCLK, SCK = BCLK.</p>
      <div class='row'>
        <div class='col field'>
          <label for='din'>SD / DIN</label>
          <input id='din' type='number' min='-1' max='48'>
        </div>
        <div class='col field'>
          <label for='ws'>WS</label>
          <input id='ws' type='number' min='-1' max='48'>
        </div>
        <div class='col field'>
          <label for='bclk'>SCK / BCLK</label>
          <input id='bclk' type='number' min='-1' max='48'>
        </div>
      </div>
      <div class='actions'>
        <button class='btn alt' onclick='applyGledoptoPins()'>Aplicar pines GLEDOPTO (26/5/21)</button>
      </div>
    </div>

    <div class='card json-card'>
      <h3>Payload</h3>
      <pre id='payloadOut'>{}</pre>
      <h3>Respuesta</h3>
      <pre id='resultOut'>Sin llamadas aun.</pre>
    </div>
  </main>

  <script>
    if(localStorage.getItem('dux_show_json')==='1') document.body.classList.add('show-json');

    function byId(id) { return document.getElementById(id); }
    function setStatus(message, isError) {
      const el = byId('status');
      el.textContent = message;
      el.className = isError ? 'hint err' : 'hint ok';
    }
    function setValue(id, value) { byId(id).value = value == null ? '' : String(value); }

    function applyGledoptoPins() {
      setValue('din', 26);
      setValue('ws', 5);
      setValue('bclk', 21);
      setValue('source', 'generic_i2c');
      setValue('profileId', 'gledopto_gl_c_017wl_d');
    }

    function fillConfig(data) {
      const m = data && data.microphone ? data.microphone : {};
      const p = m.pins || {};
      byId('enabled').checked = !!m.enabled;
      setValue('source', m.source || 'generic_i2c');
      setValue('profileId', m.profileId || 'gledopto_gl_c_017wl_d');
      setValue('sampleRate', m.sampleRate == null ? 16000 : m.sampleRate);
      setValue('fftSize', m.fftSize == null ? 512 : m.fftSize);
      setValue('gainPercent', m.gainPercent == null ? 100 : m.gainPercent);
      setValue('noiseFloorPercent', m.noiseFloorPercent == null ? 8 : m.noiseFloorPercent);
      setValue('din', p.din == null ? 26 : p.din);
      setValue('ws', p.ws == null ? 5 : p.ws);
      setValue('bclk', p.bclk == null ? 21 : p.bclk);
    }

    function buildPayload() {
      return {
        microphone: {
          enabled: byId('enabled').checked,
          source: byId('source').value,
          profileId: byId('profileId').value,
          sampleRate: Number(byId('sampleRate').value || 16000),
          fftSize: Number(byId('fftSize').value || 512),
          gainPercent: Number(byId('gainPercent').value || 100),
          noiseFloorPercent: Number(byId('noiseFloorPercent').value || 8),
          pins: {
            din: Number(byId('din').value || 26),
            ws: Number(byId('ws').value || 5),
            bclk: Number(byId('bclk').value || 21)
          }
        }
      };
    }

    async function loadConfig() {
      setStatus('Leyendo configuracion de microfono...', false);
      try {
        const res = await fetch('/api/v1/config/microphone');
        const text = await res.text();
        const parsed = JSON.parse(text);
        fillConfig(parsed);
        byId('resultOut').textContent = JSON.stringify(parsed, null, 2);
        setStatus('Configuracion cargada (HTTP ' + res.status + ').', false);
      } catch (error) {
        byId('resultOut').textContent = String(error);
        setStatus('Error leyendo configuracion.', true);
      }
    }

    async function saveConfig() {
      const payload = buildPayload();
      byId('payloadOut').textContent = JSON.stringify(payload, null, 2);
      setStatus('Guardando configuracion de microfono...', false);
      try {
        const res = await fetch('/api/v1/config/microphone', {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        const text = await res.text();
        let pretty = text;
        try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
        byId('resultOut').textContent = pretty;
        setStatus(res.ok ? 'Microfono guardado (HTTP ' + res.status + ').' : 'Error guardando (HTTP ' + res.status + ').', !res.ok);
      } catch (error) {
        byId('resultOut').textContent = String(error);
        setStatus('Error de red guardando configuracion.', true);
      }
    }

    loadConfig();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildDebugConfigHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN Debug Config</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>Configuracion Debug</h1>
      <p class='hint'>Controla trazas debug y el intervalo del heartbeat serial.</p>
      <div class='actions'>
        <button class='btn alt' onclick='loadDebug()'>Recargar</button>
        <button class='btn' onclick='saveDebug()'>Guardar</button>
      </div>
      <p id='status' class='hint'>Cargando...</p>
    </div>

    <div class='card'>
      <div class='field'>
        <label><input id='debugEnabled' type='checkbox'> Debug habilitado</label>
      </div>
      <div class='field'>
        <label for='heartbeatMs'>Heartbeat ms (0 desactiva)</label>
        <input id='heartbeatMs' type='number' min='0' max='600000' step='100'>
      </div>
    </div>

    <div class='card'>
      <h3>Preferencias UI</h3>
      <div class='field'>
        <label style='display:flex;align-items:center;gap:8px;cursor:pointer'>
          <input id='showJsonChk' type='checkbox' onchange='toggleJson(this.checked)'>
          <span>Mostrar respuestas JSON en las paginas de configuracion</span>
        </label>
        <p class='hint' style='margin-top:4px'>Se guarda en el navegador (localStorage). Por defecto desactivado.</p>
      </div>
    </div>

    <div class='card json-card'>
      <h3>Payload</h3>
      <pre id='payloadOut'>{}</pre>
      <h3>Respuesta</h3>
      <pre id='resultOut'>Sin llamadas aun.</pre>
    </div>
  </main>

  <script>
    function byId(id) { return document.getElementById(id); }
    function setStatus(message, isError) {
      const el = byId('status');
      el.textContent = message;
      el.className = isError ? 'hint err' : 'hint ok';
    }

    function toggleJson(checked) {
      localStorage.setItem('dux_show_json', checked ? '1' : '0');
      document.body.classList.toggle('show-json', checked);
    }

    function buildPayload() {
      return {
        debug: {
          enabled: byId('debugEnabled').checked,
          heartbeatMs: Number(byId('heartbeatMs').value || 0)
        }
      };
    }

    async function loadDebug() {
      // leer preferencia del browser
      const showJson = localStorage.getItem('dux_show_json') === '1';
      byId('showJsonChk').checked = showJson;
      document.body.classList.toggle('show-json', showJson);

      setStatus('Leyendo configuracion debug...', false);
      try {
        const res = await fetch('/api/v1/config/debug');
        const data = await res.json();
        const dbg = data.debug || {};
        byId('debugEnabled').checked = !!dbg.enabled;
        byId('heartbeatMs').value = dbg.heartbeatMs == null ? 5000 : dbg.heartbeatMs;
        byId('resultOut').textContent = JSON.stringify(data, null, 2);
        setStatus('Configuracion debug cargada (HTTP ' + res.status + ').', false);
      } catch (error) {
        byId('resultOut').textContent = String(error);
        setStatus('Error leyendo debug.', true);
      }
    }

    async function saveDebug() {
      const payload = buildPayload();
      byId('payloadOut').textContent = JSON.stringify(payload, null, 2);
      setStatus('Guardando configuracion debug...', false);
      try {
        const res = await fetch('/api/v1/config/debug', {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        const text = await res.text();
        let pretty = text;
        try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
        byId('resultOut').textContent = pretty;
        setStatus(res.ok ? 'Debug guardado (HTTP ' + res.status + ').' : 'Error guardando debug (HTTP ' + res.status + ').', !res.ok);
      } catch (error) {
        byId('resultOut').textContent = String(error);
        setStatus('Error de red guardando debug.', true);
      }
    }

    loadDebug();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildApiConfigMicrophoneHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN API - Config Microphone</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API Config Microphone</h1>
      <button onclick='getCfg()'>GET /api/v1/config/microphone</button>
      <button onclick='patchCfg()'>PATCH /api/v1/config/microphone</button>
      <textarea id='payload'>{"microphone":{"enabled":false,"source":"generic_i2c","profileId":"gledopto_gl_c_017wl_d","sampleRate":16000,"fftSize":512,"gainPercent":100,"noiseFloorPercent":8,"pins":{"din":26,"ws":5,"bclk":21}}}</textarea>
    </div>
    <div class='card'><pre id='out'>Sin llamadas aun.</pre></div>
  </main>
  <script>
    async function callApi(method, body) {
      const options = { method, headers: { 'Content-Type': 'application/json' } };
      if (body) options.body = body;
      const res = await fetch('/api/v1/config/microphone', options);
      const text = await res.text();
      let parsed = text;
      try { parsed = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      document.getElementById('out').textContent = method + ' /api/v1/config/microphone -> HTTP ' + res.status + '\n\n' + parsed;
    }
    function getCfg() { callApi('GET'); }
    function patchCfg() { callApi('PATCH', document.getElementById('payload').value); }
    getCfg();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildApiConfigDebugHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN API - Config Debug</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API Config Debug</h1>
      <button onclick='getCfg()'>GET /api/v1/config/debug</button>
      <button onclick='patchCfg()'>PATCH /api/v1/config/debug</button>
      <textarea id='payload'>{"debug":{"enabled":true,"heartbeatMs":5000}}</textarea>
    </div>
    <div class='card'><pre id='out'>Sin llamadas aun.</pre></div>
  </main>
  <script>
    async function callApi(method, body) {
      const options = { method, headers: { 'Content-Type': 'application/json' } };
      if (body) options.body = body;
      const res = await fetch('/api/v1/config/debug', options);
      const text = await res.text();
      let parsed = text;
      try { parsed = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      document.getElementById('out').textContent = method + ' /api/v1/config/debug -> HTTP ' + res.status + '\n\n' + parsed;
    }
    function getCfg() { callApi('GET'); }
    function patchCfg() { callApi('PATCH', document.getElementById('payload').value); }
    getCfg();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildGpioConfigHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN GPIO Config</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>Configuracion LED (GPIO)</h1>
      <p class='hint'>Configura las salidas LED del controlador. Max __MAX_OUTPUTS__ salidas.</p>
      <div class='actions'>
        <button class='btn alt' onclick='loadGpio()'>Recargar</button>
        <button class='btn' onclick='saveGpio()'>Guardar</button>
      </div>
      <p id='status' class='hint'>Cargando...</p>
    </div>

    <div class='card' id='outputsCard'>
      <div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:10px'>
        <h2>Salidas LED</h2>
        <button class='btn' onclick='addOutput()' id='btnAdd'>+ Agregar salida</button>
      </div>
      <div id='outputsList'></div>
    </div>

    <div class='card'>
      <h2>Referencia de Pines</h2>
      <p class='hint'>Pin de compilacion (BuildProfile): <strong>__BUILD_PIN__</strong> &middot; LEDs: <strong>__BUILD_COUNT__</strong></p>
      <table>
        <tr><th>Placa</th><th>Pin LED tipico</th><th>Notas</th></tr>
        <tr><td>ESP32-C3 Super Mini</td><td>GPIO 8</td><td>GPIO 0-5 strapping pins</td></tr>
        <tr><td>ESP32 DevKit</td><td>GPIO 5</td><td>GPIO 34-39 solo entrada</td></tr>
        <tr><td>ESP32-S3 DevKit</td><td>GPIO 48</td><td>GPIO 0,3,45,46 strapping</td></tr>
      </table>
    </div>

    <div class='card json-card'>
      <h3>Payload</h3>
      <pre id='payloadOut'>{}</pre>
      <h3>Respuesta</h3>
      <pre id='resultOut'>Sin llamadas aun.</pre>
    </div>
  </main>

  <script>
    if(localStorage.getItem('dux_show_json')==='1') document.body.classList.add('show-json');

    const MAX_OUTPUTS = __MAX_OUTPUTS__;
    const INPUT_ONLY = [34,35,36,39];
    const STRAPPING_C3 = [0,1,2,3,4,5];
    const STRAPPING_S3 = [0,3,45,46];
    const LED_TYPES = ['digital','ws2812b','ws2811','ws2813','ws2815','sk6812','tm1814'];
    const COLOR_ORDERS_3CH = ['GRB','RGB','BRG','RBG','GBR','BGR'];
    const COLOR_ORDERS_4CH = ['RGBW','GRBW'];
    const DIGITAL_COLORS = ['R','G','B','W'];
    const ALL_ORDERS = COLOR_ORDERS_3CH.concat(COLOR_ORDERS_4CH);

    function isDigital(type) { return type === 'digital'; }
    function colorOptionsFor(type) { return isDigital(type) ? DIGITAL_COLORS : ALL_ORDERS; }

    let outputs = [];

    function byId(id) { return document.getElementById(id); }
    function setStatus(msg, isErr) {
      const el = byId('status');
      el.textContent = msg;
      el.className = isErr ? 'hint err' : 'hint ok';
    }

    function pinWarning(pin) {
      if (INPUT_ONLY.includes(pin)) return 'GPIO ' + pin + ' es solo entrada en ESP32 clasico. No usar para LEDs.';
      if (STRAPPING_C3.includes(pin)) return 'GPIO ' + pin + ' es strapping pin en ESP32-C3. Usar con precaucion.';
      if (STRAPPING_S3.includes(pin)) return 'GPIO ' + pin + ' es strapping pin en ESP32-S3. Usar con precaucion.';
      return '';
    }

    function buildSelect(id, options, selected) {
      let s = '<select id="' + id + '">';
      for (const o of options) {
        s += '<option value="' + o + '"' + (o === selected ? ' selected' : '') + '>' + o.toUpperCase() + '</option>';
      }
      return s + '</select>';
    }

    function renderOutputs() {
      const list = byId('outputsList');
      list.innerHTML = '';
      byId('btnAdd').style.display = outputs.length >= MAX_OUTPUTS ? 'none' : '';

      for (let i = 0; i < outputs.length; i++) {
        const o = outputs[i];
        const dig = isDigital(o.ledType);
        const warn = pinWarning(o.pin);
        const colorOpts = colorOptionsFor(o.ledType);
        const colorLabel = dig ? 'Color LED' : 'Orden Color';
        const block = document.createElement('div');
        block.className = 'output-block';
        block.innerHTML =
          '<div class="output-header"><h2>Salida #' + i + (dig ? ' (digital)' : '') + '</h2>' +
          (outputs.length > 1 ? '<button class="btn danger" onclick="removeOutput(' + i + ')">Eliminar</button>' : '') +
          '</div>' +
          '<div class="row">' +
            '<div class="col field"><label>GPIO Pin</label><input type="number" min="-1" max="48" value="' + o.pin + '" onchange="updField(' + i + ',\'pin\',Number(this.value))"></div>' +
            (dig ? '' : '<div class="col field"><label>Cantidad LEDs</label><input type="number" min="1" max="1500" value="' + o.ledCount + '" onchange="updField(' + i + ',\'ledCount\',Number(this.value))"></div>') +
          '</div>' +
          '<div class="row">' +
            '<div class="col field"><label>Tipo LED</label>' + buildSelect('type_' + i, LED_TYPES, o.ledType) + '</div>' +
            '<div class="col field"><label>' + colorLabel + '</label>' + buildSelect('order_' + i, colorOpts, o.colorOrder) + '</div>' +
          '</div>' +
          (warn ? '<div class="warn">' + warn + '</div>' : '');
        list.appendChild(block);

        byId('type_' + i).addEventListener('change', function() {
          const newType = this.value;
          if (isDigital(newType)) {
            outputs[i].ledCount = 1;
            if (!DIGITAL_COLORS.includes(outputs[i].colorOrder)) outputs[i].colorOrder = 'W';
          } else {
            if (DIGITAL_COLORS.includes(outputs[i].colorOrder)) outputs[i].colorOrder = 'GRB';
          }
          outputs[i].ledType = newType;
          renderOutputs();
        });
        byId('order_' + i).addEventListener('change', function() { updField(i, 'colorOrder', this.value); });
      }
    }

    function updField(idx, field, val) {
      outputs[idx][field] = val;
      renderOutputs();
    }

    function addOutput() {
      if (outputs.length >= MAX_OUTPUTS) return;
      outputs.push({ pin: -1, ledCount: 1, ledType: 'ws2812b', colorOrder: 'GRB' });
      renderOutputs();
    }

    function removeOutput(idx) {
      if (outputs.length <= 1) return;
      outputs.splice(idx, 1);
      renderOutputs();
    }

    function fillFromConfig(cfg) {
      const gpio = cfg && cfg.gpio ? cfg.gpio : {};
      const arr = gpio.outputs || [];
      outputs = arr.map(function(o) {
        return {
          pin: o.pin != null ? o.pin : -1,
          ledCount: o.ledCount != null ? o.ledCount : 1,
          ledType: o.ledType || 'ws2812b',
          colorOrder: o.colorOrder || 'GRB'
        };
      });
      if (outputs.length === 0) outputs.push({ pin: -1, ledCount: 1, ledType: 'ws2812b', colorOrder: 'GRB' });
      renderOutputs();
    }

    function buildPayload() {
      return {
        gpio: {
          outputs: outputs.map(function(o, i) {
            return { id: i, pin: o.pin, ledCount: o.ledCount, ledType: o.ledType, colorOrder: o.colorOrder };
          })
        }
      };
    }

    async function loadGpio() {
      setStatus('Leyendo configuracion GPIO...', false);
      try {
        const res = await fetch('/api/v1/config/gpio');
        const data = await res.json();
        fillFromConfig(data);
        byId('resultOut').textContent = JSON.stringify(data, null, 2);
        setStatus('Configuracion GPIO cargada (HTTP ' + res.status + ').', false);
      } catch (err) {
        byId('resultOut').textContent = String(err);
        setStatus('Error leyendo GPIO.', true);
      }
    }

    async function saveGpio() {
      const payload = buildPayload();
      byId('payloadOut').textContent = JSON.stringify(payload, null, 2);
      setStatus('Guardando configuracion GPIO...', false);
      try {
        const res = await fetch('/api/v1/config/gpio', {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        const text = await res.text();
        let pretty = text;
        try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
        byId('resultOut').textContent = pretty;
        setStatus(res.ok ? 'GPIO guardado (HTTP ' + res.status + ').' : 'Error guardando GPIO (HTTP ' + res.status + ').', !res.ok);
      } catch (err) {
        byId('resultOut').textContent = String(err);
        setStatus('Error de red guardando GPIO.', true);
      }
    }

    loadGpio();
  </script>
</div>
</body>
</html>
)HTML";
  const String buildPin = String(BuildProfile::kLedPin);
  const String buildCount = String(BuildProfile::kLedCount);
  const String maxOutputs = String(kMaxLedOutputs);
  html.replace("__BUILD_PIN__", buildPin);
  html.replace("__BUILD_COUNT__", buildCount);
  html.replace("__MAX_OUTPUTS__", maxOutputs);
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildApiConfigGpioHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN API - Config GPIO</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API Config GPIO</h1>
      <button onclick='getCfg()'>GET /api/v1/config/gpio</button>
      <button onclick='patchCfg()'>PATCH /api/v1/config/gpio</button>
      <textarea id='payload'>{"gpio":{"outputs":[{"pin":8,"ledCount":60,"ledType":"ws2812b","colorOrder":"GRB"}]}}</textarea>
    </div>
    <div class='card'><pre id='out'>Sin llamadas aun.</pre></div>
  </main>
  <script>
    async function callApi(method, body) {
      const options = { method, headers: { 'Content-Type': 'application/json' } };
      if (body) options.body = body;
      const res = await fetch('/api/v1/config/gpio', options);
      const text = await res.text();
      let parsed = text;
      try { parsed = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      document.getElementById('out').textContent = method + ' /api/v1/config/gpio -> HTTP ' + res.status + '\n\n' + parsed;
    }
    function getCfg() { callApi('GET'); }
    function patchCfg() { callApi('PATCH', document.getElementById('payload').value); }
    getCfg();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildVersionHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN - Version</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
<main>
  <div class='hero'>
    <img src='data:image/png;base64,__LOGO_B64__' alt='DUXMAN Logo'>
    <h1 id='fwVersionTitle'>DUXMAN LED NEXT</h1>
    <p class='tagline' id='fwBranch'>Cargando...</p>
  </div>

  <p class='section-title'>Firmware</p>
  <div class='grid' id='gridRelease'><div class='loader'>Cargando datos de firmware...</div></div>

  <p class='section-title'>Hardware</p>
  <div class='grid' id='gridHardware'><div class='loader'>Cargando datos de hardware...</div></div>

  <p class='section-title'>Sistema</p>
  <div class='grid' id='gridSystem'><div class='loader'>Cargando sistema...</div></div>
</main>
<script>
  function row(lbl, val, cls) {
    return '<div class="row"><span class="lbl">' + lbl + '</span><span class="val' + (cls ? ' ' + cls : '') + '">' + val + '</span></div>';
  }
  function barRow(lbl, used, total, unit) {
    const pct = total > 0 ? Math.round(used * 100 / total) : 0;
    const cls = pct > 75 ? 'warn' : 'ok';
    return '<div class="row" style="flex-direction:column;align-items:stretch"><div style="display:flex;justify-content:space-between"><span class="lbl">' + lbl + '</span><span class="val ' + cls + '">' + pct + '%</span></div>' +
      '<div class="bar-wrap"><div class="bar-fill" style="width:' + pct + '%"></div></div>' +
      '<div style="font-size:11px;color:var(--muted);text-align:right;margin-top:2px">' + used.toLocaleString() + ' / ' + total.toLocaleString() + ' ' + unit + '</div></div>';
  }
  function boolRow(lbl, val) {
    const on = !!val;
    return '<div class="row"><span class="lbl">' + lbl + '</span><span class="val"><span class="chip-bool"><span class="dot ' + (on ? 'on' : 'off') + '"></span>' + (on ? 'Si' : 'No') + '</span></span></div>';
  }
  function card(title, body) {
    return '<div class="card"><h2>' + title + '</h2>' + body + '</div>';
  }
  function fmtUptime(s) {
    const d = Math.floor(s/86400), h = Math.floor((s%86400)/3600), m = Math.floor((s%3600)/60), ss = s%60;
    return (d>0?d+'d ':'')+String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(ss).padStart(2,'0');
  }

  async function load() {
    try {
      const [relRes, hwRes, diagRes] = await Promise.all([
        fetch('/api/v1/release'),
        fetch('/api/v1/hardware'),
        fetch('/api/v1/diag')
      ]);
      const rel  = await relRes.json();
      const hw   = await hwRes.json();
      const diag = await diagRes.json();

      // Hero
      document.getElementById('fwVersionTitle').textContent = 'DUXMAN LED NEXT ' + (rel.version || '');
      document.getElementById('fwBranch').textContent = (rel.branch || '') + ' — ' + (rel.board || '');

      // Release card
      document.getElementById('gridRelease').innerHTML =
        card('Version del firmware',
          row('Version', '<span class="badge">' + (rel.version || '?') + '</span>') +
          row('Fecha de release', rel.releaseDate || '?') +
          row('Rama / branch', rel.branch || '?', 'accent') +
          row('Placa / board', rel.board || '?') +
          (rel.repositoryUrl ? '<div class="row"><span class="lbl">Repositorio</span><a class="link" href="' + rel.repositoryUrl + '" target="_blank">' + rel.repositoryUrl + '</a></div>' : '')
        );

      // Hardware cards
      const flashMb = (hw.flashSizeBytes / (1024*1024)).toFixed(1);
      const freqGHz = (hw.flashSpeedHz / 1e6).toFixed(0);
      document.getElementById('gridHardware').innerHTML =
        card('Procesador',
          row('Modelo chip', hw.chipModel || '?', 'accent') +
          row('Revision', hw.chipRevision !== undefined ? 'rev ' + hw.chipRevision : '?') +
          row('Nucleos', hw.cores || '?') +
          row('Frecuencia CPU', (hw.cpuFreqMHz || '?') + ' MHz')
        ) +
        card('Conectividad',
          boolRow('Wi-Fi', hw.hasWifi) +
          boolRow('Bluetooth Classic', hw.hasBluetoothClassic) +
          boolRow('Bluetooth LE', hw.hasBluetoothLe)
        ) +
        card('Memoria Flash',
          row('Tamano', flashMb + ' MB') +
          row('Velocidad', freqGHz + ' MHz') +
          row('Direccion MAC', hw.mac || '?', 'accent')
        );

      // System/diag cards
      const heapFree = diag.heap_free_bytes || 0;
      const heapTotal = diag.heap_total_bytes || 1;
      const uptimeSec = diag.uptime_s || 0;
      document.getElementById('gridSystem').innerHTML =
        card('Memoria RAM',
          barRow('Heap usado', heapTotal - heapFree, heapTotal, 'bytes') +
          row('Libre', heapFree.toLocaleString() + ' B') +
          row('Total', heapTotal.toLocaleString() + ' B')
        ) +
        card('Uptime y tareas',
          row('Tiempo en linea', fmtUptime(uptimeSec), 'accent') +
          row('Tareas activas', diag.task_count || '?')
        ) +
        card('Watchdog',
          row('Resets totales', diag.watchdog ? diag.watchdog.reset_count : '?', diag.watchdog && diag.watchdog.reset_count > 0 ? 'warn' : 'ok') +
          row('Control task resets', diag.watchdog ? diag.watchdog.control_task_resets : '?') +
          row('Render task resets', diag.watchdog ? diag.watchdog.render_task_resets : '?')
        );

    } catch (err) {
      document.getElementById('gridRelease').innerHTML = '<div class="loader" style="color:#f87171">Error: ' + err + '</div>';
    }
  }
  load();
</script>
</div>
</body>
</html>
)HTML";
  html.replace("__LOGO_B64__", "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");
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildProfilesConfigHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN Perfiles</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
<main>
  <div class='card'>
    <h1 style='margin:0 0 6px'>Perfiles de Configuracion</h1>
    <p class='hint'>Un perfil guarda toda la configuracion del dispositivo: red Wi-Fi, GPIO, microfono y debug. El perfil <strong>default</strong> siempre refleja la configuracion activa en el dispositivo.</p>
    <div class='actions'>
      <button class='btn alt' onclick='loadProfiles()'>&#8635; Actualizar lista</button>
    </div>
    <p id='status' class='hint'>Cargando...</p>
  </div>

  <div class='card'>
    <h2>Guardar configuracion actual como perfil</h2>
    <p class='hint'>Captura un snapshot completo de toda la configuracion activa (red, GPIO, microfono, debug) y lo guarda con el nombre indicado. No tienes que escribir ningun JSON.</p>
    <div class='row'>
      <div class='field'>
        <label for='newId'>ID del perfil <span style='color:#a12626'>*</span></label>
        <input id='newId' type='text' placeholder='salon_luces' maxlength='48'>
        <span class='hint'>Solo minusculas, numeros, guiones y guion bajo.</span>
      </div>
      <div class='field'>
        <label for='newName'>Nombre</label>
        <input id='newName' type='text' placeholder='Salon luces RGB'>
      </div>
    </div>
    <div class='field'>
      <label for='newDesc'>Descripcion (opcional)</label>
      <input id='newDesc' type='text' placeholder='4 tiras WS2812B, microfono I2S, red Casa...'>
    </div>
    <div class='actions'>
      <button class='btn' onclick='saveCurrentAsProfile()'>Guardar como perfil</button>
    </div>
  </div>

  <div class='card'>
    <h2>Perfiles disponibles</h2>
    <p class='hint'>Haz clic en <em>Activar</em> para cargar un perfil y aplicarlo al dispositivo. <em>Ver config</em> muestra el JSON completo guardado.</p>
    <div id='profileList'><p class='hint'>Cargando...</p></div>
  </div>

  <div class='resp-card card'>
    <h3 style='margin:0 0 8px'>Respuesta</h3>
    <pre id='resultOut'>Sin llamadas aun.</pre>
  </div>
</main>
<script>
  function byId(id) { return document.getElementById(id); }
  function setStatus(msg, isErr) {
    const el = byId('status');
    el.textContent = msg;
    el.className = isErr ? 'hint err' : 'hint ok';
  }

  async function loadProfiles() {
    setStatus('Cargando perfiles...', false);
    try {
      const res = await fetch('/api/v1/profiles');
      const text = await res.text();
      const data = JSON.parse(text);
      renderProfiles(data);
      setStatus('Perfiles cargados (HTTP ' + res.status + ').', false);
    } catch (err) {
      setStatus('Error cargando perfiles: ' + err, true);
    }
  }

  function renderProfiles(data) {
    const container = byId('profileList');
    const items = (((data || {}).profiles || {}).items) || [];
    const defaultId = ((data || {}).profiles || {}).defaultId || '';
    if (items.length === 0) {
      container.innerHTML = "<p class='hint'>No hay perfiles.</p>";
      return;
    }
    const grid = document.createElement('div');
    grid.className = 'profile-grid';
    for (const item of items) {
      const isDefault = item.id === defaultId;
      const isBuiltIn = !!item.builtIn;
      const card = document.createElement('div');
      card.className = 'profile-card' + (isDefault ? ' is-default' : '');
      const badges = [];
      if (isBuiltIn) badges.push("<span class='badge builtin'>integrado</span>");
      else badges.push("<span class='badge user'>usuario</span>");
      if (isDefault) badges.push("<span class='badge default'>activo por defecto</span>");
      if (item.readOnly) badges.push("<span class='badge'>solo lectura</span>");
      let acts = '<button class="btn sm alt" onclick="viewProfile(\'' + item.id + '\')" title="Ver JSON completo guardado">Ver config</button>';
      if (!isDefault) {
        acts += '<button class="btn sm" onclick="activateProfile(\'' + item.id + '\')" title="Aplicar este perfil al dispositivo y ponerlo como predeterminado">Activar</button>';
      }
      if (!isBuiltIn) {
        acts += '<button class="btn sm danger" onclick="deleteProfile(\'' + item.id + '\')">Borrar</button>';
      }
      card.innerHTML =
        '<p class="pname">' + (item.name || item.id) + '</p>' +
        '<p class="pid">' + item.id + '</p>' +
        '<p class="pdesc">' + (item.description || '') + '</p>' +
        '<div class="badges">' + badges.join('') + '</div>' +
        '<div class="pactions">' + acts + '</div>';
      grid.appendChild(card);
    }
    container.innerHTML = '';
    container.appendChild(grid);
  }

  async function saveCurrentAsProfile() {
    const id = byId('newId').value.trim();
    if (!id) { setStatus('El ID del perfil es obligatorio.', true); return; }
    const payload = { profile: {
      id: id,
      name: byId('newName').value.trim() || id,
      description: byId('newDesc').value.trim()
    }};
    const ok = await callJson('/api/v1/profiles/save', payload, 'Guardando snapshot de configuracion...');
    if (ok) {
      byId('newId').value = '';
      byId('newName').value = '';
      byId('newDesc').value = '';
      await loadProfiles();
    }
  }

  async function activateProfile(id) {
    if (!confirm('Activar el perfil "' + id + '"?\n\nEsto reemplazara toda la configuracion activa del dispositivo (red, GPIO, microfono, debug).')) return;
    const ok = await callJson('/api/v1/profiles/apply', { profile: { id: id } }, 'Activando perfil "' + id + '"...');
    if (ok) await loadProfiles();
  }

  async function viewProfile(id) {
    setStatus('Cargando configuracion del perfil "' + id + '"...', false);
    try {
      const res = await fetch('/api/v1/profiles/get?id=' + encodeURIComponent(id));
      const text = await res.text();
      let pretty = text;
      try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      byId('resultOut').textContent = pretty;
      document.body.classList.add('show-resp');
      setStatus('Configuracion del perfil "' + id + '" cargada. Ver abajo.', false);
      const respCard = document.querySelector('.resp-card');
      if (respCard) respCard.scrollIntoView({ behavior: 'smooth' });
    } catch (err) {
      setStatus('Error cargando perfil: ' + err, true);
    }
  }

  async function deleteProfile(id) {
    if (!confirm('Borrar el perfil "' + id + '"?\n\nEsta accion no se puede deshacer.')) return;
    const ok = await callJson('/api/v1/profiles/delete', { profile: { id: id } }, 'Borrando perfil "' + id + '"...');
    if (ok) await loadProfiles();
  }

  async function callJson(url, payload, statusLabel) {
    setStatus(statusLabel, false);
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      const text = await res.text();
      let pretty = text;
      try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      byId('resultOut').textContent = pretty;
      document.body.classList.add('show-resp');
      const ok = res.ok;
      setStatus((ok ? 'Operacion completada' : 'Error') + ' (HTTP ' + res.status + ').', !ok);
      return ok;
    } catch (err) {
      byId('resultOut').textContent = String(err);
      setStatus('Error de red: ' + err, true);
      return false;
    }
  }

  loadProfiles();
</script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildApiProfilesHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN API - Profiles GPIO</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API Profiles GPIO</h1>
      <p>Aplicar un profile y copiarlo en DEFAULT es ahora el comportamiento normal.</p>
      <button onclick='callApi("GET","/api/v1/profiles/gpio")'>GET /api/v1/profiles/gpio</button>
      <button onclick='callApi("POST","/api/v1/profiles/gpio/save")'>POST /api/v1/profiles/gpio/save</button>
      <button onclick='callApi("POST","/api/v1/profiles/gpio/apply")'>POST /api/v1/profiles/gpio/apply</button>
      <button onclick='callApi("POST","/api/v1/profiles/gpio/default")'>POST /api/v1/profiles/gpio/default</button>
      <button onclick='callApi("POST","/api/v1/profiles/gpio/delete")'>POST /api/v1/profiles/gpio/delete</button>
      <textarea id='payload'>{"profile":{"id":"mi_perfil","name":"Mi perfil","gpio":{"outputs":[{"pin":5,"ledCount":60,"ledType":"ws2812b","colorOrder":"GRB"}]},"applyNow":true}}</textarea>
    </div>
    <div class='card'><pre id='out'>Sin llamadas aun.</pre></div>
  </main>
  <script>
    async function callApi(method, url) {
      const options = { method: method, headers: { 'Content-Type': 'application/json' } };
      if (method !== 'GET') options.body = document.getElementById('payload').value;
      const res = await fetch(url, options);
      const text = await res.text();
      let parsed = text;
      try { parsed = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      document.getElementById('out').textContent = method + ' ' + url + ' -> HTTP ' + res.status + '\n\n' + parsed;
    }
    callApi('GET', '/api/v1/profiles/gpio');
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildApiHardwareHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN API - Hardware</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API Hardware</h1>
      <button onclick='getHardware()'>GET /api/v1/hardware</button>
    </div>
    <div class='card'><pre id='out'>Cargando...</pre></div>
  </main>
  <script>
    async function getHardware() {
      const res = await fetch('/api/v1/hardware');
      const text = await res.text();
      let parsed = text;
      try { parsed = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      document.getElementById('out').textContent = 'GET /api/v1/hardware -> HTTP ' + res.status + '\n\n' + parsed;
    }
    getHardware();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildManualConfigHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN - Config Manual</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>Configuracion Manual</h1>
      <p class='hint'>Edita, exporta o importa toda la configuracion del dispositivo como JSON. La importacion solo se aplica si el JSON es completamente valido.</p>
      <p id='status' class='hint'>Cargando...</p>
    </div>

    <div class='card'>
      <h2>Editor JSON</h2>
      <div class='actions'>
        <button class='btn alt' onclick='loadConfig()'>Cargar del dispositivo</button>
        <button class='btn' onclick='applyConfig()'>Aplicar al dispositivo</button>
        <button class='btn alt' onclick='downloadConfig()'>Descargar .json</button>
        <button class='btn danger' onclick='validateOnly()'>Solo validar</button>
      </div>
      <textarea id='editor'></textarea>
    </div>

    <div class='card'>
      <h2>Importar archivo</h2>
      <div class='file-row'>
        <input type='file' id='fileInput' accept='.json,application/json'>
        <button class='btn alt' onclick='loadFile()'>Cargar en editor</button>
      </div>
      <p class='hint'>Carga un archivo .json en el editor. Luego pulsa "Aplicar al dispositivo" para importarlo.</p>
    </div>

    <div class='card json-card'>
      <h2>Resultado</h2>
      <pre id='resultOut'>Sin operaciones aun.</pre>
    </div>
  </main>

  <script>
    if(localStorage.getItem('dux_show_json')==='1') document.body.classList.add('show-json');

    function byId(id) { return document.getElementById(id); }
    function setStatus(msg, isErr) {
      const el = byId('status');
      el.textContent = msg;
      el.className = isErr ? 'hint err' : 'hint ok';
    }

    async function loadConfig() {
      setStatus('Cargando configuracion...', false);
      try {
        const res = await fetch('/api/v1/config/all');
        const text = await res.text();
        let pretty = text;
        try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
        byId('editor').value = pretty;
        byId('resultOut').textContent = 'Configuracion cargada (HTTP ' + res.status + ').';
        setStatus('Configuracion cargada.', false);
      } catch (err) {
        byId('resultOut').textContent = String(err);
        setStatus('Error al cargar.', true);
      }
    }

    function validateOnly() {
      const raw = byId('editor').value.trim();
      if (!raw) { setStatus('El editor esta vacio.', true); return; }
      try {
        const obj = JSON.parse(raw);
        byId('editor').value = JSON.stringify(obj, null, 2);
        setStatus('JSON valido (sintaxis correcta).', false);
        byId('resultOut').textContent = 'Validacion local OK. Pulsa "Aplicar al dispositivo" para validar contra el firmware y guardar.';
      } catch (err) {
        setStatus('JSON invalido: ' + err.message, true);
        byId('resultOut').textContent = 'Error de sintaxis JSON:\n' + err.message;
      }
    }

    async function applyConfig() {
      const raw = byId('editor').value.trim();
      if (!raw) { setStatus('El editor esta vacio.', true); return; }
      let parsed;
      try { parsed = JSON.parse(raw); } catch (err) {
        setStatus('JSON invalido: ' + err.message, true);
        byId('resultOut').textContent = 'Error de sintaxis JSON:\n' + err.message;
        return;
      }
      setStatus('Aplicando configuracion...', false);
      try {
        const res = await fetch('/api/v1/config/all', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(parsed)
        });
        const text = await res.text();
        let pretty = text;
        try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
        byId('resultOut').textContent = 'HTTP ' + res.status + '\n\n' + pretty;
        if (res.ok) {
          setStatus('Configuracion aplicada correctamente.', false);
          loadConfig();
        } else {
          setStatus('Error al aplicar (HTTP ' + res.status + '). La configuracion NO fue modificada.', true);
        }
      } catch (err) {
        byId('resultOut').textContent = String(err);
        setStatus('Error de red al aplicar.', true);
      }
    }

    function downloadConfig() {
      const raw = byId('editor').value.trim();
      if (!raw) { setStatus('Nada que descargar.', true); return; }
      const blob = new Blob([raw], { type: 'application/json' });
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = 'duxman-config.json';
      a.click();
      URL.revokeObjectURL(a.href);
      setStatus('Archivo descargado.', false);
    }

    function loadFile() {
      const file = byId('fileInput').files[0];
      if (!file) { setStatus('Selecciona un archivo primero.', true); return; }
      const reader = new FileReader();
      reader.onload = function(e) {
        const text = e.target.result;
        try {
          const obj = JSON.parse(text);
          byId('editor').value = JSON.stringify(obj, null, 2);
          setStatus('Archivo cargado en editor. Revisa y pulsa "Aplicar al dispositivo".', false);
          byId('resultOut').textContent = 'Archivo ' + file.name + ' cargado (' + text.length + ' bytes).';
        } catch (err) {
          setStatus('Archivo JSON invalido: ' + err.message, true);
          byId('resultOut').textContent = 'Error parseando ' + file.name + ':\n' + err.message;
        }
      };
      reader.readAsText(file);
    }

    loadConfig();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}

String ApiService::buildApiConfigAllHtml() const {
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>DUXMAN API - Config All</title>
  __CSS__
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>API Config All</h1>
      <p>Exportar e importar toda la configuracion del dispositivo.</p>
      <button onclick='getCfg()'>GET /api/v1/config/all</button>
      <button onclick='postCfg()'>POST /api/v1/config/all</button>
      <textarea id='payload'>{}</textarea>
    </div>
    <div class='card'><pre id='out'>Sin llamadas aun.</pre></div>
  </main>
  <script>
    async function callApi(method, body) {
      const options = { method, headers: { 'Content-Type': 'application/json' } };
      if (body) options.body = body;
      const res = await fetch('/api/v1/config/all', options);
      const text = await res.text();
      let parsed = text;
      try { parsed = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      document.getElementById('out').textContent = method + ' /api/v1/config/all -> HTTP ' + res.status + '\n\n' + parsed;
      if (method === 'GET') {
        try { document.getElementById('payload').value = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
      }
    }
    function getCfg() { callApi('GET'); }
    function postCfg() { callApi('POST', document.getElementById('payload').value); }
    getCfg();
  </script>
</div>
</body>
</html>
)HTML";
  html.replace("__CSS__", buildCommonCss());
  html.replace("__NAV__", buildNavHtml());
  return html;
}
