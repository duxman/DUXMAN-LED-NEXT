# DUXMAN-LED-NEXT — Contexto del Proyecto

## Resumen
Controlador LED embebido para ESP32 (múltiples variantes). Arquitectura modular con API REST versionada, persistencia LittleFS y web UI embebida en firmware.

## Placas soportadas (platformio.ini)
| Perfil | Board | LED Pin | USB CDC |
|--------|-------|---------|---------|
| esp32c3supermini | esp32-c3-devkitm-1 | GPIO 8 | Sí |
| esp32dev | esp32dev | GPIO 5 | No |
| esp32s3 | esp32-s3-devkitc-1 | GPIO 48 | No |

### Pines Input-Only ESP32
- GPIO 34, 35, 36, 39 son solo entrada (no usar para LEDs ni salidas).
- ESP32-C3: GPIO 0-5 pueden tener funciones especiales (strapping pins).
- ESP32-S3: GPIO 0, 3, 45, 46 son strapping pins.

## Arquitectura de módulos
```
main.cpp
├── CoreState          — Estado runtime (power, brightness, effectId)
├── NetworkConfig      — Config persistida (WiFi, IP, DNS, Debug)
│   └── Config.h/cpp   — Structs + JSON patch + validación
├── GpioConfig         — Config LED: colección de salidas (LedOutput[])
├── StorageService     — LittleFS: state.json, device-config.json, gpio-config.json
├── WifiService        — AP/STA/AP+STA con reconexión
├── EffectEngine       — Render loop 60fps → LedDriver
├── LedDriver          — Stub actual (punto extensión FastLED/NeoPixelBus)
├── ApiService         — HTTP (WebServer:80) + Serial commands
└── ReleaseInfo        — Namespace read-only (compile-time desde BuildProfile)
```

## Patrones de código establecidos
- **Patch JSON parcial**: `applyPatchJson(payload, &error)` — candidato → validar → copiar
- **Validación centralizada**: `validate(String *error)` devuelve código de error como string
- **Helpers reutilizables (anónimos)**: `setIfPresent()`, `setBoolIfPresent()`, `setUIntIfPresent()`, `patchIpConfig()`
- **Serialización manual**: `toJson()` construye con ArduinoJson y serializa a String
- **Defaults factory**: `::defaults()` método estático
- **Persistencia**: StorageService guarda/carga JSON a LittleFS con fallback
- **HTTP routes**: handler único por recurso (`handleHttpXxxRoute`) que rutea por método
- **HTML embebido**: R"HTML(...)HTML" con CSS inline, JS fetch inline, replace de placeholders `__XX__`

## API endpoints existentes
- `GET/PATCH /api/v1/state` — power, brightness, effectId
- `GET/PATCH /api/v1/config/network` — WiFi, IP, DNS todo
- `GET/PATCH /api/v1/config/gpio` — colección de salidas LED (outputs[]: pin, ledCount, ledType, colorOrder)
- `GET/PATCH /api/v1/config/debug` — enabled, heartbeatMs- `GET /api/v1/config/all` — exporta toda la config combinada (network + debug + gpio)
- `POST /api/v1/config/all` — importa config completa (valida todo antes de aplicar)- `GET /api/v1/release` — versión firmware
- `GET /api/v1/openapi.json` — spec
- UI HTML: `/`, `/config`, `/config/network`, `/config/gpio`, `/config/debug`, `/config/manual`, `/api`, `/api/state`, `/api/config/network`, `/api/config/gpio`, `/api/config/debug`, `/api/config/all`, `/api/release`, `/version`

## Config JSON (device-config.json + gpio-config.json)
```json
// device-config.json
{
  "network": { "wifi": {...}, "ip": { "ap": {...}, "sta": {...} }, "dns": {...} },
  "debug": { "enabled": true, "heartbeatMs": 5000 }
}
// gpio-config.json
{
  "gpio": {
    "outputs": [
      { "id": 0, "pin": 8, "ledCount": 60, "ledType": "ws2812b", "colorOrder": "GRB" },
      { "id": 1, "pin": 16, "ledCount": 30, "ledType": "sk6812", "colorOrder": "GRBW" }
    ]
  }
}
```

## GpioConfig — Tipos LED y Color Orders soportados
- **Tipos LED**: ws2812b, ws2811, ws2813, ws2815, sk6812, apa102, tm1814, digital
- **Tipo digital**: ledCount forzado a 1, colorOrder solo R/G/B/W
- **Color Orders (3ch)**: GRB, RGB, BRG, RBG, GBR, BGR
- **Color Orders (4ch)**: RGBW, GRBW (solo sk6812)
- **Max salidas**: 4 (kMaxLedOutputs)
- **Validación**: pin range [-1,48], input-only check (34-39), duplicates check, led count [1,1500]

## Dependencias
- ArduinoJson 7.0.4
- Framework: Arduino sobre espressif32
- Monitor: 115200 baud

## Estado actual (V0.2.0)
- Config network/debug: completo con UI y API
- Config GPIO: colección de salidas LED (max 4), con UI dinámica (add/remove), API, validación y persistencia
- Tipos LED soportados: ws2812b, ws2811, ws2813, ws2815, sk6812, apa102, tm1814, digital
- Tipo digital: ledCount=1 forzado, colorOrder=R/G/B/W
- Config manual import/export: /api/v1/config/all (GET exporta, POST importa con validación)
- Versión compilada en binario (BuildProfile.h constexpr) — no mutable en runtime
- ReleaseInfo: namespace read-only con toJson(), no struct, no persistido en LittleFS
- Partición: huge_app.csv (3 MB app, 960 KB LittleFS, sin OTA dual)
- Recursos: RAM 12.1%, Flash 29.6%
- LedDriver: stub (sin implementación real de LEDs)
- Web frontend (web/): scaffold vacío

## TODOs pendientes
- OTA: evaluar solución (sin OTA dual por ahora)
- Arquitectura hub/nodo: Raspberry Pi como hub (UI+API), ESP32 como nodo ejecutor puro
- LedDriver real con FastLED/NeoPixelBus
- Motor de efectos (solid, rainbow, chase)
- Web UI SPA
- mDNS
- Seguridad API
- Tests

## Reglas de documentación
- README.md: actualizar con cada cambio/funcionalidad nueva
- docs/roadmap.md: mantener TODOs sincronizados con README
