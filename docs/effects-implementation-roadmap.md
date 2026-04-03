# Hoja De Ruta De Implementacion De Efectos

Este documento traduce el roadmap conceptual de efectos a una estrategia de implementacion progresiva sobre la arquitectura actual de `duxman-led-next`.

No define el detalle matematico de cada efecto; ese detalle ya vive en `docs/roadmap effects.md`. Aqui se fija como introducirlos sin romper la base actual de firmware.

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

## Justificacion Del Orden

- `Triple Fixed Breathe` y `Global Gradient Breathe` reutilizan casi toda la base actual
- `Triple Chase` valida desplazamiento temporal, repeticion y suavizado
- `Gradient Meteor` introduce cabeza y cola sin necesidad de estado por pixel
- `Scanning Pulse` reutiliza casi toda la estructura del meteor con rebote
- `Trig-Ribbon`, `Lava Flow` y `Polar Ice` prueban mejor la nueva capa float
- `Twinkle`, `Color Pop` y `Bouncing` son los que fuerzan de verdad el redisenio del ciclo de vida

## Semantica Inicial De Parametros Por Efecto

Para evitar abrir aun mas controles en la UI, se reutilizaran los parametros actuales con estas reglas iniciales.

### Parametros Globales

- `brightness`: intensidad final global
- `effectSpeed`: velocidad temporal o desplazamiento
- `effectLevel`: agresividad, anchura, cola, densidad o frecuencia segun el efecto
- `sectionCount`: numero de bloques o repeticiones cuando tenga sentido

### Propuesta Inicial Por Efecto

- `Triple Fixed Breathe`
  - `effectSpeed`: velocidad del fade
  - `effectLevel`: amplitud maxima del fade
  - `sectionCount`: numero de secciones visibles
- `Global Gradient Breathe`
  - `effectSpeed`: velocidad del fade
  - `effectLevel`: amplitud maxima del fade
- `Triple Chase`
  - `effectSpeed`: velocidad del tren
  - `effectLevel`: tamano relativo del vagon
  - `sectionCount`: numero de repeticiones
- `Gradient Meteor`
  - `effectSpeed`: velocidad de avance
  - `effectLevel`: longitud de cola
- `Scanning Pulse`
  - `effectSpeed`: velocidad de barrido
  - `effectLevel`: anchura del pulso
- `Trig-Ribbon`
  - `effectSpeed`: velocidad temporal
  - `effectLevel`: sharpness o frecuencia secundaria
  - `sectionCount`: repeticiones de patron
- `Lava Flow`
  - `effectSpeed`: velocidad de deformacion
  - `effectLevel`: amplitud de deformacion
- `Polar Ice`
  - `effectSpeed`: velocidad de interferencia
  - `effectLevel`: contraste del mix
- `Stellar Twinkle`
  - `effectSpeed`: velocidad de vida
  - `effectLevel`: spawn rate o densidad
- `Random Color Pop`
  - `effectSpeed`: velocidad de fade
  - `effectLevel`: frecuencia de aparicion
- `Bouncing Physics`
  - `effectSpeed`: energia del sistema
  - `effectLevel`: tamano o numero de bolas

Esta tabla no es definitiva. Se ajustara efecto a efecto segun pruebas visuales.

## Cambios Tecnicos Concretos Por Archivo

### `firmware/src/effects/EffectEngine.h` y `EffectEngine.cpp`

Anadir:

- helpers float y mezcla de color
- gamma correction comun
- utilidades de tiempo y coordenada normalizada

### `firmware/src/effects/EffectManager.h` y `EffectManager.cpp`

Anadir:

- deteccion de cambio de efecto activo
- ciclo de vida de activacion y desactivacion
- estructura mas mantenible para registrar instancias

### `firmware/src/effects/EffectRegistry.h` y `EffectRegistry.cpp`

Anadir:

- nuevos ids
- labels y descripciones
- metadata de uso de velocidad

### `firmware/src/core/CoreState.h` y `CoreState.cpp`

Mantener inicialmente el esquema actual.

Solo ampliar si algun efecto necesita parametros nuevos que no puedan mapearse bien a:

- `sectionCount`
- `effectSpeed`
- `effectLevel`

### Nuevas clases de efectos

Crear una clase por efecto nuevo para mantener aislamiento y facilitar pruebas.

## Restricciones Y Riesgos

### Rendimiento

- el objetivo visual es 60 FPS
- antes de mover render a otro core, medir primero en el loop actual
- si aparecen tirones con WiFi o WebServer, evaluar task pinneada despues

### Estado Interno

- no guardar arrays enormes sin necesidad
- si un efecto usa buffers por pixel, dimensionarlos por salida real
- resetear estado siempre al cambiar de efecto o al cambiar parametros estructurales

### Calidad Visual

- no duplicar funciones de mezcla entre efectos
- aplicar gamma de forma centralizada
- evitar reusar formulas distintas para el mismo concepto visual en dos efectos similares

## Fase 1 Recomendada Para Empezar

La primera iteracion recomendada es:

1. refactor de `EffectEngine`
2. soporte de activacion/desactivacion en `EffectManager`
3. implementacion de `Triple Fixed Breathe`
4. implementacion de `Global Gradient Breathe`
5. alta de ambos en `EffectRegistry`
6. validacion de build y prueba visual en placa

Motivo:

- es la forma mas barata de verificar la nueva base matematica
- reutiliza mucho codigo ya existente
- deja listo el camino para meteor, chase y pulse

## Fuera De Alcance Por Ahora

Queda explicitamente fuera de esta hoja de trabajo inicial:

- scheduler de secuencia de efectos al arranque
- editor avanzado de parametros por efecto en la UI
- migracion inmediata a task pinneada por core
- efectos 2D o matrices

## Criterio De Avance

Cada efecto nuevo se considerara aceptable cuando cumpla:

- compila en `esp32dev`
- no rompe efectos ya existentes
- respeta `brightness`
- usa correctamente `effectSpeed`
- usa una semantica clara de `effectLevel`
- se comporta de forma estable al cambiar desde y hacia otros efectos
- mantiene una respuesta visual suficientemente fluida en hardware real
