# Storage & Memory

Persistence and memory-usage design for DUXMAN-LED-NEXT (state as of v0.6.3-alpha).

## Goals

- Avoid configuration corruption during unexpected reboots.
- Reduce flash wear through deferred writes.
- Keep control and render latency low by avoiding blocking I/O in critical paths.

## Storage

Primary backend:

- LittleFS for configuration, profiles, user palettes, and UI assets.

Current UI strategy:

- HTML/CSS templates under `data/ui` deployed to LittleFS.
- Embedded firmware fallback for missing-file tolerance.

Build requirement:

- `platformio.ini` sets `board_build.filesystem = littlefs` so `uploadfs` generates the correct `littlefs.bin`.

## Persistence Policy

Deferred persistence:

- Changes are first applied in RAM and then queued by `PersistenceSchedulerService`.
- Change coalescing avoids a flash write on every slider adjustment.

Persistence managed by services:

- `StorageService`: base configuration serialization/deserialization.
- `ProfileService`: full profiles and default profile.
- `UserPaletteService`: CRUD and persistence for user palettes.
- `EffectPersistenceService`: startup effect and sequences.

## Atomicity and Robustness

Applied principles:

- Validate payloads/configuration before committing global changes.
- On network configuration changes, send the HTTP response before WiFi reconfiguration to minimize client-side resets.
- Assemble `/config/all` exports by section to reduce peak memory usage and reset risk under RAM pressure.

## Memoria RAM

Main sources of consumption:

- LED render buffers per output/backend.
- Audio capture buffers and shared audio runtime state.
- Temporary JSON documents used by API/configuration paths.

Current measures:

- Avoid a large aggregate `JsonDocument` in `GET /api/v1/config/all`.
- Tune `AudioService` for shorter buffers and more frequent processing with controlled resource use.
- Keep control and render as separate tasks to avoid cross-blocking.

## Operational Recommendations

- When deploying UI changes under `data/ui`, run `uploadfs` in addition to firmware upload.
- Keep debug heartbeat and logs disabled during normal operation to reduce serial noise.
- Revalidate microphone and network configuration on real hardware after pipeline changes.

## Open Risks

- Audio sensitivity still depends on microphone hardware and ambient noise.
- Occasional mutex contention is still possible under very high API + render activity.
- A final OTA strategy compatible with current partitions and firmware growth is still pending.
