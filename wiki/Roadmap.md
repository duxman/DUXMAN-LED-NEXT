# Roadmap y evolución

## Estado actual
- Configuración persistente de red, debug y GPIO/LEDs en LittleFS
- API HTTP/Serial versionada (`/api/v1/*`) con páginas HTML embebidas
- Firmware versionado y perfiles de placa (`esp32c3supermini`, `esp32dev`, `esp32s3`)
- Partición `huge_app` (3MB app, 960KB LittleFS)
- Motor de efectos robusto, validado en hardware real
- Editor visual de paletas de usuario (CRUD)
- Perfiles GPIO completos y aplicables en caliente
- Menú de navegación responsive y unificado

## Fases siguientes
- Sincronización maestro-esclavo (multi-nodo)
- Integración con LedFx y fuentes audio-reactive externas
- Editor avanzado de efectos y presets
- OTA seguro y rollback
- Soporte ampliado para hardware y sensores

## Decisiones técnicas clave
- Priorizar arquitectura no bloqueante (FreeRTOS, tareas separadas)
- NeoPixelBus como backend preferente, FastLED como alternativo
- Efectos configurables por JSON, segmentos virtuales
- API REST versionada y validada

Para detalles técnicos, ver [Architecture](./Architecture.md) y el changelog.