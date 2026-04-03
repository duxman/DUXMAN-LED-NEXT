# Architecture

Proyecto: `DUXMAN-LED-NEXT`.

Documento actualizado según la implementación actual del firmware `v0.3.1-beta` (Fase 4A-4C, persistencia async, watchdog y catalogo de efectos dinamicos).

## Visión general

El firmware está organizado como un sistema modular para ESP32 con estas responsabilidades principales:

- configuración persistente en LittleFS
- gestión WiFi y red
- API HTTP + comandos Serial
- motor de render LED
- abstracción de backend LED seleccionable en compilación

La aplicación arranca en `main.cpp` y cablea los servicios principales sobre un conjunto de objetos globales compartidos.

## Módulos principales

### `CoreState`

Representa el estado runtime del motor de iluminación.

Campos actuales:

- `power`
- `brightness`
- `effectId`
- `sectionCount`
- `primaryColors[3]`
- `backgroundColor`

Efectos soportados actualmente:

- `0` / `fixed`: color fijo por secciones
- `1` / `gradient`: degradado fijo por sección
- `2` / `diagnostic`: diagnóstico de salida principal
- `3` / `blink_fixed`: parpadeo fijo
- `4` / `blink_gradient`: parpadeo degradado
- `5` / `breath_fixed`: respiración de secciones fijas
- `6` / `breath_gradient`: respiración con degradado global
- `7` / `triple_chase`: tren de color en movimiento
- `8` / `gradient_meteor`: cabeza y cola con gradiente
- `9` / `scanning_pulse`: pulso de barrido con rebote
- `10` / `trig_ribbon`: mezcla sinusoidal multicapa
- `11` / `lava_flow`: deformación orgánica cálida
- `12` / `polar_ice`: interferencia fría
- `13` / `stellar_twinkle`: destellos estelares
- `14` / `random_color_pop`: pops aleatorios de color
- `15` / `bouncing_physics`: bolas luminosas en rebote

Es la parte más cercana al "estado operativo" del render, no a la configuración persistente de infraestructura.

### `NetworkConfig`

Agrupa toda la configuración de red y debug:

- WiFi (`ap`, `sta`, `ap_sta`)
- política de disponibilidad del AP
- IP de AP y STA (`dhcp` o `static`)
- DNS / hostname
- debug (`enabled`, `heartbeatMs`)

Se persiste en LittleFS y también se expone por la API.

### `GpioConfig`

Define la configuración de salidas LED.

Modelo actual:

- hasta `4` salidas (`kMaxLedOutputs`)
- cada salida describe:
	- `id`
	- `pin`
	- `ledCount`
	- `ledType`
	- `colorOrder`

Tipos soportados actualmente:

- `digital`
- `ws2812b`
- `ws2811`
- `ws2813`
- `ws2815`
- `sk6812`
- `tm1814`

### `ReleaseInfo`

No es una estructura persistida. Es un namespace de solo lectura que genera JSON a partir de constantes compile-time definidas en `BuildProfile.h` y `platformio.ini`.

Esto garantiza trazabilidad de versión en binario.

### `HardwareInfo`

Es un namespace de solo lectura que consulta capacidades reales de la placa en runtime.

Datos expuestos actualmente:

- modelo de chip
- revisión
- número de cores
- frecuencia CPU
- presencia de WiFi y Bluetooth
- tamaño y velocidad de flash
- MAC base en eFuse

## Servicios principales

### `StorageService`

Responsable de cargar y guardar estado/configuración en LittleFS.

Responsabilidades actuales:

- `save()` / `load()` general
- `saveNetworkConfig()` / `loadNetworkConfig()`
- `saveGpioConfig()` / `loadGpioConfig()`
- persistencia segura mediante lectura/escritura de archivos JSON

Persistencia actual:

- `state.json`
- `device-config.json`
- `gpio-config.json`

`release-info.json` ya no existe; la versión sale de código compilado.

`StorageService` sigue gestionando solo la configuración activa del sistema, no el catálogo de perfiles.

### `ProfileService`

Responsable de los perfiles GPIO reutilizables.

Responsabilidades actuales:

- registrar presets integrados en firmware
- cargar y guardar perfiles GPIO de usuario en LittleFS
- mantener el `id` del perfil por defecto de arranque
- aplicar un perfil sobre `GpioConfig`
- reconfigurar `LedDriver` en caliente al aplicar un perfil

Persistencia actual:

- `gpio-profiles.json`
- `startup-profile.json`

Perfiles integrados actuales:

- perfil base derivado del build activo
- preset `gledopto_gl_c_017wl_d`

### `WifiService`

Responsable de aplicar y mantener la configuración de red.

Responsabilidades:

- arrancar WiFi en `begin()`
- reaplicar configuración en `applyConfig()`
- mantener políticas AP/STA en `handle()`
- construir SSID/AP según configuración

### `ApiService`

Expone dos superficies:

- HTTP con `WebServer`
- comandos Serial línea a línea con la misma semántica que la API HTTP

Responsabilidades:

- rutas `/api/v1/*`
- páginas HTML embebidas de ayuda y configuración
- import/export de configuración completa
- gestión de perfiles GPIO (listar, guardar, aplicar, fijar default, borrar)
- exposición de metadatos de hardware runtime (`/api/v1/hardware`)
- normalización de payloads debug
- coordinación con `StorageService`, `ProfileService` y `WifiService`

### `EffectEngine`

Es el puente entre `CoreState` y `LedDriver`.

Responsabilidades actuales:

- arrancar el driver LED
- renderizar frames a intervalos fijos
- aplicar brillo/power al backend LED
- dividir cada salida en secciones lógicas
- pintar color fijo o degradado estático a nivel de píxel cuando el backend lo permite
- degradar con color plano cuando la salida no soporta control por píxel

Actualmente es intencionadamente simple. La evolución prevista es convertirlo en el núcleo del motor de efectos dinámico y configurable por JSON.

## Abstracción LED

### Clase base `LedDriver`

`LedDriver` es una clase base abstracta con lógica común compartida.

Responsabilidades del padre:

- recibir `GpioConfig` mediante `configure()`
- traducir `ledType` y `colorOrder` a enums internos
- mantener configuración normalizada por output
- mantener nivel global y nivel por salida
- exponer API genérica al resto del sistema

API base actual:

- `configure(const GpioConfig&)`
- `begin()`
- `show()`
- `backendName()`
- `setAll(level)`
- `setAllColor(color)`
- `setOutputLevel(index, level)`
- `setOutputColor(index, color)`
- `setPixelColor(index, pixel, color)`
- `clear()`

### Implementaciones hijas

#### `LedDriverNeoPixelBus`

Backend principal recomendado para ESP32.

Estado actual:

- múltiples salidas reales
- respeta `ledType`, `colorOrder` y RGB/RGBW
- crea un bus por output válido
- buen encaje con configuración runtime por GPIO

#### `LedDriverDigital`

Backend simple para salidas digitales no direccionables.

Estado actual:

- múltiples salidas reales
- usa cada output `digital` como GPIO on/off
- pensado para casos simples o relés/LEDs discretos

#### `LedDriverFastLed`

Backend alternativo de compilación.

Estado actual:

- integrado en la misma jerarquía
- interpreta `ledType` y `colorOrder`
- trabaja sobre la salida que coincide con el pin de compilación
- no resuelve todavía bien el caso de múltiples GPIOs configurables en runtime

Conclusión práctica actual:

- `NeoPixelBus` es el backend más natural para configuración dinámica multi-salida
- `FastLED` queda disponible como backend alternativo, pero con limitación arquitectónica conocida

### Selección del backend activo

La implementación concreta se selecciona en compilación mediante `DUX_LED_BACKEND`.

Archivo clave:

- `drivers/CurrentLedDriver.h`

Valores actuales:

- `1` → `NeoPixelBus`
- `2` → `FastLED`
- `3` → `digital`

El resto del firmware trabaja contra el padre `LedDriver`, no contra una implementación concreta.

## Flujo de arranque

Secuencia actual en `main.cpp`:

1. iniciar Serial
2. esperar USB CDC si aplica
3. `storageService.begin()`
4. `profileService.begin()`
5. aplicar perfil GPIO por defecto si existe
6. `ledDriver.configure(gpioConfig)`
7. `wifiService.begin()`
8. `effectEngine.begin()`
9. `apiService.begin()`

Después del arranque se informa por Serial de:

- perfil de build
- backend LED activo
- outputs configurados
- estado de debug

## Bucle principal

El `loop()` actual ejecuta tres responsabilidades:

1. `apiService.handle()`
2. `wifiService.handle()`
3. render periódico a ~60 FPS (`16 ms`)

También emite heartbeat por Serial cuando `debug.enabled` está activo.

## Persistencia y aplicación de cambios

Patrón general actual:

1. llega payload por HTTP o Serial
2. se aplica patch parcial sobre la estructura correspondiente
3. se valida
4. si hay cambios, se persiste en LittleFS
5. si corresponde, se reaplica el subsistema afectado

Ejemplos:

- `network` → persistencia + `wifiService.applyConfig()`
- `gpio` → persistencia + reconfiguración en caliente del driver LED
- `profiles/gpio/apply` → persistencia + reconfiguración en caliente + opcionalmente fijar startup default
- `config/all` → validación en candidatos antes de aplicar nada

## Superficies expuestas

### API HTTP

La API real está documentada en:

- [api-v1.md](f:/desarrollo/duxman-led-next/docs/api-v1.md)

### UI embebida

El firmware todavía expone páginas HTML internas para ayuda/configuración:

- `/`
- `/config`
- `/config/network`
- `/config/gpio`
- `/config/profiles`
- `/config/debug`
- `/config/manual`
- `/api`
- `/api/profiles/gpio`
- `/api/hardware`
- `/version`

## Decisiones técnicas relevantes

- versión y fecha de release se compilan dentro del binario
- tabla de particiones actual: `huge_app`
- OTA dual desactivada por ahora
- `NeoPixelBus` es backend preferente
- `FastLED` se mantiene como backend alternativo de compilación
- `ProfileService` permite separar presets de hardware de la configuración activa
- arquitectura futura abierta entre ESP32 autocontenido o modelo hub/nodo con Raspberry Pi

## Limitaciones actuales

- `EffectEngine` implementa actualmente dos efectos estáticos: color fijo por secciones y degradado fijo por secciones
- `LedDriverFastLed` no cubre aún multi-salida runtime de forma equivalente a `NeoPixelBus`
- la reconfiguración en caliente existe para aplicar cambios de GPIO, pero falta cerrar todas las aristas del backend `FastLED`
- la UI sigue embebida en firmware; aún no se ha migrado a una SPA externa en `web/`

## Próximos pasos naturales

- motor de efectos con coordenadas normalizadas y presets JSON
- integración de audio reactivo con I2S + FFT
- revisión de arquitectura hub/nodo si la Raspberry Pi pasa a ser hub central
