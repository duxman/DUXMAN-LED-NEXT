# API v1

Proyecto: DUXMAN-LED-NEXT (firmware v0.3.10-beta)

## Base

- HTTP: /api/v1/*
- Serial: mismos comandos (115200 baud)
- Formato: JSON
- Métodos soportados: GET, PATCH, POST

## Endpoints implementados

### Estado y sistema

| Método | Ruta | Descripción |
|---|---|---|
| GET | /api/v1/state | Estado runtime actual |
| PATCH, POST | /api/v1/state | Actualización parcial de estado |
| POST | /api/v1/system/restart | Reinicio del dispositivo |
| GET | /api/v1/diag | Diagnóstico runtime (watchdog, memoria, uptime) |

### Configuración

| Método | Ruta | Descripción |
|---|---|---|
| GET | /api/v1/config/network | Configuración de red |
| PATCH, POST | /api/v1/config/network | Patch parcial de red |
| GET | /api/v1/config/microphone | Configuración de micrófono |
| PATCH, POST | /api/v1/config/microphone | Patch parcial de micrófono |
| GET | /api/v1/config/gpio | Configuración LED/GPIO |
| PATCH, POST | /api/v1/config/gpio | Patch parcial de GPIO |
| GET | /api/v1/config/debug | Configuración debug |
| PATCH, POST | /api/v1/config/debug | Patch parcial de debug |
| GET | /api/v1/config/all | Export de config completa |
| POST | /api/v1/config/all | Import de config completa con validación previa |

### Perfiles

| Método | Ruta | Descripción |
|---|---|---|
| GET | /api/v1/profiles | Lista de perfiles (metadata) |
| GET | /api/v1/profiles/get?id=<id> | Perfil completo por id |
| POST, PATCH | /api/v1/profiles/save | Crear/actualizar perfil |
| POST, PATCH | /api/v1/profiles/apply | Aplicar perfil |
| POST, PATCH | /api/v1/profiles/default | Marcar perfil por defecto |
| POST, PATCH | /api/v1/profiles/delete | Eliminar perfil |
| POST, PATCH | /api/v1/profiles/clone | Clonar perfil |

### Efectos y secuencias

| Método | Ruta | Descripción |
|---|---|---|
| GET | /api/v1/effects | Estado de efectos + secuencias |
| POST, PATCH | /api/v1/effects/startup/save | Guardar startup desde estado actual |
| POST, PATCH | /api/v1/effects/sequence/add | Añadir entrada a secuencia |
| POST, PATCH | /api/v1/effects/sequence/delete | Eliminar entrada de secuencia |

### Paletas

| Método | Ruta | Descripción |
|---|---|---|
| GET | /api/v1/palettes | Lista paletas (sistema + usuario) |
| POST, PATCH | /api/v1/palettes/apply | Aplicar paleta |
| POST, PATCH | /api/v1/palettes/save | Guardar/editar paleta usuario |
| POST, PATCH | /api/v1/palettes/delete | Eliminar paleta usuario |

### Sistema y metadatos

| Método | Ruta | Descripción |
|---|---|---|
| GET | /api/v1/hardware | Info de hardware runtime |
| GET | /api/v1/release | Metadata de release compilada |
| GET | /api/v1/openapi.json | Especificación OpenAPI |

## Esquemas principales de payload

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
    "outputs": [
      { "pin": 8, "ledCount": 60, "ledType": "ws2812b", "colorOrder": "GRB" },
      { "pin": 16, "ledCount": 1, "ledType": "digital", "colorOrder": "R" }
    ]
  }
}
```

### POST /api/v1/profiles/save

```json
{
  "profile": {
    "id": "mi_perfil",
    "name": "Mi perfil",
    "description": "Snapshot de prueba",
    "network": { "wifi": { "mode": "ap" } },
    "gpio": { "outputs": [ { "pin": 8, "ledCount": 60, "ledType": "ws2812b", "colorOrder": "GRB" } ] },
    "microphone": { "enabled": false },
    "debug": { "enabled": false }
  }
}
```

### POST /api/v1/config/all

```json
{
  "network": { "wifi": { "mode": "ap_sta" } },
  "gpio": { "outputs": [ { "pin": 8, "ledCount": 60, "ledType": "ws2812b", "colorOrder": "GRB" } ] },
  "microphone": { "enabled": false, "source": "generic_i2c", "sampleRate": 16000, "fftSize": 512 },
  "debug": { "enabled": true, "heartbeatMs": 5000 }
}
```

## Errores comunes

Respuestas típicas:

```json
{"error":"invalid_payload"}
{"error":"invalid_json"}
{"error":"not_found"}
```

En import global (/config/all), los errores se prefijan por sección cuando falla validación en candidato:

- network_...
- gpio_...
- microphone_...
- debug_...

## Notas de compatibilidad

- La ruta canónica de perfiles es /api/v1/profiles*.
- Referencias antiguas /api/v1/profiles/gpio* deben considerarse obsoletas en documentación.
