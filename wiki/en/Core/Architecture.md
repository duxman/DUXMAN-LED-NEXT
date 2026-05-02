# Architecture

Current firmware architecture for DUXMAN-LED-NEXT (v0.6.3-alpha).

## Overall Design

- Firmware C++ modular (PlatformIO, Arduino framework)
- FreeRTOS with separate control and render tasks
- HTTP API plus equivalent Serial commands
- LittleFS persistence with scheduled deferred writes
- Embedded UI served from LittleFS templates with firmware fallback

## Tasks and Execution

- controlTask (core0): API, WiFi, audio, persistence, sync control
- renderTask (core1): EffectManager.renderFrame()

Target cadence:

- control: ~10 ms
- render: ~16 ms

## Servicios

- CoreState
- ApiService
- StorageService
- PersistenceSchedulerService
- ProfileService
- UserPaletteService
- EffectManager
- EffectPersistenceService
- AudioService
- SyncService
- WifiService
- WatchdogService

## Configuration

Primary models:

- NetworkConfig
- GpioConfig (up to 4 outputs)
- GpioConfig.powerLimit (software power limiting)
- MicrophoneConfig
- GeneralConfig (language, region, debug options)
- SyncConfig (mode, role, input protocol, ports, timeout, smoothing)
- CoreState (runtime)

## LED Engine

Render chain:

- EffectManager -> EffectEngine -> LedDriver -> backend

Compile-time selectable backends:

- NeoPixelBus
- FastLED
- Digital

## Audio-Reactive Pipeline

AudioService publishes `audioLevel`, `audioPeakHold`, and `beatDetected` into `CoreState`.

Current state:

- AGC + noise gate + beat detection
- low-latency tuning for more live response

## Synchronization Layer

The current architecture includes a full sync stack:

- DDP realtime ingest with external frame buffering
- E1.31 fallback on the same render pipeline
- UDP cluster state replication (`server/client`)
- Shared effect clock for distributed phase alignment
- Runtime health telemetry and degraded-state reporting

## API and Robustness

- Base: /api/v1
- `/config/network` and `/config/all` reply before WiFi reconfiguration
- `/config/all` reduces peak memory usage by assembling JSON in sections

## References

See the rest of the wiki for the detailed technical breakdown.
