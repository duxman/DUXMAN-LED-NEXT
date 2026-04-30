# Hoja De Ruta De Implementacion De Efectos

Este documento traduce el roadmap conceptual de efectos a una estrategia de implementacion progresiva sobre la arquitectura actual de `duxman-led-next`.

No define el detalle matematico de cada efecto; ese detalle ya vive en doc de roadmap conceptual. Aqui se fija como introducirlos sin romper la base actual de firmware.

## Objetivo

Implementar la nueva familia de efectos dinamicos en varias fases, preservando:

- compatibilidad con la UI Home actual
- persistencia del estado existente
- estabilidad de render en ESP32
- posibilidad de evolucionar despues a secuencia de arranque y scheduler de efectos

## Estado Actual Del Firmware

La arquitectura actual tiene estas caracteristicas:

- `CoreState` expone `power`, `brightness`, `effectId`, `sectionCount`, `effectSpeed`, `effectLevel`, `primaryColors[3]` y `backgroundColor`
- `EffectEngine` solo ofrece `begin()`, `supports()` y `renderFrame()`
- `EffectManager` resuelve el efecto activo mediante instancias fijas en compilacion
- el `loop()` principal renderiza a un intervalo fijo de 16 ms
- no existe hoy una capa comun de matematicas float, mezcla robusta o gamma correction
- no existe ciclo de vida formal de activacion/desactivacion de efectos

Conclusiones:

- los efectos estaticos actuales sirven como base, pero no bastan para una familia dinamica grande
- antes de implementar efectos con estado, conviene reforzar el motor

## Decisiones Acordadas

Estas decisiones quedan como base para las siguientes iteraciones:

1. No implementar los 11 efectos de golpe.
2. Hacer primero una fase de infraestructura del motor.
3. Mantener la UI actual al inicio, reutilizando los controles existentes.
4. Usar esta semantica inicial de parametros globales:
   - `brightness`: brillo final global
   - `effectSpeed`: velocidad temporal del efecto
   - `effectLevel`: intensidad estructural del efecto
   - `sectionCount`: numero de bloques, repeticiones o densidad segun el efecto
5. No mover aun el render a una task separada por core; primero validar rendimiento en el loop actual.
6. Introducir ciclo de vida de efectos antes de implementar los que requieren estado persistente.
7. Centralizar helpers de mezcla, gradiente, smoothstep, coordenadas normalizadas y gamma en la base del motor, no dentro de cada efecto.

## Cambios De Infraestructura Necesarios

### Fase 1. Base Del Motor

Objetivo: preparar una capa comun para efectos temporales y con floats.

Cambios previstos:

- ampliar `EffectEngine` con helpers comunes
- introducir tiempo normalizado y helpers de coordenada por pixel
- introducir mezcla RGB comun y suma aditiva con saturacion
- introducir `smoothstep`
- introducir escalado de color en float
- introducir tabla gamma comun

Helpers esperados:

- `normalizedX(pixelIndex, pixelCount)`
- `normalizedTimeSec()`
- `clamp01(value)`
- `smoothstep(edge0, edge1, x)`
- `lerpColor(colorA, colorB, t)`
- `addColor(colorA, colorB)`
- `scaleColorFloat(color, gain)`
- `applyGamma(color)`

### Fase 2. Ciclo De Vida De Efectos

Objetivo: permitir efectos con estado interno estable.

Cambios previstos:

- anadir `onActivate()`
- anadir `onDeactivate()`
- anadir opcionalmente `onStateChanged()`
- hacer que `EffectManager` detecte el cambio real de `effectId`
- resetear o resembrar estado cuando el efecto activo cambie

Esto es obligatorio antes de:

- `Bouncing Physics`
- `Stellar Twinkle`
- `Random Color Pop`
- cualquier efecto con arrays o entidades persistentes

### Fase 3. Escalado Del Catalogo

Objetivo: que el catalogo de efectos crezca sin tocar demasiados puntos del codigo.

Cambios previstos:

- limpiar la dependencia de indices manuales en `EffectManager`
- mantener `EffectRegistry` como catalogo funcional de ids, labels y metadatos
- permitir alta de nuevos efectos con el menor numero posible de cambios cruzados

## Orden Recomendado De Implementacion

### Grupo A. Efectos Sin Estado Persistente

Estos deben ir primero porque validan la base matematica sin introducir buffers persistentes:

1. `Triple Fixed Breathe`
2. `Global Gradient Breathe`
3. `Triple Chase`
4. `Gradient Meteor`
5. `Scanning Pulse`
6. `Trig-Ribbon`
7. `Lava Flow`
8. `Polar Ice`

### Grupo B. Efectos Con Estado Ligero O Persistente

Estos deben llegar despues de la fase de ciclo de vida:

9. `Stellar Twinkle`
10. `Random Color Pop`
11. `Bouncing Physics`

## Criterio De Avance

Cada efecto nuevo se considerara aceptable cuando cumpla:

- compila correctamente
- no rompe efectos ya existentes
- respeta `brightness`
- usa correctamente `effectSpeed`
- usa una semantica clara de `effectLevel`
- se comporta de forma estable al cambiar desde y hacia otros efectos
- mantiene una respuesta visual suficientemente fluida en hardware real
