# Roadmap

Fases incrementales segun evolucion led-next.md y estado real de firmware.

## Estado implementado

- Configuracion persistente de red, debug, microfono y GPIO/LEDs en LittleFS.
- API HTTP/Serial versionada (/api/v1/*) con OpenAPI.
- UI embebida servida desde plantillas LittleFS (data/ui) con fallback embebido.
- FreeRTOS dual-core estable (control y render).
- Persistencia diferida con scheduler para evitar escrituras excesivas.
- Catalogo de efectos visuales y audio-reactivos extendido.
- CRUD de paletas de usuario y perfiles completos aplicables en caliente.
- Endpoints de configuracion endurecidos para evitar cortes por reaplicacion de red.

## Direccion tecnica consolidada

- Priorizar arquitectura no bloqueante para ESP32 con WiFi activo.
- Mantener NeoPixelBus como backend preferente en ESP32 y FastLED como alternativo.
- Preservar modelo de efectos desacoplado del numero fisico de LEDs.
- Mantener API versionada y tolerante ante payloads reales de cliente.

## Fases

### Fase 1. Driver LED real

Estado: completada

- Integracion backend real con abstraccion LedDriver.
- Validacion en tipos LED soportados por configuracion.

### Fase 2. Motor de efectos dinamico

Estado: completada

- Render estable por frame en tarea dedicada.
- Efectos independientes del numero de LEDs por salida.
- Segmentacion virtual y parametros runtime.

### Fase 3. Modelo de configuracion de efectos

Estado: completada

- Estado/persistencia de startup effect y secuencias.
- Parametros de efecto expuestos via API/UI.

### Fase 3A. Paletas predefinidas y de usuario

Estado: completada

- Catalogo predefinido de paletas.
- Endpoints de listado, aplicacion y CRUD de paletas de usuario.
- UI de paletas embebida.

### Fase 4. Concurrencia y FreeRTOS

Estado: completada

- Tareas separadas control/render con watchdog.
- Sincronizacion de estado compartido por mutex.

### Fase 5. Audio reactivo local

Estado: en evolucion

Completado:

- Captura I2S en AudioService.
- AGC, noise gate, beat detection y peak hold.
- Efectos audio-reactivos principales (Audio Pulse, Spectrum, Neon EQ, Rainbow Wave, Spectrum Chase, Section Strobe, etc.).
- Ajuste de baja latencia para respuesta mas en vivo (proceso mas frecuente y menor inercia).

Pendiente:

- Afinado por perfil de microfono y entorno.
- Exposicion de presets de sensibilidad en UI/API.

### Fase 5B. LedFx realtime

Estado: pendiente (prioridad alta)

- Definir modo operativo ledfx-realtime en firmware.
- Protocolo principal DDP con fallback E1.31.
- Diagnostico de recepcion realtime en /api/v1/diag.
- Conmutacion limpia modo local <-> modo realtime.

### Fase 6. Sincronizacion maestro-esclavo

Estado: pendiente

- SyncState v1 con versionado y deduplicacion por seq.
- Mascaras de sincronizacion por campo.
- Sincronizacion de fase y clock para efectos procedurales.

### Fase 7. OTA y despliegue

Estado: pendiente

- Recuperar estrategia OTA con rollback seguro.
- Definir compatibilidad con crecimiento de firmware y filesystem.

## Backlog transversal

- Suite de pruebas de red para latencia/jitter/perdida.
- Guia operativa de troubleshooting de audio reactivo.
- Definicion final de arquitectura standalone vs hub externo.
