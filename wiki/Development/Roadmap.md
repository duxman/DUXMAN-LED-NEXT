# Roadmap y evolucion

## Estado actual

- Configuracion persistente de red, debug, microfono y GPIO/LEDs en LittleFS.
- API HTTP/Serial versionada (/api/v1/*).
- UI embebida basada en plantillas LittleFS (data/ui) con fallback en firmware.
- FreeRTOS dual-core estable para control y render.
- Catalogo de efectos visuales y audio-reactivos ampliado.
- Paletas de usuario con CRUD y perfiles completos aplicables en caliente.
- Hardening de /config/network y /config/all para robustez en cliente web.
- Audio reactivo afinado para menor latencia percibida.

## Siguientes fases

- Integracion LedFx realtime (DDP preferente, E1.31 fallback).
- Sincronizacion entre dispositivos (SyncState v1, secuencia monotona).
- Sincronizacion de fase/clock para efectos distribuidos.
- OTA seguro con estrategia de rollback.

## Plan detallado de sincronizacion (pendiente)

### Fase S1. Ingesta LedFx por DDP (MVP)

Objetivo: reproducir en tiempo real tramas externas de LedFx con latencia baja y degradacion controlada.

Entregables:

- Receptor DDP en task no bloqueante con buffer doble de frames.
- Modo de entrada configurable: `local_effects` / `ledfx_ddp`.
- Timeout de fuente externa con retorno automatico a efecto local.
- Metricas minimas: fps recibido, frames descartados, ultimo timestamp valido.

Criterio de aceptacion:

- Render estable con fuente DDP continua.
- Si DDP se corta, recuperacion automatica sin reinicio del equipo.

### Fase S2. Fallback E1.31 (sACN)

Objetivo: compatibilidad con setups donde DDP no este disponible.

Entregables:

- Parser E1.31 limitado a casos de uso de una tira/universo inicial.
- Selector de protocolo externo: `ddp` o `e131`.
- Normalizacion de entrada para compartir el mismo pipeline de render.

Criterio de aceptacion:

- Conmutacion de protocolo desde config sin recompilar.
- Misma salida visual base usando fuente DDP o E1.31 en condiciones equivalentes.

### Fase S3. Sync maestro-esclavo entre dispositivos (SyncState v1)

Objetivo: que un nodo maestro distribuya estado de reproduccion a nodos esclavos.

Entregables:

- Payload `SyncState v1` con `sequence`, `effectId`, `params`, `brightness`, `power`.
- Transporte UDP ligero (broadcast o multicast configurable).
- Rechazo de paquetes fuera de orden por `sequence` monotona.
- Politica de ownership: solo el maestro emite, esclavos no pisan estado local en modo sync.

Criterio de aceptacion:

- Dos o mas nodos mantienen el mismo estado visual sin drift apreciable.
- Reconexion de un esclavo con re-sincronizacion completa en caliente.

### Fase S4. Sync de fase/clock para efectos distribuidos

Objetivo: alinear fase temporal de efectos animados entre dispositivos.

Entregables:

- `syncEpochMs` y `phaseOffset` en `SyncState`.
- Correccion suave de drift (sin saltos visuales bruscos).
- Modo de tolerancia para jitter de red con ventana configurable.

Criterio de aceptacion:

- Efectos periodicos arrancan y evolucionan en fase entre nodos.
- En jitter moderado, el sistema prioriza continuidad visual sobre exactitud instantanea.

### Fase S5. API/UI y observabilidad de sync

Objetivo: diagnostico y control operativo desde UI/API.

Entregables:

- Endpoints de estado sync (`/api/v1/sync/*`) para modo, rol, secuencia y salud.
- Pantalla UI de Sync con modo (`off`, `master`, `slave`, `ledfx`) y telemetria.
- Persistencia de configuracion sync en perfil/dispositivo.

Criterio de aceptacion:

- Operacion completa sin serial: configurar, activar, monitorizar y desactivar sync desde UI.

## Dependencias y riesgos

- WiFi inestable impacta jitter y perdida de paquetes UDP.
- Debe evitarse bloqueo del render por parsing de red (siempre task separada).
- El path de fallback a efectos locales debe ser deterministico para evitar estados mixtos.
- Priorizar footprint de RAM al dimensionar buffers de frames externos.

## Decisiones tecnicas clave

- Arquitectura no bloqueante con tareas separadas.
- NeoPixelBus como backend preferente y FastLED como alternativo.
- Efectos configurables desacoplados de la cantidad fisica de LEDs.
- API REST versionada y tolerante a payloads de cliente reales.

Para detalle tecnico, ver la documentacion arquitectonica en el repositorio.
