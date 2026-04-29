# API REST v1

Esta página resume la API implementada actualmente por el firmware DUXMAN-LED-NEXT (v0.3.9-beta).

Base: /api/v1

## Endpoints

### Estado y sistema

- GET /api/v1/state
- PATCH, POST /api/v1/state
- POST /api/v1/system/restart
- GET /api/v1/diag

### Configuración

- GET /api/v1/config/network
- PATCH, POST /api/v1/config/network
- GET /api/v1/config/microphone
- PATCH, POST /api/v1/config/microphone
- GET /api/v1/config/gpio
- PATCH, POST /api/v1/config/gpio
- GET /api/v1/config/debug
- PATCH, POST /api/v1/config/debug
- GET /api/v1/config/all
- POST /api/v1/config/all

### Perfiles

- GET /api/v1/profiles
- GET /api/v1/profiles/get?id=<id>
- POST, PATCH /api/v1/profiles/save
- POST, PATCH /api/v1/profiles/apply
- POST, PATCH /api/v1/profiles/default
- POST, PATCH /api/v1/profiles/delete
- POST, PATCH /api/v1/profiles/clone

### Efectos

- GET /api/v1/effects
- POST, PATCH /api/v1/effects/startup/save
- POST, PATCH /api/v1/effects/sequence/add
- POST, PATCH /api/v1/effects/sequence/delete

### Paletas

- GET /api/v1/palettes
- POST, PATCH /api/v1/palettes/apply
- POST, PATCH /api/v1/palettes/save
- POST, PATCH /api/v1/palettes/delete

### Metadatos

- GET /api/v1/hardware
- GET /api/v1/release
- GET /api/v1/openapi.json

## Notas

- PATCH y POST se aceptan como equivalentes en varios endpoints de mutación.
- La ruta canónica de perfiles es /api/v1/profiles*.
- Las referencias antiguas /api/v1/profiles/gpio* son legacy y deben evitarse.

## Detalle ampliado

Consultar el documento técnico completo en docs/api-v1.md.
