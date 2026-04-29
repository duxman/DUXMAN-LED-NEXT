# Architecture

Arquitectura actual del firmware DUXMAN-LED-NEXT (v0.3.8-beta).

## Diseño general

- Firmware C++ modular (PlatformIO, Arduino framework)
- FreeRTOS con dos tareas principales (control y render)
- API HTTP + comandos Serial equivalentes
- Persistencia en LittleFS con scheduler

## Tareas y ejecución

- controlTask (core0): API, WiFi, audio, persistencia
- renderTask (core1): EffectManager.renderFrame()

Frecuencia objetivo:

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
- WifiService
- WatchdogService

## Configuración

Modelos principales:

- NetworkConfig
- GpioConfig (hasta 4 outputs)
- MicrophoneConfig
- DebugConfig
- CoreState (runtime)

## Motor LED

Cadena de render:

- EffectManager -> EffectEngine -> LedDriver -> backend

Backends seleccionables en compilación:

- NeoPixelBus
- FastLED
- Digital

## API

Base: /api/v1

Grupos funcionales:

- state/system
- config
- profiles
- effects
- palettes
- metadata

La ruta canónica de perfiles es /api/v1/profiles*.

## Persistencia

Se persisten configuración activa, perfiles, paletas de usuario, estado runtime y datos de efectos/secuencia.

## Referencias

- API: wiki/API-v1.md
- Documentación técnica completa: docs/architecture.md y docs/api-v1.md
