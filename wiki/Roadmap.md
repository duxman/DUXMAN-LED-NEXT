# Roadmap y evolucion

## Estado actual

- Configuracion persistente de red, debug, microfono y GPIO/LEDs en LittleFS.
- API HTTP/Serial versionada (/api/v1/*).
- UI embebida basada en plantillas LittleFS (data/ui) con fallback en firmware.
- FreeRTOS dual-core estable para control y render.
- Catalogo de efectos visuales y audio-reactivos ampliado.
- Paletas de usuario con CRUD y perfiles completos aplicables en caliente.
- Hardening de /config/network y /config/all para robustez en cliente web.
- Audio reactivo afinado para menor latencia percibida.

## Siguientes fases

- LedFx realtime (DDP preferente, E1.31 fallback).
- Sincronizacion maestro-esclavo (SyncState v1, secuencia monotona).
- Sincronizacion de fase/clock para efectos distribuidos.
- OTA seguro con estrategia de rollback.

## Decisiones tecnicas clave

- Arquitectura no bloqueante con tareas separadas.
- NeoPixelBus como backend preferente y FastLED como alternativo.
- Efectos configurables desacoplados de la cantidad fisica de LEDs.
- API REST versionada y tolerante a payloads de cliente reales.

Para detalle tecnico, ver docs/roadmap.md y docs/architecture.md.
