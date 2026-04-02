# Roadmap

Fases incrementales según `evolucion led-next.md`.

## Estado base ya implementado

- Configuración persistente de red, debug y GPIO/LEDs en LittleFS.
- API HTTP/Serial versionada (`/api/v1/*`) con páginas HTML embebidas de ayuda/configuración.
- Versión de firmware compile-time desde `BuildProfile.h`.
- Múltiples perfiles de placa (`esp32c3supermini`, `esp32dev`, `esp32s3`).
- Partición `huge_app` para ganar margen de firmware a costa de desactivar OTA dual.

## Dirección técnica consolidada

- Priorizar arquitectura no bloqueante para ESP32 con WiFi activo.
- Orientar el backend LED a `NeoPixelBus` como opción preferente para ESP32 por estabilidad con red y soporte DMA/I2S, manteniendo `FastLED` como backend alternativo de compilación.
- Diseñar el motor de efectos para que sea independiente del número de LEDs físicos usando coordenadas normalizadas (`0.0..1.0`).
- Mantener efectos configurables por JSON, con segmentos virtuales y parámetros dinámicos.
- Evaluar dos caminos de arquitectura de producto:
	- ESP32 autocontenido con UI/API local y motor de efectos completo.
	- Raspberry Pi como hub de UI/API y ESP32 como nodo ejecutor puro.

## Fases propuestas

### Fase 1. Driver LED real

- Integrar backend real de LEDs.
- Opción preferente: `NeoPixelBus` con método DMA/I2S o RMT según placa.
- Mantener `LedDriver` como capa de abstracción para no acoplar el resto del sistema a una librería concreta.
- Validar compatibilidad con tipos LED ya soportados en config (`ws2812b`, `ws2811`, `ws2813`, `ws2815`, `sk6812`, `tm1814`, `digital`).

### Fase 2. Motor de efectos dinámico

- Implementar `renderFrame()` a frecuencia fija (~60 FPS) sin `delay()`.
- Modelar efectos con posiciones normalizadas para que funcionen igual en 10 o 1000 LEDs.
- Permitir definición de efectos por JSON/fichero con parámetros, segmentos virtuales y capas.
- Introducir categorías de efectos:
	- Básicos: `solid`, `rainbow`, `chase`.
	- Cadena/movimiento con huecos: `string_beads`, `running_gaps`, `meteor_shower`, `cinema_dots`.
	- Orgánicos: `perlin_noise`, `fire`, `sparkle`.
	- Reactivos: `vumeter`, `flash_on_bass`.
- Asegurar contraste real: permitir LEDs apagados dentro del patrón para que el movimiento no sea un bloque continuo.

### Fase 3. Modelo de configuración de efectos

- Definir esquema JSON para efectos y presets.
- Soportar parámetros como velocidad, densidad, gap, tail length, palette, mirror, reverse, ping-pong y mapeo de audio.
- Permitir almacenar presets en LittleFS y cargarlos en caliente sin reinicio.
- Preparar el modelo para trabajar con múltiples salidas/segmentos sin depender del número físico de LEDs.

### Fase 4. Concurrencia y FreeRTOS

- Separar tareas por responsabilidad cuando el firmware lo necesite:
	- Core de sistema/comunicaciones: WiFi, API, almacenamiento, sincronización.
	- Core de render/audio: efectos, muestreo de audio, cálculo FFT, push a LEDs.
- Migrar a tareas con `xTaskCreatePinnedToCore` y sincronización por `Mutex`/`Semaphore` sobre estado compartido.
- Sustituir esperas bloqueantes por `vTaskDelay()` o scheduling por tiempos.
- Evaluar watchdog y estrategia de recuperación ante bloqueo del subsistema de red.

### Fase 5. Audio reactivo

- Integrar micrófono I2S, preferentemente `INMP441`.
- Usar `arduinoFFT` o equivalente para separar bandas: bajos, medios y agudos.
- Exponer el resultado del análisis como entradas del motor de efectos (`speed`, `brightness`, `density`, `sparkle`, etc.).
- Mantener el pipeline preparado para escenarios musicales sin comprometer la estabilidad del render.

### Fase 6. UI y arquitectura de control

- Corto plazo: mantener la UI embebida actual solo como herramienta de configuración y diagnóstico.
- Medio plazo: mover la UI moderna a `web/` y reducir HTML embebido en firmware.
- Decisión abierta de producto:
	- Camino A: UI/API completas en el ESP32.
	- Camino B: Raspberry Pi como hub central y ESP32 como nodo ejecutor.
- Si se adopta hub/nodo, definir protocolo hub↔nodo, responsabilidades, formato de comandos y estrategia de descubrimiento.

### Fase 7. OTA y despliegue

- Recuperar una estrategia OTA compatible con el crecimiento del firmware.
- Evaluar opciones: nueva tabla de particiones, OTA desde LittleFS, OTA HTTP con rollback controlado o delegación del despliegue desde hub.

## TODO

- [ ] Pensar solución OTA — actualmente se usa partición `huge_app` (sin OTA dual). Evaluar alternativas: OTA con partición reducida, OTA desde LittleFS, o HTTP OTA con rollback manual.
- [ ] Arquitectura hub/nodo — Delegar UI, API y navegación a una Raspberry Pi (hub). ESP32 como nodo ejecutor puro (recibe config, controla LEDs). Definir protocolo de comunicación hub↔nodo y alcance de cada componente.
- [ ] Completar `LedDriver` real sobre la base ya preparada: selección por compilación entre `NeoPixelBus`, `FastLED` y `digital`, con soporte multi-salida y color order.
- [ ] Definir esquema JSON de efectos/presets independiente del número de LEDs y segmentos.
- [ ] Implementar primeros efectos dinámicos con huecos reales: `string_beads`, `meteor_shower`, `cinema_dots`.
- [ ] Diseñar migración a FreeRTOS por tareas y estado compartido seguro.
- [ ] Diseñar pipeline de audio reactivo con I2S + FFT.
