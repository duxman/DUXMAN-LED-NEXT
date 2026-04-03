# Changelog

Todos los cambios relevantes de este proyecto se documentan en este archivo.

## [0.3.3-beta] - 2026-04-03

### Added
- Telemetría de audio por Serial (`[audio.dbg]`) con `active`, `error`, `level`, `peak`, `bytes`, `readErr`, `reactive`, `effectId` y parámetros de micro.
- Exposición de `audioLevel` en `GET /api/v1/state` para verificación rápida de pipeline reactivo.

### Changed
- `AudioService` migrado a I2S `MASTER_RX` (antes `SLAVE_RX`) para generar clocks y estabilizar la captura en micros MEMS I2S.
- Reactividad de audio unificada en el motor base de efectos, aplicada de forma consistente en todo el catálogo.
- Versionado global alineado a `0.3.3-beta` en firmware y documentación.

### Fixed
- Captura de audio en cero constante (`level=0` / `peak=0`) aun con servicio activo.
- Falta de respuesta visible en `fixed`/`gradient` y otros efectos por desacople entre nivel de audio y ganancia de render.

## [0.3.2-beta] - 2026-04-03

### Added
- Presets visuales rapidos en UI (`smooth`, `show`, `aggressive`) para acelerar pruebas en hardware.
- Documento `CHANGELOG.md` para control de releases en git.

### Changed
- Versionado global alineado a `0.3.2-beta` en firmware, metadatos y documentación.
- `versionCode` incrementado a `5` en `firmware/config/release-info.json`.
- `web/package.json` actualizado a `0.3.2`.

### Improved
- README actualizado para reflejar estado real de red/mDNS (`hostname.local`) y estado actual de release.
- Documentación técnica sincronizada (`README`, API y arquitectura) para evitar drift entre firmware y docs.
