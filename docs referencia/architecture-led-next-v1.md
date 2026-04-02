# LED-NEXT Architecture v1

## 1) Scope
This document defines the technical architecture for `LED-NEXT` as a new firmware-first project built around `ESP32-S3` and `PsychicHttp`.

Goals:
- Deterministic runtime behavior for LED rendering.
- Robust configuration persistence with flash wear control.
- Clear separation of concerns between state, API, storage, and rendering.
- Low-friction evolution with versioned contracts.

## 2) System Overview

`LED-NEXT` is structured as modular services around a central runtime state:

- `CoreState`: single source of truth for mutable runtime state.
- `ApiService`: HTTP API adapter built on `PsychicHttp`.
- `StorageService`: deferred and atomic persistence orchestration.
- `ConfigService`: schema validation and migrations.
- `DiagService`: runtime health and counters.
- `EffectEngine`: frame synthesis from state.
- `LedDriver`: hardware output backend.

Data flow:
1. Client sends API request (`PATCH /api/v1/state`).
2. `ApiService` validates payload and applies patch on `CoreState`.
3. If state changed, `StorageService` schedules persistence.
4. `EffectEngine` reads `CoreState` and renders frames.
5. `LedDriver` pushes frame data to LEDs.

## 3) Module Boundaries

### 3.1 CoreState
Responsibilities:
- Hold canonical mutable state (`power`, `brightness`, segments, effect params).
- Provide controlled update methods for partial patches.
- Offer serialization/deserialization helpers.

Constraints:
- No direct filesystem or network access.
- No hardware side effects.

### 3.2 ApiService (`PsychicHttp`)
Responsibilities:
- Register routes under `/api/v1/*`.
- Parse and validate request payloads.
- Return deterministic JSON responses and errors.

Constraints:
- No direct file writes.
- Calls service interfaces only.

### 3.3 StorageService
Responsibilities:
- Manage deferred writes using debounce.
- Execute atomic config file replacement.
- Track persistence metrics (write count, last save time, failures).

Constraints:
- Must not block rendering cadence excessively.

### 3.4 ConfigService
Responsibilities:
- Validate config schema (`schemaVersion`).
- Execute in-place migration paths between schema versions.
- Provide defaults for missing fields.

### 3.5 EffectEngine
Responsibilities:
- Generate LED frames from state at target FPS.
- Keep timing stable under API/load pressure.

### 3.6 LedDriver
Responsibilities:
- Hardware abstraction for LED output method.
- Apply frame buffers with minimal jitter.

## 4) Runtime Model

Main loop responsibilities:
- Poll/schedule services (`StorageService.loop`, optional telemetry updates).
- Keep rendering cadence deterministic.

Thread/task model recommendation:
- One task for HTTP servicing (library-managed where applicable).
- One predictable rendering loop/task.
- Shared state coordination through minimal locking or lock-free snapshots.

## 5) API Contract v1

### 5.1 Endpoints
- `GET /api/v1/state`
- `PATCH /api/v1/state`
- `GET /api/v1/diag`

### 5.2 State payload (example)
```json
{
  "power": true,
  "brightness": 180,
  "segment": {
    "start": 0,
    "stop": 59,
    "effect": "solid",
    "color": [255, 160, 80]
  }
}
```

### 5.3 Error payload format
```json
{
  "code": "invalid_payload",
  "message": "brightness must be 0..255"
}
```

### 5.4 API design rules
- Version all public routes.
- Keep PATCH partial and idempotent where possible.
- Reject unknown critical fields by policy.
- Set payload size limits to protect RAM.

## 6) Persistence Design

### 6.1 Deferred persistence
- Apply runtime state immediately in RAM.
- Schedule save with debounce window (`1000-2000 ms`).
- Coalesce multiple updates into one write.

### 6.2 Atomic save algorithm
1. Serialize to `/config.tmp`.
2. Flush and close file.
3. Rotate backup (`/config.bak`) if enabled.
4. Rename `/config.tmp` to `/config.json`.

### 6.3 Recovery algorithm
- On boot, read `/config.json`.
- If invalid, attempt `/config.bak`.
- If invalid, load defaults and flag diagnostic event.

### 6.4 Flash wear controls
- Write coalescing and throttle.
- Avoid write-per-slider-step.
- Expose diagnostics counters in `/api/v1/diag`.

## 7) Diagnostics Model

`/api/v1/diag` minimum fields:
- `uptimeMs`
- `heapFree`
- `configWrites`
- `lastSaveMs`
- `configLoadErrors`

Additional optional fields:
- render loop period/fps
- worst-case API latency
- high-water mark memory values

## 8) Hardware and Pin Safety

Mandatory checks before pin assignment:
- Board-specific strapping pins.
- Reserved pins for flash/USB/JTAG.
- Input-only or restricted GPIO behavior for selected MCU variant.

Electrical rule:
- If LEDs use `5V` signaling tolerance issues arise, use a level shifter for data line from `3.3V` MCU output.

## 9) Suggested Firmware Folder Layout

```text
firmware/
  platformio.ini
  src/
    main.cpp
    core/
      CoreState.h
      CoreState.cpp
    api/
      ApiService.h
      ApiService.cpp
    services/
      StorageService.h
      StorageService.cpp
      ConfigService.h
      ConfigService.cpp
      DiagService.h
      DiagService.cpp
    effects/
      EffectEngine.h
      EffectEngine.cpp
    drivers/
      LedDriver.h
      LedDriver.cpp
  include/
  test/
```

## 10) Incremental Implementation Plan

Sprint 1:
- `CoreState` + `GET/PATCH /api/v1/state`.
- Payload validation and deterministic errors.

Sprint 2:
- `StorageService` debounce + atomic save.
- Boot-time config load and fallback.

Sprint 3:
- `DiagService` + `/api/v1/diag`.
- Basic runtime counters and persistence metrics.

Sprint 4:
- Initial `EffectEngine` and `LedDriver` integration.
- Baseline performance measurements.

## 11) Definition of Done

A sprint closes when:
- firmware compiles for selected `ESP32-S3` board,
- API contract for sprint is functional,
- storage behavior is validated under burst updates,
- docs are updated (`evolucion` + architecture file),
- no regression on previous sprint acceptance checks.

## 12) Session Resume Prompt

Use in next session:

> Continue `LED-NEXT` using `docs/architecture-led-next-v1.md` and `evolucion led-next.md`. Implement the next sprint scope only, keep module boundaries strict, and update docs with any contract or persistence changes.
