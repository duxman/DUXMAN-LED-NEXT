# Architecture

Arquitectura actual del firmware DUXMAN-LED-NEXT (v0.6.2-alpha).

## Diseno general

- Firmware C++ modular (PlatformIO, Arduino framework)
- FreeRTOS con tareas separadas de control y render
- API HTTP + comandos serial equivalentes
- Persistencia en LittleFS con scheduler
- UI embebida servida desde plantillas LittleFS con fallback en firmware

## Tareas y ejecucion

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

## Configuracion

Modelos principales:

- NetworkConfig
- GpioConfig (hasta 4 outputs)
- GpioConfig.powerLimit (limite software de consumo)
- MicrophoneConfig
- GeneralConfig (language, region, debug options)
- CoreState (runtime)

## Motor LED

Cadena de render:

- EffectManager -> EffectEngine -> LedDriver -> backend

Backends seleccionables en compilacion:

- NeoPixelBus
- FastLED
- Digital

## Audio reactivo

AudioService publica audioLevel, audioPeakHold y beatDetected en CoreState.

Estado actual:

- AGC + noise gate + beat detection
- ajustes de baja latencia para respuesta mas en vivo

## API y robustez

- Base: /api/v1
- /config/network y /config/all responden antes de reaplicar WiFi
- /config/all reduce pico de memoria al ensamblar JSON por secciones

## Referencias

Consultar la documentación técnica detallada en el repositorio.
