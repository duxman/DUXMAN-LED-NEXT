# Plan Detallado De Sincronizacion (LedFx + Multi-Dispositivo)

Este documento consolida la documentacion historica del proyecto sobre sincronizacion y la convierte en un plan de implementacion ejecutable para firmware, API y UI.

Nota de estado: la implementacion S1-S6 ya fue entregada en `0.6.3-alpha`. Este documento se conserva como referencia de diseo y plan original; lo pendiente a partir de aqui es validacion en hardware real y endurecimiento posterior.

## Fuentes localizadas en el repositorio

- `wiki/Archive/docs-referencia-legacy/protocols/ddp/ddp.txt`
- `wiki/Archive/docs-referencia-legacy/protocols/ddp/ddp.py`
- `wiki/Archive/docs-referencia-legacy/protocols/E1.31/e1.31`
- `wiki/Archive/docs-referencia-legacy/protocols/udp realtime/udp realtime .txt`
- `wiki/Archive/docs-referencia-legacy/protocols/udpsync/udpsync.txt`
- `wiki/Archive/docs-referencia-legacy/protocols/ledfx`
- `CHANGELOG.md` (entrada `0.3.4-beta`)
- `wiki/es/Development/Roadmap.md`

## Decisiones consolidadas desde la documentacion previa

1. Protocolo preferente para LedFx realtime: DDP (puerto 4048).
2. Fallback de compatibilidad: E1.31 (sACN).
3. Sincronizacion entre nodos debe evitar bloqueo del render y tolerar perdida UDP.
4. Debe existir retorno automatico a efectos locales al perder fuente externa.
5. Para sincronizacion entre equipos, separar claramente envio y recepcion (rol `server/client`).
6. Para despliegues reales, exponer diagnostico de salud y telemetria en UI/API.

## Objetivo tecnico

Permitir tres modos robustos y conmutables en caliente:

- `local_effects`: el controlador renderiza efectos internos.
- `ledfx_realtime`: el controlador renderiza tramas externas (DDP o E1.31).
- `cluster_sync`: sincronizacion `server/client` del estado logico de efecto (no solo pixeles).

## Arquitectura objetivo

## Plano de datos

1. `TaskNetIn` (no bloqueante) recibe DDP/E1.31/SyncState.
2. `IngressBuffer` valida, ordena y publica snapshot reciente.
3. `TaskRender` consume snapshot segun modo activo.
4. `TaskControl/API` cambia modos y persistencia sin interferir con render.

## Principios

- No usar `delay()` en rutas de red/render.
- Cola o doble buffer entre red y render.
- Politica de timeout deterministica por modo.
- Ownership explicito para evitar estado mixto.

## Modelo de configuracion persistente

Agregar bloque `sync` en configuracion:

```json
{
  "sync": {
    "mode": "off",
    "role": "client",
    "inputProtocol": "ddp",
    "ddpPort": 4048,
    "e131UniverseStart": 1,
    "e131UniverseCount": 1,
    "udpSyncPort": 21324,
    "groupMask": 1,
    "sourceTimeoutMs": 1500,
    "clockSmoothing": "soft"
  }
}
```

Valores permitidos:

- `mode`: `off | local_effects | ledfx_realtime | cluster_sync`
- `role`: `server | client` (aceptando aliases legacy `master | slave`)
- `inputProtocol`: `ddp | e131`

## Contrato SyncState v1 (cluster)

Payload base propuesto:

```json
{
  "v": 1,
  "sequence": 1024,
  "syncEpochMs": 1714600000000,
  "phaseOffset": 0.0,
  "state": {
    "power": true,
    "brightness": 180,
    "effectId": 7,
    "effectSpeed": 40,
    "effectLevel": 6,
    "sectionCount": 3,
    "paletteId": 2,
    "primaryColors": ["#ff3000", "#ffd200", "#00bcd4"],
    "backgroundColor": "#000000"
  }
}
```

Reglas:

- `sequence` monotona ascendente por emisor.
- Paquetes con secuencia atrasada se descartan.
- `syncEpochMs` y `phaseOffset` solo aplican en efectos temporales.
- Si expira timeout, el cliente vuelve a `local_effects` o al estado configurado.

## Plan por fases (ejecucion)

## Fase P1. Base de red y modo sync

Objetivo: activar infraestructura sin cambiar aun catalogo de efectos.

Firmware:

- Crear `SyncService` con estado interno de modo/rol/salud.
- Crear `TaskNetIn` con sockets UDP para DDP y canal cluster.
- Implementar contador de paquetes, drops y ultimo paquete valido.

API:

- `GET /api/v1/sync/state`
- `PATCH /api/v1/sync/config`
- `POST /api/v1/sync/mode`

UI:

- Vista minima de Sync (modo, rol, protocolo, timeout).

Criterio de salida:

- Cambio de modo persistente sin reiniciar.
- Telemetria visible por API.

## Fase P2. Ingesta LedFx por DDP (MVP)

Objetivo: reproducir frames DDP de forma estable.

Firmware:

- Parser DDP v1 (cabecera 10 bytes).
- Ensamblado por offset + `PUSH` para frame completo.
- Doble buffer para intercambio con render.

Notas tecnicas heredadas:

- Puerto default 4048.
- No depender de timecode DDP para MVP.

Criterio de salida:

- Render fluido con LedFx emitiendo DDP continuo.
- Fallback automatico por `sourceTimeoutMs`.

## Fase P3. Fallback E1.31

Objetivo: compatibilidad con emisores sACN.

Firmware:

- Receptor E1.31 base para `Multiple RGB`.
- Soporte inicial 1 a 3 universos (escalable).
- Conversor a buffer interno comun (igual que DDP).

Criterio de salida:

- Misma salida visual base en DDP y E1.31 para un mismo patron.

## Fase P4. Cluster server-client (SyncState v1)

Objetivo: sincronizar estado logico entre nodos.

Firmware:

- Emision periodica de SyncState desde `server`.
- Recepcion/aplicacion en nodos `client` con anti-replay por secuencia.
- Re-snapshot completo al reconectar un `client`.

API/UI:

- Selector de rol y grupo.
- Indicador de `lastSequence`, `lagMs`, `sourceIp`.

Criterio de salida:

- 2+ nodos mantienen mismo efecto/parametros de forma consistente.

## Fase P5. Sync de fase/clock

Objetivo: alinear fase temporal de efectos animados.

Firmware:

- Aplicar `syncEpochMs` a calculo temporal de efectos.
- Corregir drift por interpolacion suave.

Criterio de salida:

- Efectos periodicos sin desfasajes visibles en ejecucion sostenida.

## Fase P6. Hardening y observabilidad

Objetivo: operacion robusta en red domestica real.

Firmware:

- Politica de degradacion por jitter/perdida.
- Limites de buffer y backpressure.

API/UI:

- Dashboard de sync: fps de entrada, drops, timeout, protocolo activo.
- Export de diagnostico para soporte.

Criterio de salida:

- Sistema estable bajo perdida moderada sin bloqueos ni flicker severo.

## Backlog tecnico por sprint (archivo/clase)

Notas:

- Los archivos marcados como "nuevo" no existen aun y deben crearse.
- La prioridad esta ordenada para minimizar riesgo en firmware embebido.

### Sprint S1 (infra base de sync)

Objetivo: introducir configuracion, estado y plumbing de sincronizacion sin activar aun render externo.

Tareas firmware:

- `firmware/src/core/Config.h` y `firmware/src/core/Config.cpp`:
  - Agregar estructura `sync` persistente (modo, rol, protocolo, puertos, timeout).
  - Validaciones y defaults seguros.
- `firmware/src/core/CoreState.h` y `firmware/src/core/CoreState.cpp`:
  - Exponer snapshot runtime de estado sync (solo lectura para render).
  - Mutex y acceso atomico para contadores de salud.
- `firmware/src/services/SyncService.h` (nuevo) y `firmware/src/services/SyncService.cpp` (nuevo):
  - Estado de modo/rol, health counters, timeout manager.
  - API interna para `setMode`, `setRole`, `touchPacket`, `isSourceAlive`.
- `firmware/src/main.cpp`:
  - Instanciar `SyncService`.
  - Integrar ciclo de `tick()` en control task.

Tareas API:

- `firmware/src/api/ApiService.h` y `firmware/src/api/ApiService.cpp`:
  - `GET /api/v1/sync/state`
  - `PATCH /api/v1/sync/config`
  - `POST /api/v1/sync/mode`

DoD sprint:

- Config sync persistida correctamente tras reboot.
- API devuelve estado sync sin bloquear render/control.

### Sprint S2 (DDP ingest MVP)

Objetivo: recibir DDP y entregarlo a render con doble buffer.

Tareas firmware:

- `firmware/src/services/DdpIngressService.h` (nuevo) y `firmware/src/services/DdpIngressService.cpp` (nuevo):
  - Socket UDP puerto 4048.
  - Parser cabecera DDP v1 (flags, sequence, offset, length).
  - Ensamblado por offset y cierre con `PUSH`.
- `firmware/src/services/FrameIngressBuffer.h` (nuevo) y `firmware/src/services/FrameIngressBuffer.cpp` (nuevo):
  - Doble buffer para frame completo + metadata de timestamp.
- `firmware/src/main.cpp`:
  - Task de red dedicada (`TaskNetIn`) para DDP.
- `firmware/src/effects/EffectManager.h` y `firmware/src/effects/EffectManager.cpp`:
  - Modo de render externo cuando `sync.mode == ledfx_realtime`.
  - Fallback a `local_effects` por timeout.

DoD sprint:

- LedFx en DDP controla LEDs de forma estable.
- Al perder DDP, retorno automatico a modo local.

### Sprint S3 (E1.31 fallback)

Objetivo: agregar compatibilidad sACN con el mismo pipeline interno.

Tareas firmware:

- `firmware/src/services/E131IngressService.h` (nuevo) y `firmware/src/services/E131IngressService.cpp` (nuevo):
  - Receptor E1.31 base (`Multiple RGB`).
  - Soporte inicial 1-3 universos contiguos.
- `firmware/src/services/FrameIngressBuffer.*`:
  - Normalizacion de frame E1.31 al formato comun usado por DDP.
- `firmware/src/api/ApiService.cpp`:
  - Exponer protocolo activo (`ddp|e131`) y ultima actividad.

DoD sprint:

- Conmutacion `ddp <-> e131` sin recompilar.
- Salida visual equivalente para patrones de prueba.

### Sprint S4 (cluster server-client SyncState v1)

Objetivo: sincronizar estado logico entre controladores.

Tareas firmware:

- `firmware/src/services/ClusterSyncService.h` (nuevo) y `firmware/src/services/ClusterSyncService.cpp` (nuevo):
  - Emision UDP de `SyncState v1` (`server`).
  - Recepcion/aplicacion con anti-replay por `sequence` (`client`).
  - Re-snapshot al reconectar.
- `firmware/src/core/CoreState.*`:
  - Mapeo completo de estado sincronizable (effectId, params, power, brillo, paleta).
- `firmware/src/api/ApiService.cpp`:
  - `GET /api/v1/sync/state` con `lastSequence`, `lagMs`, `sourceIp`.

DoD sprint:

- 2 nodos mantienen estado de efecto y parametros coherente en caliente.

### Sprint S5 (sync de fase/clock)

Objetivo: alinear evolucion temporal de efectos distribuidos.

Tareas firmware:

- `firmware/src/effects/EffectEngine.h` y `firmware/src/effects/EffectEngine.cpp`:
  - Fuente temporal sincronizada opcional (`syncEpochMs`, `phaseOffset`).
  - Helpers para correccion suave de drift.
- `firmware/src/effects/EffectManager.cpp`:
  - Inyectar contexto temporal sync a efectos activos.
- `firmware/src/services/ClusterSyncService.cpp`:
  - Publicar y consumir epoch/phase en payload.

DoD sprint:

- Efectos periodicos sin desfase visible entre nodos durante corrida prolongada.

### Sprint S6 (UI + observabilidad + hardening)

Objetivo: operacion mantenible sin depender de serial.

Tareas UI/API:

- `data/ui/sync-config.html` (nuevo):
  - Modo, rol, protocolo, timeout, puertos, grupo.
- `data/ui/nav.html`:
  - Entrada de menu a Sync.
- `data/ui/i18n/es.json` y `data/ui/i18n/en.json`:
  - Claves de textos y estados de sync.
- `firmware/src/api/ApiService.cpp`:
  - Endpoint de diagnostico detallado (`/api/v1/sync/diag`).

Hardening:

- `firmware/src/services/WatchdogService.*`:
  - Supervisar TaskNetIn y alertar bloqueos.
- `firmware/src/main.cpp`:
  - Priorizacion y stack size de task de red.

DoD sprint:

- Operacion completa desde UI/API (configurar, activar, monitorizar, desactivar).
- Diagnostico exportable para soporte.

## Dependencias cruzadas entre sprints

1. S2 depende de S1 (configuracion + estado sync).
2. S3 depende de S2 (pipeline comun de frames).
3. S4 depende de S1 (estado y API base) pero puede avanzar en paralelo a S3.
4. S5 depende de S4 (payload con epoch/phase).
5. S6 depende de S1-S5 (exponer y endurecer todo lo construido).

## Recomendacion de ejecucion semanal

- Semana 1: S1
- Semana 2: S2
- Semana 3: S3 + inicio S4
- Semana 4: cierre S4 + S5
- Semana 5: S6 + pruebas de resiliencia + bugfix

## Matriz de pruebas minima

Pruebas funcionales:

- Conmutar `local_effects -> ledfx_realtime -> local_effects`.
- Conmutar protocolo `ddp <-> e131`.
- Topologia `server/client` con arranque en frio y reconexion en caliente.

Pruebas de resiliencia:

- Perdida de paquetes (5% / 10%).
- Jitter variable (5 ms a 80 ms).
- Caida y retorno de AP/router.

Pruebas de rendimiento:

- CPU de task de red y task render.
- Uso de RAM por buffers.
- Latencia entrada->pixel percibida.

## Riesgos y mitigaciones

1. Saturacion WiFi en multicast/broadcast:
- Mitigar con unicast cuando sea posible y tamaño de universos acotado.

2. Bloqueo de render por parseo de red:
- Mitigar con task separada y colas lock-free o mutex corto.

3. Estado mixto por ownership ambiguo:
- Mitigar con reglas estrictas de rol y modo.

4. Deriva de reloj entre nodos:
- Mitigar con NTP activo y correccion gradual de fase.

## Definition Of Done global

La sincronizacion se considera completada cuando:

- LedFx funciona por DDP en hardware real con fallback automatico.
- E1.31 funciona como alternativa operativa.
- SyncState v1 mantiene nodos en estado coherente.
- La fase temporal se mantiene alineada sin saltos visibles.
- Todo se configura y monitoriza desde UI/API, sin depender de Serial.
