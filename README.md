# DUXMAN-LED-NEXT

Controlador LED modular para ESP32, inspirado en WLED. Firmware beta v0.2.1.

## Características implementadas

### Arquitectura

- Firmware modular en C++ con PlatformIO (Arduino framework).
- Servicios desacoplados: `ApiService`, `StorageService`, `WifiService`, `LedDriver`, `EffectEngine`.
- Versión de firmware compilada en binario (`BuildProfile.h`) — no mutable en runtime.
- Partición `huge_app.csv` (3 MB app, 960 KB LittleFS, sin OTA dual).
- Persistencia atómica en LittleFS con escritura segura (temp → flush → rename).
- ArduinoJson 7.4.3 para serialización/deserialización.

### Perfiles de hardware

| Perfil | Board | Pin LED | LEDs por defecto |
|---|---|---|---|
| `esp32c3supermini` | ESP32-C3-DevKitM-1 | GPIO 8 | 60 |
| `esp32dev` | ESP32 DevKit | GPIO 5 | 60 |
| `esp32s3` | ESP32-S3-DevKitC-1 | GPIO 48 | 60 |

### Configuración GPIO (salidas LED)

- Hasta **4 salidas LED** independientes (`kMaxLedOutputs = 4`).
- Tipos de LED soportados: `ws2812b`, `ws2811`, `ws2813`, `ws2815`, `sk6812`, `tm1814`, `digital`.
- Órdenes de color: `GRB`, `RGB`, `BRG`, `RBG`, `GBR`, `BGR`, `RGBW`, `GRBW`.
- Tipo `digital` para LEDs no direccionables: color fijo (`R`, `G`, `B`, `W`), `ledCount` forzado a 1.
- Validación de pines duplicados y restricciones por tipo.
- `LedDriver` es ahora una clase base abstracta con lógica común de configuración de salidas, niveles por output y API genérica.
- Implementaciones hijas separadas: `LedDriverNeoPixelBus`, `LedDriverFastLed` y `LedDriverDigital`.
- `NeoPixelBus` y `digital` ya usan la configuración real de `GpioConfig` con múltiples salidas.
- `FastLED` queda preparado dentro de la jerarquía, pero por ahora trabaja sobre la salida que coincide con el pin de compilación, ya que esa librería impone restricciones más fuertes para GPIOs variables en runtime.

### WiFi y red

- Modos WiFi: `ap`, `sta`, `ap_sta`.
- Política de AP: `always` o `untilStaConnected` (AP se apaga al conectar STA, se reactiva si cae).
- IP estática o DHCP para AP y STA de forma independiente.
- Hostname mDNS configurable (1-63 caracteres alfanuméricos).
- Validación completa de IPv4, SSID, hostname.

### API REST (v1)

Interfaz dual: HTTP (puerto 80) + Serial (115200 baud) con los mismos comandos.

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/api/v1/state` | Estado actual (power, brightness, effectId) |
| PATCH | `/api/v1/state` | Actualizar estado |
| GET | `/api/v1/config/network` | Configuración WiFi + IP |
| PATCH | `/api/v1/config/network` | Actualizar red |
| GET | `/api/v1/config/gpio` | Configuración de salidas LED |
| PATCH | `/api/v1/config/gpio` | Actualizar GPIO |
| GET | `/api/v1/config/debug` | Configuración debug (heartbeat) |
| PATCH | `/api/v1/config/debug` | Actualizar debug |
| GET | `/api/v1/config/all` | Configuración completa (merge de todo) |
| POST | `/api/v1/config/all` | Importar configuración completa con validación |
| GET | `/api/v1/release` | Versión, fecha, rama, board, repositorio |
| GET | `/api/v1/openapi.json` | Especificación OpenAPI 3.0 |

Todos los endpoints PATCH/POST también aceptan POST como alternativa.

### Interfaz web embebida

| Ruta | Página |
|---|---|
| `/` | Home con navegación |
| `/config` | Índice de configuración |
| `/config/network` | Configuración WiFi e IP |
| `/config/gpio` | Configuración de salidas LED |
| `/config/debug` | Configuración debug |
| `/config/manual` | Editor JSON manual (importar/exportar/validar) |
| `/version` | Información de versión del firmware |
| `/api` | Documentación de la API |
| `/api/state` | Tester de endpoint state |
| `/api/config/network` | Tester de endpoint network |
| `/api/config/gpio` | Tester de endpoint GPIO |
| `/api/config/debug` | Tester de endpoint debug |
| `/api/config/all` | Tester de endpoint config completa |
| `/api/release` | Tester de endpoint release |

### Comandos Serial

Misma sintaxis que la API HTTP:

```
GET /api/v1/state
PATCH /api/v1/state {"power":false}
GET /api/v1/config/network
PATCH /api/v1/config/network {"network":{"wifi":{"mode":"ap"}}}
GET /api/v1/config/gpio
PATCH /api/v1/config/gpio {"outputs":[...]}
GET /api/v1/config/debug
PATCH /api/v1/config/debug {"debug":{"enabled":true}}
GET /api/v1/config/all
POST /api/v1/config/all {json completo}
GET /api/v1/release
```

## Estructura del repositorio

```
firmware/
  src/
    main.cpp              — Inicialización y loop principal
    api/ApiService.*      — HTTP + Serial API (rutas, HTML, comandos)
    core/BuildProfile.h   — Constantes de compilación (board, versión)
    core/Config.*         — Structs de configuración + validación
    core/CoreState.*      — Estado runtime (power, brightness, effect)
    core/NetworkConfig.*  — Struct de configuración de red
    core/ReleaseInfo.*    — Namespace read-only con info de versión
    drivers/LedDriver.*   — Abstracción de salida LED
    effects/EffectEngine.* — Motor de efectos (60 Hz)
    services/StorageService.* — Persistencia atómica en LittleFS
    services/WifiService.*    — Gestión WiFi (AP/STA/AP_STA)
web/                      — Futura interfaz web (MVP)
docs/                     — Documentación del proyecto
tools/flash.ps1           — Script de build/upload/monitor
```

## Arranque rápido

1. Abrir el workspace en la carpeta raíz (`duxman-led-next`).
2. Instalar PlatformIO (CLI o extensión VS Code).
3. Compilar: `pio run` (usa perfil por defecto `esp32c3supermini`).
4. Subir: `pio run -t upload -e esp32c3supermini`.

### Compilación por perfil

```
pio run -e esp32c3supermini
pio run -e esp32dev
pio run -e esp32s3
```

### Variantes de backend LED (ESP32-C3)

``` 
pio run -e esp32c3supermini_neopixelbus
pio run -e esp32c3supermini_fastled
pio run -e esp32c3supermini_digital
```

- `esp32c3supermini`: backend por defecto `NeoPixelBus`
- `esp32c3supermini_fastled`: mismo firmware con backend `FastLED`
- `esp32c3supermini_digital`: salida GPIO simple on/off

### Perfiles de subida con puerto fijo

| Perfil | Puerto |
|---|---|
| `upload_esp32c3supermini` | COM6 |
| `upload_esp32dev` | COM8 |
| `upload_esp32s3` | COM9 |

Ajustar `upload_port` y `monitor_port` en `platformio.ini` si tu puerto es distinto.

### Script de flash automático

```powershell
# Listar puertos
powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -ListPorts

# Subir con autodetección de puerto
powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -Profile esp32c3supermini -Action upload

# Subir y abrir monitor
powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -Profile esp32dev -Action upload-monitor
```

También hay VS Code Tasks predefinidas: `FW: Build C3`, `FW: Upload C3 (Auto Port)`, `FW: Upload + Monitor C3`, etc.

## Opciones WiFi permitidas

| Campo | Valores |
|---|---|
| `network.wifi.mode` | `ap`, `sta`, `ap_sta` |
| `network.wifi.apAvailability` | `always`, `untilStaConnected` |
| `network.ip.ap.mode` / `sta.mode` | `dhcp`, `static` |
| `network.dns.hostname` | 1-63 chars, `[a-zA-Z0-9-]`, sin guión al inicio/final |
| `debug.enabled` | `true`/`false` (acepta `1`/`0`) |

Ejemplo de payload:

```json
{
  "network": {
    "wifi": {
      "mode": "ap_sta",
      "apAvailability": "untilStaConnected",
      "connection": { "ssid": "MiWifi", "password": "clave-segura" }
    },
    "ip": { "sta": { "mode": "dhcp" } },
    "dns": { "hostname": "duxman-led-c3" }
  },
  "debug": { "enabled": true }
}
```

## Uso de Flash (v0.2.1)

| Recurso | Uso | Disponible |
|---|---|---|
| RAM | 12.1% | 320 KB |
| Flash (app) | 29.6% | 3072 KB |
| LittleFS | Configs | 960 KB |

## Dependencias y licencias

Dependencias de terceros actualmente usadas en el firmware:

| Librería | Licencia | Uso actual |
|---|---|---|
| `ArduinoJson` | MIT | Serialización y deserialización JSON |
| `FastLED` | MIT | Backend LED alternativo de compilación |
| `NeoPixelBus` | LGPL-3.0-or-later | Backend LED principal actual |

Nota importante:

- `ArduinoJson` y `FastLED` no requieren normalmente más que conservar su aviso de licencia.
- `NeoPixelBus` usa licencia `LGPL-3.0-or-later`, así que si distribuyes binarios o producto final conviene revisar sus obligaciones de redistribución antes de cerrar una release pública o comercial.
- Este README deja constancia de las licencias detectadas, pero no sustituye una revisión legal si el firmware se va a redistribuir.

## TODO

- [ ] **OTA** — Actualmente `huge_app` sin OTA dual. Evaluar: OTA con partición reducida, OTA desde LittleFS, o HTTP OTA con rollback manual.
- [ ] **LedDriver avanzado** — Completar soporte de reconfiguración en caliente y cerrar la brecha de `FastLED` para escenarios multi-salida con pines variables.
- [ ] **Efectos** — Implementar motor de efectos independiente del número de LEDs usando coordenadas normalizadas y configuración por JSON/presets.
- [ ] **Primeros efectos dinámicos** — Añadir `solid`, `rainbow`, `chase`, `string_beads`, `meteor_shower` y `cinema_dots`, incluyendo huecos negros reales para dar sensación de movimiento.
- [ ] **FreeRTOS** — Separar render/audio de comunicaciones cuando el firmware lo requiera, con estado compartido protegido por mutex/semaphores.
- [ ] **Audio reactivo** — Integrar micrófono I2S y FFT para mapear bajos, medios y agudos a parámetros del motor de efectos.
- [ ] **Web UI (SPA)** — Interfaz web moderna en `web/` que reemplace los HTML embebidos.
- [ ] **mDNS** — Responder a `hostname.local` para acceso sin IP.
- [ ] **Seguridad** — Autenticación básica o token para la API.
- [ ] **Tests** — Unit tests con PlatformIO Test Runner (`firmware/test/`).
- [ ] **Arquitectura hub/nodo** — Mantener abierta la opción de delegar navegación, UI y API a una Raspberry Pi como hub central, dejando el ESP32 como nodo ejecutor puro. Falta definir protocolo hub↔nodo y reparto final de responsabilidades.

## Debug en VS Code

1. Compilar: `pio run -e esp32c3supermini`.
2. Lanzar `PIO Debug` desde Run and Debug.
3. PlatformIO regenera `firmware/.vscode/launch.json` automáticamente (git-ignored).

## Agradecimientos

Este proyecto reconoce y agradece al equipo y la comunidad de WLED por su enorme trabajo.
Su enfoque técnico y evolución constante son una inspiración directa para este proyecto.
