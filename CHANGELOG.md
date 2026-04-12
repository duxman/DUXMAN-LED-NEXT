# Changelog

Todos los cambios relevantes de este proyecto se documentan en este archivo.

## [0.3.4-beta] - 2026-04-12

### Added
- Nuevo efecto `audio_pulse` como VU simetrico con `beat flash` y `peak hold`.
- Metadatos de tipo de efecto (`VISUAL` vs `AUDIO`) expuestos en catalogo y estado runtime.
- Roadmap por fases para sincronizacion maestro-esclavo y uso de LedFx como fuente audio-reactive.

### Changed
- Normalizacion de audio migrada a AGC con `noise gate` para evitar saturacion y ruido residual.
- Reactividad al microfono ya no es global: ahora se activa automaticamente solo en efectos de audio.
- UI embebida separa efectos en grupos `Visuales` y `Audio reactivos` con etiquetas claras.
- Efectos `trig_ribbon`, `stellar_twinkle` y `bouncing_physics` reforzados para respuesta de audio/beat.

### Fixed
- `audioLevel` constante en 255 en escenarios de RMS alto.
- Ambiguedad en persistencia/estado sobre cuando un efecto debe usar audio.

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
