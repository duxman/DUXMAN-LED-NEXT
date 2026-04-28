# DUXMAN-LED-NEXT

Controlador LED modular para ESP32, inspirado en WLED. Firmware beta v0.3.6-beta (FreeRTOS + Watchdog + catalogo de efectos dinamicos + paletas predefinidas + editor de paletas de usuario).

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
- Soporte de **perfiles GPIO** guardados en LittleFS y presets integrados en firmware.
- Un perfil puede guardarse, aplicarse al runtime actual y marcarse como perfil por defecto de arranque.
- Si existe perfil por defecto, se aplica en cada boot y sobrescribe la `gpio-config.json` activa.
- `LedDriver` es ahora una clase base abstracta con lógica común de configuración de salidas, niveles por output y API genérica.
- Implementaciones hijas separadas: `LedDriverNeoPixelBus`, `LedDriverFastLed` y `LedDriverDigital`.
- `NeoPixelBus` y `digital` ya usan la configuración real de `GpioConfig` con múltiples salidas.
- `FastLED` queda preparado dentro de la jerarquía, pero por ahora trabaja sobre la salida que coincide con el pin de compilación, ya que esa librería impone restricciones más fuertes para GPIOs variables en runtime.

### Perfiles GPIO

- Presets integrados actuales:
  - el preset base del build activo (`esp32c3supermini`, `esp32dev` o `esp32s3`)
  - `gledopto_gl_c_017wl_d` como preset futuro para el controlador Gledopto con GPIO `16`, `4` y `2`
- Persistencia adicional en LittleFS:
  - `gpio-profiles.json`: perfiles GPIO de usuario
  - `startup-profile.json`: id del perfil GPIO por defecto de arranque
- Aplicar un perfil reconfigura el `LedDriver` en caliente sin reiniciar.

### WiFi y red

- Modos WiFi: `ap`, `sta`, `ap_sta`.
- Política de AP: `always` o `untilStaConnected` (AP se apaga al conectar STA, se reactiva si cae).
- IP estática o DHCP para AP y STA de forma independiente.
- Hostname mDNS configurable y resolución local vía `hostname.local` (1-63 caracteres alfanuméricos).
- Validación completa de IPv4, SSID, hostname.

### API REST (v1)

Interfaz dual: HTTP (puerto 80) + Serial (115200 baud) con los mismos comandos.

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/api/v1/state` | Estado actual (power, brightness, effecto, secciones y colores) |
| PATCH | `/api/v1/state` | Actualizar estado |
| GET | `/api/v1/config/network` | Configuración WiFi + IP |
| PATCH | `/api/v1/config/network` | Actualizar red |
| GET | `/api/v1/config/microphone` | Configuración de micrófono (`generic_i2c`) |
| PATCH | `/api/v1/config/microphone` | Actualizar configuración de micrófono |
| GET | `/api/v1/config/gpio` | Configuración de salidas LED |
| PATCH | `/api/v1/config/gpio` | Actualizar GPIO |
| GET | `/api/v1/profiles/gpio` | Listar perfiles GPIO integrados y guardados |
| POST | `/api/v1/profiles/gpio/save` | Guardar o actualizar un perfil GPIO |
| POST | `/api/v1/profiles/gpio/apply` | Aplicar un perfil GPIO al runtime |
| POST | `/api/v1/profiles/gpio/default` | Fijar o limpiar el perfil GPIO por defecto |
| POST | `/api/v1/profiles/gpio/delete` | Eliminar un perfil GPIO de usuario |
| GET | `/api/v1/config/debug` | Configuración debug (heartbeat) |
| PATCH | `/api/v1/config/debug` | Actualizar debug |
| GET | `/api/v1/config/all` | Configuración completa (merge de todo) |
| POST | `/api/v1/config/all` | Importar configuración completa con validación |
| GET | `/api/v1/hardware` | Capacidades runtime de la placa: chip, flash y MAC |
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
| `/config/profiles` | Gestión de perfiles GPIO |
| `/config/debug` | Configuración debug |
| `/config/manual` | Editor JSON manual (importar/exportar/validar) |
| `/version` | Información de versión del firmware |
| `/api` | Documentación de la API |
| `/api/state` | Tester de endpoint state |
| `/api/config/network` | Tester de endpoint network |
| `/api/config/gpio` | Tester de endpoint GPIO |
| `/api/config/debug` | Tester de endpoint debug |
| `/api/config/all` | Tester de endpoint config completa |
| `/api/profiles/gpio` | Tester de perfiles GPIO |
| `/api/hardware` | Tester de hardware runtime |
| `/api/release` | Tester de endpoint release |

### Comandos Serial

Misma sintaxis que la API HTTP:

```
GET /api/v1/state
PATCH /api/v1/state {"power":true,"brightness":160,"effect":"gradient","sectionCount":6,"primaryColors":["#FF4D00","#FFD400","#00B8D9"],"backgroundColor":"#050505"}
GET /api/v1/config/network
PATCH /api/v1/config/network {"network":{"wifi":{"mode":"ap"}}}
GET /api/v1/config/microphone
PATCH /api/v1/config/microphone {"microphone":{"enabled":false,"source":"generic_i2c","profileId":"gledopto_gl_c_017wl_d","sampleRate":16000,"fftSize":512,"gainPercent":100,"noiseFloorPercent":8,"pins":{"bclk":21,"ws":5,"din":26}}}
GET /api/v1/config/gpio
PATCH /api/v1/config/gpio {"outputs":[...]}
GET /api/v1/profiles/gpio
POST /api/v1/profiles/gpio/save {"profile":{"id":"mi_perfil","name":"Mi perfil","gpio":{"outputs":[...]},"autoApplyOnBoot":true}}
POST /api/v1/profiles/gpio/apply {"profile":{"id":"gledopto_gl_c_017wl_d","setDefault":true}}
POST /api/v1/profiles/gpio/default {"profile":{"id":"gledopto_gl_c_017wl_d"}}
POST /api/v1/profiles/gpio/delete {"profile":{"id":"mi_perfil"}}
GET /api/v1/config/debug
PATCH /api/v1/config/debug {"debug":{"enabled":true}}
GET /api/v1/config/all
POST /api/v1/config/all {json completo}
GET /api/v1/hardware
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
    core/CoreState.*      — Estado runtime (power, brightness, efecto, secciones y colores)
    core/NetworkConfig.*  — Struct de configuración de red
    core/ReleaseInfo.*    — Namespace read-only con info de versión
    drivers/LedDriver.*   — Abstracción de salida LED
    effects/EffectEngine.* — Motor de efectos (60 Hz)
    services/ProfileService.* — Perfiles GPIO integrados + guardados
    services/StorageService.* — Persistencia atómica en LittleFS
    services/WifiService.*    — Gestión WiFi (AP/STA/AP_STA)
web/                      — Futura interfaz web (MVP)
docs/                     — Documentación del proyecto
tools/flash.ps1           — Script de build/upload/monitor
```

## Arranque rápido

1. Abrir el workspace en la carpeta raíz (`duxman-led-next`).
2. Instalar PlatformIO (CLI o extensión VS Code).
3. Compilar: `pio run` (usa perfil por defecto `esp32dev`).
4. Subir: `pio run -t upload -e esp32dev`.

### Compilación por perfil

```
pio run -e esp32c3supermini
pio run -e esp32dev
pio run -e esp32s3
```

## GitHub Actions

El repositorio incluye un workflow en `.github/workflows/firmware-build.yml` para compilar automáticamente el firmware en GitHub Actions.

- Se ejecuta en `push` a `main` o `master`, en `pull_request` y manualmente con `workflow_dispatch`.
- Compila estos entornos: `esp32c3supermini`, `esp32c3supermini_fastled`, `esp32c3supermini_digital`, `esp32dev` y `esp32s3`.
- Sube un artefacto por entorno con `firmware.bin`, `bootloader.bin`, `partitions.bin`, `firmware.elf`, `firmware.map`, `flash_args`, checksums SHA-256 y un `BUILD_INFO.txt`.
- Si haces push de un tag `v*` como `v0.3.5`, el workflow crea además una GitHub Release y adjunta un `.zip` por entorno con esos archivos.
- El nombre, la descripción y si la release es `prerelease` salen de `firmware/config/release-info.json`.

Para descargar una build:

1. Ir a la pestaña `Actions` del repositorio.
2. Abrir el workflow `Firmware Builds`.
3. Descargar el artefacto del entorno deseado.

Para publicar una release:

1. Crear el tag local, por ejemplo `git tag v0.3.5`.
2. Subirlo al remoto con `git push origin v0.3.5`.
3. Esperar a que termine el workflow `Firmware Builds`.
4. Revisar la release generada en la pestaña `Releases`.

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
| `upload_esp32dev` | COM7 |
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

Consulta directa de capacidades de la placa conectada:

```powershell
python -m esptool --chip auto --port COM7 chip-id
python -m esptool --chip auto --port COM7 flash-id
```

También hay VS Code Tasks predefinidas: `FW: Build C3`, `FW: Upload C3 (Auto Port)`, `FW: Upload + Monitor C3`, etc.

## Opciones WiFi permitidas

| Campo | Valores |
|---|---|
| `network.wifi.mode` | `ap`, `sta`, `ap_sta` |
| `network.wifi.apAvailability` | `always`, `untilStaConnected` |
| `network.ip.ap.mode` / `sta.mode` | `dhcp`, `static` |
| `network.dns.hostname` | 1-63 chars, `[a-zA-Z0-9-]`, sin guión al inicio/final |
| `microphone.source` | `generic_i2c` |
| `microphone.profileId` | `DEFAULT`, `gledopto_gl_c_017wl_d` |
| `microphone.sampleRate` | `8000..48000` |
| `microphone.fftSize` | `256`, `512`, `1024`, `2048` |
| `microphone.gainPercent` | `1..200` |
| `microphone.noiseFloorPercent` | `0..100` |
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
  "microphone": {
    "enabled": false,
    "source": "generic_i2c",
    "profileId": "gledopto_gl_c_017wl_d",
    "sampleRate": 16000,
    "fftSize": 512,
    "gainPercent": 100,
    "noiseFloorPercent": 8,
    "pins": { "bclk": 21, "ws": 5, "din": 26 }
  },
  "debug": { "enabled": true }
}
```

## Uso de Flash (v0.3.6-beta)

| Recurso | Uso | Disponible |
|---|---|---|
| RAM | 16.1% | 320 KB |
| Flash (app) | 38.2% | 3072 KB |
| LittleFS | Configs | 960 KB |

## Catalogo de efectos dinamicos (v0.3.6-beta)

Disponibles actualmente en runtime:

- `fixed`, `gradient`, `blink_fixed`, `blink_gradient`
- `breath_fixed`, `breath_gradient`
- `triple_chase`, `gradient_meteor`, `scanning_pulse`, `trig_ribbon`
- `lava_flow`, `polar_ice`, `stellar_twinkle`, `random_color_pop`, `bouncing_physics`

Calibrado de controles:

- `effectSpeed` en escala `1..100` unificada (bajo = lento, alto = rapido)
- `effectLevel` en escala `1..10` con impacto visual consistente por efecto
- Respeto de `primaryColors[3]` y `backgroundColor` en todo el pipeline

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
- [ ] **Efectos (afinado)** — Ajustar presets visuales por hardware, curvas no lineales de `effectLevel` y perfiles por tipo de tira LED.
- [ ] **Audio reactivo (FFT)** — Captura I2S y reactividad global ya integradas; pendiente mapear bajos, medios y agudos (FFT) a parámetros por efecto.
- [ ] **Web UI (SPA)** — Interfaz web moderna en `web/` que reemplace los HTML embebidos.
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
