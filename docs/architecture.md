# Architecture

Proyecto: DUXMAN-LED-NEXT (firmware v0.3.9-beta)

## Resumen

Firmware modular para ESP32 con separación por servicios, API HTTP/Serial, persistencia en LittleFS y render de efectos en tarea dedicada.

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
- EffectManager: orquestación de 17 efectos y ciclo de vida
- EffectPersistenceService: startup effect + secuencias
- AudioService: captura I2S + AGC + beat
- WifiService: AP/STA/AP+STA
- WatchdogService: supervisión de tareas

## Flujo de arranque

1. Serial + espera USB CDC opcional
2. storageService.begin()
3. creación mutex CoreState y watchdog init
4. begin() de effectPersistenceService, profileService, userPaletteService
5. applyStartupProfile() y applyStartupEffect()
6. ledDriver.configure(gpioConfig)
7. wifiService.begin(), audioService.begin(), effectManager.begin(), apiService.begin()
8. creación de tareas FreeRTOS

## Configuración y modelo de datos

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

Perfiles:

- AppProfile captura completa de network+gpio+microphone+debug
- perfiles integrados + hasta 8 perfiles de usuario

Paletas:

- catálogo del sistema + hasta 20 paletas de usuario

Efectos:

- 17 entradas de catálogo en EffectRegistry
- efectos visuales y audio-reactivos

## Render y driver LED

Jerarquía:

- EffectManager -> EffectEngine (base) -> LedDriver (abstracción) -> backend

Selección de backend en compilación (DUX_LED_BACKEND):

- 1: NeoPixelBus
- 2: FastLED
- 3: Digital

## Persistencia

La persistencia se canaliza por scheduler para evitar escritura excesiva.

Entradas persistidas incluyen:

- configuración activa
- estado runtime
- perfiles y perfil por defecto
- paletas de usuario
- persistencia de efectos/secuencia

## API y UI

ApiService expone:

- API v1 (/api/v1/*)
- OpenAPI (/api/v1/openapi.json)
- UI embebida de configuración y testers

Rutas canónicas de perfiles:

- /api/v1/profiles*

## Riesgos técnicos abiertos

- Contención de mutex de CoreState durante render
- Costo por píxel de algunas operaciones de color/gamma en escenarios de alta densidad
- Limpieza de referencias legacy /profiles/gpio* en páginas de prueba antiguas

## Referencias

- API: docs/api-v1.md
- README: README.md
- Refactor: docs/analisis-arquitectura-refactor.md
