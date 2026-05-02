# Titulo sugerido

`v0.6.3-alpha - Stack completo de Sync S1-S6`

# Cuerpo para GitHub Release

DUXMAN LED-NEXT `0.6.3-alpha` completa la pila de sincronizacion planificada S1-S6 y alinea firmware, API, UI y documentacion con la arquitectura actual.

## Puntos destacados

- Pipeline completo de sincronizacion distribuida para nodos ESP32
- Entrada realtime de LedFx por `DDP` con fallback por timeout
- Fallback `E1.31/sACN` sobre el mismo pipeline de frame externo
- `cluster_sync` por UDP con secuencia monotona, anti-replay y filtrado por `groupMask`
- Reloj global sincronizado para efectos con correccion suave de drift
- Nueva UI y nuevas rutas API de Sync para configuracion y observabilidad

## Incluido en esta version

- `SyncConfig` persistente integrado en la configuracion del firmware
- `SyncService` runtime integrado en el lazo de control
- Nueva pagina UI: `/config/sync`
- Nueva consola API: `/api/config/sync`
- Nuevo endpoint: `GET /api/v1/sync/connected`
- Banner de estado en Home para nodos `client` conectados
- Documentacion actualizada en espanol e ingles

## Estado de validacion

- Build `esp32dev` verificado correctamente
- La validacion manual en hardware real multi-dispositivo sigue pendiente
- Guia recomendada de validacion: `wiki/Development/Sync-Test-Plan.md`

## Limitaciones conocidas

- Todavia no hay soporte OTA con el layout actual `huge_app`
- Sync aun requiere validacion real bajo jitter prolongado y varios dispositivos
- Las escrituras LittleFS estan diferidas, pero siguen ejecutandose de forma sincronica en la tarea de control

## Siguiente fase

- Validacion hardware de sync S1-S6
- Pruebas de resiliencia bajo condiciones reales de red
- Estrategia OTA y rollback
