# API v1

Proyecto: DUXMAN-LED-NEXT (firmware v0.4.2-beta)

## Base

- HTTP: /api/v1/*
- Serial: comandos equivalentes cuando aplica
- Formato: JSON
- Métodos soportados: GET, PATCH, POST

## Endpoints implementados

### Estado y sistema

| Metodo | Ruta | Descripcion |
|---|---|---|
| GET | /api/v1/state | Estado runtime actual |
| PATCH, POST | /api/v1/state | Actualizacion parcial de estado |
| POST | /api/v1/system/restart | Reinicio del dispositivo |
| GET | /api/v1/diag | Diagnostico runtime (watchdog, memoria, uptime) |

### Configuracion

| Metodo | Ruta | Descripcion |
|---|---|---|
| GET | /api/v1/config/network | Configuracion de red |
| PATCH, POST | /api/v1/config/network | Patch parcial de red |
| GET | /api/v1/config/microphone | Configuracion de microfono |
| PATCH, POST | /api/v1/config/microphone | Patch parcial de microfono |
| GET | /api/v1/config/gpio | Configuracion LED/GPIO |
| PATCH, POST | /api/v1/config/gpio | Patch parcial de GPIO (outputs + powerLimit) |
| GET | /api/v1/config/debug | Configuracion debug |
| PATCH, POST | /api/v1/config/debug | Patch parcial de debug |
| GET | /api/v1/config/all | Export de configuracion completa |
| POST | /api/v1/config/all | Import de configuracion completa con validacion previa |

Notas importantes de comportamiento:

- /api/v1/config/network responde HTTP antes de reaplicar WiFi para evitar cortes de conexion del cliente durante cambios de red.
- /api/v1/config/all aplica el mismo enfoque de respuesta temprana para minimizar ERR_CONNECTION_RESET.
- /api/v1/config/all acepta payload normal y tambien payload doble-serializado (normalizacion defensiva del body).

### Perfiles

| Metodo | Ruta | Descripcion |
|---|---|---|
| GET | /api/v1/profiles | Lista de perfiles (metadata) |
| GET | /api/v1/profiles/get?id=<id> | Perfil completo por id |
| POST, PATCH | /api/v1/profiles/save | Crear/actualizar perfil |
| POST, PATCH | /api/v1/profiles/apply | Aplicar perfil |
| POST, PATCH | /api/v1/profiles/default | Marcar perfil por defecto |
| POST, PATCH | /api/v1/profiles/delete | Eliminar perfil |
| POST, PATCH | /api/v1/profiles/clone | Clonar perfil |

### Efectos y secuencias

| Metodo | Ruta | Descripcion |
|---|---|---|
| GET | /api/v1/effects | Estado de efectos + secuencias |
| POST, PATCH | /api/v1/effects/startup/save | Guardar startup desde estado actual |
| POST, PATCH | /api/v1/effects/sequence/add | Anadir entrada a secuencia |
| POST, PATCH | /api/v1/effects/sequence/delete | Eliminar entrada de secuencia |

### Paletas

| Metodo | Ruta | Descripcion |
|---|---|---|
| GET | /api/v1/palettes | Lista paletas (sistema + usuario) |
| POST, PATCH | /api/v1/palettes/apply | Aplicar paleta |
| POST, PATCH | /api/v1/palettes/save | Guardar/editar paleta de usuario |
| POST, PATCH | /api/v1/palettes/delete | Eliminar paleta de usuario |

### Sistema y metadatos

| Metodo | Ruta | Descripcion |
|---|---|---|
| GET | /api/v1/hardware | Informacion de hardware runtime |
| GET | /api/v1/release | Metadata de release compilada |
| GET | /api/v1/openapi.json | Especificacion OpenAPI |

## Ejemplos de payload

### PATCH /api/v1/state

```json
{
  "power": true,
  "brightness": 160,
  "effect": "gradient",
  "sectionCount": 6,
  "effectSpeed": 30,
  "effectLevel": 5,
  "palette": "sunset_drive",
  "primaryColors": ["#FF4D00", "#FFD400", "#00B8D9"],
  "backgroundColor": "#050505"
}
```

### PATCH /api/v1/config/gpio

```json
{
  "gpio": {
    "powerLimit": {
      "enabled": true,
      "maxCurrentmA": 2500,
      "milliAmpsPerLed": 60
    },
    "outputs": [
      { "pin": 8, "ledCount": 60, "ledType": "ws2812b", "colorOrder": "GRB" },
      { "pin": 16, "ledCount": 1, "ledType": "digital", "colorOrder": "R" }
    ]
  }
}
```

### POST /api/v1/config/all

```json
{
  "network": { "wifi": { "mode": "ap_sta" } },
  "gpio": { "outputs": [ { "pin": 8, "ledCount": 60, "ledType": "ws2812b", "colorOrder": "GRB" } ] },
  "microphone": { "enabled": true, "source": "generic_i2c", "sampleRate": 16000, "fftSize": 512 },
  "debug": { "enabled": false, "heartbeatMs": 5000 }
}
```

## Errores comunes

Respuestas tipicas:

```json
{"error":"invalid_payload"}
{"error":"invalid_json"}
{"error":"not_found"}
```

En import global (/config/all), los errores de validacion se prefijan por seccion:

- network_...
- gpio_...
- microphone_...
- debug_...

## Compatibilidad y notas

- La ruta canonica de perfiles es /api/v1/profiles*.
- Referencias legacy /api/v1/profiles/gpio* deben considerarse obsoletas.
- La UI de prueba y configuracion se sirve desde plantillas LittleFS en data/ui, con fallback embebido en firmware si falta algun archivo.
