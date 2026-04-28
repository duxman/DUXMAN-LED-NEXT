#include "api/ApiService.h"
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
String ApiService::buildNavHtml() const {
  return R"HTML(
<style>
  /* ── Outer page box: 90% wide, contains nav + content ── */
  .gen-page-outer{width:90%;max-width:940px;margin:20px auto;border-radius:16px;box-shadow:0 12px 36px rgba(10,30,20,.15);overflow:hidden;background:var(--card,#fff);}
  .gen-page-outer>main{max-width:100%!important;width:100%!important;margin:0!important;box-sizing:border-box;}
  /* ── Nav: flush inside box, no own radius ── */
  .gen-nav{display:flex;align-items:center;background:linear-gradient(135deg,#0a3d4a 0%,#0f6a7a 100%);min-height:52px;border-radius:0;box-shadow:none;padding:0 16px;position:relative;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;margin:0;width:100%;}
  .gen-logo{display:flex;align-items:center;gap:8px;flex-shrink:0;margin-right:auto;text-decoration:none;}
  .gen-logo-text{color:#e8f6f8;font-size:18px;font-weight:600;white-space:nowrap;letter-spacing:.01em;}
  .gen-hamburger{display:none;background:none;border:none;cursor:pointer;grid-template-columns:1fr 1fr;gap:3px;padding:4px;}
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
    <svg width='32' height='32' viewBox='0 0 100 100' fill='none' xmlns='http://www.w3.org/2000/svg'>
      <polygon points='50,4 61,35 95,35 68,57 79,91 50,70 21,91 32,57 5,35 39,35' fill='#facc15' stroke='#ca8a04' stroke-width='3'/>
    </svg>
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
  <style>
    :root {
      --bg:#f8f2e8;
      --bg2:#e5f0f5;
      --card:#fffdf8;
      --line:#d5c8b6;
      --text:#2a1d14;
      --muted:#6e5d4f;
      --primary:#0f6a7a;
      --accent:#9d3d1f;
    }
    * { box-sizing:border-box; }
    body {
      margin:0;
      min-height:100vh;
      color:var(--text);
      font-family:Trebuchet MS, Segoe UI, sans-serif;
      background:
        radial-gradient(1000px 420px at -5% -5%, #efd9bc 0%, rgba(239,217,188,0) 70%),
        radial-gradient(900px 460px at 110% 0%, #d8e9f1 0%, rgba(216,233,241,0) 65%),
        linear-gradient(180deg, var(--bg) 0%, var(--bg2) 100%);
      padding:16px;
    }
    main { width:100%; }
    .hero {
      background:var(--card);
      padding:22px;
      margin-bottom:0;
      display:grid;
      gap:14px;
    }
    .hero-title {
      display:flex;
      align-items:baseline;
      gap:12px;
      flex-wrap:wrap;
    }
    h1 { margin:0 0 8px 0; font-size:30px; }
    .boot-at {
      display:inline-flex;
      align-items:center;
      padding:2px 8px;
      border-radius:999px;
      background:rgba(15,106,122,.08);
      color:var(--muted);
      font-size:11px;
      font-weight:700;
      letter-spacing:.02em;
      margin-bottom:8px;
    }
    p { margin:0; color:var(--muted); }
    .menu {
      display:grid;
      grid-template-columns:repeat(3, minmax(0, 1fr));
      gap:10px;
      width:100%;
      padding:10px;
      border:1px solid var(--line);
      border-radius:14px;
      background:linear-gradient(180deg, rgba(255,255,255,.86) 0%, rgba(249,241,229,.95) 100%);
      box-shadow:inset 0 1px 0 rgba(255,255,255,.7);
    }
    .panel {
      display:block;
    }
    .controls {
      display:grid;
      gap:14px;
      background:#fff;
      border:1px solid var(--line);
      border-radius:14px;
      padding:16px;
      box-shadow:0 16px 34px rgba(31,22,14,.08);
    }
    .controls-top {
      display:grid;
      grid-template-columns:minmax(280px, 1fr) minmax(0, 1.35fr);
      gap:12px;
    }
    .controls-bottom {
      display:grid;
      grid-template-columns:1fr;
      gap:12px;
    }
    .power-card {
      display:grid;
      gap:14px;
      padding:14px;
      border:1px solid var(--line);
      border-radius:12px;
      background:linear-gradient(180deg, #fffdf9 0%, #f7efe4 100%);
    }
    .power-copy {
      display:grid;
      gap:6px;
    }
    .power-copy h2 {
      margin:0;
      font-size:18px;
    }
    .power-actions {
      display:grid;
      gap:10px;
    }
    .sequence-tools {
      display:grid;
      gap:10px;
      padding:12px;
      border:1px solid rgba(15,106,122,.16);
      border-radius:12px;
      background:rgba(255,255,255,.6);
    }
    .slider-readout {
      font-size:12px;
      font-weight:700;
      color:var(--accent);
    }
    .saved-effects-card {
      display:grid;
      gap:14px;
      padding:14px;
      border:1px solid var(--line);
      border-radius:12px;
      background:linear-gradient(180deg, #fbfffd 0%, #eef7f5 100%);
    }
    .saved-effects-copy {
      display:grid;
      gap:6px;
    }
    .saved-effects-copy h2 {
      margin:0;
      font-size:18px;
    }
    .effects-status {
      font-size:12px;
      font-weight:700;
      color:var(--primary);
    }
    .sequence-list {
      display:grid;
      gap:10px;
    }
    .sequence-item {
      display:grid;
      gap:8px;
      padding:12px;
      border:1px solid rgba(15,106,122,.16);
      border-radius:12px;
      background:#fff;
      box-shadow:0 8px 16px rgba(31,22,14,.05);
    }
    .sequence-head {
      display:flex;
      align-items:flex-start;
      justify-content:space-between;
      gap:10px;
    }
    .sequence-title {
      display:grid;
      gap:4px;
    }
    .sequence-title strong {
      font-size:14px;
    }
    .sequence-meta {
      font-size:12px;
      color:var(--muted);
    }
    .sequence-colors {
      display:flex;
      gap:8px;
      flex-wrap:wrap;
    }
    .sequence-color {
      width:22px;
      height:22px;
      border-radius:999px;
      border:1px solid rgba(0,0,0,.18);
      box-shadow:inset 0 1px 0 rgba(255,255,255,.25);
    }
    .empty-note {
      padding:12px;
      border:1px dashed rgba(15,106,122,.26);
      border-radius:12px;
      color:var(--muted);
      background:rgba(255,255,255,.55);
    }
    .delete-btn {
      width:auto;
      border:none;
      background:var(--accent);
      color:#fff;
      font-weight:700;
      cursor:pointer;
    }
    .runtime-card {
      background:linear-gradient(180deg, #fffdfa 0%, #f6efe5 100%);
      border:1px solid var(--line);
      border-radius:14px;
      padding:16px;
      box-shadow:0 16px 34px rgba(31,22,14,.08);
      margin-top:12px;
    }
    .runtime-head {
      display:flex;
      align-items:flex-start;
      justify-content:space-between;
      gap:12px;
    }
    .runtime-copy {
      display:grid;
      gap:6px;
    }
    .runtime-copy h2 {
      margin:0;
      display:flex;
      align-items:center;
      gap:10px;
    }
    .runtime-pill {
      display:inline-flex;
      align-items:center;
      gap:6px;
      padding:4px 10px;
      border-radius:999px;
      background:#14313a;
      color:#dff7ff;
      font-size:11px;
      font-weight:700;
      letter-spacing:.04em;
      text-transform:uppercase;
    }
    .toggle-btn {
      width:auto;
      min-width:140px;
      border:none;
      background:var(--primary);
      color:#fff;
      font-weight:700;
      cursor:pointer;
    }
    .runtime-body.hidden {
      display:none;
    }
    .runtime-body {
      margin-top:12px;
    }
    .row {
      display:grid;
      grid-template-columns:repeat(2, minmax(0, 1fr));
      gap:10px;
    }
    .control-sections {
      display:grid;
      grid-template-columns:1fr;
      gap:12px;
    }
    .control-card {
      display:grid;
      gap:12px;
      padding:14px;
      border:1px solid var(--line);
      border-radius:12px;
      background:linear-gradient(180deg, #fffdf9 0%, #fdf7ee 100%);
    }
    .control-card.effect {
      background:linear-gradient(180deg, #fbfefe 0%, #f1f8fa 100%);
    }
    .control-card.colors {
      background:linear-gradient(180deg, #fffefb 0%, #fff7ec 100%);
    }
    .control-card h2 {
      margin:0;
      font-size:18px;
    }
    .control-card p {
      font-size:13px;
    }
    .row.colors {
      grid-template-columns:repeat(4, minmax(0, 1fr));
    }
    label {
      display:grid;
      gap:6px;
      font-size:13px;
      color:var(--muted);
    }
    input, select, button {
      width:100%;
      border:1px solid var(--line);
      border-radius:10px;
      padding:10px 12px;
      font:inherit;
      color:var(--text);
      background:#fff;
    }
    input[type='color'] {
      min-height:46px;
      padding:4px;
      cursor:pointer;
    }
    input[type='range'] {
      padding:0;
    }
    .switch {
      display:flex;
      align-items:center;
      gap:10px;
      color:var(--text);
      font-weight:600;
    }
    .switch input {
      width:auto;
      transform:scale(1.2);
    }
    .actions {
      display:flex;
      gap:10px;
      flex-wrap:wrap;
    }
    .actions button {
      width:auto;
      cursor:pointer;
      background:var(--primary);
      color:#fff;
      border:none;
      font-weight:700;
    }
    .actions button.alt {
      background:#fff;
      color:var(--primary);
      border:1px solid var(--primary);
    }
    .badge {
      display:inline-block;
      margin-top:4px;
      font-size:12px;
      color:var(--accent);
      font-weight:700;
    }
    pre {
      margin:10px 0 0 0;
      min-height:160px;
      white-space:pre-wrap;
      border:1px solid #314b53;
      border-radius:12px;
      background:
        linear-gradient(180deg, rgba(255,255,255,.03) 0%, rgba(255,255,255,0) 100%),
        #13252b;
      color:#d9edf0;
      padding:14px;
      overflow:auto;
      box-shadow:inset 0 1px 0 rgba(255,255,255,.04);
      font-family:Consolas, Courier New, monospace;
      font-size:12px;
      line-height:1.45;
    }
    .json-debug { display:none; }
    .json-card { display:none; }
    body.show-json .json-debug { display:block; }
    body.show-json .json-card { display:block; }
    .item {
      display:grid;
      gap:4px;
      text-decoration:none;
      color:var(--text);
      background:#fff;
      border:1px solid rgba(15,106,122,.18);
      border-radius:12px;
      padding:14px 16px;
      transition:transform .14s ease, box-shadow .14s ease;
      box-shadow:0 8px 16px rgba(31,22,14,.05);
    }
    .item.version {
      border-color:rgba(157,61,31,.22);
    }
    .item:hover { transform:translateY(-2px); box-shadow:0 10px 20px rgba(0,0,0,.08); }
    .item h2 { margin:0; font-size:16px; }
    .item span { color:var(--muted); font-size:12px; }
    @media (max-width: 760px) {
      .menu, .controls-top, .controls-bottom, .control-sections, .row, .row.colors { grid-template-columns:1fr; }
      .runtime-head {
        flex-direction:column;
        align-items:stretch;
      }
      .toggle-btn {
        width:100%;
      }
    }
  </style>
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
  <style>
    :root { --bg:#f2f6f4; --card:#fff; --line:#d6e0db; --text:#183025; --muted:#4e6a5c; --primary:#1d6a4e; }
    body { margin:0; font-family:Trebuchet MS,Segoe UI,sans-serif; color:var(--text);
      background:radial-gradient(700px 320px at 100% 0%,#deefe7 0%,rgba(222,239,231,0) 65%),var(--bg); padding:16px; }
    main { max-width:940px; margin:0 auto; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:14px;
      padding:16px; box-shadow:0 10px 28px rgba(20,35,28,.08); margin-bottom:16px; }
    h1,h2,h3 { margin:0 0 6px 0; }
    p { margin:0; color:var(--muted); }
    .topnav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:12px; }
    .topnav a { color:#0b5e91; text-decoration:none; font-weight:600; }
    .badge { font-size:.7rem; border-radius:6px; padding:2px 6px; font-weight:600; margin-left:4px; vertical-align:middle; }
    .badge-sys { background:#e8f5e9; color:#1b5e20; border:1px solid #a5d6a7; }
    .badge-user { background:#e3f2fd; color:#0d47a1; border:1px solid #90caf9; }
    .palettes-grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(190px,1fr)); gap:10px; margin-top:10px; }
    .pal-card { border:1px solid var(--line); border-radius:10px; padding:10px; background:var(--bg);
      transition:box-shadow .15s; }
    .pal-card.editing { border-color:var(--primary); box-shadow:0 0 0 2px rgba(29,106,78,.25); }
    .pal-card h3 { font-size:.9rem; margin:0 0 4px 0; }
    .swatches { display:flex; gap:3px; margin:6px 0; }
    .swatch { width:24px; height:16px; border-radius:3px; border:1px solid rgba(0,0,0,.1); }
    .pal-actions { display:flex; gap:5px; flex-wrap:wrap; margin-top:6px; }
    .btn { padding:4px 10px; border-radius:6px; border:none; cursor:pointer; font-size:.78rem; font-weight:600; }
    .btn-primary { background:var(--primary); color:#fff; }
    .btn-ghost { background:transparent; border:1px solid var(--line); color:var(--text); }
    .btn-danger { background:#dc3545; color:#fff; }
    .btn-sm { padding:3px 8px; font-size:.73rem; }
    .btn:hover { opacity:.85; }
    /* panel de edicion inline */
    .edit-panel { margin-top:14px; padding:14px; background:#f8fdfb; border:1px solid var(--line);
      border-radius:10px; border-left:4px solid var(--primary); }
    .edit-panel h3 { color:var(--primary); margin:0 0 10px 0; font-size:.95rem; }
    .form-row { display:flex; flex-wrap:wrap; gap:10px; margin-bottom:8px; }
    .form-group { display:flex; flex-direction:column; gap:3px; flex:1; min-width:130px; }
    .form-label { font-size:.76rem; font-weight:600; color:var(--muted); }
    input[type=text], select { padding:6px 8px; border:1px solid var(--line); border-radius:6px;
      font-size:.86rem; background:#fff; color:var(--text); width:100%; box-sizing:border-box; }
    /* color pickers compactos */
    .colors-row { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:8px; }
    .color-slot { display:flex; flex-direction:column; gap:3px; align-items:flex-start; }
    .color-slot .form-label { font-size:.72rem; }
    .color-pair { display:flex; align-items:center; gap:4px; }
    input[type=color] { width:28px; height:22px; border:1px solid var(--line); border-radius:4px;
      cursor:pointer; padding:1px; }
    .color-hex { width:72px; font-family:monospace; font-size:.8rem; padding:4px 6px; }
    .form-actions { display:flex; gap:8px; align-items:center; flex-wrap:wrap; margin-top:10px; }
    .hint { font-size:.76rem; }
    .hint.ok { color:#1d6a4e; }
    .hint.err { color:#c0392b; }
    .section-hdr { display:flex; align-items:center; gap:8px; margin-bottom:4px; }
    .no-palettes { color:var(--muted); font-style:italic; font-size:.85rem; }
    /* debug JSON */
    .json-debug { display:none; margin-top:10px; }
    body.show-json .json-debug { display:block; }
    pre.json-pre { background:#f1edf8; border:1px solid #ddd4ea; border-radius:6px;
      padding:8px; font-size:.75rem; white-space:pre-wrap; overflow:auto; max-height:160px; margin:0; }
  </style>
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
  <style>
    :root {
      --bg:#f2f6f4;
      --card:#ffffff;
      --line:#d6e0db;
      --text:#183025;
      --muted:#4e6a5c;
      --primary:#1d6a4e;
    }
    body {
      margin:0;
      min-height:100vh;
      font-family:Trebuchet MS, Segoe UI, sans-serif;
      color:var(--text);
      background:
        radial-gradient(700px 320px at 100% 0%, #deefe7 0%, rgba(222,239,231,0) 65%),
        var(--bg);
      padding:16px;
    }
    main { max-width:880px; margin:0 auto; }
    .card {
      background:var(--card);
      border:1px solid var(--line);
      border-radius:14px;
      padding:16px;
      box-shadow:0 10px 28px rgba(20,35,28,.08);
      margin-bottom:12px;
    }
    h1,h2 { margin:0 0 8px 0; }
    p { margin:0; color:var(--muted); }
    .grid {
      display:grid;
      grid-template-columns:repeat(auto-fit, minmax(220px, 1fr));
      gap:10px;
      margin-top:12px;
    }
    a.box {
      text-decoration:none;
      color:var(--text);
      border:1px solid var(--line);
      border-left:6px solid var(--primary);
      border-radius:10px;
      padding:12px;
      background:#fff;
      display:block;
    }
    .topnav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .topnav a { color:#0b5e91; text-decoration:none; font-weight:600; }
  </style>
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
  <style>
    :root { --bg:#eff5f7; --card:#ffffff; --line:#d3e0e7; --text:#1a2a33; --muted:#556b78; --primary:#116f94; }
    body { margin:0; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; padding:16px; }
    main { max-width:920px; margin:0 auto; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    h1,h2 { margin:0 0 8px 0; }
    p { margin:0; color:var(--muted); }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#0f5c85; text-decoration:none; font-weight:600; }
    .grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(220px, 1fr)); gap:10px; margin-top:12px; }
    a.box { text-decoration:none; color:var(--text); background:#fff; border:1px solid var(--line); border-left:6px solid var(--primary); border-radius:10px; padding:12px; display:block; }
    a.spec { display:inline-block; margin-top:10px; color:#0f5c85; }
  </style>
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
  <style>
    :root { --bg:#f4f7f9; --card:#fff; --line:#d6e0e7; --text:#1f2f3a; --primary:#2f6fb0; }
    body { margin:0; padding:16px; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#0f5c85; text-decoration:none; font-weight:600; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    textarea, pre { width:100%; min-height:120px; border:1px solid var(--line); border-radius:8px; padding:8px; font-family:Consolas, monospace; white-space:pre-wrap; }
    button { border:0; border-radius:8px; padding:8px 10px; color:#fff; background:var(--primary); cursor:pointer; margin-right:8px; }
  </style>
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
  <style>
    :root { --bg:#f2f7f3; --card:#fff; --line:#d5e2d8; --text:#1f3025; --primary:#2c7a56; }
    body { margin:0; padding:16px; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#136347; text-decoration:none; font-weight:600; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    textarea, pre { width:100%; min-height:130px; border:1px solid var(--line); border-radius:8px; padding:8px; font-family:Consolas, monospace; white-space:pre-wrap; }
    button { border:0; border-radius:8px; padding:8px 10px; color:#fff; background:var(--primary); cursor:pointer; margin-right:8px; }
  </style>
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
  <style>
    :root { --bg:#f5f3f9; --card:#fff; --line:#ddd7eb; --text:#2b2440; --primary:#5a4bb2; }
    body { margin:0; padding:16px; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#3f3590; text-decoration:none; font-weight:600; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    pre { width:100%; min-height:140px; border:1px solid var(--line); border-radius:8px; padding:8px; font-family:Consolas, monospace; white-space:pre-wrap; }
    button { border:0; border-radius:8px; padding:8px 10px; color:#fff; background:var(--primary); cursor:pointer; }
  </style>
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
  <style>
    :root {
      --bg:#f7f3ed;
      --card:#fffdf8;
      --line:#d8c7b5;
      --text:#2a221b;
      --muted:#6e6256;
      --primary:#8c2f14;
      --secondary:#245f73;
    }
    * { box-sizing:border-box; }
    body {
      margin:0;
      min-height:100vh;
      color:var(--text);
      font-family:Trebuchet MS, Segoe UI, sans-serif;
      background:
        radial-gradient(1100px 420px at -10% -5%, #f0d7b5 0%, rgba(240,215,181,0) 70%),
        radial-gradient(900px 420px at 100% 0%, #d3e7ef 0%, rgba(211,231,239,0) 60%),
        var(--bg);
    }
    main { max-width:980px; margin:20px auto; padding:14px; }
    .card {
      background:var(--card);
      border:1px solid var(--line);
      border-radius:12px;
      padding:14px;
      margin-bottom:12px;
      box-shadow:0 8px 28px rgba(42,34,27,.08);
    }
    h1,h2,h3 { margin:0 0 10px 0; }
    h1 { font-size:26px; letter-spacing:.2px; }
    h2 { font-size:18px; color:#4c3a2b; }
    h3 { font-size:14px; color:#5b4a3a; text-transform:uppercase; letter-spacing:.6px; }
    .row { display:flex; flex-wrap:wrap; gap:10px; }
    .col { flex:1 1 260px; }
    .field { margin-bottom:10px; }
    label { display:block; font-size:13px; color:var(--muted); margin-bottom:4px; }
    input, select {
      width:100%;
      border:1px solid var(--line);
      border-radius:8px;
      padding:8px 10px;
      font-size:14px;
      background:#fff;
      color:var(--text);
    }
    .btn {
      border:0;
      border-radius:8px;
      padding:9px 12px;
      font-weight:600;
      cursor:pointer;
      color:#fff;
      background:var(--primary);
    }
    .btn.alt { background:var(--secondary); }
    .btn.ghost { background:#7d8b95; }
    .actions { display:flex; flex-wrap:wrap; gap:10px; }
    .hint { font-size:13px; color:var(--muted); }
    pre {
      margin:0;
      background:#f2ece5;
      border:1px solid #ddcec0;
      border-radius:8px;
      padding:10px;
      white-space:pre-wrap;
      overflow:auto;
      font-family:Consolas, monospace;
      font-size:12px;
      max-height:270px;
    }
    .ok { color:#1f6a39; }
    .err { color:#9f1c1c; }
    @media (max-width:700px) {
      .json-card { display:none; }
      body.show-json .json-card { display:block; }
      @media (max-width: 760px) {
      main { padding:10px; }
    }
  </style>
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
  <style>
    :root {
      --bg:#f4f7f3;
      --card:#ffffff;
      --line:#d5dfd7;
      --text:#203328;
      --muted:#5f7366;
      --primary:#2f7a5a;
      --secondary:#2b5f86;
    }
    body { margin:0; min-height:100vh; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:860px; margin:20px auto; padding:14px; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    .actions { display:flex; flex-wrap:wrap; gap:10px; }
    .btn { border:0; border-radius:8px; padding:9px 12px; font-weight:600; cursor:pointer; color:#fff; background:var(--primary); }
    .btn.alt { background:var(--secondary); }
    .btn.ghost { background:#7d8b95; text-decoration:none; display:inline-block; }
    .field { margin-bottom:10px; }
    label { display:block; font-size:13px; color:var(--muted); margin-bottom:4px; }
    input, select { width:100%; border:1px solid var(--line); border-radius:8px; padding:8px 10px; font-size:14px; box-sizing:border-box; }
    .row { display:flex; flex-wrap:wrap; gap:10px; }
    .col { flex:1 1 220px; }
    pre { background:#eef5f0; border:1px solid var(--line); border-radius:8px; padding:10px; white-space:pre-wrap; overflow:auto; font-family:Consolas, monospace; }
    .hint { color:var(--muted); font-size:13px; }
    .ok { color:#1f6a39; }
    .err { color:#9f1c1c; }
    .json-card { display:none; }
    body.show-json .json-card { display:block; }
  </style>
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
  <style>
    :root {
      --bg:#f5f2f8;
      --card:#ffffff;
      --line:#ddd4ea;
      --text:#2d2540;
      --muted:#655a7e;
      --primary:#5a4bb2;
      --secondary:#3566a3;
    }
    body { margin:0; min-height:100vh; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:860px; margin:20px auto; padding:14px; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    .actions { display:flex; flex-wrap:wrap; gap:10px; }
    .btn { border:0; border-radius:8px; padding:9px 12px; font-weight:600; cursor:pointer; color:#fff; background:var(--primary); }
    .btn.alt { background:var(--secondary); }
    .btn.ghost { background:#7d8b95; text-decoration:none; display:inline-block; }
    .field { margin:10px 0; }
    label { display:block; margin-bottom:4px; color:var(--muted); }
    input[type='number'] { width:100%; border:1px solid var(--line); border-radius:8px; padding:8px 10px; }
    pre { background:#f1edf8; border:1px solid var(--line); border-radius:8px; padding:10px; white-space:pre-wrap; overflow:auto; }
    .hint { color:var(--muted); }
    .ok { color:#1f6a39; }
    .err { color:#9f1c1c; }
    .json-card { display:none; }
    body.show-json .json-card { display:block; }
  </style>
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
  <style>
    :root { --bg:#f2f7f3; --card:#fff; --line:#d5e2d8; --text:#1f3025; --primary:#2c7a56; }
    body { margin:0; padding:16px; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#136347; text-decoration:none; font-weight:600; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    textarea, pre { width:100%; min-height:130px; border:1px solid var(--line); border-radius:8px; padding:8px; font-family:Consolas, monospace; white-space:pre-wrap; }
    button { border:0; border-radius:8px; padding:8px 10px; color:#fff; background:var(--primary); cursor:pointer; margin-right:8px; }
  </style>
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
  <style>
    :root { --bg:#f5f3f9; --card:#fff; --line:#ddd7eb; --text:#2b2440; --primary:#5a4bb2; }
    body { margin:0; padding:16px; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#3f3590; text-decoration:none; font-weight:600; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    textarea, pre { width:100%; min-height:130px; border:1px solid var(--line); border-radius:8px; padding:8px; font-family:Consolas, monospace; white-space:pre-wrap; }
    button { border:0; border-radius:8px; padding:8px 10px; color:#fff; background:var(--primary); cursor:pointer; margin-right:8px; }
  </style>
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
  <style>
    :root {
      --bg:#f3f7ed;
      --card:#fdfff8;
      --line:#c8d5b8;
      --text:#2a3320;
      --muted:#5e6d50;
      --primary:#3a7d2c;
      --secondary:#2e6b8a;
      --danger:#9f1c1c;
    }
    body { margin:0; min-height:100vh; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:860px; margin:20px auto; padding:14px; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; box-shadow:0 8px 28px rgba(42,51,32,.08); }
    .actions { display:flex; flex-wrap:wrap; gap:10px; }
    .btn { border:0; border-radius:8px; padding:9px 12px; font-weight:600; cursor:pointer; color:#fff; background:var(--primary); }
    .btn.alt { background:var(--secondary); }
    .btn.ghost { background:#7d8b95; text-decoration:none; display:inline-block; }
    .btn.danger { background:var(--danger); }
    h1,h2,h3 { margin:0 0 10px 0; }
    .row { display:flex; flex-wrap:wrap; gap:10px; }
    .col { flex:1 1 180px; }
    .field { margin-bottom:10px; }
    label { display:block; font-size:13px; color:var(--muted); margin-bottom:4px; }
    input, select { width:100%; border:1px solid var(--line); border-radius:8px; padding:8px 10px; font-size:14px; background:#fff; box-sizing:border-box; }
    pre { background:#f0f5e8; border:1px solid var(--line); border-radius:8px; padding:10px; white-space:pre-wrap; overflow:auto; font-family:Consolas, monospace; font-size:12px; }
    .hint { font-size:13px; color:var(--muted); }
    .ok { color:#1f6a39; }
    .err { color:#9f1c1c; }
    .json-card { display:none; }
    body.show-json .json-card { display:block; }
    .warn { background:#fff8e0; border:1px solid #e8d48a; border-radius:8px; padding:10px; margin-top:8px; font-size:13px; color:#6b5a10; }
    table { width:100%; border-collapse:collapse; font-size:13px; margin-top:8px; }
    th,td { text-align:left; padding:6px 8px; border-bottom:1px solid var(--line); }
    th { color:var(--muted); font-weight:600; }
    .output-header { display:flex; align-items:center; justify-content:space-between; }
    .output-header h2 { margin:0; }
    .output-block { border-top:2px solid var(--line); padding-top:12px; margin-top:12px; }
    .output-block:first-child { border-top:none; padding-top:0; margin-top:0; }
  </style>
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
  <style>
    :root { --bg:#f3f7ed; --card:#fff; --line:#c8d5b8; --text:#2a3320; --primary:#3a7d2c; }
    body { margin:0; padding:16px; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#2a6320; text-decoration:none; font-weight:600; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    textarea, pre { width:100%; min-height:130px; border:1px solid var(--line); border-radius:8px; padding:8px; font-family:Consolas, monospace; white-space:pre-wrap; }
    button { border:0; border-radius:8px; padding:8px 10px; color:#fff; background:var(--primary); cursor:pointer; margin-right:8px; }
  </style>
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
  <style>
    :root {
      --bg:#f6f5fa;
      --card:#ffffff;
      --line:#ddd8ef;
      --text:#29233f;
      --muted:#5a5476;
      --primary:#3558a7;
    }
    body {
      margin:0;
      min-height:100vh;
      font-family:Trebuchet MS, Segoe UI, sans-serif;
      color:var(--text);
      background:
        radial-gradient(780px 320px at -10% -5%, #ece7fb 0%, rgba(236,231,251,0) 70%),
        radial-gradient(740px 300px at 110% 0%, #e3ebff 0%, rgba(227,235,255,0) 65%),
        var(--bg);
      padding:16px;
    }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#1f5fa8; text-decoration:none; font-weight:600; }
    .card {
      background:var(--card);
      border:1px solid var(--line);
      border-radius:14px;
      padding:16px;
      box-shadow:0 10px 28px rgba(20,25,45,.08);
      margin-bottom:12px;
    }
    h1,h2 { margin:0 0 8px 0; }
    p { margin:0; color:var(--muted); }
    .btn {
      margin-top:10px;
      border:0;
      border-radius:8px;
      padding:9px 12px;
      cursor:pointer;
      color:#fff;
      background:var(--primary);
      font-weight:600;
    }
    pre {
      margin:0;
      background:#f0eef8;
      border:1px solid #ddd8ef;
      border-radius:10px;
      padding:10px;
      font-family:Consolas, monospace;
      white-space:pre-wrap;
      overflow:auto;
    }
  </style>
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <section class='card'>
      <h1>Informacion de Version</h1>
      <p>Datos obtenidos desde /api/v1/release y /api/v1/hardware.</p>
      <button class='btn' onclick='loadRelease()'>Recargar</button>
    </section>

    <section class='card'>
      <h2>Release JSON</h2>
      <pre id='releaseOut'>Cargando...</pre>
    </section>

    <section class='card'>
      <h2>Hardware JSON</h2>
      <pre id='hardwareOut'>Cargando...</pre>
    </section>
  </main>

  <script>
    async function loadRelease() {
      try {
        const res = await fetch('/api/v1/release');
        const text = await res.text();
        let pretty = text;
        try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
        document.getElementById('releaseOut').textContent =
          'HTTP ' + res.status + '\n\n' + pretty;
      } catch (error) {
        document.getElementById('releaseOut').textContent = String(error);
      }
    }

    async function loadHardware() {
      try {
        const res = await fetch('/api/v1/hardware');
        const text = await res.text();
        let pretty = text;
        try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
        document.getElementById('hardwareOut').textContent =
          'HTTP ' + res.status + '\n\n' + pretty;
      } catch (error) {
        document.getElementById('hardwareOut').textContent = String(error);
      }
    }

    loadRelease();
    loadHardware();
  </script>
</div>
</body>
</html>
)HTML";
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
  <title>DUXMAN Profiles Config</title>
  <style>
    :root { --bg:#f4f1e8; --card:#fffdfa; --line:#d8cdb8; --text:#33291e; --muted:#6e6355; --primary:#8a5c1d; --secondary:#2f6c83; --danger:#a12626; }
    body { margin:0; min-height:100vh; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:980px; margin:20px auto; padding:14px; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; box-shadow:0 8px 28px rgba(30,25,18,.08); }
    .actions, .row { display:flex; flex-wrap:wrap; gap:10px; }
    .btn { border:0; border-radius:8px; padding:9px 12px; font-weight:600; cursor:pointer; color:#fff; background:var(--primary); }
    .btn.alt { background:var(--secondary); }
    .btn.ghost { background:#7d8b95; text-decoration:none; display:inline-block; }
    .btn.danger { background:var(--danger); }
    .field { flex:1 1 240px; margin-bottom:10px; }
    label { display:block; font-size:13px; color:var(--muted); margin-bottom:4px; }
    input, textarea, select { width:100%; border:1px solid var(--line); border-radius:8px; padding:8px 10px; font-size:14px; background:#fff; box-sizing:border-box; }
    textarea { min-height:140px; font-family:Consolas, monospace; white-space:pre; }
    pre { background:#f4eee3; border:1px solid var(--line); border-radius:8px; padding:10px; white-space:pre-wrap; overflow:auto; font-family:Consolas, monospace; font-size:12px; }
    table { width:100%; border-collapse:collapse; margin-top:8px; }
    th, td { text-align:left; padding:8px; border-bottom:1px solid var(--line); font-size:13px; }
    th { color:var(--muted); }
    .hint { font-size:13px; color:var(--muted); }
    .ok { color:#1f6a39; }
    .err { color:#9f1c1c; }
    .json-card { display:none; }
    body.show-json .json-card { display:block; }
  </style>
</head>
<body>
<div class='gen-page-outer'>
__NAV__
  <main>
    <div class='card'>
      <h1>Perfiles GPIO</h1>
      <p class='hint'>Gestiona presets integrados y perfiles guardados de usuario. La configuracion GPIO activa siempre se refleja en el profile DEFAULT. El borrado esta abierto temporalmente para limpieza, salvo DEFAULT.</p>
      <div class='actions'>
        <button class='btn alt' onclick='loadProfiles()'>Recargar</button>
        <button class='btn' onclick='saveProfile()'>Guardar perfil</button>
        <button class='btn alt' onclick='applySelected()'>Aplicar</button>
        <button class='btn alt' onclick='applyAsDefault()'>Copiar a DEFAULT</button>
        <button class='btn danger' onclick='deleteSelected()'>Borrar</button>
      </div>
      <p id='status' class='hint'>Cargando...</p>
    </div>

    <div class='card'>
      <h2>Perfiles disponibles</h2>
      <table>
        <thead><tr><th>ID</th><th>Nombre</th><th>Tipo</th><th>Estado</th></tr></thead>
        <tbody id='profilesTable'></tbody>
      </table>
    </div>

    <div class='card'>
      <h2>Editor rapido</h2>
      <div class='row'>
        <div class='field'><label for='profileId'>ID</label><input id='profileId' type='text' placeholder='salon_4_tiras'></div>
        <div class='field'><label for='profileName'>Nombre</label><input id='profileName' type='text' placeholder='Salon 4 tiras'></div>
      </div>
      <div class='field'><label for='profileDescription'>Descripcion</label><input id='profileDescription' type='text' placeholder='Perfil de instalacion'></div>
      <div class='row'>
        <div class='field'><label><input id='applyNow' type='checkbox'> Aplicar al guardar</label></div>
        <div class='field'><label><input id='autoApplyOnBoot' type='checkbox'> Activar al guardar y copiar en DEFAULT</label></div>
      </div>
      <div class='field'><label for='profilePayload'>GPIO JSON</label><textarea id='profilePayload'>{"outputs":[{"pin":5,"ledCount":60,"ledType":"ws2812b","colorOrder":"GRB"}]}</textarea></div>
      <div class='field'><label for='microphonePayload'>Microphone JSON</label><textarea id='microphonePayload'>{"enabled":false,"source":"generic_i2c","profileId":"DEFAULT","sampleRate":16000,"fftSize":512,"gainPercent":100,"noiseFloorPercent":8,"pins":{"bclk":-1,"ws":-1,"din":-1}}</textarea></div>
    </div>

    <div class='card json-card'>
      <h3>Respuesta</h3>
      <pre id='resultOut'>Sin llamadas aun.</pre>
    </div>
  </main>
  <script>
    if(localStorage.getItem('dux_show_json')==='1') document.body.classList.add('show-json');

    let profiles = [];
    let selectedProfileId = '';

    function byId(id) { return document.getElementById(id); }
    function setStatus(msg, isErr) {
      const el = byId('status');
      el.textContent = msg;
      el.className = isErr ? 'hint err' : 'hint ok';
    }
    function renderProfiles(data) {
      const table = byId('profilesTable');
      table.innerHTML = '';
      const items = (((data || {}).profiles || {}).items) || [];
      profiles = items;
      for (const item of items) {
        const tr = document.createElement('tr');
        const type = item.builtIn ? 'integrado' : 'usuario';
        const flags = [];
        if (item.isActive) flags.push('activo');
        if (item.isDefault) flags.push('default');
        if (item.readOnly) flags.push('solo lectura');
        tr.innerHTML = '<td><button class="btn ghost" style="padding:4px 8px" onclick="selectProfile(\'' + item.id + '\')">' + item.id + '</button></td>' +
          '<td>' + (item.name || '') + '</td><td>' + type + '</td><td>' + flags.join(', ') + '</td>';
        table.appendChild(tr);
      }
    }
    function selectProfile(id) {
      selectedProfileId = id;
      const item = profiles.find(function(p) { return p.id === id; });
      if (!item) return;
      byId('profileId').value = item.id || '';
      byId('profileName').value = item.name || '';
      byId('profileDescription').value = item.description || '';
      byId('profilePayload').value = JSON.stringify(item.gpio || { outputs: [] }, null, 2);
      byId('microphonePayload').value = JSON.stringify(item.microphone || {
        enabled: false,
        source: 'generic_i2c',
        profileId: 'DEFAULT',
        sampleRate: 16000,
        fftSize: 512,
        gainPercent: 100,
        noiseFloorPercent: 8,
        pins: { bclk: -1, ws: -1, din: -1 }
      }, null, 2);
      byId('autoApplyOnBoot').checked = false;
      byId('applyNow').checked = false;
    }
    async function loadProfiles() {
      setStatus('Leyendo perfiles...', false);
      try {
        const res = await fetch('/api/v1/profiles/gpio');
        const text = await res.text();
        const parsed = JSON.parse(text);
        renderProfiles(parsed);
        byId('resultOut').textContent = JSON.stringify(parsed, null, 2);
        setStatus('Perfiles cargados (HTTP ' + res.status + ').', false);
      } catch (err) {
        byId('resultOut').textContent = String(err);
        setStatus('Error leyendo perfiles.', true);
      }
    }
    async function saveProfile() {
      let gpio;
      try { gpio = JSON.parse(byId('profilePayload').value); } catch (err) {
        setStatus('GPIO JSON invalido.', true);
        byId('resultOut').textContent = String(err);
        return;
      }

      let microphone = null;
      const microphoneRaw = byId('microphonePayload').value.trim();
      if (microphoneRaw.length > 0) {
        try { microphone = JSON.parse(microphoneRaw); } catch (err) {
          setStatus('Microphone JSON invalido.', true);
          byId('resultOut').textContent = String(err);
          return;
        }
      }

      const payload = { profile: {
        id: byId('profileId').value.trim(),
        name: byId('profileName').value.trim(),
        description: byId('profileDescription').value.trim(),
        applyNow: byId('applyNow').checked,
        autoApplyOnBoot: byId('autoApplyOnBoot').checked,
        gpio: gpio
      }};

      if (microphone) {
        payload.profile.microphone = microphone;
      }

      await callJson('/api/v1/profiles/gpio/save', payload, 'Guardando perfil...');

      if (microphone && (payload.profile.applyNow || payload.profile.autoApplyOnBoot)) {
        await callJson('/api/v1/config/microphone', { microphone: microphone }, 'Aplicando microfono del editor...');
      }

      await loadProfiles();
    }
    async function applySelected() {
      const id = selectedProfileId || byId('profileId').value.trim();
      if (!id) { setStatus('Selecciona un perfil.', true); return; }
      await callJson('/api/v1/profiles/gpio/apply', { profile: { id: id } }, 'Aplicando perfil y copiando en DEFAULT...');

      let microphone = null;
      const microphoneRaw = byId('microphonePayload').value.trim();
      if (microphoneRaw.length > 0) {
        try { microphone = JSON.parse(microphoneRaw); } catch (_) { microphone = null; }
      }
      if (microphone) {
        await callJson('/api/v1/config/microphone', { microphone: microphone }, 'Aplicando microfono del perfil...');
      }

      await loadProfiles();
    }
    async function applyAsDefault() {
      const id = selectedProfileId || byId('profileId').value.trim();
      if (!id) { setStatus('Selecciona un perfil.', true); return; }
      await callJson('/api/v1/profiles/gpio/default', { profile: { id: id } }, 'Copiando perfil en DEFAULT...');
      await loadProfiles();
    }
    async function deleteSelected() {
      const id = selectedProfileId || byId('profileId').value.trim();
      if (!id) { setStatus('Selecciona un perfil.', true); return; }
      await callJson('/api/v1/profiles/gpio/delete', { profile: { id: id } }, 'Borrando perfil...');
      await loadProfiles();
    }
    async function callJson(url, payload, statusLabel) {
      setStatus(statusLabel, false);
      try {
        const res = await fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) });
        const text = await res.text();
        let pretty = text;
        try { pretty = JSON.stringify(JSON.parse(text), null, 2); } catch (_) {}
        byId('resultOut').textContent = pretty;
        setStatus((res.ok ? 'Operacion completada' : 'Error en operacion') + ' (HTTP ' + res.status + ').', !res.ok);
      } catch (err) {
        byId('resultOut').textContent = String(err);
        setStatus('Error de red.', true);
      }
    }
    loadProfiles();
  </script>
</div>
</body>
</html>
)HTML";
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
  <style>
    :root { --bg:#f4f1e8; --card:#fff; --line:#d8cdb8; --text:#33291e; --primary:#8a5c1d; }
    body { margin:0; padding:16px; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#7a4f15; text-decoration:none; font-weight:600; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    textarea, pre { width:100%; min-height:130px; border:1px solid var(--line); border-radius:8px; padding:8px; font-family:Consolas, monospace; white-space:pre-wrap; }
    button { border:0; border-radius:8px; padding:8px 10px; color:#fff; background:var(--primary); cursor:pointer; margin-right:8px; margin-bottom:8px; }
  </style>
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
  <style>
    :root { --bg:#edf3f8; --card:#fff; --line:#ccd8e3; --text:#22303a; --primary:#246a92; }
    body { margin:0; padding:16px; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#1d5e86; text-decoration:none; font-weight:600; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    pre { width:100%; min-height:140px; border:1px solid var(--line); border-radius:8px; padding:8px; font-family:Consolas, monospace; white-space:pre-wrap; }
    button { border:0; border-radius:8px; padding:8px 10px; color:#fff; background:var(--primary); cursor:pointer; }
  </style>
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
  <style>
    :root {
      --bg:#f4f2ee;
      --card:#fffdf8;
      --line:#d5cfc2;
      --text:#2a2518;
      --muted:#6e6050;
      --primary:#8a5d1c;
      --secondary:#2e6b8a;
      --danger:#9f1c1c;
      --ok:#1f6a39;
    }
    body { margin:0; min-height:100vh; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:20px auto; padding:14px; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; box-shadow:0 8px 28px rgba(42,37,24,.08); }
    .actions { display:flex; flex-wrap:wrap; gap:10px; margin-bottom:10px; }
    .btn { border:0; border-radius:8px; padding:9px 14px; font-weight:600; cursor:pointer; color:#fff; background:var(--primary); font-size:14px; }
    .btn.alt { background:var(--secondary); }
    .btn.ghost { background:#7d8b95; text-decoration:none; display:inline-block; }
    .btn.danger { background:var(--danger); }
    h1,h2 { margin:0 0 10px 0; }
    .hint { font-size:13px; color:var(--muted); }
    .ok { color:var(--ok); }
    .err { color:var(--danger); }
    textarea { width:100%; min-height:350px; border:1px solid var(--line); border-radius:8px; padding:10px; font-family:Consolas,monospace; font-size:13px; resize:vertical; box-sizing:border-box; }
    pre { background:#f0ece4; border:1px solid var(--line); border-radius:8px; padding:10px; white-space:pre-wrap; overflow:auto; font-family:Consolas,monospace; font-size:12px; }
    .file-row { display:flex; gap:10px; align-items:center; flex-wrap:wrap; }
    input[type=file] { font-size:14px; }
    .json-card { display:none; }
    body.show-json .json-card { display:block; }
  </style>
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
  <style>
    :root { --bg:#f4f2ee; --card:#fff; --line:#d5cfc2; --text:#2a2518; --primary:#8a5d1c; }
    body { margin:0; padding:16px; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { max-width:900px; margin:0 auto; }
    .nav { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:10px; }
    .nav a { color:#6a4a10; text-decoration:none; font-weight:600; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
    textarea, pre { width:100%; min-height:200px; border:1px solid var(--line); border-radius:8px; padding:8px; font-family:Consolas, monospace; white-space:pre-wrap; box-sizing:border-box; }
    button { border:0; border-radius:8px; padding:8px 10px; color:#fff; background:var(--primary); cursor:pointer; margin-right:8px; }
  </style>
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
  html.replace("__NAV__", buildNavHtml());
  return html;
}
