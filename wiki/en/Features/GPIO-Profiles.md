# GPIO Profiles

## What Is a Profile?

A profile is a full snapshot of device configuration (`network + gpio + microphone + debug`), with practical focus on LED topology.

## Types

- Built-in (read-only): presets included in firmware
- User profiles: up to 8 profiles persisted in LittleFS

## Canonical API Routes

- GET /api/v1/profiles
- GET /api/v1/profiles/get?id=<id>
- POST, PATCH /api/v1/profiles/save
- POST, PATCH /api/v1/profiles/apply
- POST, PATCH /api/v1/profiles/default
- POST, PATCH /api/v1/profiles/delete
- POST, PATCH /api/v1/profiles/clone

Note: old `/api/v1/profiles/gpio*` routes are considered legacy and should be avoided.

## Save Example

```json
{
  "profile": {
    "id": "custom1",
    "name": "My profile",
    "description": "Main output + auxiliary digital output",
    "gpio": {
      "outputs": [
        { "pin": 8, "ledType": "ws2812b", "colorOrder": "GRB", "ledCount": 60 },
        { "pin": 16, "ledType": "digital", "colorOrder": "R", "ledCount": 1 }
      ]
    },
    "network": { "wifi": { "mode": "ap" } },
    "microphone": { "enabled": false },
    "debug": { "enabled": false }
  }
}
```

Applying a profile reconfigures the LED driver live.
