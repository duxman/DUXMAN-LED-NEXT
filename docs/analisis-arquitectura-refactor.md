# AnÃ¡lisis de Arquitectura y Plan de Refactor

Documento generado a partir de la revisiÃ³n completa del firmware `v0.3.8-beta`.

Cubre: driver LED, sistema de efectos, puntos de acoplamiento, flujo de frame, plan de refactor incremental por commits y mapa de riesgos.

---

## 1. Driver LED

### Clase base abstracta: `LedDriver`

**Archivos:** `firmware/src/drivers/LedDriver.h` / `LedDriver.cpp`

`LedDriver` es una clase base virtual con lÃ³gica compartida que define la API de alto nivel expuesta al resto del firmware.

| MÃ©todo | DescripciÃ³n |
|--------|-------------|
| `configure(GpioConfig&)` | Parsea el `GpioConfig` y rellena `outputs_[]` con `LedDriverOutputConfig` |
| `begin()` | Inicializa hardware (abstracto) |
| `show()` | EnvÃ­a el frame al hardware (abstracto) |
| `setAllColor(color)` | Bucle sobre todas las salidas |
| `setOutputColor(outputIndex, color)` | Color sÃ³lido por salida |
| `setPixelColor(outputIndex, pixelIndex, color)` | PÃ­xel individual |
| `supportsPerPixelColor(outputIndex)` | `true` solo para tipos addressable |
| `clear()` | Llama a `setAllColor(0)` |
| `scheduleShowLog()` | Flag de diagnÃ³stico para el prÃ³ximo `show()` |

**Datos internos:** `outputs_[kMaxLedOutputs]` (4 mÃ¡ximo, `Config.h:99`), `outputLevels_[4]`, `level_`, `logNextShow_`.

> **Acoplamiento conocido:** La base implementa `setPixelColor` como fallback que delega en `setOutputColor` (`LedDriver.cpp:134`). Si una subclase no sobreescribe `setPixelColor`, las llamadas con `pixelIndex` se silencian sin error.

### SelecciÃ³n de backend: `CurrentLedDriver.h`

**Archivo:** `firmware/src/drivers/CurrentLedDriver.h`

La selecciÃ³n es **100% compile-time** mediante `DUX_LED_BACKEND`:

| Valor | Backend | Clase |
|-------|---------|-------|
| `1` | NeoPixelBus | `LedDriverNeoPixelBus` |
| `2` | FastLED | `LedDriverFastLed` |
| `3` | Digital | `LedDriverDigital` |

El valor viene de `platformio.ini` en cada entorno (`build_flags`). Todos los `.cpp` de backends incluyen guards `#if DUX_LED_BACKEND == ...` internos, por lo que compilan en todos los targets pero son no-ops fuera del backend activo.

### Backend NeoPixelBus (principal)

**Archivo:** `firmware/src/drivers/LedDriverNeoPixelBus.cpp`

- Crea un objeto `NeoOutputBase*` por salida habilitada (hasta 4).
- Asigna canal RMT de ESP32 por Ã­ndice de output: `Rmt0`, `Rmt1`, `Rmt2`, `Rmt3` (lÃ­neas 200-210).
- Soporta RGB y RGBW, con 8 Ã³rdenes de color vÃ­a templates de C++ (`NeoGrbFeature`, `NeoRgbwFeature`, etc.).
- `peakLedCounts_[]` (lÃ­nea 29): mantiene el mÃ¡ximo `ledCount` histÃ³rico para dimensionar el bus y poder apagar LEDs fÃ­sicos cuando se reduce la config.
- **Timing:** El bus usa DMA/RMT async. `canShow()` evita que `begin()` llame a `Show()` mientras hay una transferencia en curso (comentario en lÃ­neas 97-101).

### Backend FastLED

**Archivo:** `firmware/src/drivers/LedDriverFastLed.cpp`

- **Solo soporta una salida** (la que coincide con `DUX_LED_PIN` en compilaciÃ³n). LÃ­nea 94: `if (output.pin != BuildProfile::kLedPin)`.
- Usa dos globals de archivo: `CLEDController *gFastLedController` y `CRGB *gFastLedPixels` (lÃ­neas 16-17). Esto impide mÃºltiples instancias y hace el estado no-reentrant.
- `FastLED.show()` es bloqueante durante la transmisiÃ³n.

### Backend Digital

**Archivo:** `firmware/src/drivers/LedDriverDigital.cpp`

- Simple `digitalWrite(pin, HIGH/LOW)` basado en `outputLevel(i) > 0`.
- `show()` aplica el nivel a cada GPIO configurado como `digital`.

---

## 2. Sistema de Efectos

### Estructura de carpetas

```
firmware/src/effects/
â”œâ”€â”€ EffectEngine.h / .cpp         â† Base abstracta + helpers matemÃ¡ticos
â”œâ”€â”€ EffectManager.h / .cpp        â† Orquestador, posee todas las instancias
â”œâ”€â”€ EffectRegistry.h / .cpp       â† CatÃ¡logo estÃ¡tico de descriptores
â”œâ”€â”€ EffectFixed.h / .cpp
â”œâ”€â”€ EffectGradient.h / .cpp
â”œâ”€â”€ EffectBlinkFixed.h / .cpp
â”œâ”€â”€ EffectBlinkGradient.h / .cpp
â”œâ”€â”€ EffectBreathFixed.h / .cpp
â”œâ”€â”€ EffectBreathGradient.h / .cpp
â”œâ”€â”€ EffectDiagnostic.h / .cpp
â”œâ”€â”€ EffectTripleChase.h / .cpp
â”œâ”€â”€ EffectGradientMeteor.h / .cpp
â”œâ”€â”€ EffectScanningPulse.h / .cpp
â”œâ”€â”€ EffectTrigRibbon.h / .cpp
â”œâ”€â”€ EffectLavaFlow.h / .cpp
â”œâ”€â”€ EffectPolarIce.h / .cpp
â”œâ”€â”€ EffectStellarTwinkle.h / .cpp
â”œâ”€â”€ EffectRandomColorPop.h / .cpp
â”œâ”€â”€ EffectBouncingPhysics.h / .cpp
â””â”€â”€ EffectAudioPulse.h / .cpp
```

### `EffectEngine` â€” base abstracta y helpers

**Archivos:** `firmware/src/effects/EffectEngine.h` / `EffectEngine.cpp`

Mantiene referencias protegidas a `CoreState&` y `LedDriver&`. Expone dos mÃ©todos abstractos:

- `supports(uint8_t effectId)` â€” identifica quÃ© ID gestiona este efecto.
- `renderFrame()` â€” lÃ³gica de render del frame actual.

Y un ciclo de vida con implementaciones vacÃ­as por defecto:

- `onActivate()` / `onDeactivate()`

**Helpers matemÃ¡ticos disponibles para todos los efectos:**

| Helper | DescripciÃ³n |
|--------|-------------|
| `normalizedX(pixel, count)` | PosiciÃ³n de pÃ­xel en `[0.0..1.0]` |
| `normalizedTimeSec()` | `millis() / 1000.0f` |
| `clamp01(v)` | Sujeta a `[0.0..1.0]` |
| `smoothstep(edge0, edge1, x)` | InterpolaciÃ³n cÃºbica suave |
| `lerpColor(cA, cB, t)` | InterpolaciÃ³n lineal RGB |
| `addColor(cA, cB)` | Suma aditiva con saturaciÃ³n en 255 |
| `scaleColor(color, brightness)` | Escala uint8 con `reactiveGain` |
| `scaleColorFloat(color, gain)` | Escala float con `reactiveGain` |
| `gradientColor(cA, cB, cC, px, n)` | Gradiente de 3 colores por posiciÃ³n |
| `applyGamma(color)` | CorrecciÃ³n gamma 2.2 con `powf` |
| `audioColorShift(cA, cB, cC)` | Desplazamiento de color por nivel de audio |
| `effectIntervalMs(speedScale)` | Mapea velocidad 1-100 a ~1200-40 ms |
| `speed01(speedScale)` | Normaliza 1-100 a `[0.0..1.0]` |
| `level01(levelScale)` | Normaliza 1-10 a `[0.0..1.0]` |
| `reactiveAudio01()` | `audioLevel/255` si reactivo, sino `1.0f` |
| `reactiveGain()` | Curva `powf(audio, 0.55)` en rango `[0.20..2.40]` |
| `resolveSectionSize(ledCount, sections)` | TamaÃ±o de secciÃ³n con redondeo hacia arriba |

### `EffectRegistry` â€” catÃ¡logo

**Archivos:** `firmware/src/effects/EffectRegistry.h` / `EffectRegistry.cpp`

`EffectDescriptor` contiene: `id`, `key`, `label`, `description`, `usesSpeed`, `usesAudio`.

El catÃ¡logo es un array estÃ¡tico de 17 entradas (sin heap). La selecciÃ³n es por ID numÃ©rico (0-16) o por string key, con aliases en espaÃ±ol (lÃ­neas 80-123 de `EffectRegistry.cpp`).

`usesAudio` determina si `CoreState::reactiveToAudio` se activa automÃ¡ticamente al seleccionar el efecto (ver `CoreState.cpp:230`).

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
| 10 | `trig_ribbon` | **sÃ­** |
| 11 | `lava_flow` | no |
| 12 | `polar_ice` | no |
| 13 | `stellar_twinkle` | **sÃ­** |
| 14 | `random_color_pop` | no |
| 15 | `bouncing_physics` | **sÃ­** |
| 16 | `audio_pulse` | **sÃ­** |

### `EffectManager` â€” orquestador

**Archivos:** `firmware/src/effects/EffectManager.h` / `EffectManager.cpp`

- Posee **todas las instancias como miembros valor** (sin heap para efectos).
- El array `effects_[17]` contiene punteros a esos miembros, asignados en el constructor.
- `resolveActiveEffect()` hace bÃºsqueda lineal O(n) en `effects_[]` por `supports(state_.effectId)`.
- **Lifecycle:** Detecta `state_.effectId != lastEffectId_`, llama `onDeactivate()` al efecto saliente, limpia LEDs, llama `onActivate()` al nuevo.
- `lastEffectId_` se inicializa a `255` para forzar la activaciÃ³n en el primer frame.

### ParÃ¡metros disponibles para efectos (vÃ­a `CoreState`)

| Campo | Rango | Uso |
|-------|-------|-----|
| `brightness` | 0-255 | Escala global de color |
| `effectSpeed` | 1-100 | Velocidad temporal |
| `effectLevel` | 1-10 | Intensidad/agresividad estructural |
| `sectionCount` | 1-10 | Bloques, repeticiones o densidad |
| `primaryColors[3]` | RGBÃ—3 | Paleta de trabajo |
| `backgroundColor` | RGB | Color de fondo |
| `reactiveToAudio` | bool | Derivado automÃ¡ticamente del efecto activo |
| `audioLevel` | 0-255 | RMS suavizado del micrÃ³fono (AGC) |
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

**Archivo:** `firmware/src/effects/EffectManager.h`, lÃ­neas 12-29

```cpp
#include "effects/EffectBlinkFixed.h"
#include "effects/EffectBlinkGradient.h"
// ... 15 mÃ¡s
```

Cualquier cambio en cualquier efecto provoca la recompilaciÃ³n de `EffectManager.cpp` completo. AÃ±adir un efecto nuevo requiere editar exactamente **5 archivos**: `EffectManager.h` (include + miembro), `EffectManager.cpp` (inicializaciÃ³n + asignaciÃ³n al array), `EffectRegistry.cpp` (descriptor), `EffectRegistry.h` (constante), `CoreState.h` (re-export de constante).

### 3.2 `CoreState.h` re-exporta constantes de `EffectRegistry`

**Archivo:** `firmware/src/core/CoreState.h`, lÃ­neas 18-34

```cpp
static constexpr uint8_t kEffectFixed = EffectRegistry::kEffectFixed;
// ... 16 mÃ¡s
```

`CoreState.h` incluye `EffectRegistry.h`. Toda unidad de compilaciÃ³n que incluya `CoreState.h` (prÃ¡cticamente todas) arrastra tambiÃ©n `EffectRegistry.h` aunque no lo necesite.

### 3.3 Efectos acceden directamente a `LedDriver` (sin framebuffer)

Todos los efectos llaman `driver().setPixelColor(outIdx, px, color)` y `driver().show()` directamente desde `renderFrame()`. No hay capa de framebuffer intermedia. Esto impide:

- Preview o simulaciÃ³n sin hardware real.
- Post-procesado global (gamma unificado, brightness hardware).
- Tests unitarios de efectos sin un driver completo.

### 3.4 `AudioService` escribe en `CoreState` sin mutex en dos rutas

**Archivo:** `firmware/src/services/AudioService.cpp`

```cpp
// LÃ­nea 180 â€” SIN mutex:
coreState_.beatDetected = false;

// LÃ­neas 287-290 â€” fallback sin mutex si lock falla:
} else {
    coreState_.audioLevel    = audioLevel_;
    coreState_.audioPeakHold = peakHold_;
}
```

Estas escrituras ocurren desde `controlTask` (core0) mientras `renderTask` (core1) puede leer `CoreState` concurrentemente. El fallback en particular escribe sin protecciÃ³n si el semÃ¡foro estÃ¡ tomado.

### 3.5 `LedDriverFastLed` usa globals de archivo

**Archivo:** `firmware/src/drivers/LedDriverFastLed.cpp`, lÃ­neas 16-17

```cpp
CLEDController *gFastLedController = nullptr;
CRGB           *gFastLedPixels     = nullptr;
```

Solo puede existir una instancia activa. La memoria se gestiona con `new`/`delete[]` sin smart pointers ni verificaciÃ³n de transferencias RMT en curso.

### 3.6 DuplicaciÃ³n de helpers de color

La funciÃ³n `parseHexColor` / `parseColorValue` / `formatHexColor` estÃ¡ duplicada textualmente en:

- `firmware/src/core/CoreState.cpp` (lÃ­neas 15-70)
- `firmware/src/services/EffectPersistenceService.cpp` (lÃ­neas 16-66)

### 3.7 `EffectBlinkFixed` duplica `resolveSectionSize`

**Archivo:** `firmware/src/effects/EffectBlinkFixed.cpp`, lÃ­neas 11-14

Define `resolveSectionSizeValue` como funciÃ³n local que es idÃ©ntica al helper `resolveSectionSize` ya disponible como `protected` en `EffectEngine`. No usa el mÃ©todo heredado.

### 3.8 Mezcla de referencias a IDs de efectos

Los `supports()` de algunos efectos usan `CoreState::kEffectFixed` y otros usan `EffectRegistry::kEffectTripleChase` sin consistencia. Ambas son el mismo valor pero la referencia deberÃ­a ser siempre desde `EffectRegistry`.

---

## 4. Flujo de un Frame

```
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚                     CONTROL TASK (core0, cada 10 ms)                â”‚
â”‚                                                                      â”‚
â”‚  audioService.handle()                                               â”‚
â”‚    â†’ processAudioBuffer()                                            â”‚
â”‚    â†’ coreState_.audioLevel = ...   (con mutex, o race si falla)     â”‚
â”‚                                                                      â”‚
â”‚  apiService.handle()                                                 â”‚
â”‚    â†’ state_.applyPatchJson()                                         â”‚
â”‚    â†’ state_.effectId = ...          (con mutex)                      â”‚
â”‚    â†’ state_.reactiveToAudio = EffectRegistry::usesAudio(effectId)   â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
                                   â”‚ CoreState (mutex FreeRTOS)
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–¼â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚                     RENDER TASK (core1, cada 16 ms)                 â”‚
â”‚                                                                      â”‚
â”‚  EffectManager::renderFrame()                                        â”‚
â”‚    1. state_.lock()         â† toma el mutex de CoreState            â”‚
â”‚    2. driver_.isInitialized() ? ok : driver_.begin()                â”‚
â”‚    3. if (!state_.power)                                             â”‚
â”‚         driver_.clear(); driver_.show(); unlock; return              â”‚
â”‚    4. resolveActiveEffect()                                          â”‚
â”‚         â†’ bÃºsqueda lineal en effects_[17] por supports(effectId)    â”‚
â”‚    5. if (effectId != lastEffectId_)                                 â”‚
â”‚         efectoAnterior.onDeactivate()                                â”‚
â”‚         driver_.clear(); driver_.show()                              â”‚
â”‚         driver_.scheduleShowLog()                                    â”‚
â”‚         efectoNuevo.onActivate()                                     â”‚
â”‚         lastEffectId_ = effectId                                     â”‚
â”‚    6. activeEffect.renderFrame()                                     â”‚
â”‚         â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”              â”‚
â”‚         â”‚ Lee state(): brightness, colors, speed, â€¦   â”‚              â”‚
â”‚         â”‚ Por cada outputIndex 0..outputCount-1:      â”‚              â”‚
â”‚         â”‚   if !supportsPerPixelColor:                â”‚              â”‚
â”‚         â”‚     driver.setOutputColor(outIdx, color)    â”‚              â”‚
â”‚         â”‚   else:                                     â”‚              â”‚
â”‚         â”‚     for px in 0..ledCount:                  â”‚              â”‚
â”‚         â”‚       driver.setPixelColor(outIdx, px, col) â”‚              â”‚
â”‚         â”‚         (NeoPixelBus: escribe en buffer RAM) â”‚             â”‚
â”‚         â”‚ driver.show()                               â”‚              â”‚
â”‚         â”‚   (NeoPixelBus: lanza DMA/RMT async)        â”‚              â”‚
â”‚         â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜              â”‚
â”‚    7. state_.unlock()                                                â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```

**Timing real:** `vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(16))` â†’ ~62,5 FPS target. El mutex puede aÃ±adir latencia variable si `controlTask` y `renderTask` compiten al mismo tiempo. El `NeoPixelBus::show()` es async (RMT DMA), por lo que retorna antes de que los datos lleguen al LED fÃ­sico.

**Sin framebuffer:** El efecto escribe pÃ­xel a pÃ­xel directamente en el buffer interno de NeoPixelBus. No hay etapa de post-procesado entre el efecto y el `show()`.

---

## 5. Plan de Refactor Incremental

Cada fase es un conjunto de commits atÃ³micos. El firmware debe compilar y funcionar correctamente al final de cada commit.

### Fase A â€” Saneamiento interno (sin cambios de interfaz pÃºblica)

**A1. Extraer helpers de color a `ColorUtils.h`**

- Crear `firmware/src/core/ColorUtils.h` con `parseHexColor()`, `parseColorValue()`, `formatHexColor()`.
- Eliminar duplicados de `CoreState.cpp` y `EffectPersistenceService.cpp`.
- Sin cambio de interfaz pÃºblica.

**A2. `EffectBlinkFixed` â†’ usar `resolveSectionSize` heredado**

- Eliminar `resolveSectionSizeValue` local (`EffectBlinkFixed.cpp:11-14`).
- Llamar directamente al mÃ©todo `protected` `resolveSectionSize(ledCount, sectionCount)`.

**A3. Unificar constantes de ID de efectos en `supports()`**

- Todos los `supports()` deben referenciar `EffectRegistry::kEffectXxx` (no `CoreState::kEffectXxx`).
- Comprobar con `grep` que no queda ningÃºn uso de `CoreState::kEffectXxx` internamente.

**A4. Corregir race condition de `AudioService`**

- `beatDetected = false` (lÃ­nea 180) debe ejecutarse dentro del bloque con mutex.
- Eliminar el fallback de escritura sin mutex (lÃ­neas 287-290); si el semÃ¡foro no estÃ¡ disponible, omitir la actualizaciÃ³n de ese ciclo (el valor anterior es aceptable a 20 Hz).

### Fase B â€” Desacoplamiento de `CoreState` del catÃ¡logo de efectos

**B1. Eliminar re-exports de constantes en `CoreState.h`**

- Reemplazar todos los usos de `CoreState::kEffectXxx` en el codebase por `EffectRegistry::kEffectXxx`.
- Eliminar las lÃ­neas 18-34 de `CoreState.h` (los 17 `static constexpr` re-exportados).
- Eliminar el `#include "effects/EffectRegistry.h"` de `CoreState.h`.

**B2. Desacoplar `CoreState.h` de `PaletteRegistry.h`**

- `CoreState` solo necesita `int16_t paletteId` y `uint32_t primaryColors[3]` como campos.
- `PaletteRegistry.h` puede quedar como include solo en `CoreState.cpp` (unidad de compilaciÃ³n).

### Fase C â€” Framebuffer intermedio (habilita post-procesado y tests)

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

### Fase D â€” Reducir fan-in de `EffectManager`

**D1. Mover includes de efectos de `.h` a `.cpp`**

- `EffectManager.h` actualmente incluye los 17 headers de efectos.
- Conservar los miembros como forward declarations o moverlos a `EffectManager.cpp`.
- Resultado: cambios en un efecto solo recompilan `EffectManager.cpp`, no todo lo que incluye `EffectManager.h`.

**D2. Separar presentaciÃ³n de `EffectRegistry`**

- `toJsonArray()` y `buildHtmlOptions()` (en `EffectRegistry.cpp`) dependen de `ArduinoJson` y generan HTML.
- Mover estos helpers a `ApiService` o a un helper de presentaciÃ³n especÃ­fico.
- `EffectRegistry` queda como catÃ¡logo puro de metadatos: IDs, keys, labels, flags.

### Fase E â€” Encapsular estado de `LedDriverFastLed`

**E1. Eliminar globals de archivo en `LedDriverFastLed`**

- Mover `gFastLedController` y `gFastLedPixels` como miembros privados de `LedDriverFastLed`.
- Gestionar lifetime con RAII o destructor explÃ­cito equivalente al de `LedDriverNeoPixelBus`.

---

## 6. Interfaces Sugeridas tras el Refactor

```
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚           EffectEngine              â”‚  â† escribe a FrameBuffer (no al driver)
â”‚  renderFrame(), onActivate(),       â”‚
â”‚  onDeactivate(), supports()         â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
                   â”‚ setPixelColor / setOutputColor
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–¼â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚           LedFrameBuffer            â”‚  â† array de pixels por output en RAM
â”‚  setPixelColor(), setOutputColor()  â”‚
â”‚  flush(LedDriver&)                  â”‚  â† gamma + volcado al driver aquÃ­
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
                   â”‚ setPixelColor / show
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–¼â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚             LedDriver               â”‚  â† sin cambio de interfaz pÃºblica
â”‚  (NeoPixelBus / FastLED / Digital)  â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```

### Archivos a crear, mover o renombrar

| AcciÃ³n | Archivo actual | Destino / Notas |
|--------|---------------|-----------------|
| Crear | â€” | `firmware/src/core/ColorUtils.h` |
| Crear | â€” | `firmware/src/drivers/LedFrameBuffer.h/.cpp` |
| Dividir | `effects/EffectRegistry.cpp` | Extraer `toJsonArray()`/`buildHtmlOptions()` a `ApiService` |
| Limpiar | `core/CoreState.h` | Eliminar re-exports de IDs y el include de `EffectRegistry.h` |
| Reorganizar | `effects/EffectManager.h` | Mover los 17 includes de efectos al `.cpp` |

---

## 7. Riesgos y Mitigaciones

### R1. Performance â€” `applyGamma` con `powf` por pÃ­xel

**Riesgo:** `EffectEngine::applyGamma()` usa `powf(v/255, 2.2)`, operaciÃ³n de punto flotante costosa. Si se centraliza en el flush del framebuffer y se aplica a todos los pÃ­xeles en cada frame (hasta 4 Ã— 300 = 1 200 pÃ­xeles), puede consumir mÃ¡s del budget de 16 ms.

**MitigaciÃ³n:** Sustituir `powf` por una LUT precalculada de 256 `uint8_t`. La LUT se calcula una vez en `setup()` y consume solo 256 bytes de RAM.

### R2. Memoria â€” `LedFrameBuffer` con arrays estÃ¡ticos

**Riesgo:** Si el framebuffer se define como `uint32_t pixels[4][kMaxLedCount]` con `kMaxLedCount` fijo grande, desperdicia RAM en configuraciones con pocas luces. En ESP32 la SRAM es ~320 KB y es fragmentable.

**MitigaciÃ³n:** Dimensionar el buffer por el `ledCount` real de la config activa, o asignar dinÃ¡micamente en `configure()` con tamaÃ±o mÃ¡ximo conocido. Limitar siempre a `kMaxLedOutputs Ã— max(ledCount)`.

### R3. Compatibilidad con MCU â€” `LedDriverFastLed` con pin en template

**Riesgo:** `LedDriverFastLed` usa `DUX_LED_PIN` como parÃ¡metro de template (`addFastLedController<TChipset, DUX_LED_PIN, ...>`). Si se refactoriza para aceptar pin dinÃ¡mico, la API de FastLED no lo soporta directamente.

**MitigaciÃ³n:** Documentar la limitaciÃ³n y mantener el approach actual para FastLED. Solo `NeoPixelBus` soportarÃ¡ verdadero multi-output runtime.

### R4. Concurrencia â€” Mutex de `CoreState` retenido durante todo el render

**Riesgo:** `EffectManager::renderFrame()` toma `CoreState::lock()` durante todo el render, incluyendo los `setPixelColor` de hasta 1 200 pÃ­xeles. Si `controlTask` intenta el mutex durante ese tiempo, se bloquea hasta 16 ms.

**MitigaciÃ³n:** Hacer snapshot de `CoreState` al inicio del frame (ya existe `CoreState::snapshot()`), liberar el mutex inmediatamente, y renderizar sobre la copia. Esto reduce la ventana crÃ­tica a microsegundos.

### R5. Concurrencia â€” `AudioService` escribe en `CoreState` sin mutex

**Riesgo:** `coreState_.beatDetected = false` (lÃ­nea 180 de `AudioService.cpp`) y el fallback de escritura sin mutex (lÃ­neas 287-290) son accesos no-atÃ³micos desde `controlTask` mientras `renderTask` puede estar leyendo los mismos campos desde core1.

**MitigaciÃ³n a corto plazo:** Eliminar el fallback; si no se logra el mutex, omitir la actualizaciÃ³n de ese ciclo. A 20 Hz de actualizaciÃ³n de audio, el valor anterior es aceptable. La escritura de `beatDetected = false` debe hacerse tambiÃ©n dentro del bloque protegido.

### R6. Compatibilidad â€” EliminaciÃ³n de `CoreState::kEffectXxx`

**Riesgo:** Si alguna unidad de compilaciÃ³n (o test futuro) depende de `CoreState::kEffectFixed`, el cambio rompe compilaciÃ³n.

**MitigaciÃ³n:** Hacer el cambio en un commit atÃ³mico con bÃºsqueda exhaustiva por `grep`. El compilador detecta todos los usos no actualizados en tiempo de compilaciÃ³n.

### R7. Doble buffer con `NeoPixelBus`

**Riesgo:** `NeoPixelBus` ya tiene su propio buffer de pixels en RAM. Introducir `LedFrameBuffer` aÃ±ade un segundo buffer de igual tamaÃ±o, duplicando el uso de RAM para pixels.

**MitigaciÃ³n:** Evaluar si el framebuffer es solo necesario para simulaciÃ³n y tests. En producciÃ³n, si no se aÃ±ade post-procesado global, se puede omitir y mantener el write-directo al driver para preservar RAM.

---

## 8. Checklist Priorizado de AcciÃ³n

### Alta prioridad (bugs y correcciÃ³n de comportamiento)

- [ ] **R5** Corregir race condition en `AudioService.cpp` lÃ­neas 180 y 287-290
- [ ] **R1** Sustituir `applyGamma` con `powf` por LUT de 256 bytes

### Media prioridad (deuda tÃ©cnica, mantenibilidad)

- [x] **A1** Extraer `ColorUtils.h` (eliminar duplicaciÃ³n de `parseHexColor`)
- [x] **A2** `EffectBlinkFixed` â†’ usar `resolveSectionSize` heredado de `EffectEngine`
- [x] **A3** Unificar uso de `EffectRegistry::kEffectXxx` en todos los `supports()`
- [x] **B1** Desacoplar `CoreState.h` de `EffectRegistry.h`
- [x] **B2** Desacoplar `CoreState.h` de `PaletteRegistry.h`
- [ ] **E1** Encapsular globals de `LedDriverFastLed` en miembros de clase

### Baja prioridad (arquitectura futura)

- [ ] **R4** Snapshot de `CoreState` al inicio del frame para reducir contenciÃ³n de mutex
- [ ] **C1** Introducir `LedFrameBuffer` entre efectos y driver
- [ ] **C2** Centralizar gamma correction en el flush del framebuffer
- [x] **D1** Mover includes de efectos de `EffectManager.h` a `EffectManager.cpp`
- [ ] **D2** Separar helpers de presentaciÃ³n JSON/HTML de `EffectRegistry`
- [ ] **R6** Migrar todos los usos de `CoreState::kEffectXxx` a `EffectRegistry::kEffectXxx`

---

## Referencia RÃ¡pida de Archivos

| Archivo | PropÃ³sito |
|---------|-----------|
| `firmware/src/main.cpp` | Wiring de servicios, FreeRTOS tasks, `setup()` / `loop()` |
| `firmware/src/drivers/LedDriver.h/.cpp` | Clase base abstracta del driver |
| `firmware/src/drivers/CurrentLedDriver.h` | SelecciÃ³n compile-time de backend |
| `firmware/src/drivers/LedDriverNeoPixelBus.cpp` | Backend principal (RMT async, multi-output) |
| `firmware/src/drivers/LedDriverFastLed.cpp` | Backend alternativo (globals, single output) |
| `firmware/src/drivers/LedDriverDigital.cpp` | Backend GPIO on/off |
| `firmware/src/core/CoreState.h/.cpp` | Estado runtime compartido + mutex FreeRTOS |
| `firmware/src/core/BuildProfile.h` | Constantes de compilaciÃ³n (`DUX_LED_BACKEND`, `DUX_LED_PIN`, â€¦) |
| `firmware/src/core/Config.h` | Structs de configuraciÃ³n (`GpioConfig`, `kMaxLedOutputs`) |
| `firmware/src/effects/EffectEngine.h/.cpp` | Base de efectos + todos los helpers matemÃ¡ticos |
| `firmware/src/effects/EffectManager.h/.cpp` | Orquestador, instancias de efectos, lifecycle |
| `firmware/src/effects/EffectRegistry.h/.cpp` | CatÃ¡logo estÃ¡tico de 17 efectos |
| `firmware/src/services/AudioService.h/.cpp` | I2S, AGC, beat detection, escribe a `CoreState` |
| `firmware/src/services/EffectPersistenceService.cpp` | Persistencia de efectos y secuencias |
| `platformio.ini` | Perfiles de build y selecciÃ³n de backend |
