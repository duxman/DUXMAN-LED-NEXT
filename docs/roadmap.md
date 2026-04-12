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

> Plan ajustado para ejecutar por etapas cortas, con foco en sincronizacion maestro-esclavo y uso de LedFx como fuente audio-reactive.

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

### Fase 5A. Audio reactivo base (estado actual)

- [x] Captura I2S estable en modo `MASTER_RX`.
- [x] Nivel de audio normalizado + suavizado `attack/decay`.
- [x] `beatDetected` y `audioPeakHold` en estado runtime.
- [x] Efectos marcados por tipo (`VISUAL` vs `AUDIO`) y reactividad automatica por efecto.
- [x] Efectos audio destacados: `audio_pulse`, `trig_ribbon`, `stellar_twinkle`, `bouncing_physics`.

### Fase 5B. LedFx como fuente audio-reactive (prioridad alta)

- [ ] Definir modo operativo `ledfx-realtime` en firmware (sin mezcla de render local).
- [ ] Seleccionar protocolo principal para LedFx: `DDP` (preferente) y fallback `E1.31`.
- [ ] Implementar receptor robusto de tramas realtime (timeout, watchdog, salida segura).
- [ ] Añadir conmutacion limpia: `modo local` <-> `modo LedFx` sin reinicio.
- [ ] Exponer diagnostico realtime en `/api/v1/diag` (`rxFps`, `droppedPackets`, `lastPacketMs`, `sourceIp`).
- [ ] Validar en red real 15/30/60 FPS con 1, 2 y 4 nodos.

### Fase 5C. Audio avanzado local (opcional, despues de LedFx)

- [ ] Integrar FFT por bandas (`bass`, `mid`, `treble`).
- [ ] Publicar bandas en estado interno para efectos generativos locales.
- [ ] Perfilado de CPU/RAM para asegurar estabilidad con WiFi activo.

### Fase 6. UI y arquitectura de control

- Corto plazo: mantener la UI embebida actual solo como herramienta de configuración y diagnóstico.
- Medio plazo: mover la UI moderna a `web/` y reducir HTML embebido en firmware.
- Decisión abierta de producto:
	- Camino A: UI/API completas en el ESP32.
	- Camino B: Raspberry Pi como hub central y ESP32 como nodo ejecutor.
- Si se adopta hub/nodo, definir protocolo hub↔nodo, responsabilidades, formato de comandos y estrategia de descubrimiento.

### Fase 6A. Sincronizacion maestro-esclavo de configuracion

- [ ] Definir `SyncState v1` (power, brightness, effectId, effectSpeed, effectLevel, colors, background, flags).
- [ ] Añadir `sourceId`, `groupId`, `seq`, `timestampMs` para orden y deduplicacion.
- [ ] Implementar envio en maestro ante cambios directos (`UI/API`) + snapshot periodico.
- [ ] Implementar recepcion en esclavo con validacion de version y `seq` monotono.
- [ ] Mascaras de sincronizacion por campo (solo brillo, solo color, solo efecto, todo).
- [ ] UI de red: rol (`standalone/master/slave`) y pertenencia a grupo.

### Fase 6B. Sincronizacion de tiempo y fase de efectos

- [ ] Sincronizar reloj base entre nodos (NTP local + correccion drift).
- [ ] Compartir `seed` y `frameClock` para alinear efectos procedurales.
- [ ] Propagar metadatos de audio desde maestro (`audioLevel`, `beatDetected`, `peakHold`).
- [ ] Verificar desfase visual objetivo < 30 ms entre nodos en misma LAN.

### Fase 6C. Hub externo (Raspberry Pi / PC)

- [ ] Crear servicio hub que descubra nodos y gestione grupos.
- [ ] En modo `ledfx-realtime`, el hub enruta/controla origen de tramas para todos los esclavos.
- [ ] Endpoint de orquestacion de escenas (activar modo local o modo LedFx por grupo).

### Fase 7. OTA y despliegue

- Recuperar una estrategia OTA compatible con el crecimiento del firmware.
- Evaluar opciones: nueva tabla de particiones, OTA desde LittleFS, OTA HTTP con rollback controlado o delegación del despliegue desde hub.

## Backlog transversal

- [ ] OTA: definir estrategia final compatible con crecimiento de firmware (manteniendo rollback seguro).
- [ ] Hub/nodo: cerrar decision de despliegue (ESP32 standalone vs hub central obligatorio).
- [ ] Validacion de protocolos en campo: `DDP` (prioritario), `E1.31` (compatibilidad), `UDP sync` (configuracion).
- [ ] Suite de pruebas de red: perdida de paquetes, reconexion AP, latencia y jitter por grupo.
- [ ] Documentar guia de operacion: maestro-esclavo, modo LedFx, troubleshooting y limites recomendados.
