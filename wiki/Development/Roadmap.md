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

## Estado de ejecucion (checklist verificable)

Donde estan definidos los sprints de sincronizacion:

- Plan detallado: `wiki/es/Development/Sync-Implementation-Plan.md`
- Registro de entrega S1-S6: `CHANGELOG.md` (`0.6.3-alpha`)

Checklist de avance:

- [x] S1 - Infra base de sync (configuracion persistente + servicio base + API)
	- [x] `SyncConfig` persistente integrado en `config.json`.
	- [x] `SyncService` creado con estado y metrica basica (`packetsReceived`, `packetsDropped`, `sourceAlive`).
	- [x] Endpoints `GET /api/v1/sync/state`, `GET|PATCH|POST /api/v1/sync/config`, `POST|PATCH /api/v1/sync/mode`.
	- [x] Integracion en `main.cpp` (`begin()` + `tick()` en control task).
	- [x] `GET/POST /api/v1/config/all` extendido con seccion `sync`.
- [x] S2 - DDP ingest MVP (TaskNetIn + parser + doble buffer + fallback por timeout).
- 	- [x] Listener UDP DDP no bloqueante sobre puerto configurable (`ddpPort`).
- 	- [x] Parser DDP v1 RGB8 con `offset` + `Push` y soporte de `timecode` ignorado en MVP.
- 	- [x] Doble buffer de frame con swap al recibir `Push`.
- 	- [x] Bypass de render local en `mode=ledfx_realtime` con retorno automatico a efectos locales por `sourceTimeoutMs`.
- 	- [x] Metricas ampliadas: `framesApplied`, `lastFrameAtMs`, `frameBytes`.
- [x] S3 - Fallback E1.31 (normalizacion al mismo pipeline de render).
- 	- [x] Listener UDP E1.31 (sACN) no bloqueante en puerto 5568.
- 	- [x] Parser base de Data Packet para `Multiple RGB` con start code 0.
- 	- [x] Soporte inicial multi-universo configurable (`e131UniverseStart`, `e131UniverseCount`) con mapeo al buffer comun.
- 	- [x] Commit de frame por universo completo o timeout corto de ensamblado para evitar stutter.
- [x] S4 - Cluster maestro-esclavo (SyncState v1 + secuencia monotona + anti-replay).
- 	- [x] Canal UDP ligero sobre `udpSyncPort` para `SyncState v1` en modo `cluster_sync`.
- 	- [x] Emision periodica de snapshot completo desde nodo `server` con `sequence` monotona y `groupMask`.
- 	- [x] Recepcion y aplicacion en nodos `client` con rechazo anti-replay por secuencia.
- 	- [x] Telemetria base ampliada: `lastSequence`, `syncStateSent`, `syncStateApplied`, `sourceIp`.
- [x] S5 - Sync de fase/clock para efectos distribuidos.
- 	- [x] `syncEpochMs` y `phaseOffset` aplicados al reloj global de efectos en nodos `client`.
- 	- [x] Correccion suave de drift (`clockSmoothing=soft`) con slew progresivo del offset.
- 	- [x] Telemetria base ampliada: `lastSyncEpochMs` y `clockOffsetMs`.
- [x] S6 - UI de Sync + observabilidad + hardening.
	- [x] UI Sync ampliada en `/config/sync` con salud runtime, telemetria y auto-refresh.
	- [x] Consola `/api/config/sync` ampliada para diagnostico rapido y polling de estado.
	- [x] Telemetria de hardening: `timeoutEvents`, `pollSaturationEvents`, `inputFps`, `activeProtocol`, `degraded`.
	- [x] Politica base de degradacion por timeout, saturacion de polling y offset de clock excesivo.

## Plan detallado de sincronizacion

Nota: la implementacion actual de S1-S6 queda pendiente de validacion en hardware real. Ver `wiki/Development/Sync-Test-Plan.md`.

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
