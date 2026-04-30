# Changelog

Todos los cambios relevantes de este proyecto se documentan en este archivo.

## [0.4.0-beta] - 2026-04-30

### Added
- **Tres nuevos efectos audio-reactivos espectaculares**:
  - `AUDIO · Rainbow Wave` (id=19): Onda de colores que viaja (Rojo→Naranja→Amarillo→Blanco según audioLevel). Movimiento fluido, muy visual.
  - `AUDIO · Spectrum Chase` (id=20): Chase tipo Knight Rider con colores reactivos por rango de frecuencia (Rojo=bajos, Verde=medios, Azul=altos). Velocidad acelerada con audio.
  - `AUDIO · Section Strobe` (id=21): Efecto discoteca: LEDs divididos en secciones que flashean por turno en cada beat. Colores rotativos (Rojo→Verde→Azul→Magenta). Muy dramático.

- **Reorganización de carpetas de efectos** para mejor mantenibilidad:
  - `firmware/src/effects/audio-reactive/`: Contiene todos los efectos reactivos al audio (9 efectos: AudioPulse, AudioSpectrum, AudioNeonEq, TrigRibbon, StellarTwinkle, BouncingPhysics + 3 nuevos).
  - `firmware/src/effects/visual-only/`: Contiene todos los efectos puramente visuales (13 efectos: Fixed, Gradient, Blink*, Breath*, Diagnostic, Triple Chase, Meteor, Scanning Pulse, Lava Flow, Polar Ice, Random Pop).
  - Actualizadas rutas de include en `EffectManager.cpp` y todos los archivos `.cpp` de efectos.

### Changed
- Estructura del motor de efectos ahora completamente modular por tipo.
- Registry de efectos (EffectRegistry) extendido con 3 nuevas entradas (IDs 19, 20, 21) e indicador `usesAudio` más preciso.

### Build Info
- RAM: 17.9% (58.6 KB / 320 KB)
- Flash: 39.8% (1252 KB / 3145 KB)
- Compilación: **SUCCESS** en 5.7 segundos

## [0.3.11-beta] - 2026-04-30

### Added
- Nuevo efecto `AUDIO · Neon EQ` (`audio_neon_eq`, id=18): ecualizador visual de 3 bandas con barras segmentadas, cabezal brillante por barra y flash de beat.
- Mapeo de color por banda con `primaryColors[0..2]` (bajos/medios/altos) y alternancia de dirección por segmento para un look más dinámico.

### Changed
- Configuración GPIO extendida con `powerLimit` para control de consumo software: `enabled`, `maxCurrentmA`, `milliAmpsPerLed`.
- Drivers LED (`NeoPixelBus`/`FastLED`) ahora aplican escala global de potencia estimada cuando el límite está activo.
- UI/API de configuración GPIO actualizadas para editar y persistir `powerLimit`.

## [0.3.10-beta] - 2026-04-30

### Added
- Nuevo efecto `AUDIO � Spectrum VU` (`audio_spectrum`, id=17): VU-meter de 3 bandas (bajos / medios / altos) mapeadas a `primaryColors[0/1/2]`.
  - Modo **multi-salida** (>=3 outputs): cada salida recibe una banda completa con relleno desde los bordes hacia el centro.
  - Modo **segmentos** (<3 outputs): la tira se divide en `sectionCount` segmentos, segmento i dibuja la banda `i % 3`.
  - Beat flash en la banda de bajos con mezcla blanca de 120 ms.
  - Suavizado exponencial por banda: ataque rapido (0.35), decay lento (0.08) para aspecto VU clasico.
  - Sin `reactiveToAudio`: muestra los 3 colores fijos al 80% de brillo.
## [0.3.9-beta] - 2026-04-30

### Fixed
- **Píxeles aleatorios al cambiar efecto**: eliminado el `show()` intermedio en la transición de efecto en `EffectManager`. Hacer `clear()` + `show()` (inicia DMA) y luego escribir el buffer en el mismo ciclo provocaba corrupción de la trama en NeoPixelBus/RMT.
- **Doble `begin()` en el arranque**: `EffectEngine::begin()` ya no llama a `driver_.begin()`. El ciclo de vida del driver es responsabilidad exclusiva de `applyActiveConfig()` y del guard `isInitialized()` en `EffectManager`.

## [0.3.8-beta] - 2026-04-29

### Added
- Documento técnico de refactor arquitectónico: `docs/analisis-arquitectura-refactor.md`.
- Utilidades compartidas de color en `ColorUtils` para parse/format de colores hex.

### Changed
- Rutas de perfiles documentadas de forma canónica como `/api/v1/profiles*` en README, docs y wiki.
- Sincronización integral de documentación técnica (`README`, `docs/api-v1.md`, `docs/architecture.md`, `wiki/API-v1.md`, `wiki/Architecture.md`, `wiki/GPIO-Profiles.md`).
- `EffectManager` desacoplado en cabecera usando `Impl` para reducir fan-in de includes.

### Fixed
- Publicación de métricas de audio y pulso de beat en `AudioService` sin escrituras no protegidas al estado compartido.
- Compatibilidad de `NeoPixelBus` en ESP32-C3 evitando métodos RMT no disponibles para canales 2/3.
- Costo de gamma por píxel optimizado mediante LUT en `EffectEngine`.

## [0.3.7-beta] - 2026-04-28

### Added
- Menú de navegación horizontal unificado (`buildNavHtml()`) compartido por las 20 páginas HTML embebidas. Colores teal coherentes con la paleta de la aplicación (`#0a3d4a → #0f6a7a`), dropdowns por sección (Config / API / Sobre) y hamburger responsive para móvil.
- Envoltorio `gen-page-outer` (90% de ancho, centrado, con `border-radius` y sombra) que contiene el nav + contenido en todas las páginas.

### Changed
- Eliminados los botones de navegación (`btn ghost`) dentro de los formularios de configuración (Network, Microphone, GPIO, Debug, Profiles, Manual JSON). Las secciones de acción solo muestran los botones funcionales: Recargar, Guardar, Aplicar, etc.
- La página principal ya no muestra el bloque de tiles de navegación (Config / API / Versión) redundante con el nuevo menú.
- El nav deja de ser un elemento flotante sobre la página y pasa a ser el cabezal superior del `gen-page-outer`, sin radio propio y al 100% del ancho del contenedor.

## [0.3.6-beta] - 2026-04-13

### Added
- `UserPaletteService`: servicio completo de paletas de usuario con CRUD, persistencia asíncrona en LittleFS (`/user-palettes.json`) y límite de 20 paletas.
- Distinción explícita entre paletas del **sistema** (`source: "system"`, read-only, ids ≥ 0) y paletas de **usuario** (`source: "user"`, editables, ids codificados ≤ −100).
- Endpoints `POST /api/v1/palettes/save` y `POST /api/v1/palettes/delete` para gestión de paletas de usuario.
- `GET /api/v1/palettes` ahora devuelve la lista combinada sistema + usuario con campos `source` y `readOnly` por cada entrada.
- Página de configuración embebida `/config/palettes`: editor visual con swatches de 3 colores, formulario de creación/edición (id slug, nombre, estilo, descripción, 3 color-pickers), y botones de aplicar / editar / eliminar por paleta.
- Box "Paletas" añadido al índice `/config` para acceso directo al editor.

### Changed
- `ApiService` actualizado para inyectar `UserPaletteService` y enrutar los nuevos endpoints.
- `handleHttpPalettesRoute()` delega el listado a `userPaletteService_.listAllJson()` en lugar de `PaletteRegistry::toJsonArray()`.
- `saveFromJson()` acepta dos formatos de entrada: `colors.{color1,color2,color3}` (frontend) y `primaryColors[]` (API genérica).

## [0.3.5-beta] - 2026-04-12

### Added
- Sistema de paletas predefinidas de 3 colores con catálogo curado de 12 combinaciones.
- Endpoints `GET /api/v1/palettes` y `POST /api/v1/palettes/apply` para listado y aplicación de paletas.
- Documento `docs/palettes.md` con recomendaciones por escenario y evidencia de validación.

### Changed
- `CoreState` y la persistencia de efectos incorporan `paletteId` y metadatos de paleta manteniendo compatibilidad con colores manuales.
- UI embebida añade selector de paleta, vista previa rápida y flujo de `aplicar` / `aplicar y guardar`.
- Documentación técnica sincronizada (`README`, API, arquitectura y roadmap) para reflejar el cierre completo de la Fase 3A.

### Fixed
- Ambigüedad entre modo paleta y modo manual al enviar `primaryColors` por API.
- Falta de trazabilidad documental sobre validación real en `ws2812b`, `ws2815` y `sk6812` con múltiples niveles de brillo.

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
