# Arquitectura

Firmware modular para ESP32, inspirado en WLED pero con arquitectura propia, API REST versionada y separación estricta de servicios.

## Módulos principales
- **CoreState**: Estado runtime del motor de iluminación
- **ApiService**: Adaptador HTTP/Serial, expone `/api/v1/*` y páginas HTML embebidas
- **StorageService**: Persistencia asíncrona y atómica en LittleFS
- **UserPaletteService**: CRUD de paletas de usuario
- **EffectEngine**: Motor de efectos visuales y audio-reactivos
- **LedDriver**: Abstracción de hardware LED, múltiples salidas/backends
- **WifiService**: Gestión de modos AP/STA, hostname, IP, mDNS

## Modelo runtime
1. Arranque: carga de configuración y perfiles por defecto
2. Inicialización de servicios y hardware
3. Loop principal: render de efectos, atención a API, persistencia diferida
4. Watchdog y recuperación ante errores

## Persistencia y perfiles
- Configuración persistente en LittleFS (`config.json`, `gpio-profiles.json`, `user-palettes.json`)
- Perfiles GPIO: presets integrados y de usuario, arranque automático
- Paletas: catálogo de sistema y usuario, editable en UI

## API REST v1
- HTTP (`/api/v1/*`) y Serial (comandos idénticos)
- Endpoints para estado, configuración, perfiles, paletas, debug, hardware
- Respuestas JSON deterministas, validación estricta

## Novedades recientes
- Menú de navegación responsive y unificado en todas las páginas
- Editor visual de paletas de usuario (CRUD)
- Perfiles GPIO completos y aplicables en caliente
- Motor de efectos robusto, validado en hardware real