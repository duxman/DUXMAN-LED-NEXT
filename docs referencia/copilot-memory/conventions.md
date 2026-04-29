# Convenciones de código — DUXMAN-LED-NEXT

## C++ / Firmware
- Nombres de struct: PascalCase (`NetworkConfig`, `CoreState`)
- Campos de struct: camelCase (`heartbeatMs`, `apAvailability`)
- Miembros privados clase: trailing underscore (`state_`, `httpServer_`)
- Constantes: kCamelCase en namespace anónimo (`kModeAp`, `kIpStatic`)
- JSON keys: camelCase (coinciden con campos de struct)
- Validación: devuelve `bool` con `String *error` opcional
- Includes: ruta relativa desde src/ (`"core/Config.h"`, `"services/StorageService.h"`)
- Guardas: `#pragma once`
- ArduinoJson: versión 7.x API (`JsonDocument` sin tamaño, `to<JsonObject>()`)

## HTML embebido
- CSS custom properties en `:root`
- Estilo visual: tarjetas redondeadas, gradientes suaves, tipografía Trebuchet MS
- Patrón: card layout con topnav de links, formularios con select/input
- JavaScript: fetch API inline, sin frameworks
- Colores por sección: verde (config), azul (API), púrpura (release), marrón cálido (network)

## API
- Versionado: `/api/v1/...`
- Métodos: GET para leer, PATCH para actualizar parcial, POST como alias de PATCH
- Respuesta update: `{"updated": true/false, "<section>": {...}}`
- Errores: `{"error": "codigo_snake_case"}`

## Archivos de config
- device-config.json: red + debug (persistido en LittleFS)
- gpio-config.json: configuración de salidas LED (persistido en LittleFS)
- gpio-profiles.json: perfiles GPIO de usuario
- startup-profile.json: id del perfil GPIO por defecto de arranque
- state.json: estado runtime
- platformio.ini: perfiles de placa con macros DUX_*
- Versión NO se persiste en LittleFS — viene de BuildProfile.h (compile-time)

## Documentación
- README.md: SIEMPRE actualizar con cada cambio o funcionalidad nueva
- docs/: SIEMPRE mantener actualizados los documentos afectados por el cambio (`roadmap.md`, `api-v1.md`, `architecture.md`, etc.)
- docs/roadmap.md: mantener TODOs sincronizados con README
- .copilot-memory/: actualizar contexto, convenciones y decisiones cuando cambie arquitectura, flujo o reglas del proyecto
- El usuario se comunica en español
