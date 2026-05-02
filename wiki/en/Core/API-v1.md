# API REST v1

Summary of the API currently implemented by DUXMAN-LED-NEXT (v0.6.3-alpha).

Base: /api/v1

## Endpoints

### State and system

- GET /api/v1/state
- PATCH, POST /api/v1/state
- POST /api/v1/system/restart
- GET /api/v1/diag

### Configuration

- GET /api/v1/config/network
- PATCH, POST /api/v1/config/network
- GET /api/v1/config/microphone
- PATCH, POST /api/v1/config/microphone
- GET /api/v1/config/gpio
- PATCH, POST /api/v1/config/gpio
- GET /api/v1/config/debug
- PATCH, POST /api/v1/config/debug
- GET /api/v1/sync/state
- GET /api/v1/sync/connected
- GET /api/v1/sync/config
- PATCH, POST /api/v1/sync/config
- PATCH, POST /api/v1/sync/mode
- GET /api/v1/config/all
- POST /api/v1/config/all

Notes:

- `/config/network` and `/config/all` respond before WiFi reconfiguration to reduce client disconnects.
- `/config/all` normalizes double-serialized JSON payloads.

### Profiles

- GET /api/v1/profiles
- GET /api/v1/profiles/get?id=<id>
- POST, PATCH /api/v1/profiles/save
- POST, PATCH /api/v1/profiles/apply
- POST, PATCH /api/v1/profiles/default
- POST, PATCH /api/v1/profiles/delete
- POST, PATCH /api/v1/profiles/clone

### Effects

- GET /api/v1/effects
- POST, PATCH /api/v1/effects/startup/save
- POST, PATCH /api/v1/effects/sequence/add
- POST, PATCH /api/v1/effects/sequence/delete

### Palettes

- GET /api/v1/palettes
- POST, PATCH /api/v1/palettes/apply
- POST, PATCH /api/v1/palettes/save
- POST, PATCH /api/v1/palettes/delete

### Metadata

- GET /api/v1/hardware
- GET /api/v1/release
- GET /api/v1/openapi.json

## Compatibility

- PATCH and POST are accepted as equivalents on several mutating endpoints.
- The canonical profiles route is `/api/v1/profiles*`.
- Legacy `/api/v1/profiles/gpio*` routes should be avoided.

## Detailed Reference

See the rest of the wiki for expanded technical details and workflow examples.
