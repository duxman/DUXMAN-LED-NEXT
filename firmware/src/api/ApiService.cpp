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
    :root { --bg:#0d1b22; --card:#132530; --card2:#0f2028; --line:#1e3a48; --text:#d8edf5; --muted:#7aacbf; --accent:#0fd0e0; --ok:#22d68a; --warn:#f5a623; }
    body { margin:0; min-height:100vh; background:var(--bg); color:var(--text); font-family:Trebuchet MS,Segoe UI,sans-serif; }
    main { padding:28px 20px; }
    .hero { display:flex; flex-direction:column; align-items:center; text-align:center; margin-bottom:24px; }
    .hero img { max-width:220px; margin-bottom:16px; }
    .hero h1 { font-size:26px; margin:0 0 4px; color:#fff; }
    .hero .tagline { font-size:14px; color:var(--muted); margin:0; }
    .grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(260px,1fr)); gap:14px; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:16px; }
    .card h2 { font-size:13px; text-transform:uppercase; letter-spacing:.08em; color:var(--muted); margin:0 0 12px; border-bottom:1px solid var(--line); padding-bottom:8px; }
    .row { display:flex; justify-content:space-between; align-items:baseline; padding:5px 0; border-bottom:1px solid rgba(255,255,255,.04); }
    .row:last-child { border-bottom:none; }
    .lbl { font-size:13px; color:var(--muted); }
    .val { font-size:14px; font-weight:600; color:var(--text); text-align:right; font-family:Consolas,monospace; }
    .val.accent { color:var(--accent); }
    .val.ok { color:var(--ok); }
    .val.warn { color:var(--warn); }
    .badge { display:inline-block; font-size:11px; padding:2px 8px; border-radius:99px; background:rgba(15,208,224,.15); color:var(--accent); border:1px solid rgba(15,208,224,.3); }
    .bar-wrap { background:rgba(255,255,255,.06); border-radius:4px; height:6px; width:100%; margin-top:4px; }
    .bar-fill { height:6px; border-radius:4px; background:linear-gradient(90deg,var(--ok),var(--accent)); transition:width .4s; }
    .link { color:var(--accent); font-size:12px; text-decoration:none; word-break:break-all; }
    .link:hover { text-decoration:underline; }
    .loader { text-align:center; color:var(--muted); padding:30px; }
    .section-title { font-size:11px; text-transform:uppercase; letter-spacing:.1em; color:var(--muted); margin:20px 0 8px; }
    .chip-bool { display:inline-flex; align-items:center; gap:4px; font-size:12px; }
    .dot { width:8px; height:8px; border-radius:50%; display:inline-block; }
    .dot.on { background:var(--ok); } .dot.off { background:#444; }
  </style>
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
  <style>
    :root { --bg:#f4f1e8; --card:#fffdfa; --line:#d8cdb8; --text:#33291e; --muted:#6e6355; --primary:#8a5c1d; --secondary:#2f6c83; --danger:#a12626; --success:#1f6a39; }
    body { margin:0; min-height:100vh; background:var(--bg); color:var(--text); font-family:Trebuchet MS, Segoe UI, sans-serif; }
    main { padding:20px; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:16px; margin-bottom:14px; }
    .card h2 { margin:0 0 10px; font-size:17px; }
    .actions { display:flex; flex-wrap:wrap; gap:10px; margin-top:12px; }
    .row { display:flex; flex-wrap:wrap; gap:12px; }
    .field { flex:1 1 200px; margin-bottom:10px; }
    .btn { border:0; border-radius:8px; padding:9px 14px; font-weight:600; cursor:pointer; color:#fff; background:var(--primary); font-size:13px; }
    .btn.alt { background:var(--secondary); }
    .btn.danger { background:var(--danger); }
    .btn.sm { padding:5px 10px; font-size:12px; }
    label { display:block; font-size:13px; color:var(--muted); margin-bottom:4px; }
    input { width:100%; border:1px solid var(--line); border-radius:8px; padding:8px 10px; font-size:14px; background:#fff; box-sizing:border-box; }
    pre { background:#f4eee3; border:1px solid var(--line); border-radius:8px; padding:10px; white-space:pre-wrap; overflow:auto; font-family:Consolas,monospace; font-size:12px; margin:0; max-height:400px; }
    .hint { font-size:13px; color:var(--muted); margin:6px 0 0; }
    .ok { color:var(--success); }
    .err { color:var(--danger); }
    .profile-grid { display:grid; gap:10px; grid-template-columns:repeat(auto-fill,minmax(280px,1fr)); margin-top:10px; }
    .profile-card { border:2px solid var(--line); border-radius:10px; padding:12px; background:#fffef8; }
    .profile-card.is-default { border-color:#2f6c83; background:#f0f7fb; }
    .profile-card .pname { font-size:15px; font-weight:700; margin:0 0 2px; }
    .profile-card .pid { font-size:11px; color:var(--muted); margin:0 0 4px; font-family:Consolas,monospace; }
    .profile-card .pdesc { font-size:12px; color:var(--muted); margin:0 0 8px; min-height:16px; }
    .badges { display:flex; gap:5px; flex-wrap:wrap; margin-bottom:8px; }
    .badge { font-size:10px; font-weight:700; padding:2px 7px; border-radius:99px; background:#ddd; color:#444; }
    .badge.builtin { background:#d8cdb8; color:#5a4a3a; }
    .badge.default { background:#2f6c83; color:#fff; }
    .badge.user { background:#8a5c1d; color:#fff; }
    .pactions { display:flex; gap:6px; flex-wrap:wrap; }
    .resp-card { display:none; }
    body.show-resp .resp-card { display:block; }
  </style>
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
