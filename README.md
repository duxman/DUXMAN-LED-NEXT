# DUXMAN-LED-NEXT

Firmware modular para control LED en ESP32 (v0.3.9-beta), con API REST/Serial, UI embebida, perfiles de configuración completos, paletas de usuario y motor de efectos visuales/audio.

## Estado actual

- Framework: Arduino sobre ESP32 + FreeRTOS
- Particiones: huge_app (app ~3 MB, LittleFS ~960 KB, sin OTA dual)
- Persistencia: LittleFS con guardado diferido vía scheduler
- API: HTTP (puerto 80) + comandos Serial equivalentes
- Backends LED: NeoPixelBus (principal), FastLED (alternativo), Digital

## Arquitectura funcional

Servicios activos en el arranque:

- CoreState
- StorageService
- PersistenceSchedulerService
- ProfileService
- UserPaletteService
- WifiService
- AudioService
- EffectPersistenceService
- EffectManager
- ApiService
- WatchdogService

Tareas FreeRTOS:

- controlTask (core0, ~10 ms): API, WiFi, audio, persistencia
- renderTask (core1, ~16 ms): render de efectos (~62.5 FPS)

## Hardware soportado (build profiles)

| Perfil | Board | Pin LED por defecto | LEDs por defecto |
|---|---|---:|---:|
| esp32c3supermini | esp32-c3-devkitm-1 | 8 | 60 |
| esp32dev | esp32dev | 5 | 60 |
| esp32s3 | esp32-s3-devkitc-1 | 48 | 60 |

## Configuración y persistencia

Configuraciones principales:

- State runtime: power, brightness, effectId, speed, level, sectionCount, palette/colors, audio metrics
- NetworkConfig: wifi, ip(ap/sta), dns, time
- GpioConfig: hasta 4 salidas LED
- MicrophoneConfig: I2S, rate, fftSize, gain, noise floor, pins
- DebugConfig: enabled, heartbeatMs

Entidades persistidas:

- Config activa (network/gpio/microphone/debug)
- Perfil de arranque y perfiles (integrados + usuario)
- Paletas de usuario
- Estado runtime y persistencia de efectos/secuencias

## API v1 (resumen)

Base HTTP: /api/v1

### Estado y sistema

- GET /state
- PATCH, POST /state
- POST /system/restart
- GET /diag

### Configuración

- GET /config/network
- PATCH, POST /config/network
- GET /config/microphone
- PATCH, POST /config/microphone
- GET /config/gpio
- PATCH, POST /config/gpio
- GET /config/debug
- PATCH, POST /config/debug
- GET /config/all
- POST /config/all

### Perfiles

- GET /profiles
- GET /profiles/get?id=<id>
- POST, PATCH /profiles/save
- POST, PATCH /profiles/apply
- POST, PATCH /profiles/default
- POST, PATCH /profiles/delete
- POST, PATCH /profiles/clone

### Efectos y secuencias

- GET /effects
- POST, PATCH /effects/startup/save
- POST, PATCH /effects/sequence/add
- POST, PATCH /effects/sequence/delete

### Paletas

- GET /palettes
- POST, PATCH /palettes/apply
- POST, PATCH /palettes/save
- POST, PATCH /palettes/delete

### Sistema / metadatos

- GET /hardware
- GET /release
- GET /openapi.json

Nota: existen referencias legacy a rutas /api/v1/profiles/gpio* en algunas páginas de prueba antiguas; la ruta canónica implementada es /api/v1/profiles*.

## UI embebida

Rutas principales:

- /
- /config, /config/network, /config/microphone, /config/gpio, /config/profiles, /config/palettes, /config/debug, /config/manual
- /api y testers de endpoints
- /version

## Build y flash

Compilar:

- pio run -e esp32c3supermini
- pio run -e esp32dev
- pio run -e esp32s3

Subir:

- pio run -e esp32c3supermini -t upload

Script auxiliar:

- tools/flash.ps1

## Documentación

- API detallada: docs/api-v1.md
- Arquitectura: docs/architecture.md
- Wiki API: wiki/API-v1.md
- Wiki Arquitectura: wiki/Architecture.md
- Plan de refactor: docs/analisis-arquitectura-refactor.md
