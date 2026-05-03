# DUXMAN-LED-NEXT

Language / Idioma:

- English: `README.en.md`
- Espanol: `README.md`
- Release Notes EN: `RELEASE_NOTES.en.md`
- Changelog EN: `CHANGELOG.en.md`
- Wiki selector: `wiki/README.md`

Modular ESP32 LED controller firmware (v0.6.3-alpha) with REST/Serial API, embedded UI, full configuration profiles, user palettes, distributed synchronization, and a visual/audio effects engine.

## Workspace Structure

- `projects/firmware-platformio`: ESP32 firmware (PlatformIO + LittleFS UI)
- `projects/mockup-python`: API/UI simulator for hardware-free testing
- `projects/android-app`: Android app (Java/Kotlin)

## Current Status

- Framework: Arduino on ESP32 + FreeRTOS
- Partitions: huge_app (app ~3 MB, LittleFS ~960 KB, no dual OTA yet)
- Persistence: LittleFS with deferred scheduled writes
- API: HTTP (port 80) + equivalent Serial commands
- LED backends: NeoPixelBus (primary), FastLED (alternative), Digital

## Functional Architecture

Active services at boot:

- CoreState
- StorageService
- PersistenceSchedulerService
- ProfileService
- UserPaletteService
- WifiService
- AudioService
- EffectPersistenceService
- EffectManager
- SyncService
- ApiService
- WatchdogService

FreeRTOS tasks:

- controlTask (core0, ~10 ms): API, WiFi, audio, persistence
- renderTask (core1, ~16 ms): effect rendering (~62.5 FPS)

## Supported Hardware (build profiles)

| Profile | Board | Default LED Pin | Default LEDs |
|---|---|---:|---:|
| esp32c3supermini | esp32-c3-devkitm-1 | 8 | 60 |
| esp32dev | esp32dev | 5 | 60 |
| esp32s3 | esp32-s3-devkitc-1 | 48 | 60 |

## Configuration and Persistence

Main configuration domains:

- Runtime state: power, brightness, effectId, speed, level, sectionCount, palette/colors, audio metrics
- NetworkConfig: WiFi, AP/STA IP, DNS, time
- GpioConfig: up to 4 LED outputs
- GpioConfig.powerLimit: software power limiting (`enabled`, `maxCurrentmA`, `milliAmpsPerLed`)
- MicrophoneConfig: I2S, sample rate, fftSize, gain, noise floor, pins
- DebugConfig: enabled, heartbeatMs

Persisted entities:

- Active config (network/gpio/microphone/debug)
- Startup profile and saved profiles (built-in + user)
- User palettes
- Runtime state and effect/sequence persistence

## API v1 Summary

Base HTTP path: `/api/v1`

### State and system

- GET `/state`
- PATCH, POST `/state`
- POST `/system/restart`
- GET `/diag`

### Configuration

- GET `/config/network`
- PATCH, POST `/config/network`
- GET `/config/microphone`
- PATCH, POST `/config/microphone`
- GET `/config/gpio`
- PATCH, POST `/config/gpio`
- GET `/config/debug`
- PATCH, POST `/config/debug`
- GET `/sync/state`
- GET `/sync/connected`
- GET `/sync/config`
- PATCH, POST `/sync/config`
- PATCH, POST `/sync/mode`
- GET `/config/all`
- POST `/config/all`

### Profiles

- GET `/profiles`
- GET `/profiles/get?id=<id>`
- POST, PATCH `/profiles/save`
- POST, PATCH `/profiles/apply`
- POST, PATCH `/profiles/default`
- POST, PATCH `/profiles/delete`
- POST, PATCH `/profiles/clone`

### Effects and sequences

- GET `/effects`
- POST, PATCH `/effects/startup/save`
- POST, PATCH `/effects/sequence/add`
- POST, PATCH `/effects/sequence/delete`

### Palettes

- GET `/palettes`
- POST, PATCH `/palettes/apply`
- POST, PATCH `/palettes/save`
- POST, PATCH `/palettes/delete`

### System metadata

- GET `/hardware`
- GET `/release`
- GET `/openapi.json`

Note: some old test pages still reference `/api/v1/profiles/gpio*`; the canonical route is `/api/v1/profiles*`.

## Embedded UI

The UI is no longer limited to HTML strings embedded in firmware. Main pages are served from LittleFS templates under `projects/firmware-platformio/data/ui`, with embedded firmware fallback if a file is missing.

Recent improvements:

- External HTML/CSS templates in LittleFS for simpler maintenance.
- `POST /api/v1/config/network` and `POST /api/v1/config/all` reply before reapplying WiFi to reduce `ERR_CONNECTION_RESET` in browsers.
- `GET /api/v1/config/all` generates the full JSON with lower peak memory usage.
- Audio pipeline tuned for more live response: less I2S buffering, faster processing cadence, and lower peak-hold inertia.
- Integrated help available from `/docs` inside the embedded UI.
- Sync stack S1-S6 completed: DDP, E1.31, cluster sync, shared effect clock, connection banner, and runtime telemetry.

Main routes:

- `/`
- `/config`, `/config/network`, `/config/microphone`, `/config/gpio`, `/config/sync`, `/config/profiles`, `/config/palettes`, `/config/debug`, `/config/manual`
- `/api` and endpoint testing pages
- `/version`

## Build and Flash

Build:

- `cd projects/firmware-platformio`
- `pio run -e esp32c3supermini`
- `pio run -e esp32dev`
- `pio run -e esp32s3`

Upload:

- `cd projects/firmware-platformio`
- `pio run -e esp32c3supermini -t upload`

Helper script:

- `projects/firmware-platformio/tools/flash.ps1`

## Documentation

- Wiki language selector: `wiki/README.md`
- API reference (EN): `wiki/en/Core/API-v1.md`
- Architecture (EN): `wiki/en/Core/Architecture.md`
- UI guide (EN): `wiki/en/UI/UI-Guide.md`
- Effects catalog (EN): `wiki/en/Features/Effects.md`
- Roadmap (EN): `wiki/en/Development/Roadmap.md`