# Sync Test Plan

Estado: pendiente de ejecucion en hardware real.

Este documento define las pruebas manuales para validar la implementacion actual de sincronizacion en firmware.
No implica trabajo adicional de desarrollo; sirve para verificar en un momento posterior lo ya integrado en S1-S6.

## Alcance

- S1: base de configuracion, API y metricas de sync.
- S2: ingesta DDP con doble buffer y fallback por timeout.
- S3: fallback E1.31 con normalizacion al mismo pipeline de render.
- S4: cluster `server/client` con `SyncState v1`, `sequence` monotona y anti-replay.
- S5: sincronizacion de fase y clock para efectos periodicos soportados por el reloj global.
- S6: observabilidad de sync y hardening basico ante timeout, saturacion y drift excesivo.

## Fuera de alcance

- Ensayos de estres prolongados de mas de 1 hora.

## Topologias a probar

### Topologia A - LedFx a un nodo

- 1 controlador ESP32.
- 1 PC con LedFx.
- Misma red WiFi.
- Tira LED o carga equivalente.

### Topologia B - Server y client

- 2 controladores ESP32.
- 1 configurado como `server`.
- 1 configurado como `client`.
- Misma red WiFi.
- 1 tira LED por nodo.

### Topologia C - Server y dos clients

- 3 controladores ESP32.
- 1 `server` y 2 `client`.
- Misma red WiFi.
- `groupMask` comun para todos.

## Prerrequisitos

- Firmware compilado y cargado en los dispositivos.
- UI static disponible en LittleFS o fallback embebido.
- Acceso a estas rutas:
  - `/config/sync`
  - `/api/config/sync`
  - `/api/v1/sync/state`
  - `/api/v1/sync/connected`
- Red WiFi estable.
- Anotar version firmware, board, backend LED y numero de LEDs por salida.

## Datos a registrar en cada prueba

- Fecha y hora.
- Topologia usada.
- Board y backend LED.
- Configuracion `sync` aplicada.
- Resultado: `OK` o `FALLO`.
- Observaciones visuales: lag, flicker, cortes, perdida de color, desync.
- Respuesta de API relevante.

## Checklist rapido

- [ ] La UI de sync carga y persiste cambios.
- [ ] `GET /api/v1/sync/state` refleja el modo y metrica correctos.
- [ ] `GET /api/v1/sync/connected` cambia de estado cuando hay trafico real.
- [ ] DDP renderiza sin bloquear el efecto local al perder fuente.
- [ ] E1.31 renderiza con salida equivalente a DDP para el mismo patron base.
- [ ] Cluster sync replica estado del `server` en nodos `client`.
- [ ] Paquetes fuera de orden no pisan el estado actual del esclavo.
- [ ] Los efectos periodicos soportados no muestran desfase visible sostenido entre nodos.
- [ ] La UI y la consola API reflejan salud y degradacion reales del sistema.

## Orden recomendado de ejecucion

Objetivo: validar primero lo que desbloquea mas superficie funcional con el menor numero de dispositivos y con la menor ambiguedad de diagnostico.

### Bloque 1 - Smoke test de un solo nodo

Ejecutar en este orden:

1. `P1 - Persistencia de configuracion sync`
2. `P2 - DDP continuo`
3. `P3 - Fallback DDP por timeout`
4. `P13 - Observabilidad UI/API`

Salida esperada del bloque:

- El dispositivo guarda y recupera configuracion sync.
- El pipeline DDP funciona de forma estable.
- El fallback a render local es determinista.
- La telemetria base expone estado coherente.

Si este bloque falla:

- No continuar con cluster ni con E1.31 hasta corregirlo.
- Guardar siempre captura de `/api/v1/sync/state` antes y despues del fallo.

### Bloque 2 - Compatibilidad de protocolo externo

Ejecutar en este orden:

1. `P4 - E1.31 un universo`
2. `P5 - E1.31 multi-universo`
3. `P6 - Equivalencia DDP vs E1.31`

Salida esperada del bloque:

- El pipeline externo funciona con DDP y E1.31.
- La union entre universos no introduce artefactos visibles.
- La salida base es suficientemente equivalente entre ambos protocolos.

Si este bloque falla:

- Etiquetar claramente si el fallo es de parser, ensamblado o mapeo de color.
- No atribuir fallos de cluster o clock a E1.31 hasta tener este bloque estable.

### Bloque 3 - Cluster logico

Ejecutar en este orden:

1. `P7 - Cluster server -> client`
2. `P9 - Group mask`
3. `P10 - Reconexion del esclavo`
4. `P8 - Anti-replay en cluster`

Salida esperada del bloque:

- El `server` replica estado en `client` sin inconsistencias.
- El filtrado por grupo funciona.
- La resincronizacion tras reconexion es limpia.
- Tramas viejas no pisan el estado nuevo.

Si este bloque falla:

- Registrar `lastSequence`, `syncStateSent`, `syncStateApplied`, `sourceIp` y `packetsDropped`.
- No pasar a validacion de fase hasta tener sincronizacion logica estable.

### Bloque 4 - Fase y clock distribuido

Ejecutar en este orden:

1. `P11 - Sync de fase en efectos periodicos`
2. `P12 - Recuperacion de clock tras perdida temporal`

Salida esperada del bloque:

- No hay drift visible sostenido.
- La correccion de fase no introduce saltos bruscos.
- `clockOffsetMs` converge o se mantiene acotado.

Si este bloque falla:

- Registrar efecto usado, duracion, `clockOffsetMs`, `lastSyncEpochMs` y comportamiento visual.
- Repetir con al menos dos efectos distintos antes de concluir que el problema es global.

### Bloque 5 - Hardening y resiliencia

Ejecutar en este orden:

1. `P14 - Degradacion por timeout`
2. `P15 - Saturacion de polling`
3. Repetir `P2`, `P7` y `P11` en una sesion continua de 20-30 minutos si el tiempo lo permite.

Salida esperada del bloque:

- La degradacion es observable y coherente.
- El firmware no se bloquea ante trafico alto o perdida de fuente.
- No aparecen regresiones tras varios minutos de funcionamiento.

## Duracion minima recomendada

- Validacion minima util: Bloques 1 a 3.
- Validacion recomendada para cerrar sync: Bloques 1 a 5 completos.
- Validacion rapida si solo tienes una sesion corta:
  1. `P1`
  2. `P2`
  3. `P3`
  4. `P4`
  5. `P7`
  6. `P11`
  7. `P13`
  8. `P14`

## Casos de prueba

### P1 - Persistencia de configuracion sync

Objetivo: validar S1.

Pasos:

1. Abrir `/config/sync`.
2. Configurar `mode=ledfx_realtime`, `role=client`, `inputProtocol=ddp`, `ddpPort=4048`.
3. Guardar cambios.
4. Reiniciar el dispositivo.
5. Consultar `/api/v1/sync/state`.

Esperado:

- La configuracion persiste tras reinicio.
- `enabled=true`.
- `mode`, `role`, `inputProtocol` y puertos coinciden con lo configurado.

### P2 - DDP continuo

Objetivo: validar S2 en flujo normal.

Pasos:

1. Configurar LedFx para emitir DDP al dispositivo.
2. Ajustar `mode=ledfx_realtime`, `role=client`, `inputProtocol=ddp`.
3. Lanzar una escena continua de color/patron claro.
4. Observar durante 2 minutos.
5. Consultar `/api/v1/sync/state` cada 20-30 segundos.

Esperado:

- Render estable.
- `connected=true`.
- `packetsReceived` y `framesApplied` aumentan.
- `sourceIp` se rellena.
- Sin reinicios ni congelaciones.

### P3 - Fallback DDP por timeout

Objetivo: validar recuperacion automatica.

Pasos:

1. Mantener DDP activo y comprobar que el frame externo esta renderizando.
2. Cortar emision DDP desde LedFx.
3. Medir tiempo hasta volver al efecto local.
4. Consultar `/api/v1/sync/connected`.

Esperado:

- El render vuelve a `local_effects` sin reboot.
- `connected` pasa a `false` al expirar `sourceTimeoutMs`.
- No quedan colores residuales persistentes si el efecto local cambia.

### P4 - E1.31 un universo

Objetivo: validar S3 basico.

Pasos:

1. Configurar `mode=ledfx_realtime`, `role=client`, `inputProtocol=e131`.
2. Ajustar `e131UniverseStart=1`, `e131UniverseCount=1`.
3. Emitir `Multiple RGB` con start code 0 desde la herramienta elegida.
4. Usar un patron simple: rojo, verde, azul, blanco, chase.

Esperado:

- La salida visual responde al emisor.
- `packetsReceived` aumenta.
- No hay cambios de brillo o orden de color inesperados mas alla de la configuracion del strip.

### P5 - E1.31 multi-universo

Objetivo: validar ensamblado inicial de varios universos.

Pasos:

1. Configurar `e131UniverseStart=1`, `e131UniverseCount=2` o `3`.
2. Emitir un patron que cruce el limite de 170 LEDs.
3. Verificar el punto de union entre universos.

Esperado:

- No hay salto visual en el cambio de universo.
- El frame se ensambla sin bandas negras o colores desplazados.
- No aparece flicker sistematico en el borde entre universos.

### P6 - Equivalencia DDP vs E1.31

Objetivo: comprobar que ambos protocolos alimentan el mismo pipeline de render.

Pasos:

1. Preparar el mismo layout y mismo patron base.
2. Ejecutar primero por DDP y luego por E1.31.
3. Comparar visualmente color, continuidad y tiempo de reaccion.

Esperado:

- Misma respuesta base en ambos protocolos.
- Diferencias menores de latencia son aceptables si no rompen continuidad visual.

### P7 - Cluster server -> client

Objetivo: validar S4 basico.

Pasos:

1. Nodo A: `mode=cluster_sync`, `role=server`, `udpSyncPort=21324`, `groupMask=1`.
2. Nodo B: `mode=cluster_sync`, `role=client`, `udpSyncPort=21324`, `groupMask=1`.
3. Cambiar en A: power, brightness, effectId, effectSpeed, effectLevel, paletteId y colores.
4. Observar replicacion en B.
5. Consultar `/api/v1/sync/state` en B.

Esperado:

- B replica el estado de A en caliente.
- `syncStateApplied` y `lastSequence` aumentan en B.
- `syncStateSent` aumenta en A.
- `sourceIp` queda informado en B.

### P8 - Anti-replay en cluster

Objetivo: validar rechazo de secuencias atrasadas.

Pasos:

1. Con cluster funcionando, capturar o reinyectar un payload antiguo si se dispone de herramienta de replay.
2. Alternativamente, reiniciar temporalmente el emisor y observar que un estado anterior no vuelva a imponerse mientras exista una secuencia mas nueva valida.

Esperado:

- El cliente no revierte a un estado viejo.
- `packetsDropped` puede aumentar si entra trafico atrasado o invalido.

### P9 - Group mask

Objetivo: validar aislamiento entre grupos.

Pasos:

1. Nodo A `server` con `groupMask=1`.
2. Nodo B `client` con `groupMask=1`.
3. Nodo C `client` con `groupMask=2`.
4. Cambiar el estado en A.

Esperado:

- B sigue al `server`.
- C ignora los cambios.

### P10 - Reconexion del esclavo

Objetivo: validar resincronizacion en caliente.

Pasos:

1. Con cluster operativo, apagar o aislar el cliente de la red.
2. Cambiar varios parametros en el `server`.
3. Reincorporar el cliente.

Esperado:

- El cliente vuelve a alinearse con el snapshot emitido por el `server`.
- No requiere reinicio manual adicional del firmware.

### P11 - Sync de fase en efectos periodicos

Objetivo: validar S5 sobre efectos que usan el reloj global sincronizado.

Pasos:

1. Preparar un nodo `server` y un nodo `client` en `mode=cluster_sync`.
2. Seleccionar en el `server` un efecto periodico soportado por reloj global, por ejemplo `breath_fixed`, `breath_gradient`, `triple_chase`, `gradient_meteor`, `scanning_pulse`, `lava_flow` o `polar_ice`.
3. Ajustar `clockSmoothing=soft` en el cliente.
4. Observar ambos nodos durante al menos 2 minutos.
5. Consultar `/api/v1/sync/state` en el cliente y anotar `lastSyncEpochMs` y `clockOffsetMs`.

Esperado:

- El patron mantiene fase visual alineada o con desvio minimo no perceptible.
- No hay saltos bruscos durante la correccion.
- `clockOffsetMs` converge y varia suavemente.

### P12 - Recuperacion de clock tras perdida temporal

Objetivo: validar comportamiento del slew cuando la red introduce jitter o una interrupcion corta.

Pasos:

1. Con P11 estable, introducir una interrupcion breve de red en el cliente.
2. Restaurar conectividad.
3. Mantener observacion durante 1-2 minutos.

Esperado:

- El efecto recupera la fase sin salto brusco visible.
- El cliente no queda permanentemente desfasado tras volver el trafico.

### P13 - Observabilidad UI/API

Objetivo: validar S6 en la capa operativa.

Pasos:

1. Abrir `/config/sync` y `/api/config/sync`.
2. Activar un flujo real de sync: DDP, E1.31 o cluster.
3. Verificar que la UI muestra `health`, `activeProtocol`, `inputFps`, `framesApplied`, `lastSequence`, `clockOffsetMs`, `timeoutEvents` y `pollSaturationEvents`.
4. Confirmar que la consola API devuelve los mismos valores en `/api/v1/sync/state`.

Esperado:

- UI y API reflejan datos consistentes.
- Los contadores cambian en tiempo real cuando hay trafico.
- `activeProtocol` coincide con la fuente efectiva.

### P14 - Degradacion por timeout

Objetivo: validar la politica base de hardening.

Pasos:

1. Establecer sync operativo.
2. Cortar la fuente de datos.
3. Esperar a que expire `sourceTimeoutMs`.
4. Revisar `/api/v1/sync/state` y la UI.

Esperado:

- `timeoutEvents` aumenta.
- `degraded=true` tras perdida sostenida.
- `connected=false` y el sistema vuelve al fallback esperado.

### P15 - Saturacion de polling

Objetivo: vigilar comportamiento base ante rafagas o exceso de paquetes.

Pasos:

1. Si se dispone de herramienta de carga, enviar una rafaga alta de trafico sync.
2. Consultar `/api/v1/sync/state` durante la rafaga.

Esperado:

- `pollSaturationEvents` aumenta si el limite de polling por tick se alcanza.
- `degraded=true` cuando el sistema detecta saturacion.
- El firmware sigue operativo y no entra en bloqueo.

## Criterios de aceptacion global

- S1-S6 funcionan sin reinicio espontaneo ni bloqueo visible.
- Los endpoints de API reflejan el estado real del sistema.
- La transicion entre fuente externa y render local es determinista.
- El cluster replica el estado logico sin drift visible entre nodos para cambios discretos.
- Los efectos periodicos soportados mantienen fase visual alineada de forma sostenida.
- La observabilidad expone degradacion y salud de forma coherente para diagnostico.

## Checklist de cierre de sync

Usar esta lista solo cuando quieras decidir si la funcionalidad puede darse por cerrada a nivel de hardware.

### Cierre funcional minimo

- [ ] `P1` completada en al menos una board objetivo.
- [ ] `P2` y `P3` completadas con DDP real y fallback correcto.
- [ ] `P4` completada con E1.31 en un universo.
- [ ] `P7` completada con dos nodos.
- [ ] `P11` completada con al menos un efecto periodico soportado.
- [ ] `P13` completada y consistente con la salida visual.
- [ ] `P14` completada con `timeoutEvents` y `degraded` observables.

### Cierre recomendado

- [ ] `P5` completada para multi-universo E1.31 si el hardware lo requiere.
- [ ] `P8`, `P9` y `P10` completadas en cluster.
- [ ] `P12` completada sin saltos visuales bruscos.
- [ ] `P15` completada o sustituida por una rafaga de trafico razonablemente agresiva.
- [ ] Sesion continua de al menos 20 minutos sin reinicio espontaneo ni cuelgue.

### Evidencias que deberian quedar guardadas

- [ ] Captura de `/api/v1/sync/state` para DDP estable.
- [ ] Captura de `/api/v1/sync/state` para cluster estable.
- [ ] Captura de `/api/v1/sync/state` durante degradacion por timeout.
- [ ] Nota de board, backend LED, numero de LEDs y topologia usada.
- [ ] Lista breve de incidencias observadas, aunque no bloqueen el cierre.

### Regla de decision final

- Declarar sync como validado si el cierre funcional minimo esta completo y no hay bloqueos, reinicios espontaneos ni desalineacion grave persistente.
- Declarar sync como validado con observaciones si el cierre recomendado no esta completo pero los riesgos restantes estan acotados y documentados.
- No declarar sync como cerrado si fallan `P2`, `P3`, `P7`, `P11` o `P14`, porque cubren el nucleo del comportamiento esperado.

## Incidencias a vigilar

- WiFi con jitter alto o perdida puntual.
- `sourceIp` vacio pese a trafico real.
- `connected=true` sin salida visual real.
- Corte parcial entre universos E1.31.
- Desfase entre nodos al cambiar rapidamente muchos parametros.
- Reaparicion de estado antiguo en cluster.
- Correccion de fase demasiado agresiva o demasiado lenta.
- Telemetria inconsistente entre UI y API.
- `degraded` activado sin causa aparente o no activado ante fallo real.

## Plantilla de resultado

```md
Prueba: P7 - Cluster server -> client
Fecha: ____
Topologia: ____
Board: ____
Backend LED: ____
Config sync: ____
Resultado: OK / FALLO
Observaciones:
- ____
- ____
API state relevante:
```json
{}
```
```
