# Changelog

All notable changes in this project are documented in this file.

Language / Idioma:

- English: `CHANGELOG.en.md`
- Espanol: `CHANGELOG.md`

## [0.6.3-alpha] - 2026-05-02

### Added
- Full synchronization stack S1-S6 completed in firmware:
  - LedFx realtime ingest over `DDP` with double buffering, `Push`-based frame commit, and timeout fallback.
  - `E1.31/sACN` fallback with multi-universe support on top of the same external frame pipeline.
  - UDP `cluster_sync` using `SyncState v1`, monotonic sequence, anti-replay, and `groupMask` filtering.
  - Global synchronized clock for distributed effects with `clockSmoothing=soft`.
- New `/config/sync` UI with persistent configuration, configurable ports, runtime state, and auto-refresh.
- New `/api/config/sync` console and lightweight `GET /api/v1/sync/connected` endpoint.
- Sync connection banner on Home for `client` nodes with an active source.
- Manual hardware validation document: `wiki/Development/Sync-Test-Plan.md`.

### Changed
- `SyncConfig` now normalizes legacy `master/slave` roles to `server/client` and exposes `enabled` as the activation semantic.
- `main.cpp` integrates `SyncService` into the control loop and bypasses local rendering whenever a valid external frame is available.
- `EffectEngine` now supports a reusable synchronized clock for time-based effects.
- Navigation, configuration index, Home, and i18n text were updated to expose the new Sync layer.
- Release metadata and main documentation were aligned with the actual S1-S6 delivery.

### Documentation
- Development roadmaps updated with a verifiable S1-S6 checklist.
- README and release metadata expanded to reflect sync architecture, API/UI routes, and current limitations.

### Validation
- `esp32dev` build verified successfully after S6 integration.
- Real hardware validation is still pending; see `wiki/Development/Sync-Test-Plan.md`.

## [0.6.2-alpha] - 2026-05-03

### Added
- Sprint 1 of synchronization implemented in firmware:
  - New persistent `SyncConfig` in `config.json`.
  - New `SyncService` with runtime state and basic health/packet metrics.
  - New REST endpoints:
    - `GET /api/v1/sync/state`
    - `GET|PATCH|POST /api/v1/sync/config`
    - `POST|PATCH /api/v1/sync/mode`

### Changed
- `StorageService` now includes the `sync` section when saving/loading unified configuration.
- `ApiService` integrates the new `sync` routes and extends `GET/POST /api/v1/config/all` to import/export the `sync` section.
- `main.cpp` integrates `SyncService` initialization and `tick()` in the control loop.

### Documentation
- Detailed synchronization plan consolidated in `wiki/es/Development/Sync-Implementation-Plan.md`.
- Development roadmaps updated with sync phases and a link to the detailed plan.

## [0.6.1-alpha] - 2026-05-02

### Changed
- Global versioning aligned to `0.6.1-alpha` in firmware, release metadata, web package, and main/wiki documentation.
- Embedded UI navigation loading hardened: HTML templates now mount `nav.html` through a dedicated loader to avoid raw placeholder rendering on direct `/ui/*.html` routes.
- `ApiService` updated to resolve `/ui/*.html` pages with controlled rendering and an additional `/docs` route.

### Fixed
- Rendering issue where the literal `__NAV__` could appear on some routes or load conditions.
- Initial Home status messages integrated into the explicit i18n catalog flow.

## [0.6.0-alpha] - 2026-04-30

### Added
- Full multilingual i18n system (EN/ES, extensible to FR/DE/IT).
- New General Options screen at `/config/general`.
- New REST API endpoints for general configuration.
- Multilingual wiki structure with `wiki/en/` and `wiki/es/`.

### Changed
- `GeneralConfig` renamed and expanded from the old `DebugConfig` model.
- Navigation now points users from Debug to General settings.

### Deprecated
- `/config/debug` and `/api/v1/config/debug` remain as compatibility redirects and will be removed in a future release.

Note: older historical entries remain available in `CHANGELOG.md` and can be translated incrementally if needed.