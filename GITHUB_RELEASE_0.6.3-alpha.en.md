# Suggested Title

`v0.6.3-alpha - Full Sync Stack S1-S6`

# Release Body

DUXMAN LED-NEXT `0.6.3-alpha` completes the planned S1-S6 synchronization stack and aligns firmware, API, UI, and documentation around the current architecture.

## Highlights

- Full distributed sync pipeline for ESP32 nodes
- LedFx realtime input over `DDP` with timeout fallback
- `E1.31/sACN` fallback routed through the same external frame pipeline
- UDP `cluster_sync` with monotonic sequence, anti-replay, and `groupMask` filtering
- Global synchronized effect clock with soft drift correction
- New Sync configuration UI and API routes for configuration and observability

## Included in this release

- Persistent `SyncConfig` integrated into firmware configuration
- Runtime `SyncService` integrated into the control loop
- New UI page: `/config/sync`
- New API console: `/api/config/sync`
- New endpoint: `GET /api/v1/sync/connected`
- Home status banner for connected `client` nodes
- Updated Spanish and English documentation

## Validation status

- `esp32dev` build verified successfully
- Manual multi-device hardware validation is still pending
- Suggested validation guide: `wiki/Development/Sync-Test-Plan.md`

## Known limitations

- No OTA support yet with the current `huge_app` partition layout
- Sync still requires real hardware validation under long-run jitter and multi-device conditions
- LittleFS writes are deferred but still executed synchronously in the control task

## Next phase

- Hardware validation of sync S1-S6
- Resilience testing under real network conditions
- OTA and rollback strategy
