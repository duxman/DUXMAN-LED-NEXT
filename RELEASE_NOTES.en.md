# Release Notes

## v0.6.3-alpha - 2026-05-02

DUXMAN LED-NEXT `0.6.3-alpha` completes the planned S1-S6 synchronization stack and aligns firmware, API, UI, and documentation around the current architecture.

## Highlights

- Full distributed sync pipeline for ESP32 nodes.
- LedFx realtime input over `DDP` with timeout fallback.
- `E1.31/sACN` fallback routed through the same external frame pipeline.
- UDP `cluster_sync` with monotonic sequence, anti-replay, and `groupMask` filtering.
- Global synchronized effect clock with soft drift correction.
- New Sync configuration UI and API routes for configuration and observability.

## Included In This Release

- Persistent `SyncConfig` integrated into the firmware configuration model.
- Runtime `SyncService` integrated into the control loop.
- New UI page: `/config/sync`.
- New API console: `/api/config/sync`.
- New endpoint: `GET /api/v1/sync/connected`.
- Home status banner for connected `client` nodes.
- Updated project documentation in Spanish and English.

## Validation Status

- Firmware build verified successfully for `esp32dev`.
- Manual multi-device hardware validation is still pending.
- Recommended validation procedure: `wiki/Development/Sync-Test-Plan.md`.

## Known Limitations

- No OTA support yet with the current `huge_app` partition layout.
- Sync still requires real hardware validation under long-run jitter and multi-device conditions.
- LittleFS writes are deferred but still executed synchronously in the control task.

## Next Phase

- Hardware validation of sync S1-S6.
- Resilience testing under real network conditions.
- OTA and rollback strategy.

## Useful Links

- Main README: `README.en.md`
- Full changelog: `CHANGELOG.en.md`
- English wiki: `wiki/en/Home.md`
- Spanish wiki: `wiki/es/Home.md`
