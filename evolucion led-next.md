# Evolución DUXMAN-LED-NEXT

## 1) Contexto y propósito
Este documento es el **punto único de verdad** para continuar el proyecto `DUXMAN-LED-NEXT` en cualquier workspace limpio.

Objetivo del proyecto:
- Crear un controlador LED moderno para `ESP32-S3` con enfoque rompedor.
- Usar `WLED` como referencia funcional/benchmark, **no** como límite tecnológico.
- Priorizar mantenibilidad, robustez de memoria flash y API limpia.

---

## 2) Decisiones cerradas de esta sesión (ADR)

### ADR-001: Proyecto nuevo
- Se comienza desde cero (`greenfield`).
- No se migra arquitectura legacy de forma directa.

### ADR-002: Plataforma hardware inicial
- Objetivo inicial: `ESP32-S3`.
- Se excluye `ESP8266` en MVP para evitar deuda y límites de recursos.

### ADR-003: Servidor HTTP
- Elegido: `PsychicHttp`.

### ADR-004: Persistencia
- Estado caliente en RAM.
- Persistencia diferida (debounce).
- Escritura atómica con recuperación ante corrupción.

### ADR-005: API
- Versionada desde inicio: `/api/v1/*`.
- Cambios parciales por `PATCH`.

---

## 3) Alcance MVP v0.1

### Incluye
1. `CoreState` como fuente única de verdad.
2. Endpoints:
   - `GET /api/v1/state`
   - `PATCH /api/v1/state`
   - `GET /api/v1/diag`
3. `StorageService` con:
   - debounce de guardado (`1000–2000 ms`),
   - escritura atómica (`config.tmp -> config.json`),
   - fallback a backup/default.
4. Diagnóstico básico de salud de sistema.

### No incluye (de momento)
- Compatibilidad total con protocolos legacy.
- Soporte `ESP8266`.
- Editor avanzado de efectos en web.

---

## 4) Arquitectura objetivo

- `CoreState`
  - Estado runtime: `power`, `brightness`, segmentos, efecto, color.
- `ApiService` (sobre `PsychicHttp`)
  - Rutas REST `v1` y validación de payload.
- `StorageService`
  - Cola de persistencia + debounce + atomic write.
- `ConfigService`
  - `schemaVersion`, validación y migraciones futuras.
- `DiagService`
  - `uptime`, heap libre, contadores de escritura, errores de parseo.
- `EffectEngine`
  - Cálculo de frame desacoplado de HTTP/persistencia.
- `LedDriver`
  - Abstracción de salida LED por backend (`RMT`/`SPI` según chipset).

---

## 5) Diseño de memoria y persistencia (clave del proyecto)

### Flujo de guardado
1. `PATCH` actualiza `CoreState` en RAM.
2. Se marca `dirty`.
3. Se programa guardado diferido (debounce).
4. Se escribe atómicamente:
   - serializar a `/config.tmp`,
   - `flush/sync`,
   - rename a `/config.json` (con backup opcional).

### Política anti-desgaste flash
- Nunca guardar en cada cambio de slider.
- Coalescing de cambios en ventana corta.
- Contador de writes expuesto en `/api/v1/diag`.
- Throttle por tipo de dato si es necesario.

### Recuperación
- Si `/config.json` es inválido:
  1. intentar backup,
  2. si falla, cargar defaults seguros,
  3. registrar incidente en diagnóstico.

---

## 6) Contrato API v1 (draft inicial)

### `GET /api/v1/state`
Respuesta ejemplo:
```json
{
  "power": true,
  "brightness": 180,
  "segment": {
    "start": 0,
    "stop": 59,
    "effect": "solid",
    "color": [255, 160, 80]
  }
}
```

### `PATCH /api/v1/state`
Payload ejemplo:
```json
{
  "brightness": 120,
  "segment": {
    "effect": "rainbow"
  }
}
```
Respuesta ejemplo:
```json
{
  "ok": true,
  "changed": true
}
```

### `GET /api/v1/diag`
Respuesta ejemplo:
```json
{
  "uptimeMs": 123456,
  "heapFree": 198432,
  "configWrites": 17,
  "lastSaveMs": 123400
}
```

### Errores estándar
```json
{
  "code": "invalid_payload",
  "message": "brightness must be 0..255"
}
```

---

## 7) Reglas de hardware y pinout (obligatorias)

Antes de fijar cualquier `GPIO`:
- Verificar datasheet de la placa exacta (`ESP32-S3` concreta).
- Revisar pines de strapping/boot y pines reservados.
- Confirmar si algún pin tiene función especial que afecte estabilidad.

Regla de nivel lógico:
- MCU a `3.3V`; si tira LED usa `5V`, usar level shifter para línea de datos.

---

## 8) Estructura recomendada del nuevo repo

```text
duxman-led-next/
  firmware/
    platformio.ini
    src/
      main.cpp
      core/
      api/
      services/
      effects/
      drivers/
    include/
    test/
  web/
    package.json
    src/
    public/
  docs/
    architecture.md
    api-v1.md
    storage-memory.md
    roadmap.md
    reference-library.md
  tools/
```

---

## 9) Librería de referencia tecnológica (consulta rápida)

### Proyectos
- `WLED`: referencia UX y features reales de usuario.
- `ESPHome`: buen enfoque declarativo e integración domótica.
- `Hyperion`: referencia de sync/realtime en red.
- `Pixelblaze`: referencia creativa de efectos.

### Criterios de adopción de dependencias
Una librería entra solo si cumple:
1. mantenimiento activo,
2. soporte `ESP32-S3`,
3. footprint razonable `RAM/Flash`,
4. licencia compatible,
5. prueba mínima reproducible.

---

## 10) Roadmap por fases (implementación incremental)

### Fase 1: Núcleo API
- `CoreState`
- `GET/PATCH /api/v1/state`
- validación estricta de payload

### Fase 2: Persistencia robusta
- `StorageService`
- atomic write + debounce + recovery

### Fase 3: Diagnóstico
- `GET /api/v1/diag`
- métricas de memoria/escrituras/errores

### Fase 4: UI web base
- `power`, `brightness`, `color`, `effect`, `segment`
- actualización de estado en tiempo real

### Fase 5: Integraciones
- `MQTT`
- `DDP` o `E1.31` (según demanda real)

### Fase 6: Hardening
- stress de escritura flash
- test de brownout/recovery
- profiling latencia/RAM/Flash

---

## 11) Checklist para arrancar en workspace limpio

1. Crear proyecto `duxman-led-next`.
2. Inicializar `firmware` con PlatformIO para `ESP32-S3`.
3. Integrar `PsychicHttp`.
4. Implementar endpoints `v1` del MVP.
5. Implementar persistencia diferida + atómica.
6. Añadir pruebas mínimas de contrato.
7. Documentar cambios en `docs/`.

---

## 12) Comandos base (PowerShell)

```powershell
cd F:\desarrollo
mkdir duxman-led-next
cd duxman-led-next
mkdir firmware
cd firmware
pio project init --board esp32-s3-devkitc-1 --project-option "framework=arduino"
pio run
```

---

## 13) Definition of Done por iteración

Una iteración se considera terminada cuando:
- Compila para el target `ESP32-S3` definido.
- Endpoints del sprint responden correctamente.
- Validación de payload rechaza entradas inválidas.
- Persistencia no bloquea loop principal.
- No hay ráfaga de escrituras por cambios continuos.
- Documentación actualizada en este archivo y `docs/`.

---

## 14) Riesgos y mitigaciones

### Riesgo: acoplamiento excesivo entre HTTP y lógica
Mitigación: usar interfaces y servicios separados.

### Riesgo: desgaste de flash
Mitigación: debounce + coalescing + métricas de writes.

### Riesgo: errores por pinout incorrecto
Mitigación: validación por placa exacta antes de cablear.

### Riesgo: payload JSON grande
Mitigación: límites de tamaño + validación estricta.

---

## 15) Estado actual de sesión (snapshot)

**Fase 4A-4C COMPLETADAS (v0.3.2-beta)**
- ✅ FreeRTOS dual-core: `TaskControl` (core 0, 10ms) + `TaskRender` (core 1, 16ms)
- ✅ CoreState sincronizado: Mutex para API-render + snapshot pattern
- ✅ Persistencia asíncrona: `PersistenceSchedulerService` con coalescent flags
- ✅ ProfileService desacoplado: Cola interna para saves diferidos
- ✅ Task Watchdog Timers (TWDT): Monitoreo per-core con recuperación
- ✅ Boot timestamp: Fecha/hora último arranque en respuesta state
- ✅ Catalogo de efectos dinámicos (ids 0..15) integrado y validado en hardware
- ✅ Calibrado de sliders (`effectSpeed` y `effectLevel`) unificado y corregido

**Próximas fases:**
- Fase 5: Audio reactivity (I2S + FFT para efectos reactivos)
- Fase 4C.1: Métricas extendidas (contadores, latencias)
- Fase 5B: Compatibilidad WLED protocol (dmx, artnet)

---

## 16) Prompt de reanudación para futuras sesiones

Usar este bloque al abrir un chat nuevo:

> "Continuamos DUXMAN-LED-NEXT en v0.3.2-beta según `evolucion led-next.md`.
> Stack cerrado: ESP32 (C3/Dev/S3) + PlatformIO + Arduino + FreeRTOS.
> 
> **Estado actual (Fases 4A-4C COMPLETAS):**
> - FreeRTOS dual-core (TaskControl 10ms, TaskRender 16ms) ✅
> - Persistencia asíncrona con coalescent queues ✅
> - Task Watchdog Timers con recuperación ✅
> - CoreState mutex-protected para API-render sync ✅
> 
> **Siguiente paso:** Fase 5 (Audio reactivity I2S+FFT) o métricas extendidas."

---

## 17) Próxima tarea exacta

**Fase 5: Audio Reactivity (I2S + FFT)**

1. Integrar módulo PDM/I2S según ESP32 (recomendar SPM1423 o INMP441).
2. Implementar `AudioService` con buffer circular y análisis FFT.
3. Exponer bands de frecuencia (`bass`, `mid`, `treble`, `peak`) en estado.
4. Crear efectos reactivos:
   - `vumeter`: Altura de columna por bass
   - `spectrum`: Barras independientes por frecuencia
   - `flash_on_kick`: Estrobo en picos de bass
5. Documentar wiring y `platformio.ini` para I2S pins por placa.
6. Testing en hardware con audio real (música, voces, etc.).

---

## 18) Documentos vinculados

- Arquitectura técnica base: `docs/architecture-led-next-v1.md`
- Referencia operativa WLED (solo consulta en bloqueos): `docs/wled-reference-implementation-guide.md`

Fin del documento.