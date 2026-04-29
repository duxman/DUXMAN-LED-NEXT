# API REST v1

La API REST de DUXMAN-LED-NEXT permite controlar el firmware vía HTTP o Serial (comandos idénticos, 115200 baud).

## Endpoints principales
| Método | Ruta | Descripción |
|---|---|---|
| GET | `/api/v1/state` | Estado runtime actual |
| PATCH/POST | `/api/v1/state` | Actualizar estado |
| GET | `/api/v1/palettes` | Listar paletas (sistema + usuario) |
| POST | `/api/v1/palettes/apply` | Aplicar paleta |
| POST | `/api/v1/palettes/save` | Guardar/editar paleta de usuario |
| POST | `/api/v1/palettes/delete` | Eliminar paleta de usuario |
| GET | `/api/v1/config/network` | Configuración WiFi/IP |
| PATCH/POST | `/api/v1/config/network` | Actualizar red |
| GET | `/api/v1/config/microphone` | Config micrófono |
| PATCH/POST | `/api/v1/config/microphone` | Actualizar micrófono |
| GET | `/api/v1/config/gpio` | Configuración salidas LED |
| PATCH/POST | `/api/v1/config/gpio` | Actualizar GPIO |
| GET | `/api/v1/profiles/gpio` | Listar perfiles GPIO |
| POST | `/api/v1/profiles/gpio/save` | Guardar perfil GPIO |
| POST | `/api/v1/profiles/gpio/apply` | Aplicar perfil GPIO |
| POST | `/api/v1/profiles/gpio/default` | Fijar perfil GPIO por defecto |
| POST | `/api/v1/profiles/gpio/delete` | Eliminar perfil GPIO |
| GET | `/api/v1/config/debug` | Configuración debug |
| PATCH/POST | `/api/v1/config/debug` | Actualizar debug |
| GET | `/api/v1/config/all` | Exportar configuración completa |
| POST | `/api/v1/config/all` | Importar configuración completa |
| GET | `/api/v1/hardware` | Info hardware runtime |
| GET | `/api/v1/release` | Info de versión/release |

## Ejemplo: Estado runtime
```json
{
  "power": true,
  "brightness": 128,
  "effectId": 0,
  "effect": "fixed",
  "sectionCount": 3,
  "primaryColors": ["#FF4D00", "#FFD400", "#00B8D9"],
  "backgroundColor": "#000000",
  "paletteId": 2
}
```

## Ejemplo: Paletas
```json
[
  { "id": 0, "label": "Sunset", "primaryColors": ["#FF4D00", "#FFD400", "#00B8D9"], "source": "system", "readOnly": true },
  { "id": -101, "label": "Mi paleta", "primaryColors": ["#123456", "#654321", "#ABCDEF"], "source": "user", "readOnly": false }
]
```

## Ejemplo: Perfiles GPIO
```json
[
  { "id": "esp32dev", "label": "Default ESP32 DevKit", "outputs": [ ... ] },
  { "id": "custom1", "label": "Mi perfil", "outputs": [ ... ] }
]
```

Para detalles de arquitectura y evolución, ver [Architecture](./Architecture.md) y el changelog del repositorio.