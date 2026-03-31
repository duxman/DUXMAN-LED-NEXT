#include "api/ApiService.h"

#include <ArduinoJson.h>

ApiService::ApiService(CoreState &state, NetworkConfig &networkConfig, ReleaseInfo &releaseInfo,
                       StorageService &storageService, WifiService &wifiService)
    : state_(state), networkConfig_(networkConfig), releaseInfo_(releaseInfo),
      storageService_(storageService), wifiService_(wifiService), httpServer_(80) {}

void ApiService::begin() {
  setupHttpRoutes();
  httpServer_.begin();

  Serial.println("[api] ready: GET /api/v1/state | PATCH /api/v1/state {json}");
  Serial.println("[api] ready: GET /api/v1/config/network | PATCH /api/v1/config/network {json}");
  Serial.println("[api] ready: GET /api/v1/config/debug | PATCH /api/v1/config/debug {json}");
  Serial.println("[api] ready: GET /api/v1/release");
  Serial.println("[api] ui: GET / | GET /config | GET /config/network | GET /config/debug | GET /api | GET /version");
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
    Serial.println(releaseInfo_.toJson());
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

  httpServer_.on("/api/release", HTTP_GET, [this]() {
    httpServer_.send(200, "text/html", buildApiReleaseHtml());
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

  httpServer_.on("/api/v1/config/debug", HTTP_ANY, [this]() {
    handleHttpDebugRoute();
  });

  httpServer_.on("/api/v1/release", HTTP_GET, [this]() {
    httpServer_.send(200, "application/json", releaseInfo_.toJson());
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

  JsonObject releasePath = paths["/api/v1/release"].to<JsonObject>();
  releasePath["get"]["summary"] = "Obtener metadatos de release";

  JsonObject homePath = paths["/"].to<JsonObject>();
  homePath["get"]["summary"] = "Pagina principal";

  JsonObject configPath = paths["/config"].to<JsonObject>();
  configPath["get"]["summary"] = "Indice de configuracion";

  JsonObject configUiPath = paths["/config/network"].to<JsonObject>();
  configUiPath["get"]["summary"] = "UI de configuracion de red";

  JsonObject configDebugUiPath = paths["/config/debug"].to<JsonObject>();
  configDebugUiPath["get"]["summary"] = "UI de configuracion debug";

  JsonObject apiUiPath = paths["/api"].to<JsonObject>();
  apiUiPath["get"]["summary"] = "Indice de UI API";

  JsonObject apiStateUiPath = paths["/api/state"].to<JsonObject>();
  apiStateUiPath["get"]["summary"] = "UI API para estado";

  JsonObject apiConfigUiPath = paths["/api/config/network"].to<JsonObject>();
  apiConfigUiPath["get"]["summary"] = "UI API para configuracion de red";

  JsonObject apiConfigDebugUiPath = paths["/api/config/debug"].to<JsonObject>();
  apiConfigDebugUiPath["get"]["summary"] = "UI API para configuracion debug";

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
  </style>
</head>
<body>
  <main>
    <section class='hero'>
      <h1>DUXMAN-LED-NEXT __FW_VERSION__</h1>
      <p>Menu principal de navegacion.</p>
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
</body>
</html>
)HTML";
  const String versionLabel = releaseInfo_.version.isEmpty() ? "V0.0" : releaseInfo_.version;
  html.replace("__FW_VERSION__", versionLabel);
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
        <a class='box' href='/config/debug'>
          <h2>Debug</h2>
          <p>Habilitar debug y ajustar intervalo de heartbeat.</p>
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
        <a class='box' href='/api/config/debug'>
          <h2>Config Debug</h2>
          <p>GET/PATCH de /api/v1/config/debug.</p>
        </a>
        <a class='box' href='/api/release'>
          <h2>Release</h2>
          <p>GET de /api/v1/release.</p>
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
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/config/network'>Config Network</a><a href='/api/config/debug'>Config Debug</a><a href='/api/release'>Release</a></div>
    <div class='card'>
      <h1>API State</h1>
      <button onclick='getState()'>GET /api/v1/state</button>
      <button onclick='patchState()'>PATCH /api/v1/state</button>
      <textarea id='payload'>{"power":true,"brightness":180,"effectId":0}</textarea>
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
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/state'>State</a><a href='/api/config/debug'>Config Debug</a><a href='/api/release'>Release</a></div>
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
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/state'>State</a><a href='/api/config/network'>Config Network</a><a href='/api/config/debug'>Config Debug</a></div>
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
    <div class='nav'><a href='/'>Inicio</a><a href='/api'>API</a><a href='/api/state'>State</a><a href='/api/config/network'>Config Network</a><a href='/api/release'>Release</a></div>
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
      <a href='/config/debug'>Debug</a>
      <a href='/api'>API</a>
    </div>

    <section class='card'>
      <h1>Informacion de Version</h1>
      <p>Datos obtenidos desde /api/v1/release.</p>
      <button class='btn' onclick='loadRelease()'>Recargar</button>
    </section>

    <section class='card'>
      <h2>Release JSON</h2>
      <pre id='releaseOut'>Cargando...</pre>
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

    loadRelease();
  </script>
</body>
</html>
)HTML";
  return html;
}
