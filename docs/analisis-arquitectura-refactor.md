# Análisis de Arquitectura y Plan de Refactor

Documento generado a partir de la revisión completa del firmware `v0.3.7-beta`.

Cubre: driver LED, sistema de efectos, puntos de acoplamiento, flujo de frame, plan de refactor incremental por commits y mapa de riesgos.

---

## 1. Driver LED

### Clase base abstracta: `LedDriver`

**Archivos:** `firmware/src/drivers/LedDriver.h` / `LedDriver.cpp`

`LedDriver` es una clase base virtual con lógica compartida que define la API de alto nivel expuesta al resto del firmware.

| Método | Descripción |
|--------|-------------|
| `configure(GpioConfig&)` | Parsea el `GpioConfig` y rellena `outputs_[]` con `LedDriverOutputConfig` |
| `begin()` | Inicializa hardware (abstracto) |
| `show()` | Envía el frame al hardware (abstracto) |
| `setAllColor(color)` | Bucle sobre todas las salidas |
| `setOutputColor(outputIndex, color)` | Color sólido por salida |
| `setPixelColor(outputIndex, pixelIndex, color)` | Píxel individual |
| `supportsPerPixelColor(outputIndex)` | `true` solo para tipos addressable |
| `clear()` | Llama a `setAllColor(0)` |
| `scheduleShowLog()` | Flag de diagnóstico para el próximo `show()` |

**Datos internos:** `outputs_[kMaxLedOutputs]` (4 máximo, `Config.h:99`), `outputLevels_[4]`, `level_`, `logNextShow_`.

> **Acoplamiento conocido:** La base implementa `setPixelColor` como fallback que delega en `setOutputColor` (`LedDriver.cpp:134`). Si una subclase no sobreescribe `setPixelColor`, las llamadas con `pixelIndex` se silencian sin error.

### Selección de backend: `CurrentLedDriver.h`

**Archivo:** `firmware/src/drivers/CurrentLedDriver.h`

La selección es **100% compile-time** mediante `DUX_LED_BACKEND`:

| Valor | Backend | Clase |
|-------|---------|-------|
| `1` | NeoPixelBus | `LedDriverNeoPixelBus` |
| `2` | FastLED | `LedDriverFastLed` |
| `3` | Digital | `LedDriverDigital` |

El valor viene de `platformio.ini` en cada entorno (`build_flags`). Todos los `.cpp` de backends incluyen guards `#if DUX_LED_BACKEND == ...` internos, por lo que compilan en todos los targets pero son no-ops fuera del backend activo.

### Backend NeoPixelBus (principal)

**Archivo:** `firmware/src/drivers/LedDriverNeoPixelBus.cpp`

- Crea un objeto `NeoOutputBase*` por salida habilitada (hasta 4).
- Asigna canal RMT de ESP32 por índice de output: `Rmt0`, `Rmt1`, `Rmt2`, `Rmt3` (líneas 200-210).
- Soporta RGB y RGBW, con 8 órdenes de color vía templates de C++ (`NeoGrbFeature`, `NeoRgbwFeature`, etc.).
- `peakLedCounts_[]` (línea 29): mantiene el máximo `ledCount` histórico para dimensionar el bus y poder apagar LEDs físicos cuando se reduce la config.
- **Timing:** El bus usa DMA/RMT async. `canShow()` evita que `begin()` llame a `Show()` mientras hay una transferencia en curso (comentario en líneas 97-101).

### Backend FastLED

**Archivo:** `firmware/src/drivers/LedDriverFastLed.cpp`

- **Solo soporta una salida** (la que coincide con `DUX_LED_PIN` en compilación). Línea 94: `if (output.pin != BuildProfile::kLedPin)`.
- Usa dos globals de archivo: `CLEDController *gFastLedController` y `CRGB *gFastLedPixels` (líneas 16-17). Esto impide múltiples instancias y hace el estado no-reentrant.
- `FastLED.show()` es bloqueante durante la transmisión.

### Backend Digital

**Archivo:** `firmware/src/drivers/LedDriverDigital.cpp`

- Simple `digitalWrite(pin, HIGH/LOW)` basado en `outputLevel(i) > 0`.
- `show()` aplica el nivel a cada GPIO configurado como `digital`.

---

## 2. Sistema de Efectos

### Estructura de carpetas

```
firmware/src/effects/
├── EffectEngine.h / .cpp         ← Base abstracta + helpers matemáticos
├── EffectManager.h / .cpp        ← Orquestador, posee todas las instancias
├── EffectRegistry.h / .cpp       ← Catálogo estático de descriptores
├── EffectFixed.h / .cpp
├── EffectGradient.h / .cpp
├── EffectBlinkFixed.h / .cpp
├── EffectBlinkGradient.h / .cpp
├── EffectBreathFixed.h / .cpp
├── EffectBreathGradient.h / .cpp
├── EffectDiagnostic.h / .cpp
├── EffectTripleChase.h / .cpp
├── EffectGradientMeteor.h / .cpp
├── EffectScanningPulse.h / .cpp
├── EffectTrigRibbon.h / .cpp
├── EffectLavaFlow.h / .cpp
├── EffectPolarIce.h / .cpp
├── EffectStellarTwinkle.h / .cpp
├── EffectRandomColorPop.h / .cpp
├── EffectBouncingPhysics.h / .cpp
└── EffectAudioPulse.h / .cpp
```

### `EffectEngine` — base abstracta y helpers

**Archivos:** `firmware/src/effects/EffectEngine.h` / `EffectEngine.cpp`

Mantiene referencias protegidas a `CoreState&` y `LedDriver&`. Expone dos métodos abstractos:

- `supports(uint8_t effectId)` — identifica qué ID gestiona este efecto.
- `renderFrame()` — lógica de render del frame actual.

Y un ciclo de vida con implementaciones vacías por defecto:

- `onActivate()` / `onDeactivate()`

**Helpers matemáticos disponibles para todos los efectos:**

| Helper | Descripción |
|--------|-------------|
| `normalizedX(pixel, count)` | Posición de píxel en `[0.0..1.0]` |
| `normalizedTimeSec()` | `millis() / 1000.0f` |
| `clamp01(v)` | Sujeta a `[0.0..1.0]` |
| `smoothstep(edge0, edge1, x)` | Interpolación cúbica suave |
| `lerpColor(cA, cB, t)` | Interpolación lineal RGB |
| `addColor(cA, cB)` | Suma aditiva con saturación en 255 |
| `scaleColor(color, brightness)` | Escala uint8 con `reactiveGain` |
| `scaleColorFloat(color, gain)` | Escala float con `reactiveGain` |
| `gradientColor(cA, cB, cC, px, n)` | Gradiente de 3 colores por posición |
| `applyGamma(color)` | Corrección gamma 2.2 con `powf` |
| `audioColorShift(cA, cB, cC)` | Desplazamiento de color por nivel de audio |
| `effectIntervalMs(speedScale)` | Mapea velocidad 1-100 a ~1200-40 ms |
| `speed01(speedScale)` | Normaliza 1-100 a `[0.0..1.0]` |
| `level01(levelScale)` | Normaliza 1-10 a `[0.0..1.0]` |
| `reactiveAudio01()` | `audioLevel/255` si reactivo, sino `1.0f` |
| `reactiveGain()` | Curva `powf(audio, 0.55)` en rango `[0.20..2.40]` |
| `resolveSectionSize(ledCount, sections)` | Tamaño de sección con redondeo hacia arriba |

### `EffectRegistry` — catálogo

**Archivos:** `firmware/src/effects/EffectRegistry.h` / `EffectRegistry.cpp`

`EffectDescriptor` contiene: `id`, `key`, `label`, `description`, `usesSpeed`, `usesAudio`.

El catálogo es un array estático de 17 entradas (sin heap). La selección es por ID numérico (0-16) o por string key, con aliases en español (líneas 80-123 de `EffectRegistry.cpp`).

`usesAudio` determina si `CoreState::reactiveToAudio` se activa automáticamente al seleccionar el efecto (ver `CoreState.cpp:230`).

| ID | Key | Audio |
|----|-----|-------|
| 0 | `fixed` | no |
| 1 | `gradient` | no |
| 2 | `diagnostic` | no |
| 3 | `blink_fixed` | no |
| 4 | `blink_gradient` | no |
| 5 | `breath_fixed` | no |
| 6 | `breath_gradient` | no |
| 7 | `triple_chase` | no |
| 8 | `gradient_meteor` | no |
| 9 | `scanning_pulse` | no |
| 10 | `trig_ribbon` | **sí** |
| 11 | `lava_flow` | no |
| 12 | `polar_ice` | no |
| 13 | `stellar_twinkle` | **sí** |
| 14 | `random_color_pop` | no |
| 15 | `bouncing_physics` | **sí** |
| 16 | `audio_pulse` | **sí** |

### `EffectManager` — orquestador

**Archivos:** `firmware/src/effects/EffectManager.h` / `EffectManager.cpp`

- Posee **todas las instancias como miembros valor** (sin heap para efectos).
- El array `effects_[17]` contiene punteros a esos miembros, asignados en el constructor.
- `resolveActiveEffect()` hace búsqueda lineal O(n) en `effects_[]` por `supports(state_.effectId)`.
- **Lifecycle:** Detecta `state_.effectId != lastEffectId_`, llama `onDeactivate()` al efecto saliente, limpia LEDs, llama `onActivate()` al nuevo.
- `lastEffectId_` se inicializa a `255` para forzar la activación en el primer frame.

### Parámetros disponibles para efectos (vía `CoreState`)

| Campo | Rango | Uso |
|-------|-------|-----|
| `brightness` | 0-255 | Escala global de color |
| `effectSpeed` | 1-100 | Velocidad temporal |
| `effectLevel` | 1-10 | Intensidad/agresividad estructural |
| `sectionCount` | 1-10 | Bloques, repeticiones o densidad |
| `primaryColors[3]` | RGB×3 | Paleta de trabajo |
| `backgroundColor` | RGB | Color de fondo |
| `reactiveToAudio` | bool | Derivado automáticamente del efecto activo |
| `audioLevel` | 0-255 | RMS suavizado del micrófono (AGC) |
| `audioPeakHold` | 0-255 | Pico con decaimiento (P6) |
| `beatDetected` | bool | Pulso de beat (~50 ms, P2) |

### Estado interno en efectos

Solo algunos efectos mantienen estado entre frames:

| Efecto | Estado |
|--------|--------|
| `EffectBlinkFixed` | `visible_`, `lastToggleAtMs_` |
| `EffectBlinkGradient` | `visible_`, `lastToggleAtMs_` |
| `EffectBreathFixed` | fase senoidal |
| `EffectBreathGradient` | fase senoidal |
| `EffectAudioPulse` | `beatFlashMs_` |

El resto usa `millis()` directamente como fuente de tiempo sin estado persistente entre frames.

---

## 3. Puntos de Acoplamiento y Complejidad

### 3.1 `EffectManager.h` incluye todos los efectos (fan-in)

**Archivo:** `firmware/src/effects/EffectManager.h`, líneas 12-29

```cpp
#include "effects/EffectBlinkFixed.h"
#include "effects/EffectBlinkGradient.h"
// ... 15 más
```

Cualquier cambio en cualquier efecto provoca la recompilación de `EffectManager.cpp` completo. Añadir un efecto nuevo requiere editar exactamente **5 archivos**: `EffectManager.h` (include + miembro), `EffectManager.cpp` (inicialización + asignación al array), `EffectRegistry.cpp` (descriptor), `EffectRegistry.h` (constante), `CoreState.h` (re-export de constante).

### 3.2 `CoreState.h` re-exporta constantes de `EffectRegistry`

**Archivo:** `firmware/src/core/CoreState.h`, líneas 18-34

```cpp
static constexpr uint8_t kEffectFixed = EffectRegistry::kEffectFixed;
// ... 16 más
```

`CoreState.h` incluye `EffectRegistry.h`. Toda unidad de compilación que incluya `CoreState.h` (prácticamente todas) arrastra también `EffectRegistry.h` aunque no lo necesite.

### 3.3 Efectos acceden directamente a `LedDriver` (sin framebuffer)

Todos los efectos llaman `driver().setPixelColor(outIdx, px, color)` y `driver().show()` directamente desde `renderFrame()`. No hay capa de framebuffer intermedia. Esto impide:

- Preview o simulación sin hardware real.
- Post-procesado global (gamma unificado, brightness hardware).
- Tests unitarios de efectos sin un driver completo.

### 3.4 `AudioService` escribe en `CoreState` sin mutex en dos rutas

**Archivo:** `firmware/src/services/AudioService.cpp`

```cpp
// Línea 180 — SIN mutex:
coreState_.beatDetected = false;

// Líneas 287-290 — fallback sin mutex si lock falla:
} else {
    coreState_.audioLevel    = audioLevel_;
    coreState_.audioPeakHold = peakHold_;
}
```

Estas escrituras ocurren desde `controlTask` (core0) mientras `renderTask` (core1) puede leer `CoreState` concurrentemente. El fallback en particular escribe sin protección si el semáforo está tomado.

### 3.5 `LedDriverFastLed` usa globals de archivo

**Archivo:** `firmware/src/drivers/LedDriverFastLed.cpp`, líneas 16-17

```cpp
CLEDController *gFastLedController = nullptr;
CRGB           *gFastLedPixels     = nullptr;
```

Solo puede existir una instancia activa. La memoria se gestiona con `new`/`delete[]` sin smart pointers ni verificación de transferencias RMT en curso.

### 3.6 Duplicación de helpers de color

La función `parseHexColor` / `parseColorValue` / `formatHexColor` está duplicada textualmente en:

- `firmware/src/core/CoreState.cpp` (líneas 15-70)
- `firmware/src/services/EffectPersistenceService.cpp` (líneas 16-66)

### 3.7 `EffectBlinkFixed` duplica `resolveSectionSize`

**Archivo:** `firmware/src/effects/EffectBlinkFixed.cpp`, líneas 11-14

Define `resolveSectionSizeValue` como función local que es idéntica al helper `resolveSectionSize` ya disponible como `protected` en `EffectEngine`. No usa el método heredado.

### 3.8 Mezcla de referencias a IDs de efectos

Los `supports()` de algunos efectos usan `CoreState::kEffectFixed` y otros usan `EffectRegistry::kEffectTripleChase` sin consistencia. Ambas son el mismo valor pero la referencia debería ser siempre desde `EffectRegistry`.

---

## 4. Flujo de un Frame

```
┌─────────────────────────────────────────────────────────────────────┐
│                     CONTROL TASK (core0, cada 10 ms)                │
│                                                                      │
│  audioService.handle()                                               │
│    → processAudioBuffer()                                            │
│    → coreState_.audioLevel = ...   (con mutex, o race si falla)     │
│                                                                      │
│  apiService.handle()                                                 │
│    → state_.applyPatchJson()                                         │
│    → state_.effectId = ...          (con mutex)                      │
│    → state_.reactiveToAudio = EffectRegistry::usesAudio(effectId)   │
└──────────────────────────────────┬──────────────────────────────────┘
                                   │ CoreState (mutex FreeRTOS)
┌──────────────────────────────────▼──────────────────────────────────┐
│                     RENDER TASK (core1, cada 16 ms)                 │
│                                                                      │
│  EffectManager::renderFrame()                                        │
│    1. state_.lock()         ← toma el mutex de CoreState            │
│    2. driver_.isInitialized() ? ok : driver_.begin()                │
│    3. if (!state_.power)                                             │
│         driver_.clear(); driver_.show(); unlock; return              │
│    4. resolveActiveEffect()                                          │
│         → búsqueda lineal en effects_[17] por supports(effectId)    │
│    5. if (effectId != lastEffectId_)                                 │
│         efectoAnterior.onDeactivate()                                │
│         driver_.clear(); driver_.show()                              │
│         driver_.scheduleShowLog()                                    │
│         efectoNuevo.onActivate()                                     │
│         lastEffectId_ = effectId                                     │
│    6. activeEffect.renderFrame()                                     │
│         ┌────────────────────────────────────────────┐              │
│         │ Lee state(): brightness, colors, speed, …   │              │
│         │ Por cada outputIndex 0..outputCount-1:      │              │
│         │   if !supportsPerPixelColor:                │              │
│         │     driver.setOutputColor(outIdx, color)    │              │
│         │   else:                                     │              │
│         │     for px in 0..ledCount:                  │              │
│         │       driver.setPixelColor(outIdx, px, col) │              │
│         │         (NeoPixelBus: escribe en buffer RAM) │             │
│         │ driver.show()                               │              │
│         │   (NeoPixelBus: lanza DMA/RMT async)        │              │
│         └────────────────────────────────────────────┘              │
│    7. state_.unlock()                                                │
└──────────────────────────────────────────────────────────────────────┘
```

**Timing real:** `vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(16))` → ~62,5 FPS target. El mutex puede añadir latencia variable si `controlTask` y `renderTask` compiten al mismo tiempo. El `NeoPixelBus::show()` es async (RMT DMA), por lo que retorna antes de que los datos lleguen al LED físico.

**Sin framebuffer:** El efecto escribe píxel a píxel directamente en el buffer interno de NeoPixelBus. No hay etapa de post-procesado entre el efecto y el `show()`.

---

## 5. Plan de Refactor Incremental

Cada fase es un conjunto de commits atómicos. El firmware debe compilar y funcionar correctamente al final de cada commit.

### Fase A — Saneamiento interno (sin cambios de interfaz pública)

**A1. Extraer helpers de color a `ColorUtils.h`**

- Crear `firmware/src/core/ColorUtils.h` con `parseHexColor()`, `parseColorValue()`, `formatHexColor()`.
- Eliminar duplicados de `CoreState.cpp` y `EffectPersistenceService.cpp`.
- Sin cambio de interfaz pública.

**A2. `EffectBlinkFixed` → usar `resolveSectionSize` heredado**

- Eliminar `resolveSectionSizeValue` local (`EffectBlinkFixed.cpp:11-14`).
- Llamar directamente al método `protected` `resolveSectionSize(ledCount, sectionCount)`.

**A3. Unificar constantes de ID de efectos en `supports()`**

- Todos los `supports()` deben referenciar `EffectRegistry::kEffectXxx` (no `CoreState::kEffectXxx`).
- Comprobar con `grep` que no queda ningún uso de `CoreState::kEffectXxx` internamente.

**A4. Corregir race condition de `AudioService`**

- `beatDetected = false` (línea 180) debe ejecutarse dentro del bloque con mutex.
- Eliminar el fallback de escritura sin mutex (líneas 287-290); si el semáforo no está disponible, omitir la actualización de ese ciclo (el valor anterior es aceptable a 20 Hz).

### Fase B — Desacoplamiento de `CoreState` del catálogo de efectos

**B1. Eliminar re-exports de constantes en `CoreState.h`**

- Reemplazar todos los usos de `CoreState::kEffectXxx` en el codebase por `EffectRegistry::kEffectXxx`.
- Eliminar las líneas 18-34 de `CoreState.h` (los 17 `static constexpr` re-exportados).
- Eliminar el `#include "effects/EffectRegistry.h"` de `CoreState.h`.

**B2. Desacoplar `CoreState.h` de `PaletteRegistry.h`**

- `CoreState` solo necesita `int16_t paletteId` y `uint32_t primaryColors[3]` como campos.
- `PaletteRegistry.h` puede quedar como include solo en `CoreState.cpp` (unidad de compilación).

### Fase C — Framebuffer intermedio (habilita post-procesado y tests)

**C1. Introducir `LedFrameBuffer`**

- Crear `firmware/src/drivers/LedFrameBuffer.h/.cpp`.
- Los efectos escriben al framebuffer en lugar de llamar directamente a `driver().setPixelColor()`.
- `EffectEngine` expone `setPixelColor(outIdx, px, color)` que redirige al framebuffer local.
- Al final de `renderFrame()`, el framebuffer vuelca al driver mediante `flush(LedDriver&)`.
- Esto permite tests unitarios de efectos sin driver real.

**C2. Centralizar gamma correction en el flush**

- `applyGamma()` actualmente se llama de forma inconsistente dentro de efectos individuales.
- Aplicar gamma de forma uniforme en `flush()` del framebuffer.
- Eliminar llamadas dispersas a `applyGamma()` dentro de cada efecto.

### Fase D — Reducir fan-in de `EffectManager`

**D1. Mover includes de efectos de `.h` a `.cpp`**

- `EffectManager.h` actualmente incluye los 17 headers de efectos.
- Conservar los miembros como forward declarations o moverlos a `EffectManager.cpp`.
- Resultado: cambios en un efecto solo recompilan `EffectManager.cpp`, no todo lo que incluye `EffectManager.h`.

**D2. Separar presentación de `EffectRegistry`**

- `toJsonArray()` y `buildHtmlOptions()` (en `EffectRegistry.cpp`) dependen de `ArduinoJson` y generan HTML.
- Mover estos helpers a `ApiService` o a un helper de presentación específico.
- `EffectRegistry` queda como catálogo puro de metadatos: IDs, keys, labels, flags.

### Fase E — Encapsular estado de `LedDriverFastLed`

**E1. Eliminar globals de archivo en `LedDriverFastLed`**

- Mover `gFastLedController` y `gFastLedPixels` como miembros privados de `LedDriverFastLed`.
- Gestionar lifetime con RAII o destructor explícito equivalente al de `LedDriverNeoPixelBus`.

---

## 6. Interfaces Sugeridas tras el Refactor

```
┌─────────────────────────────────────┐
│           EffectEngine              │  ← escribe a FrameBuffer (no al driver)
│  renderFrame(), onActivate(),       │
│  onDeactivate(), supports()         │
└──────────────────┬──────────────────┘
                   │ setPixelColor / setOutputColor
┌──────────────────▼──────────────────┐
│           LedFrameBuffer            │  ← array de pixels por output en RAM
│  setPixelColor(), setOutputColor()  │
│  flush(LedDriver&)                  │  ← gamma + volcado al driver aquí
└──────────────────┬──────────────────┘
                   │ setPixelColor / show
┌──────────────────▼──────────────────┐
│             LedDriver               │  ← sin cambio de interfaz pública
│  (NeoPixelBus / FastLED / Digital)  │
└─────────────────────────────────────┘
```

### Archivos a crear, mover o renombrar

| Acción | Archivo actual | Destino / Notas |
|--------|---------------|-----------------|
| Crear | — | `firmware/src/core/ColorUtils.h` |
| Crear | — | `firmware/src/drivers/LedFrameBuffer.h/.cpp` |
| Dividir | `effects/EffectRegistry.cpp` | Extraer `toJsonArray()`/`buildHtmlOptions()` a `ApiService` |
| Limpiar | `core/CoreState.h` | Eliminar re-exports de IDs y el include de `EffectRegistry.h` |
| Reorganizar | `effects/EffectManager.h` | Mover los 17 includes de efectos al `.cpp` |

---

## 7. Riesgos y Mitigaciones

### R1. Performance — `applyGamma` con `powf` por píxel

**Riesgo:** `EffectEngine::applyGamma()` usa `powf(v/255, 2.2)`, operación de punto flotante costosa. Si se centraliza en el flush del framebuffer y se aplica a todos los píxeles en cada frame (hasta 4 × 300 = 1 200 píxeles), puede consumir más del budget de 16 ms.

**Mitigación:** Sustituir `powf` por una LUT precalculada de 256 `uint8_t`. La LUT se calcula una vez en `setup()` y consume solo 256 bytes de RAM.

### R2. Memoria — `LedFrameBuffer` con arrays estáticos

**Riesgo:** Si el framebuffer se define como `uint32_t pixels[4][kMaxLedCount]` con `kMaxLedCount` fijo grande, desperdicia RAM en configuraciones con pocas luces. En ESP32 la SRAM es ~320 KB y es fragmentable.

**Mitigación:** Dimensionar el buffer por el `ledCount` real de la config activa, o asignar dinámicamente en `configure()` con tamaño máximo conocido. Limitar siempre a `kMaxLedOutputs × max(ledCount)`.

### R3. Compatibilidad con MCU — `LedDriverFastLed` con pin en template

**Riesgo:** `LedDriverFastLed` usa `DUX_LED_PIN` como parámetro de template (`addFastLedController<TChipset, DUX_LED_PIN, ...>`). Si se refactoriza para aceptar pin dinámico, la API de FastLED no lo soporta directamente.

**Mitigación:** Documentar la limitación y mantener el approach actual para FastLED. Solo `NeoPixelBus` soportará verdadero multi-output runtime.

### R4. Concurrencia — Mutex de `CoreState` retenido durante todo el render

**Riesgo:** `EffectManager::renderFrame()` toma `CoreState::lock()` durante todo el render, incluyendo los `setPixelColor` de hasta 1 200 píxeles. Si `controlTask` intenta el mutex durante ese tiempo, se bloquea hasta 16 ms.

**Mitigación:** Hacer snapshot de `CoreState` al inicio del frame (ya existe `CoreState::snapshot()`), liberar el mutex inmediatamente, y renderizar sobre la copia. Esto reduce la ventana crítica a microsegundos.

### R5. Concurrencia — `AudioService` escribe en `CoreState` sin mutex

**Riesgo:** `coreState_.beatDetected = false` (línea 180 de `AudioService.cpp`) y el fallback de escritura sin mutex (líneas 287-290) son accesos no-atómicos desde `controlTask` mientras `renderTask` puede estar leyendo los mismos campos desde core1.

**Mitigación a corto plazo:** Eliminar el fallback; si no se logra el mutex, omitir la actualización de ese ciclo. A 20 Hz de actualización de audio, el valor anterior es aceptable. La escritura de `beatDetected = false` debe hacerse también dentro del bloque protegido.

### R6. Compatibilidad — Eliminación de `CoreState::kEffectXxx`

**Riesgo:** Si alguna unidad de compilación (o test futuro) depende de `CoreState::kEffectFixed`, el cambio rompe compilación.

**Mitigación:** Hacer el cambio en un commit atómico con búsqueda exhaustiva por `grep`. El compilador detecta todos los usos no actualizados en tiempo de compilación.

### R7. Doble buffer con `NeoPixelBus`

**Riesgo:** `NeoPixelBus` ya tiene su propio buffer de pixels en RAM. Introducir `LedFrameBuffer` añade un segundo buffer de igual tamaño, duplicando el uso de RAM para pixels.

**Mitigación:** Evaluar si el framebuffer es solo necesario para simulación y tests. En producción, si no se añade post-procesado global, se puede omitir y mantener el write-directo al driver para preservar RAM.

---

## 8. Checklist Priorizado de Acción

### Alta prioridad (bugs y corrección de comportamiento)

- [ ] **R5** Corregir race condition en `AudioService.cpp` líneas 180 y 287-290
- [ ] **R1** Sustituir `applyGamma` con `powf` por LUT de 256 bytes

### Media prioridad (deuda técnica, mantenibilidad)

- [ ] **A1** Extraer `ColorUtils.h` (eliminar duplicación de `parseHexColor`)
- [ ] **A2** `EffectBlinkFixed` → usar `resolveSectionSize` heredado de `EffectEngine`
- [ ] **A3** Unificar uso de `EffectRegistry::kEffectXxx` en todos los `supports()`
- [ ] **B1** Desacoplar `CoreState.h` de `EffectRegistry.h`
- [ ] **B2** Desacoplar `CoreState.h` de `PaletteRegistry.h`
- [ ] **E1** Encapsular globals de `LedDriverFastLed` en miembros de clase

### Baja prioridad (arquitectura futura)

- [ ] **R4** Snapshot de `CoreState` al inicio del frame para reducir contención de mutex
- [ ] **C1** Introducir `LedFrameBuffer` entre efectos y driver
- [ ] **C2** Centralizar gamma correction en el flush del framebuffer
- [ ] **D1** Mover includes de efectos de `EffectManager.h` a `EffectManager.cpp`
- [ ] **D2** Separar helpers de presentación JSON/HTML de `EffectRegistry`
- [ ] **R6** Migrar todos los usos de `CoreState::kEffectXxx` a `EffectRegistry::kEffectXxx`

---

## Referencia Rápida de Archivos

| Archivo | Propósito |
|---------|-----------|
| `firmware/src/main.cpp` | Wiring de servicios, FreeRTOS tasks, `setup()` / `loop()` |
| `firmware/src/drivers/LedDriver.h/.cpp` | Clase base abstracta del driver |
| `firmware/src/drivers/CurrentLedDriver.h` | Selección compile-time de backend |
| `firmware/src/drivers/LedDriverNeoPixelBus.cpp` | Backend principal (RMT async, multi-output) |
| `firmware/src/drivers/LedDriverFastLed.cpp` | Backend alternativo (globals, single output) |
| `firmware/src/drivers/LedDriverDigital.cpp` | Backend GPIO on/off |
| `firmware/src/core/CoreState.h/.cpp` | Estado runtime compartido + mutex FreeRTOS |
| `firmware/src/core/BuildProfile.h` | Constantes de compilación (`DUX_LED_BACKEND`, `DUX_LED_PIN`, …) |
| `firmware/src/core/Config.h` | Structs de configuración (`GpioConfig`, `kMaxLedOutputs`) |
| `firmware/src/effects/EffectEngine.h/.cpp` | Base de efectos + todos los helpers matemáticos |
| `firmware/src/effects/EffectManager.h/.cpp` | Orquestador, instancias de efectos, lifecycle |
| `firmware/src/effects/EffectRegistry.h/.cpp` | Catálogo estático de 17 efectos |
| `firmware/src/services/AudioService.h/.cpp` | I2S, AGC, beat detection, escribe a `CoreState` |
| `firmware/src/services/EffectPersistenceService.cpp` | Persistencia de efectos y secuencias |
| `platformio.ini` | Perfiles de build y selección de backend |
