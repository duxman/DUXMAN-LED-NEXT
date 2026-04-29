# Perfiles GPIO

## ¿Qué es un perfil GPIO?
Un perfil GPIO define la configuración de todas las salidas LED del dispositivo (pin, tipo, orden de color, cantidad de LEDs, etc). Permite cambiar la topología de hardware sin reconfigurar manualmente cada parámetro.

## Tipos de perfiles
- **Integrados**: presets por placa (`esp32dev`, `esp32c3supermini`, `esp32s3`)
- **Usuario**: creados y guardados desde la UI o API

## Endpoints relevantes
- `GET /api/v1/profiles/gpio` — Listar todos los perfiles
- `POST /api/v1/profiles/gpio/save` — Guardar/editar perfil
- `POST /api/v1/profiles/gpio/apply` — Aplicar perfil
- `POST /api/v1/profiles/gpio/default` — Fijar perfil por defecto
- `POST /api/v1/profiles/gpio/delete` — Eliminar perfil

## Ejemplo de perfil
```json
{
  "id": "custom1",
  "label": "Mi perfil",
  "outputs": [
    { "pin": 5, "ledType": "ws2812b", "colorOrder": "GRB", "ledCount": 60 }
  ]
}
```

Al aplicar un perfil, el driver LED se reconfigura en caliente sin reinicio.