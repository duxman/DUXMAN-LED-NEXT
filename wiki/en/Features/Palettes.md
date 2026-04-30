# Paletas de color

## Paletas predefinidas

Catálogo curado de 12+ paletas de 3 colores principales, clasificadas por estilo (warm, cold, neon, pastel, high-contrast, party).

## Paletas de usuario

- Creadas, editadas y eliminadas desde la UI o API
- Persisten en `/user-palettes.json` (LittleFS)
- Distinción entre paletas de sistema (read-only) y usuario (editables)

## Endpoints relevantes

- `GET /api/v1/palettes` — Listar todas las paletas
- `POST /api/v1/palettes/apply` — Aplicar paleta
- `POST /api/v1/palettes/save` — Guardar/editar paleta de usuario
- `POST /api/v1/palettes/delete` — Eliminar paleta de usuario

## Ejemplo de paleta

```json
{
  "id": -101,
  "label": "Mi paleta",
  "primaryColors": ["#123456", "#654321", "#ABCDEF"],
  "style": "neon",
  "description": "Paleta personalizada"
}
```

Las paletas pueden aplicarse en caliente y usarse en cualquier efecto.

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
