# Architecture

Proyecto: DUXMAN-LED-NEXT (firmware v0.4.2-beta)

## Resumen

Firmware modular para ESP32 con separacion por servicios, API HTTP/Serial, persistencia en LittleFS y render de efectos en tarea dedicada.

## Concurrencia y tareas

El sistema usa FreeRTOS con dos tareas principales:

- controlTask (core0, cada ~10 ms)
- renderTask (core1, cada ~16 ms)

controlTask ejecuta:

- apiService.handle()
- wifiService.handle()
- audioService.handle()
- profileService.processPendingPersistence()
- userPaletteService.processPendingPersistence()
- persistenceSchedulerService.processPending()
- effectPersistenceService.handle()

renderTask ejecuta:

- effectManager.renderFrame()

Sin tareas RTOS (fallback), loop() atiende API/WiFi/effects en modo cooperativo.

## Servicios principales

- CoreState: estado runtime compartido con mutex
- ApiService: rutas HTTP, comandos serial y UI embebida
- StorageService: carga/guardado en LittleFS
- PersistenceSchedulerService: debounce y guardado diferido
- ProfileService: perfiles completos (network/gpio/microphone/debug)
- UserPaletteService: paletas de usuario CRUD
- EffectManager: orquestacion del catalogo de efectos y ciclo de vida
- EffectPersistenceService: startup effect + secuencias
- AudioService: captura I2S + AGC + beat detection
- WifiService: AP/STA/AP+STA
- WatchdogService: supervision de tareas

## Flujo de arranque

1. Serial + espera USB CDC opcional
2. storageService.begin()
3. creacion mutex CoreState y watchdog init
4. begin() de effectPersistenceService, profileService, userPaletteService
5. applyStartupProfile() y applyStartupEffect()
6. ledDriver.configure(gpioConfig)
7. wifiService.begin(), audioService.begin(), effectManager.begin(), apiService.begin()
8. creacion de tareas FreeRTOS

## Configuracion y modelo de datos

Configuraciones activas:

- NetworkConfig
- GpioConfig
- MicrophoneConfig
- DebugConfig
- CoreState (runtime)

GpioConfig:

- hasta 4 outputs
- tipos: ws2812b, ws2811, ws2813, ws2815, sk6812, tm1814, digital
- orden/color: GRB, RGB, BRG, RBG, GBR, BGR, RGBW, GRBW, y en digital R/G/B/W
- powerLimit software: enabled, maxCurrentmA, milliAmpsPerLed

Perfiles:

- AppProfile captura completa de network+gpio+microphone+debug
- perfiles integrados + hasta 8 perfiles de usuario

Paletas:

- catalogo de sistema + hasta 20 paletas de usuario

## Motor de efectos y render

Jerarquia de render:

- EffectManager -> EffectEngine (base) -> LedDriver (abstraccion) -> backend

Seleccion de backend en compilacion (DUX_LED_BACKEND):

- 1: NeoPixelBus
- 2: FastLED
- 3: Digital

Estado actual del catalogo:

- Efectos visuales y audio-reactivos separados por carpeta
- IDs de catalogo extendidos hasta la familia audio-reactive reciente (incluye Rainbow Wave, Spectrum Chase y Section Strobe)

## Pipeline de audio reactivo

AudioService usa entrada I2S y publica en CoreState:

- audioLevel (0..255)
- audioPeakHold (0..255)
- beatDetected (bool)

Ajustes recientes para modo mas en vivo:

- buffering I2S mas corto
- frecuencia de proceso mas alta (intervalo de proceso reducido)
- envolvente y peak hold con menor inercia

## Persistencia

La persistencia se canaliza por scheduler para evitar escrituras excesivas.

Entradas persistidas:

- configuracion activa (network/gpio/microphone/debug)
- estado runtime
- perfiles y perfil por defecto
- paletas de usuario
- startup effect y secuencias

## API y UI

ApiService expone:

- API v1 (/api/v1/*)
- OpenAPI (/api/v1/openapi.json)
- UI embebida

Estrategia de UI actual:

- Plantillas HTML/CSS externas en LittleFS (data/ui/*.html)
- Fallback a HTML embebido en firmware si no se encuentra plantilla

Hardening reciente de configuracion:

- /api/v1/config/network y /api/v1/config/all responden antes de reaplicar WiFi
- /api/v1/config/all evita un JsonDocument agregado grande para reducir pico de memoria
- normalizacion defensiva de payload JSON

## Riesgos tecnicos abiertos

- Contencion de mutex de CoreState durante picos de carga
- Ajuste fino de sensibilidad de audio segun microfono y entorno
- Cierre de estrategia final LedFx realtime vs procesamiento local avanzado

## Referencias

- API: docs/api-v1.md
- Persistencia: docs/storage-memory.md
- Roadmap: docs/roadmap.md
- README: README.md
