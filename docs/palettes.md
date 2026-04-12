# Paletas predefinidas (Fase 3A)

Este documento consolida el uso de paletas predefinidas de 3 colores principales y su recomendacion por escenario.

## Escenarios recomendados

### Salon / ambiente

- `sunset_drive` (warm): calida y relajada.
- `golden_hour` (warm): luz suave para uso continuo.
- `mint_lavender` (pastel): bajo cansancio visual.
- `aurora_soft` (pastel): equilibrada para brillo medio/bajo.

### Fiesta / show

- `vaporwave` (party): look nocturno con alto impacto.
- `synthwave` (party): fuerte presencia en movimiento.
- `club_rgb` (neon): saturacion alta para escenas ritmicas.
- `lava_ice` (high-contrast): contraste agresivo para efectos dinamicos.

### Audio reactivo (LedFx/local)

- `ember_citrus_cyan` (high-contrast): buena separacion visual en picos.
- `ocean_neon` (neon): lectura clara en efectos de barra/cinta.
- `club_rgb` (neon): respuesta perceptible en transitorios.
- `lava_ice` (high-contrast): maxima diferenciacion entre zonas.

## Evidencia de validacion 3A

Validacion ejecutada contra dispositivo real en red (`esp32dev`) con endpoints de runtime.

### Matriz de prueba (tipos LED x brillo)

- Resultado: `9/9` casos correctos.
- Criterio: `PATCH /api/v1/config/gpio` y `PATCH /api/v1/state` devuelven `updated=true` y `paletteId=1`.

| ledType | brillo | gpioUpdated | stateUpdated | paletteId |
|---|---:|---:|---:|---:|
| ws2812b | 32 | true | true | 1 |
| ws2812b | 128 | true | true | 1 |
| ws2812b | 255 | true | true | 1 |
| ws2815 | 32 | true | true | 1 |
| ws2815 | 128 | true | true | 1 |
| ws2815 | 255 | true | true | 1 |
| sk6812 | 32 | true | true | 1 |
| sk6812 | 128 | true | true | 1 |
| sk6812 | 255 | true | true | 1 |

### Persistencia de paleta activa

- Endpoint: `POST /api/v1/effects/startup/save`
- Resultado: `saved=true`
- Evidencia: `startupEffect.paletteId=1`, `startupEffect.palette="sunset_drive"`.

## Notas de uso

- Para volver a modo manual (sin paleta), enviar `primaryColors` en `PATCH /api/v1/state`.
- Para aplicar paleta por clave: `POST /api/v1/palettes/apply {"palette":"sunset_drive"}`.
- Para aplicar por identificador: `POST /api/v1/palettes/apply {"paletteId":1}`.
