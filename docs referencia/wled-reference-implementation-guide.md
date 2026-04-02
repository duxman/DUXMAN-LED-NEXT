# Referencia Operativa WLED (Solo Consulta en Bloqueos)

## 1) Propósito
Este documento es **solo de consulta** para desbloqueos, basado en comportamientos que hoy funcionan en WLED.

No define arquitectura nueva ni decisiones de diseño para `LED-NEXT`.

Usarlo únicamente cuando haya dudas sobre:
- cómo responde WLED en API/estado,
- cómo gestiona configuración/archivos,
- cómo organiza la UI y settings.

---

## 2) Dónde mirar primero (ruta rápida)

### API y estado
- `wled00/json.cpp`
  - `deserializeState(...)`
  - `serializeState(...)`
  - `deserializeSegment(...)`
  - `serializeSegment(...)`
- `wled00/wled_server.cpp`
- `wled00/ws.cpp`
- `wled00/set.cpp`

### Configuración y almacenamiento
- `wled00/cfg.cpp`
  - `deserializeConfig(...)`
  - `deserializeConfigFromFS()`
  - `serializeConfigToFS()`
  - `backupConfig()`
  - `restoreConfig()`
  - `verifyConfig()`
- `wled00/file.cpp`
  - `writeObjectToFile(...)`
  - `readObjectFromFile(...)`
  - `backupFile(...)`
  - `restoreFile(...)`
  - `validateJsonFile(...)`
  - `updateFSInfo()`

### UI web
- `wled00/data/index.htm`
- `wled00/data/index.js`
- `wled00/data/common.js`
- `wled00/data/settings*.htm`

### Presets
- `wled00/presets.cpp`
- `wled00/playlist.cpp`

---

## 3) Casos de bloqueo y ruta de consulta

### Bloqueo A: “No sé qué JSON acepta estado”
1. Revisar `deserializeState(...)` en `wled00/json.cpp`.
2. Revisar `deserializeSegment(...)` para detalles de segmentos.
3. Confirmar estructura de respuesta en `serializeState(...)`.

### Bloqueo B: “No sé cómo WLED persiste config”
1. Revisar `deserializeConfigFromFS()` en `wled00/cfg.cpp`.
2. Revisar `serializeConfigToFS()` en `wled00/cfg.cpp`.
3. Revisar `writeObjectToFile(...)` y `readObjectFromFile(...)` en `wled00/file.cpp`.

### Bloqueo C: “No sé cómo recupera ante corrupción”
1. Revisar `backupConfig()`, `restoreConfig()`, `verifyConfig()` en `wled00/cfg.cpp`.
2. Revisar `backupFile()`, `restoreFile()`, `validateJsonFile()` en `wled00/file.cpp`.

### Bloqueo D: “No sé dónde está un ajuste en UI”
1. Localizar página en `wled00/data/settings*.htm`.
2. Revisar lógica común en `wled00/data/common.js`.
3. Revisar acciones de interfaz en `wled00/data/index.js`.

---

## 4) Comandos de consulta (PowerShell)

```powershell
cd F:\desarrollo\WLED

# Estado JSON
Select-String -Path .\wled00\json.cpp -Pattern "deserializeState|serializeState|deserializeSegment|serializeSegment"

# Configuración
Select-String -Path .\wled00\cfg.cpp -Pattern "deserializeConfigFromFS|serializeConfigToFS|backupConfig|restoreConfig|verifyConfig"

# Archivos y validación JSON
Select-String -Path .\wled00\file.cpp -Pattern "writeObjectToFile|readObjectFromFile|backupFile|restoreFile|validateJsonFile|updateFSInfo"

# UI settings
Get-ChildItem .\wled00\data\settings*.htm
```

---

## 5) Criterio de uso de esta referencia

Una consulta se considera resuelta cuando se identifica:
1. archivo exacto en WLED,
2. función exacta relacionada,
3. comportamiento actual observado en ese código.

Este documento **no** debe incluir propuestas de rediseño.

---

## 6) Nota de mantenimiento

Actualizar solo si cambian rutas o funciones de referencia en WLED.

Mantenerlo como:
- índice de consulta,
- guía de localización rápida,
- apoyo en bloqueos técnicos.
