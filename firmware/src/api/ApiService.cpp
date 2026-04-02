#include "api/ApiService.h"
#include "core/BuildProfile.h"

#include "effects/EffectRegistry.h"

#include <ArduinoJson.h>

ApiService::ApiService(CoreState &state, NetworkConfig &networkConfig, GpioConfig &gpioConfig,
                       StorageService &storageService, WifiService &wifiService,
                       ProfileService &profileService)
    : state_(state), networkConfig_(networkConfig), gpioConfig_(gpioConfig),
      storageService_(storageService), wifiService_(wifiService),
      profileService_(profileService), httpServer_(80) {}

void ApiService::begin() {
  setupHttpRoutes();
  httpServer_.begin();

  Serial.println("[api] ready: GET /api/v1/state | PATCH /api/v1/state {json}");
  Serial.println("[api] ready: GET /api/v1/config/network | PATCH /api/v1/config/network {json}");
  Serial.println("[api] ready: GET /api/v1/config/gpio | PATCH /api/v1/config/gpio {json}");
  Serial.println("[api] ready: GET /api/v1/profiles/gpio | POST /api/v1/profiles/gpio/save|apply|default|delete {json}");
  Serial.println("[api] ready: GET /api/v1/config/debug | PATCH /api/v1/config/debug {json}");
  Serial.println("[api] ready: GET /api/v1/config/all | POST /api/v1/config/all {json}");
  Serial.println("[api] ready: GET /api/v1/hardware");
  Serial.println("[api] ready: GET /api/v1/release");
  Serial.println("[api] ui: GET / | GET /config | GET /config/network | GET /config/gpio | GET /config/debug | GET /config/manual | GET /api | GET /version");
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
    if (changed && !storageService_.saveState()) {
      Serial.println("{\"error\":\"persistence_failed\"}");
      return;
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

  if (command == "GET /api/v1/config/debug") {
    Serial.print("{\"debug\":{\"enabled\":");
    Serial.print(networkConfig_.debug.enabled ? "true" : "false");
    Serial.print(",\"heartbeatMs\":");
    Serial.print(networkConfig_.debug.heartbeatMs);
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

    if (changed && !storageService_.saveNetworkConfig()) {
      Serial.println("{\"error\":\"persistence_failed\"}");
      return;
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

  if (command == "GET /api/v1/config/gpio") {
    Serial.println(gpioConfig_.toJson());
    return;
  }

  if (command == "GET /api/v1/profiles/gpio") {
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

    if (changed && !storageService_.saveGpioConfig()) {
      Serial.println("{\"error\":\"persistence_failed\"}");
      return;
    }

    if (changed) {
      profileService_.applyActiveConfig();
    }

    Serial.print("{\"updated\":");
    Serial.print(changed ? "true" : "false");
    Serial.print(",\"gpio\":");
    Serial.print(gpioConfig_.toJson());
    Serial.println("}");
    return;
  }

  if (command.startsWith("POST /api/v1/profiles/gpio/save ") ||
      command.startsWith("PATCH /api/v1/profiles/gpio/save ")) {
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

  if (command.startsWith("POST /api/v1/profiles/gpio/apply ") ||
      command.startsWith("PATCH /api/v1/profiles/gpio/apply ")) {
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

  if (command.startsWith("POST /api/v1/profiles/gpio/default ") ||
      command.startsWith("PATCH /api/v1/profiles/gpio/default ")) {
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

  if (command.startsWith("POST /api/v1/profiles/gpio/delete ") ||
      command.startsWith("PATCH /api/v1/profiles/gpio/delete ")) {
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

    const String rawPayload = command.substring(payloadPos);
    JsonDocument parsed;
    const DeserializationError parseResult = deserializeJson(parsed, rawPayload);
    if (parseResult) {
      Serial.println("{\"error\":\"invalid_json\"}");
      return;
    }

    JsonObjectConst incoming = parsed.as<JsonObjectConst>();
    JsonDocument patchDoc;
    JsonObject patchRoot = patchDoc.to<JsonObject>();
    if (!incoming["debug"].isNull()) {
      patchRoot["debug"] = incoming["debug"];
    } else {
      patchRoot["debug"] = incoming;
    }

    String normalizedPayload;
    serializeJson(patchDoc, normalizedPayload);

    String error;
    const bool changed = networkConfig_.applyPatchJson(normalizedPayload, &error);
    if (!error.isEmpty()) {
      Serial.print("{\"error\":\"");
      Serial.print(error);
      Serial.println("\"}");
      return;
    }

    if (changed && !storageService_.saveNetworkConfig()) {
      Serial.println("{\"error\":\"persistence_failed\"}");
      return;
    }

    Serial.print("{\"updated\":");
    Serial.print(changed ? "true" : "false");
    Serial.print(",\"debug\":{\"enabled\":");
    Serial.print(networkConfig_.debug.enabled ? "true" : "false");
    Serial.print(",\"heartbeatMs\":");
    Serial.print(networkConfig_.debug.heartbeatMs);
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

    String netPayload;
    { JsonDocument d; d["network"] = root["network"]; d["debug"] = root["debug"]; serializeJson(d, netPayload); }
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

    networkConfig_ = netCandidate;
    gpioConfig_ = gpioCandidate;
    storageService_.saveNetworkConfig();
    storageService_.saveGpioConfig();
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

  httpServer_.on("/api/profiles/gpio", HTTP_GET, [this]() {
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

  httpServer_.on("/api/v1/config/gpio", HTTP_ANY, [this]() {
    handleHttpGpioRoute();
  });

  httpServer_.on("/api/v1/profiles/gpio", HTTP_ANY, [this]() {
    handleHttpGpioProfilesRoute();
  });

  httpServer_.on("/api/v1/profiles/gpio/save", HTTP_ANY, [this]() {
    handleHttpGpioProfilesSaveRoute();
  });

  httpServer_.on("/api/v1/profiles/gpio/apply", HTTP_ANY, [this]() {
    handleHttpGpioProfilesApplyRoute();
  });

  httpServer_.on("/api/v1/profiles/gpio/default", HTTP_ANY, [this]() {
    handleHttpGpioProfilesDefaultRoute();
  });

  httpServer_.on("/api/v1/profiles/gpio/delete", HTTP_ANY, [this]() {
    handleHttpGpioProfilesDeleteRoute();
  });

  httpServer_.on("/api/v1/config/debug", HTTP_ANY, [this]() {
    handleHttpDebugRoute();
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
    if (changed && !storageService_.saveState()) {
      httpServer_.send(500, "application/json", "{\"error\":\"persistence_failed\"}");
      return;
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

    if (changed && !storageService_.saveNetworkConfig()) {
      httpServer_.send(500, "application/json", "{\"error\":\"persistence_failed\"}");
      return;
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

    if (changed && !storageService_.saveGpioConfig()) {
      httpServer_.send(500, "application/json", "{\"error\":\"persistence_failed\"}");
      return;
    }

    if (changed) {
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

void ApiService::handleHttpGpioProfilesRoute() {
  if (httpServer_.method() == HTTP_GET) {
    httpServer_.send(200, "application/json", profileService_.listProfilesJson());
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

void ApiService::handleHttpGpioProfilesSaveRoute() {
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

void ApiService::handleHttpGpioProfilesApplyRoute() {
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

void ApiService::handleHttpGpioProfilesDefaultRoute() {
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

void ApiService::handleHttpGpioProfilesDeleteRoute() {
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

void ApiService::handleHttpDebugRoute() {
  const HTTPMethod method = httpServer_.method();

  if (method == HTTP_GET) {
    String response = "{\"debug\":{\"enabled\":";
    response += networkConfig_.debug.enabled ? "true" : "false";
    response += ",\"heartbeatMs\":";
    response += networkConfig_.debug.heartbeatMs;
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

    JsonDocument parsed;
    const DeserializationError parseResult = deserializeJson(parsed, payload);
    if (parseResult) {
      httpServer_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
      return;
    }

    JsonObjectConst incoming = parsed.as<JsonObjectConst>();
    JsonDocument patchDoc;
    JsonObject patchRoot = patchDoc.to<JsonObject>();
    if (!incoming["debug"].isNull()) {
      patchRoot["debug"] = incoming["debug"];
    } else {
      patchRoot["debug"] = incoming;
    }

    String normalizedPayload;
    serializeJson(patchDoc, normalizedPayload);

    String error;
    const bool changed = networkConfig_.applyPatchJson(normalizedPayload, &error);
    if (!error.isEmpty()) {
      String response = "{\"error\":\"";
      response += error;
      response += "\"}";
      httpServer_.send(400, "application/json", response);
      return;
    }

    if (changed && !storageService_.saveNetworkConfig()) {
      httpServer_.send(500, "application/json", "{\"error\":\"persistence_failed\"}");
      return;
    }

    String response = "{\"updated\":";
    response += changed ? "true" : "false";
    response += ",\"debug\":{\"enabled\":";
    response += networkConfig_.debug.enabled ? "true" : "false";
    response += ",\"heartbeatMs\":";
    response += networkConfig_.debug.heartbeatMs;
    response += "}}";
    httpServer_.send(200, "application/json", response);
    return;
  }

  httpServer_.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
}

String ApiService::buildFullConfigJson() const {
  JsonDocument doc;

  // Merge networkConfig
  JsonDocument netDoc;
  deserializeJson(netDoc, networkConfig_.toJson());
  doc["network"] = netDoc["network"];
  doc["debug"] = netDoc["debug"];

  // Merge gpioConfig
  JsonDocument gpioDoc;
  deserializeJson(gpioDoc, gpioConfig_.toJson());
  doc["gpio"] = gpioDoc["gpio"];

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

    String netPayload;
    { JsonDocument d; d["network"] = root["network"]; d["debug"] = root["debug"]; serializeJson(d, netPayload); }
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

    // Todo válido — aplicar
    networkConfig_ = netCandidate;
    gpioConfig_ = gpioCandidate;

    if (!storageService_.saveNetworkConfig() || !storageService_.saveGpioConfig()) {
      httpServer_.send(500, "application/json", "{\"error\":\"persistence_failed\"}");
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
  netPath["patch"]["summary"] = "Actualizar configuracion de red con JSON parcial";
  netPath["post"]["summary"] = "Alias de PATCH para clientes limitados";

  JsonObject debugPath = paths["/api/v1/config/debug"].to<JsonObject>();
  debugPath["get"]["summary"] = "Obtener configuracion de debug";
  debugPath["patch"]["summary"] = "Actualizar debug (enabled y heartbeatMs)";
  debugPath["post"]["summary"] = "Alias de PATCH para clientes limitados";

  JsonObject gpioPath = paths["/api/v1/config/gpio"].to<JsonObject>();
  gpioPath["get"]["summary"] = "Obtener configuracion GPIO";
  gpioPath["patch"]["summary"] = "Actualizar GPIO (outputs: pin, ledCount, ledType, colorOrder)";
  gpioPath["post"]["summary"] = "Alias de PATCH para clientes limitados";

  JsonObject profilesPath = paths["/api/v1/profiles/gpio"].to<JsonObject>();
  profilesPath["get"]["summary"] = "Listar perfiles GPIO guardados e integrados";

  JsonObject profilesSavePath = paths["/api/v1/profiles/gpio/save"].to<JsonObject>();
  profilesSavePath["post"]["summary"] = "Guardar o actualizar un perfil GPIO de usuario";
  profilesSavePath["patch"]["summary"] = "Alias de POST para guardar perfil GPIO";

  JsonObject profilesApplyPath = paths["/api/v1/profiles/gpio/apply"].to<JsonObject>();
  profilesApplyPath["post"]["summary"] = "Aplicar un perfil GPIO al runtime actual";
  profilesApplyPath["patch"]["summary"] = "Alias de POST para aplicar perfil GPIO";

  JsonObject profilesDefaultPath = paths["/api/v1/profiles/gpio/default"].to<JsonObject>();
  profilesDefaultPath["post"]["summary"] = "Establecer o limpiar el perfil GPIO por defecto de arranque";
  profilesDefaultPath["patch"]["summary"] = "Alias de POST para fijar perfil por defecto";

  JsonObject profilesDeletePath = paths["/api/v1/profiles/gpio/delete"].to<JsonObject>();
  profilesDeletePath["post"]["summary"] = "Eliminar un perfil GPIO de usuario";
  profilesDeletePath["patch"]["summary"] = "Alias de POST para eliminar perfil GPIO";

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

  JsonObject apiConfigDebugUiPath = paths["/api/config/debug"].to<JsonObject>();
  apiConfigDebugUiPath["get"]["summary"] = "UI API para configuracion debug";

  JsonObject apiConfigGpioUiPath = paths["/api/config/gpio"].to<JsonObject>();
  apiConfigGpioUiPath["get"]["summary"] = "UI API para configuracion GPIO";

  JsonObject apiProfilesUiPath = paths["/api/profiles/gpio"].to<JsonObject>();
  apiProfilesUiPath["get"]["summary"] = "UI API para perfiles GPIO";

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
  serialCommands.add("PATCH /api/v1/config/network {json}");
  serialCommands.add("GET /api/v1/config/debug");
  serialCommands.add("PATCH /api/v1/config/debug {\"debug\":{\"enabled\":true,\"heartbeatMs\":5000}}");
  serialCommands.add("GET /api/v1/config/gpio");
  serialCommands.add("PATCH /api/v1/config/gpio {\"gpio\":{\"outputs\":[{\"pin\":8,\"ledCount\":60,\"ledType\":\"ws2812b\",\"colorOrder\":\"GRB\"}]}}");
  serialCommands.add("GET /api/v1/profiles/gpio");
  serialCommands.add("POST /api/v1/profiles/gpio/save {\"profile\":{\"id\":\"mi_perfil\",\"name\":\"Mi perfil\",\"gpio\":{\"outputs\":[{\"pin\":8,\"ledCount\":60,\"ledType\":\"ws2812b\",\"colorOrder\":\"GRB\"}]},\"autoApplyOnBoot\":true}} ");
  serialCommands.add("POST /api/v1/profiles/gpio/apply {\"profile\":{\"id\":\"gledopto_gl_c_017wl_d\",\"setDefault\":true}} ");
  serialCommands.add("POST /api/v1/profiles/gpio/default {\"profile\":{\"id\":\"gledopto_gl_c_017wl_d\"}} ");
  serialCommands.add("POST /api/v1/profiles/gpio/delete {\"profile\":{\"id\":\"mi_perfil\"}} ");
  serialCommands.add("GET /api/v1/config/all");
  serialCommands.add("POST /api/v1/config/all {json_completo}");
  serialCommands.add("GET /api/v1/hardware");
  serialCommands.add("GET /api/v1/release");

  String out;
  serializeJsonPretty(doc, out);
  return out;
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
      display:grid;
      place-items:center;
      padding:16px;
    }
    main { width:min(920px, 100%); }
    .hero {
      background:var(--card);
      border:1px solid var(--line);
      border-radius:16px;
      box-shadow:0 16px 34px rgba(31,22,14,.1);
      padding:22px;
      margin-bottom:12px;
    }
    h1 { margin:0 0 8px 0; font-size:30px; }
    p { margin:0; color:var(--muted); }
    .menu {
      display:grid;
      grid-template-columns:repeat(auto-fit, minmax(220px, 1fr));
      gap:10px;
      margin-top:14px;
    }
    .panel {
      display:grid;
      grid-template-columns:1.5fr .9fr;
      gap:12px;
      margin-top:12px;
    }
    .controls {
      display:grid;
      gap:12px;
      background:#fff;
      border:1px solid var(--line);
      border-radius:14px;
      padding:16px;
      box-shadow:0 16px 34px rgba(31,22,14,.08);
    }
    .stats {
      background:#fff;
      border:1px solid var(--line);
      border-radius:14px;
      padding:16px;
      box-shadow:0 16px 34px rgba(31,22,14,.08);
    }
    .row {
      display:grid;
      grid-template-columns:repeat(2, minmax(0, 1fr));
      gap:10px;
    }
    .control-sections {
      display:grid;
      grid-template-columns:repeat(2, minmax(0, 1fr));
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
    .control-card h2 {
      margin:0;
      font-size:18px;
    }
    .control-card p {
      font-size:13px;
    }
    .row.colors {
      grid-template-columns:repeat(2, minmax(0, 1fr));
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
      border:1px solid var(--line);
      border-radius:12px;
      background:#fffaf2;
      padding:12px;
      overflow:auto;
      font-family:Consolas, monospace;
      font-size:12px;
    }
    .item {
      display:block;
      text-decoration:none;
      color:var(--text);
      background:#fff;
      border:1px solid var(--line);
      border-left:6px solid var(--primary);
      border-radius:12px;
      padding:12px;
      transition:transform .14s ease, box-shadow .14s ease;
    }
    .item.version { border-left-color:var(--accent); }
    .item:hover { transform:translateY(-2px); box-shadow:0 10px 20px rgba(0,0,0,.08); }
    .item h2 { margin:0 0 6px 0; font-size:18px; }
    .item span { color:var(--muted); font-size:13px; }
    @media (max-width: 760px) {
      .panel, .control-sections, .row, .row.colors { grid-template-columns:1fr; }
    }
  </style>
</head>
<body>
  <main>
    <section class='hero'>
      <h1>DUXMAN-LED-NEXT __FW_VERSION__</h1>
      <p>Control rapido de efectos fijos y navegacion principal.</p>
      <div class='panel'>
        <section class='controls'>
          <div>
            <div class='switch'>
              <input id='power' type='checkbox'>
              <span>Salida activa</span>
            </div>
            <span class='badge' id='status'>Cargando estado...</span>
          </div>

          <div class='control-sections'>
            <section class='control-card'>
              <div>
                <h2>Seleccion de colores</h2>
                <p>Define los tres colores de primer plano y el color de fondo.</p>
              </div>

              <label>
                Color de fondo / off
                <input id='backgroundColor' type='color' value='#000000'>
              </label>

              <div class='row colors'>
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
            </section>

            <section class='control-card effect'>
              <div>
                <h2>Seleccion de efecto</h2>
                <p>Elige el tipo de render, brillo y numero de secciones.</p>
              </div>

              <label>
                Efecto
                <select id='effect'>
                  __EFFECT_OPTIONS__
                </select>
              </label>

              <label>
                Numero de secciones
                <input id='sectionCount' type='number' min='1' max='255' value='3'>
              </label>

              <label>
                Brillo global
                <input id='brightness' type='range' min='0' max='255' value='128'>
                <span id='brightnessValue'>128</span>
              </label>
            </section>
          </div>

          <div class='actions'>
            <button onclick='applyState()'>Aplicar</button>
            <button class='alt' onclick='loadState()'>Recargar</button>
          </div>
        </section>

        <aside class='stats'>
          <h2>Estado runtime</h2>
          <p>El efecto se guarda en LittleFS al aplicar cambios.</p>
          <pre id='stateOut'>Sin datos aun.</pre>
        </aside>
      </div>
      <div class='menu'>
        <a class='item' href='/config'>
          <h2>Configuracion</h2>
          <span>Acceso a secciones configurables del dispositivo.</span>
        </a>
        <a class='item' href='/api'>
          <h2>API</h2>
          <span>Documentacion y tester HTTP/Serial.</span>
        </a>
        <a class='item version' href='/version'>
          <h2>Version</h2>
          <span>Informacion de release y metadatos del firmware.</span>
        </a>
      </div>
    </section>
  </main>
  <script>
    const out = document.getElementById('stateOut');
    const status = document.getElementById('status');
    const brightness = document.getElementById('brightness');
    const brightnessValue = document.getElementById('brightnessValue');
    brightness.addEventListener('input', () => {
      brightnessValue.textContent = brightness.value;
    });

    function renderState(state) {
      document.getElementById('power').checked = !!state.power;
      document.getElementById('effect').value = state.effect || (state.effectId === 1 ? 'gradient' : 'fixed');
      document.getElementById('brightness').value = state.brightness ?? 128;
      brightnessValue.textContent = document.getElementById('brightness').value;
      document.getElementById('sectionCount').value = state.sectionCount ?? 3;
      const colors = Array.isArray(state.primaryColors) ? state.primaryColors : ['#ff4d00','#ffd400','#00b8d9'];
      document.getElementById('color0').value = colors[0] || '#ff4d00';
      document.getElementById('color1').value = colors[1] || '#ffd400';
      document.getElementById('color2').value = colors[2] || '#00b8d9';
      document.getElementById('backgroundColor').value = state.backgroundColor || '#000000';
      out.textContent = JSON.stringify(state, null, 2);
      status.textContent = 'Estado cargado';
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
        primaryColors: [
          document.getElementById('color0').value,
          document.getElementById('color1').value,
          document.getElementById('color2').value
        ],
        backgroundColor: document.getElementById('backgroundColor').value
      };
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

    loadState();
  </script>
</body>
</html>
)HTML";
  const String versionLabel = String(BuildProfile::kFwVersion);
  html.replace("__FW_VERSION__", versionLabel);
  html.replace("__EFFECT_OPTIONS__", EffectRegistry::buildHtmlOptions(state_.effectId));
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
  <main>
    <div class='topnav'>
      <a href='/'>Inicio</a>
      <a href='/api'>API</a>
      <a href='/version'>Version</a>
    </div>

    <section class='card'>
      <h1>Configuracion</h1>
      <p>Selecciona la seccion de configuracion que quieres editar.</p>
      <div class='grid'>
        <a class='box' href='/config/network'>
          <h2>Network</h2>
          <p>WiFi, AP/STA, hostname, IP y DNS.</p>
        </a>
        <a class='box' href='/config/gpio'>
          <h2>GPIO</h2>
          <p>Pin LED, cantidad, tipo de LED y orden de color.</p>
        </a>
        <a class='box' href='/config/profiles'>
          <h2>Profiles</h2>
          <p>Guardar, aplicar y fijar perfiles GPIO por defecto.</p>
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
</body>
</html>
)HTML";
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
  <main>
    <div class='nav'>
      <a href='/'>Inicio</a>
      <a href='/config'>Configuracion</a>
      <a href='/version'>Version</a>
    </div>

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
</body>
</html>
)HTML";
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
  <main>
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/config/network'>Config Network</a><a href='/api/config/gpio'>Config GPIO</a><a href='/api/config/debug'>Config Debug</a><a href='/api/profiles/gpio'>Profiles</a><a href='/api/hardware'>Hardware</a><a href='/api/release'>Release</a></div>
    <div class='card'>
      <h1>API State</h1>
      <button onclick='getState()'>GET /api/v1/state</button>
      <button onclick='patchState()'>PATCH /api/v1/state</button>
      <textarea id='payload'>{"power":true,"brightness":180,"effect":"fixed","sectionCount":6,"primaryColors":["#FF4D00","#FFD400","#00B8D9"],"backgroundColor":"#050505"}</textarea>
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
</body>
</html>
)HTML";
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
  <main>
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/state'>State</a><a href='/api/config/gpio'>Config GPIO</a><a href='/api/config/debug'>Config Debug</a><a href='/api/release'>Release</a></div>
    <div class='card'>
      <h1>API Config Network</h1>
      <button onclick='getCfg()'>GET /api/v1/config/network</button>
      <button onclick='patchCfg()'>PATCH /api/v1/config/network</button>
      <textarea id='payload'>{"network":{"wifi":{"mode":"ap_sta","apAvailability":"always"},"dns":{"hostname":"duxman-led-1"}}}</textarea>
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
</body>
</html>
)HTML";
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
  <main>
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/state'>State</a><a href='/api/config/network'>Config Network</a><a href='/api/config/gpio'>Config GPIO</a><a href='/api/config/debug'>Config Debug</a></div>
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
</body>
</html>
)HTML";
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
      h1 { font-size:22px; }
      main { padding:10px; }
    }
  </style>
</head>
<body>
  <main>
    <div class='card'>
      <h1>Configuracion de Red</h1>
      <p class='hint'>Edita WiFi/AP/STA y guarda en el dispositivo. Los cambios se aplican al momento.</p>
      <div class='actions'>
        <a href='/' class='btn ghost' style='text-decoration:none;display:inline-block;'>Inicio</a>
        <a href='/config' class='btn ghost' style='text-decoration:none;display:inline-block;'>Config</a>
        <a href='/config/gpio' class='btn ghost' style='text-decoration:none;display:inline-block;'>GPIO</a>
        <a href='/config/debug' class='btn ghost' style='text-decoration:none;display:inline-block;'>Debug</a>
        <a href='/api' class='btn ghost' style='text-decoration:none;display:inline-block;'>API</a>
        <a href='/version' class='btn ghost' style='text-decoration:none;display:inline-block;'>Version</a>
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

    <div class='card'>
      <h2>Payload / Respuesta</h2>
      <h3>Payload Enviado</h3>
      <pre id='payloadOut'>{}</pre>
      <h3>Respuesta API</h3>
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

      setValue('wifiMode', wifi.mode || 'ap');
      setValue('apAvailability', wifi.apAvailability || 'always');
      setValue('ssid', conn.ssid || '');
      setValue('password', conn.password || '');
      setValue('hostname', dns.hostname || 'duxman-led');

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
</body>
</html>
)HTML";
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
  </style>
</head>
<body>
  <main>
    <div class='card'>
      <h1>Configuracion Debug</h1>
      <p class='hint'>Controla trazas debug y el intervalo del heartbeat serial.</p>
      <div class='actions'>
        <a href='/' class='btn ghost'>Inicio</a>
        <a href='/config' class='btn ghost'>Config</a>
        <a href='/config/network' class='btn ghost'>Network</a>
        <a href='/config/gpio' class='btn ghost'>GPIO</a>
        <a href='/api/config/debug' class='btn ghost'>API Debug</a>
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

    function buildPayload() {
      return {
        debug: {
          enabled: byId('debugEnabled').checked,
          heartbeatMs: Number(byId('heartbeatMs').value || 0)
        }
      };
    }

    async function loadDebug() {
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
</body>
</html>
)HTML";
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
  <main>
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/state'>State</a><a href='/api/config/network'>Config Network</a><a href='/api/config/gpio'>Config GPIO</a><a href='/api/release'>Release</a></div>
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
</body>
</html>
)HTML";
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
  <main>
    <div class='card'>
      <h1>Configuracion LED (GPIO)</h1>
      <p class='hint'>Configura las salidas LED del controlador. Max __MAX_OUTPUTS__ salidas.</p>
      <div class='actions'>
        <a href='/' class='btn ghost'>Inicio</a>
        <a href='/config' class='btn ghost'>Config</a>
        <a href='/config/network' class='btn ghost'>Network</a>
        <a href='/config/debug' class='btn ghost'>Debug</a>
        <a href='/config/profiles' class='btn ghost'>Profiles</a>
        <a href='/api/config/gpio' class='btn ghost'>API GPIO</a>
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

    <div class='card'>
      <h3>Payload</h3>
      <pre id='payloadOut'>{}</pre>
      <h3>Respuesta</h3>
      <pre id='resultOut'>Sin llamadas aun.</pre>
    </div>
  </main>

  <script>
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
</body>
</html>
)HTML";
  const String buildPin = String(BuildProfile::kLedPin);
  const String buildCount = String(BuildProfile::kLedCount);
  const String maxOutputs = String(kMaxLedOutputs);
  html.replace("__BUILD_PIN__", buildPin);
  html.replace("__BUILD_COUNT__", buildCount);
  html.replace("__MAX_OUTPUTS__", maxOutputs);
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
  <main>
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/state'>State</a><a href='/api/config/network'>Config Network</a><a href='/api/config/gpio'>Config GPIO</a><a href='/api/config/debug'>Config Debug</a><a href='/api/release'>Release</a></div>
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
</body>
</html>
)HTML";
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
  <main>
    <div class='nav'>
      <a href='/'>Inicio</a>
      <a href='/config'>Configuracion</a>
      <a href='/config/network'>Network</a>
      <a href='/config/gpio'>GPIO</a>
      <a href='/config/profiles'>Profiles</a>
      <a href='/config/debug'>Debug</a>
      <a href='/api/hardware'>Hardware</a>
      <a href='/api'>API</a>
    </div>

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
</body>
</html>
)HTML";
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
  </style>
</head>
<body>
  <main>
    <div class='card'>
      <h1>Perfiles GPIO</h1>
      <p class='hint'>Gestiona presets integrados y perfiles guardados de usuario.</p>
      <div class='actions'>
        <a href='/' class='btn ghost'>Inicio</a>
        <a href='/config' class='btn ghost'>Config</a>
        <a href='/config/gpio' class='btn ghost'>GPIO</a>
        <a href='/api/profiles/gpio' class='btn ghost'>API Profiles</a>
        <button class='btn alt' onclick='loadProfiles()'>Recargar</button>
        <button class='btn' onclick='saveProfile()'>Guardar perfil</button>
        <button class='btn alt' onclick='applySelected(false)'>Aplicar</button>
        <button class='btn alt' onclick='applySelected(true)'>Aplicar + Default</button>
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
        <div class='field'><label><input id='autoApplyOnBoot' type='checkbox'> Guardar como perfil por defecto</label></div>
      </div>
      <div class='field'><label for='profilePayload'>GPIO JSON</label><textarea id='profilePayload'>{"outputs":[{"pin":5,"ledCount":60,"ledType":"ws2812b","colorOrder":"GRB"}]}</textarea></div>
    </div>

    <div class='card'>
      <h3>Respuesta</h3>
      <pre id='resultOut'>Sin llamadas aun.</pre>
    </div>
  </main>
  <script>
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
      byId('autoApplyOnBoot').checked = !!item.isDefault;
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
      const payload = { profile: {
        id: byId('profileId').value.trim(),
        name: byId('profileName').value.trim(),
        description: byId('profileDescription').value.trim(),
        applyNow: byId('applyNow').checked,
        autoApplyOnBoot: byId('autoApplyOnBoot').checked,
        gpio: gpio
      }};
      await callJson('/api/v1/profiles/gpio/save', payload, 'Guardando perfil...');
      await loadProfiles();
    }
    async function applySelected(setDefault) {
      const id = selectedProfileId || byId('profileId').value.trim();
      if (!id) { setStatus('Selecciona un perfil.', true); return; }
      await callJson('/api/v1/profiles/gpio/apply', { profile: { id: id, setDefault: setDefault } }, 'Aplicando perfil...');
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
</body>
</html>
)HTML";
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
  <main>
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/config/profiles'>Config Profiles</a><a href='/api/hardware'>Hardware</a><a href='/api/release'>Release</a></div>
    <div class='card'>
      <h1>API Profiles GPIO</h1>
      <button onclick='callApi("GET","/api/v1/profiles/gpio")'>GET /api/v1/profiles/gpio</button>
      <button onclick='callApi("POST","/api/v1/profiles/gpio/save")'>POST /api/v1/profiles/gpio/save</button>
      <button onclick='callApi("POST","/api/v1/profiles/gpio/apply")'>POST /api/v1/profiles/gpio/apply</button>
      <button onclick='callApi("POST","/api/v1/profiles/gpio/default")'>POST /api/v1/profiles/gpio/default</button>
      <button onclick='callApi("POST","/api/v1/profiles/gpio/delete")'>POST /api/v1/profiles/gpio/delete</button>
      <textarea id='payload'>{"profile":{"id":"mi_perfil","name":"Mi perfil","gpio":{"outputs":[{"pin":5,"ledCount":60,"ledType":"ws2812b","colorOrder":"GRB"}]},"autoApplyOnBoot":false}}</textarea>
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
</body>
</html>
)HTML";
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
  <main>
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/profiles/gpio'>Profiles</a><a href='/api/release'>Release</a><a href='/version'>Version</a></div>
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
</body>
</html>
)HTML";
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
  </style>
</head>
<body>
  <main>
    <div class='card'>
      <h1>Configuracion Manual</h1>
      <p class='hint'>Edita, exporta o importa toda la configuracion del dispositivo como JSON. La importacion solo se aplica si el JSON es completamente valido.</p>
      <div class='actions'>
        <a href='/' class='btn ghost'>Inicio</a>
        <a href='/config' class='btn ghost'>Config</a>
        <a href='/config/network' class='btn ghost'>Network</a>
        <a href='/config/gpio' class='btn ghost'>GPIO</a>
        <a href='/config/debug' class='btn ghost'>Debug</a>
        <a href='/api/config/all' class='btn ghost'>API Config All</a>
      </div>
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

    <div class='card'>
      <h2>Resultado</h2>
      <pre id='resultOut'>Sin operaciones aun.</pre>
    </div>
  </main>

  <script>
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
</body>
</html>
)HTML";
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
  <main>
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/state'>State</a><a href='/api/config/network'>Config Network</a><a href='/api/config/gpio'>Config GPIO</a><a href='/api/config/debug'>Config Debug</a><a href='/api/config/all'>Config All</a><a href='/api/release'>Release</a></div>
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
</body>
</html>
)HTML";
  return html;
}
