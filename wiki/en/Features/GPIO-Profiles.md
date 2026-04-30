# Perfiles GPIO

## Qué es un perfil

Un perfil es un snapshot completo de configuración de dispositivo (network + gpio + microphone + debug), con foco práctico en la topología LED.

## Tipos

- Integrados (read-only): presets incluidos en firmware
- Usuario: hasta 8 perfiles persistidos en LittleFS

## Rutas canónicas de API

- GET /api/v1/profiles
- GET /api/v1/profiles/get?id=<id>
- POST, PATCH /api/v1/profiles/save
- POST, PATCH /api/v1/profiles/apply
- POST, PATCH /api/v1/profiles/default
- POST, PATCH /api/v1/profiles/delete
- POST, PATCH /api/v1/profiles/clone

Nota: rutas antiguas /api/v1/profiles/gpio* se consideran legacy y deben evitarse.

## Ejemplo de guardado

```json
{
  "profile": {
    "id": "custom1",
    "name": "Mi perfil",
    "description": "Salida principal + digital auxiliar",
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

Al aplicar un perfil, el driver LED se reconfigura en caliente.
