# DUXMAN-LED-NEXT — Servidor Mockup

Servidor Python que simula **todos los endpoints REST del firmware** sin necesitar hardware real (ningún ESP32, ningún LED).

Útil para:
- Desarrollar y probar la UI web sin el controlador físico.
- Probar la app Android sin el dispositivo encendido.
- Demos y presentaciones.

---

## Requisitos

- Python 3.10 o superior
- Flask 3.x

```bash
pip install -r requirements.txt
```

---

## Arrancar el servidor

```bash
# Desde el directorio projects/mockup-python/
python mock_server.py

# Opciones
python mock_server.py --host 0.0.0.0 --port 8080

# Log de interacciones (JSONL)
python mock_server.py --log-file ./mock_interactions.log --log-level INFO
```

El servidor escucha en **http://127.0.0.1:8080** por defecto.

| URL | Descripción |
|-----|-------------|
| `http://localhost:8080/` | Redirige a la UI principal |
| `http://localhost:8080/ui/home.html` | Página de inicio |
| `http://localhost:8080/api/v1/state` | Estado actual (JSON) |

---

## Estado editable — `mock_state.json`

El fichero `mock_state.json` contiene el **estado inicial** con el que arranca el servidor. Puedes editarlo a mano para simular distintas configuraciones sin tocar el código Python.

### Secciones del fichero

| Clave | Descripción |
|-------|-------------|
| `state` | Estado de los LEDs (efecto, brillo, colores, paleta…) |
| `network` | Configuración WiFi, IP, DNS, NTP |
| `microphone` | Configuración del micrófono I2S |
| `gpio` | Salidas LED (pin, tipo, cantidad) |
| `general` | Idioma, región, debug |
| `sync` | Modo de sincronización (DDP, E1.31, cluster) |
| `profiles` | Lista de perfiles guardados |
| `startup_effect` | Efecto que se aplica al arrancar |
| `effect_sequence` | Secuencia de efectos automática |

Los cambios hechos desde la UI se mantienen **en memoria** durante la sesión. Para que persistan entre reinicios, edita el fichero JSON.

> **Truco:** enviar `POST /api/v1/system/restart` recarga el estado desde `mock_state.json`, simulando un reinicio del controlador.

---

## Logging de interacciones

El mockup registra cada request/response en un fichero JSONL para auditar como interactua la app.

- Fichero por defecto: `projects/mockup-python/mock_interactions.log`
- Campos: `timestamp`, `durationMs`, `method`, `path`, `query`, `status`, `requestBody`, `responseBody`
- Rotacion: 2 MB por fichero, hasta 3 backups

Endpoints de soporte:

| Metodo | Ruta | Descripcion |
|--------|------|-------------|
| GET | `/api/v1/mock/logs?limit=200` | Devuelve las ultimas entradas del log |
| POST | `/api/v1/mock/logs/clear` | Limpia el log actual y sus rotaciones |

---

## Endpoints disponibles

### Estado

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/api/v1/state` | Lee el estado completo (efecto, brillo, colores, paletas disponibles…) |
| PATCH / POST | `/api/v1/state` | Modifica el estado (campos parciales admitidos) |

### Configuración

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET / PATCH | `/api/v1/config/network` | Configuración de red |
| GET / PATCH | `/api/v1/config/microphone` | Configuración del micrófono |
| GET / PATCH | `/api/v1/config/gpio` | Salidas LED |
| GET / PATCH | `/api/v1/config/debug` | Config general / debug |
| GET / PATCH | `/api/v1/config/sync` | Sincronización |
| GET / POST | `/api/v1/config/all` | Todas las secciones a la vez |

### Efectos

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/api/v1/effects` | Catálogo de efectos + efecto de inicio + secuencia |
| POST | `/api/v1/effects/startup/save` | Guarda el efecto de arranque |
| POST | `/api/v1/effects/sequence/add` | Añade un paso a la secuencia |
| POST | `/api/v1/effects/sequence/delete` | Elimina un paso de la secuencia |

### Paletas

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/api/v1/palettes` | Catálogo de paletas (incluye las de usuario) |
| POST | `/api/v1/palettes/apply` | Aplica una paleta al estado actual |
| POST | `/api/v1/palettes/save` | Guarda / actualiza una paleta de usuario |
| POST | `/api/v1/palettes/delete` | Elimina una paleta de usuario |

### Perfiles

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/api/v1/profiles` | Lista de perfiles |
| GET | `/api/v1/profiles/get?id=<id>` | Perfil completo |
| POST | `/api/v1/profiles/save` | Crea o actualiza un perfil |
| POST | `/api/v1/profiles/clone` | Clona un perfil existente |
| POST | `/api/v1/profiles/apply` | Aplica un perfil |
| POST | `/api/v1/profiles/default` | Marca un perfil como predeterminado |
| POST | `/api/v1/profiles/delete` | Elimina un perfil de usuario |

### Sistema

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/api/v1/hardware` | Info del chip (simulada) |
| GET | `/api/v1/release` | Versión del firmware |
| GET | `/api/v1/diag` | Diagnóstico (heap, WiFi, uptime) |
| POST | `/api/v1/system/restart` | Recarga el estado desde `mock_state.json` |
| GET | `/api/v1/sync/connected` | Estado de sincronización |
| GET | `/api/v1/openapi.json` | Esquema OpenAPI 3.0 |

---

## Estructura de ficheros

```
projects/mockup-python/
├── mock_server.py     ← Servidor Flask (no editar para cambiar estado)
├── mock_state.json    ← Estado inicial editable
├── requirements.txt   ← Dependencias Python
└── README.md          ← Esta guía
```

La UI se sirve directamente desde `../firmware-platformio/data/ui/` (los mismos ficheros que usa el firmware real).
