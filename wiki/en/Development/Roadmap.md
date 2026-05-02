# Roadmap and Evolution

## Current Status

- Persistent network, debug, microphone, and GPIO/LED configuration in LittleFS.
- Versioned HTTP/Serial API (`/api/v1/*`).
- Embedded UI based on LittleFS templates (`data/ui`) with firmware fallback.
- Stable dual-core FreeRTOS split for control and render.
- Expanded visual and audio-reactive effects catalog.
- User palettes with CRUD and full hot-applied profiles.
- Hardened `/config/network` and `/config/all` behavior for web-client robustness.
- Audio-reactive pipeline tuned for lower perceived latency.

## Next Phases

- Real hardware validation of the S1-S6 sync stack.
- Resilience runs with jitter, moderate packet loss and longer multi-device sessions.
- OTA with rollback strategy.

## Execution Status (Verifiable Checklist)

Where the synchronization sprints are defined:

- Detailed plan: `wiki/es/Development/Sync-Implementation-Plan.md`
- Delivery record for S1-S6: `CHANGELOG.md` (`0.6.3-alpha`)

Progress checklist:

- [x] S1 - Sync base infrastructure (persistent config + core service + API)
	- [x] `SyncConfig` persistente integrado en `config.json`.
	- [x] `SyncService` creado con estado y metrica basica (`packetsReceived`, `packetsDropped`, `sourceAlive`).
	- [x] Endpoints `GET /api/v1/sync/state`, `GET|PATCH|POST /api/v1/sync/config`, `POST|PATCH /api/v1/sync/mode`.
	- [x] Integracion en `main.cpp` (`begin()` + `tick()` en control task).
	- [x] `GET/POST /api/v1/config/all` extendido con seccion `sync`.
- [x] S2 - DDP ingest MVP (TaskNetIn + parser + doble buffer + fallback por timeout).
- 	- [x] Non-blocking UDP DDP listener on configurable `ddpPort`.
- 	- [x] DDP v1 RGB8 parser with `offset` + `Push`, while timecode remains ignored in the MVP.
- 	- [x] Double frame buffer swapped on `Push`.
- 	- [x] Local render bypass in `mode=ledfx_realtime` with automatic fallback to local effects after `sourceTimeoutMs`.
- 	- [x] Expanded metrics: `framesApplied`, `lastFrameAtMs`, `frameBytes`.
- [x] S3 - E1.31 fallback (normalized into the same render pipeline).
- 	- [x] Non-blocking E1.31 (sACN) UDP listener on port 5568.
- 	- [x] Base Data Packet parser for `Multiple RGB` with start code 0.
- 	- [x] Initial configurable multi-universe support (`e131UniverseStart`, `e131UniverseCount`) mapped into the shared frame buffer.
- 	- [x] Frame commit on complete universe set or short assembly timeout to avoid stutter.
- [x] S4 - Cluster server-client (SyncState v1 + monotonic sequence + anti-replay).
- 	- [x] Lightweight UDP channel on `udpSyncPort` for `SyncState v1` in `cluster_sync` mode.
- 	- [x] Periodic full snapshot broadcast from `server` nodes with monotonic `sequence` and `groupMask`.
- 	- [x] Receive/apply on `client` nodes with anti-replay rejection based on sequence.
- 	- [x] Expanded base telemetry: `lastSequence`, `syncStateSent`, `syncStateApplied`, `sourceIp`.
- [x] S5 - Phase/clock sync for distributed effects.
- 	- [x] `syncEpochMs` and `phaseOffset` applied to the global effect clock on `client` nodes.
- 	- [x] Smooth drift correction (`clockSmoothing=soft`) with progressive offset slew.
- 	- [x] Expanded base telemetry: `lastSyncEpochMs` and `clockOffsetMs`.
- [x] S6 - Sync UI + observability + hardening.
	- [x] Expanded Sync UI at `/config/sync` with runtime health, telemetry and auto-refresh.
	- [x] Expanded `/api/config/sync` console for quick diagnostics and state polling.
	- [x] Hardening telemetry: `timeoutEvents`, `pollSaturationEvents`, `inputFps`, `activeProtocol`, `degraded`.
	- [x] Base degradation policy for timeout, polling saturation and excessive clock offset.

## Plan detallado de sincronizacion

### Fase S1. Ingesta LedFx por DDP (MVP)

Objetivo: reproducir en tiempo real tramas externas de LedFx con latencia baja y degradacion controlada.

Entregables:

- Receptor DDP en task no bloqueante con buffer doble de frames.
- Modo de entrada configurable: `local_effects` / `ledfx_ddp`.
- Timeout de fuente externa con retorno automatico a efecto local.
- Metricas minimas: fps recibido, frames descartados, ultimo timestamp valido.

Criterio de aceptacion:

## Synchronization Plan Summary

Note: implementation S1-S6 is complete. What remains is hardware validation and resilience testing.
- Si DDP se corta, recuperacion automatica sin reinicio del equipo.


Goal: realtime playback of LedFx external frames with low latency and controlled degradation.

Deliverables:

- Non-blocking DDP receiver with double frame buffer.
- Configurable input mode: `local_effects` / `ledfx_ddp`.
- External source timeout with automatic fallback to local effects.
- Minimum metrics: input FPS, dropped frames, and last valid timestamp.

Acceptance criteria:

- Stable render with continuous DDP source.
- Automatic recovery without reboot when DDP stops.

### Phase S2. E1.31 Fallback

Goal: compatibility with setups where DDP is not available.

Deliverables:

- Base E1.31 parser for initial single-strip/universe scenarios.
- External protocol selector: `ddp` or `e131`.
- Input normalization into the shared render pipeline.

Acceptance criteria:

- Protocol switching from configuration without recompilation.
- Equivalent baseline visual output under comparable DDP and E1.31 conditions.

### Phase S3. Device-to-Device Cluster Sync

Goal: let one `server` node distribute playback state to `client` nodes.

Deliverables:

- `SyncState v1` payload with `sequence`, `effectId`, `params`, `brightness`, and `power`.
- Lightweight UDP transport.
- Anti-replay rejection based on monotonic sequence.
- Ownership policy: only the `server` publishes state in sync mode.

Acceptance criteria:

- Two or more nodes remain visually aligned at the state level.
- A reconnecting client can fully resynchronize live.

### Phase S4. Phase/Clock Sync for Distributed Effects

Goal: align time phase across animated effects on multiple devices.

Deliverables:

- `syncEpochMs` and `phaseOffset` in `SyncState`.
- Smooth drift correction.
- Jitter tolerance window.

Acceptance criteria:

- Periodic effects start and evolve in phase across nodes.
- Under moderate jitter, continuity is prioritized over instant exactness.

### Phase S5. Sync API/UI and Observability

Goal: operational control and diagnostics through UI/API.

Deliverables:

- `/api/v1/sync/*` endpoints for mode, role, sequence, and health.
- Sync UI with mode, role, and telemetry.
- Persistent sync configuration.

Acceptance criteria:

- Full operation without Serial: configure, activate, monitor, and disable sync from the UI.

## Dependencies and Risks

- Unstable WiFi affects jitter and UDP packet loss.
- Render must not be blocked by network parsing.
- Fallback to local effects must be deterministic to avoid mixed state.
- RAM footprint must remain controlled while sizing external frame buffers.

## Key Technical Decisions

- Non-blocking architecture with separate task responsibilities.
- NeoPixelBus as the preferred backend and FastLED as an alternative.
- Effect behavior decoupled from physical LED count where possible.
- Versioned REST API with tolerance for real-world client payloads.

For deeper details, see the architecture documentation in this wiki.
