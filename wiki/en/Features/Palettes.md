# Color Palettes

## Predefined Palettes

Curated catalog of 12+ three-color palettes grouped by style (`warm`, `cold`, `neon`, `pastel`, `high-contrast`, `party`).

## User Palettes

- Created, edited, and deleted from either the UI or the API
- Persisted in `/user-palettes.json` (LittleFS)
- Clear distinction between system palettes (read-only) and user palettes (editable)

## Relevant Endpoints

- `GET /api/v1/palettes` — List all palettes
- `POST /api/v1/palettes/apply` — Apply palette
- `POST /api/v1/palettes/save` — Save/edit user palette
- `POST /api/v1/palettes/delete` — Delete user palette

## Palette Example

```json
{
  "id": -101,
  "label": "My palette",
  "primaryColors": ["#123456", "#654321", "#ABCDEF"],
  "style": "neon",
  "description": "Custom palette"
}
```

Palettes can be applied live and used by any effect.

## Recommended Scenarios

### Living Room / Ambient

- `sunset_drive` (warm): warm and relaxing.
- `golden_hour` (warm): soft light for continuous use.
- `mint_lavender` (pastel): low visual fatigue.
- `aurora_soft` (pastel): balanced at medium/low brightness.

### Party / Show

- `vaporwave` (party): high-impact nighttime look.
- `synthwave` (party): strong presence in motion-heavy scenes.
- `club_rgb` (neon): high saturation for rhythmic scenes.
- `lava_ice` (high-contrast): aggressive contrast for dynamic effects.

### Audio Reactive (LedFx/local)

- `ember_citrus_cyan` (high-contrast): strong visual separation on peaks.
- `ocean_neon` (neon): clear reading in bar/ribbon effects.
- `club_rgb` (neon): noticeable response on transients.
- `lava_ice` (high-contrast): maximum zone differentiation.
