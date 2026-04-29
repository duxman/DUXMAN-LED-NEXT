
# API v1

Proyecto: `DUXMAN-LED-NEXT` (firmware v0.3.7-beta)

## Resumen
- HTTP: `http://<ip>/api/v1/*` (puerto 80)
- Serial: mismos comandos, 115200 baud
- Formato: JSON
- Métodos: `GET`, `PATCH`, `POST` (alias)

## Endpoints principales
| Método | Ruta | Descripción |
|---|---|---|
| GET | `/api/v1/state` | Estado runtime actual |
| PATCH/POST | `/api/v1/state` | Actualizar estado |
| GET | `/api/v1/palettes` | Listar paletas (sistema + usuario) |
| POST | `/api/v1/palettes/apply` | Aplicar paleta |
| POST | `/api/v1/palettes/save` | Guardar/editar paleta de usuario |
| POST | `/api/v1/palettes/delete` | Eliminar paleta de usuario |
| GET | `/api/v1/config/network` | Configuración WiFi/IP |
| PATCH/POST | `/api/v1/config/network` | Actualizar red |
| GET | `/api/v1/config/microphone` | Config micrófono |
| PATCH/POST | `/api/v1/config/microphone` | Actualizar micrófono |
| GET | `/api/v1/config/gpio` | Configuración salidas LED |
| PATCH/POST | `/api/v1/config/gpio` | Actualizar GPIO |
| GET | `/api/v1/profiles/gpio` | Listar perfiles GPIO |
| POST | `/api/v1/profiles/gpio/save` | Guardar perfil GPIO |
| POST | `/api/v1/profiles/gpio/apply` | Aplicar perfil GPIO |
| POST | `/api/v1/profiles/gpio/default` | Fijar perfil GPIO por defecto |
| POST | `/api/v1/profiles/gpio/delete` | Eliminar perfil GPIO |
| GET | `/api/v1/config/debug` | Configuración debug |
| PATCH/POST | `/api/v1/config/debug` | Actualizar debug |
| GET | `/api/v1/config/all` | Exportar configuración completa |
| POST | `/api/v1/config/all` | Importar configuración completa |
| GET | `/api/v1/hardware` | Info hardware runtime |
| GET | `/api/v1/release` | Info de versión/release |

## Ejemplo: Estado runtime
### `GET /api/v1/state`
```json
{
	"power": true,
	"brightness": 128,
	"effectId": 0,
	"effect": "fixed",
	"sectionCount": 3,
	"primaryColors": ["#FF4D00", "#FFD400", "#00B8D9"],
	"backgroundColor": "#000000",
	"paletteId": 2
}
```

### `PATCH /api/v1/state`
Permite actualizar cualquier campo del estado. Ejemplo:
```json
{
	"power": false,
	"brightness": 200,
	"effectId": 7,
	"primaryColors": ["#00FF00", "#0000FF", "#FF00FF"]
}
```

## Ejemplo: Paletas
### `GET /api/v1/palettes`
Devuelve lista combinada de paletas de sistema y usuario:
```json
[
	{ "id": 0, "label": "Sunset", "primaryColors": ["#FF4D00", "#FFD400", "#00B8D9"], "source": "system", "readOnly": true },
	{ "id": -101, "label": "Mi paleta", "primaryColors": ["#123456", "#654321", "#ABCDEF"], "source": "user", "readOnly": false }
]
```

### `POST /api/v1/palettes/save`
Guardar o editar una paleta de usuario:
```json
{
	"id": -101,
	"label": "Mi paleta",
	"primaryColors": ["#123456", "#654321", "#ABCDEF"],
	"style": "neon",
	"description": "Paleta personalizada"
}
```

### `POST /api/v1/palettes/delete`
Eliminar una paleta de usuario:
```json
{ "id": -101 }
```

## Ejemplo: Perfiles GPIO
### `GET /api/v1/profiles/gpio`
Lista todos los perfiles GPIO disponibles (integrados y usuario):
```json
[
	{ "id": "esp32dev", "label": "Default ESP32 DevKit", "outputs": [ ... ] },
	{ "id": "custom1", "label": "Mi perfil", "outputs": [ ... ] }
]
```

### `POST /api/v1/profiles/gpio/apply`
Aplica un perfil GPIO:
```json
{ "id": "custom1" }
```

---
Para detalles de arquitectura y evolución, ver `docs/architecture.md` y `CHANGELOG.md`.

Actualiza parcialmente el estado actual.

Ejemplo de payload:

```json
{
	"power": true,
	"brightness": 160,
	"effect": "gradient",
	"sectionCount": 6,
	"primaryColors": [
		"#FF4D00",
		"#FFD400",
		"#00B8D9"
	],
	"backgroundColor": "#050505"
}
```

Ejemplo de respuesta:

```json
{
	"updated": true,
	"state": {
		"power": true,
		"brightness": 160,
		"effectId": 1,
		"effect": "gradient",
		"sectionCount": 6,
		"primaryColors": [
			"#FF4D00",
			"#FFD400",
			"#00B8D9"
		],
		"backgroundColor": "#050505"
	}
}
```

Campos soportados actualmente en el payload de estado:

- `power`: encendido general
- `brightness`: brillo global `0..255`
- `effectId`: identificador numérico del efecto
- `effect`: alias string del efecto (por ejemplo `fixed`, `gradient`, `lava_flow`)
- `sectionCount`: número de secciones repartidas sobre cada salida
- `effectSpeed`: velocidad del efecto (`1..10`)
- `effectLevel`: nivel/intensidad del efecto (`1..10`)
- `paletteId`: identificador de paleta predefinida (`-1` modo manual)
- `palette`: clave string de paleta (alternativa a `paletteId`)
- `primaryColors`: array de 3 colores hex `#RRGGBB` (fuerza modo manual)
- `backgroundColor`: color de fondo/off `#RRGGBB`
- `reactiveToAudio`: habilita/deshabilita reactividad cuando el efecto soporta audio

Errores típicos:

```json
{"error":"invalid_payload"}
```

## Paletas predefinidas

### `GET /api/v1/palettes`

Devuelve el catálogo curado de paletas de 3 colores.

Campos por entrada:

- `id`: identificador numérico de paleta
- `key`: clave estable para API
- `label`: nombre legible para UI
- `style`: clasificación (`warm`, `cold`, `neon`, `pastel`, `high-contrast`, `party`)
- `description`: intención visual de la paleta
- `primaryColors`: trío de colores `#RRGGBB`

### `POST /api/v1/palettes/apply`

Aplica una paleta al estado runtime y programa persistencia.

Payload soportado (usar uno de los dos):

```json
{"paletteId":1}
```

```json
{"palette":"sunset_drive"}
```

Respuesta:

```json
{
	"updated": true,
	"state": {
		"paletteId": 1,
		"palette": "sunset_drive"
	}
}
```

Errores posibles:

```json
{"error":"invalid_payload"}
{"error":"invalid_json"}
{"error":"missing_palette"}
```

## Configuración de red

### `GET /api/v1/config/network`

Devuelve la configuración persistida de red, IP, DNS y debug.

Estructura general:

```json
{
	"network": {
		"wifi": {
			"mode": "ap",
			"apAvailability": "untilStaConnected",
			"connection": {
				"ssid": "",
				"password": ""
			}
		},
		"ip": {
			"ap": {
				"mode": "dhcp",
				"address": "",
				"gateway": "",
				"subnet": "",
				"primaryDns": "",
				"secondaryDns": ""
			},
			"sta": {
				"mode": "dhcp",
				"address": "",
				"gateway": "",
				"subnet": "",
				"primaryDns": "",
				"secondaryDns": ""
			}
		},
		"dns": {
			"hostname": "duxman-led"
		}
	},
	"debug": {
		"enabled": false,
		"heartbeatMs": 5000
	}
}
```

### `PATCH /api/v1/config/network`

Actualiza parcialmente la configuración de red.

Valores válidos conocidos:

- `network.wifi.mode`: `ap`, `sta`, `ap_sta`
- `network.wifi.apAvailability`: `always`, `untilStaConnected`
- `network.ip.ap.mode`: `dhcp`, `static`
- `network.ip.sta.mode`: `dhcp`, `static`
- `debug.enabled`: `true`, `false`, `1`, `0`
- `debug.heartbeatMs`: `0..600000`

Ejemplo de payload:

```json
{
	"network": {
		"wifi": {
			"mode": "ap_sta",
			"apAvailability": "untilStaConnected",
			"connection": {
				"ssid": "MiWifi",
				"password": "clave-segura"
			}
		},
		"ip": {
			"sta": {
				"mode": "dhcp"
			}
		},
		"dns": {
			"hostname": "duxman-led-c3"
		}
	},
	"debug": {
		"enabled": true,
		"heartbeatMs": 3000
	}
}
```

Ejemplo de respuesta:

```json
{
	"updated": true,
	"network": {
		"network": {
			"wifi": {
				"mode": "ap_sta",
				"apAvailability": "untilStaConnected",
				"connection": {
					"ssid": "MiWifi",
					"password": "clave-segura"
				}
			},
			"ip": {
				"ap": {
					"mode": "dhcp"
				},
				"sta": {
					"mode": "dhcp"
				}
			},
			"dns": {
				"hostname": "duxman-led-c3"
			}
		},
		"debug": {
			"enabled": true,
			"heartbeatMs": 3000
		}
	}
}
```

Errores posibles:

```json
{"error":"invalid_payload"}
{"error":"invalid_json"}
{"error":"persistence_failed"}
{"error":"wifi_apply_failed"}
{"error":"<codigo_de_validacion>"}
```

## Configuración GPIO / LED

### `GET /api/v1/config/gpio`

Devuelve la configuración persistida de salidas LED.

Ejemplo de respuesta:

```json
{
	"gpio": {
		"outputs": [
			{
				"id": 0,
				"pin": 8,
				"ledCount": 60,
				"ledType": "ws2812b",
				"colorOrder": "GRB"
			}
		]
	}
}
```

### `PATCH /api/v1/config/gpio`

Actualiza parcialmente la configuración de salidas.

Tipos LED soportados:

- `digital`
- `ws2812b`
- `ws2811`
- `ws2813`
- `ws2815`
- `sk6812`
- `tm1814`

Órdenes de color soportados:

- RGB 3 canales: `GRB`, `RGB`, `BRG`, `RBG`, `GBR`, `BGR`
- RGBW: `RGBW`, `GRBW`
- Digital: `R`, `G`, `B`, `W`

Reglas relevantes:

- máximo `4` salidas
- `pin` entre `-1` y `48`
- `ledCount` entre `1` y `1500`
- `digital` exige `ledCount = 1`
- no se permiten pines duplicados

Ejemplo de payload:

```json
{
	"gpio": {
		"outputs": [
			{
				"id": 0,
				"pin": 8,
				"ledCount": 60,
				"ledType": "ws2812b",
				"colorOrder": "GRB"
			},
			{
				"id": 1,
				"pin": 10,
				"ledCount": 1,
				"ledType": "digital",
				"colorOrder": "W"
			}
		]
	}
}
```

Ejemplo de respuesta:

```json
{
	"updated": true,
	"gpio": {
		"gpio": {
			"outputs": [
				{
					"id": 0,
					"pin": 8,
					"ledCount": 60,
					"ledType": "ws2812b",
					"colorOrder": "GRB"
				},
				{
					"id": 1,
					"pin": 10,
					"ledCount": 1,
					"ledType": "digital",
					"colorOrder": "W"
				}
			]
		}
	}
}
```

Errores posibles:

```json
{"error":"invalid_payload"}
{"error":"invalid_json"}
{"error":"persistence_failed"}
{"error":"invalid_led_type_output_0"}
{"error":"invalid_color_order_output_0"}
{"error":"invalid_digital_color_output_1"}
{"error":"digital_led_count_must_be_1_output_1"}
{"error":"duplicate_pin_outputs_0_1"}
```

Nota de comportamiento:

- Cuando la configuración GPIO cambia por API, el `LedDriver` se reconfigura en caliente.
- Si hay un perfil GPIO por defecto definido, ese perfil se aplica en cada arranque y sustituye la `gpio-config.json` activa.

## Perfiles GPIO

Los perfiles permiten guardar configuraciones GPIO reutilizables y marcar una de ellas como perfil por defecto de arranque.

Persistencia usada:

- `gpio-profiles.json` para perfiles de usuario
- `startup-profile.json` para el `id` del perfil por defecto

Perfiles integrados actuales:

- preset del build activo (`esp32c3supermini`, `esp32dev` o `esp32s3`, según firmware compilado)
- `gledopto_gl_c_017wl_d`

### `GET /api/v1/profiles/gpio`

Devuelve la colección completa de perfiles.

### `POST /api/v1/profiles/gpio/save`

Guarda o actualiza un perfil GPIO de usuario.

Ejemplo de payload:

```json
{
	"profile": {
		"id": "salon_4_tiras",
		"name": "Salon 4 tiras",
		"description": "Perfil de instalacion del salon",
		"autoApplyOnBoot": true,
		"applyNow": true,
		"gpio": {
			"outputs": [
				{ "pin": 16, "ledCount": 120, "ledType": "ws2812b", "colorOrder": "GRB" },
				{ "pin": 4, "ledCount": 120, "ledType": "ws2812b", "colorOrder": "GRB" }
			]
		}
	}
}
```

Restricciones:

- `id` solo admite `[a-z0-9_-]`
- los perfiles integrados son `readOnly` y no se pueden sobrescribir
- límite actual: `8` perfiles de usuario

### `POST /api/v1/profiles/gpio/apply`

Aplica un perfil existente al runtime actual y guarda esa GPIO activa.

Ejemplo:

```json
{
	"profile": {
		"id": "gledopto_gl_c_017wl_d",
		"setDefault": true
	}
}
```

### `POST /api/v1/profiles/gpio/default`

Fija o limpia el perfil por defecto de arranque.

Ejemplo para fijar:

```json
{
	"profile": {
		"id": "gledopto_gl_c_017wl_d"
	}
}
```

Ejemplo para limpiar:

```json
{
	"profile": {
		"id": ""
	}
}
```

### `POST /api/v1/profiles/gpio/delete`

Elimina un perfil de usuario.

Errores típicos:

```json
{"error":"invalid_profile_id"}
{"error":"profile_not_found"}
{"error":"readonly_profile"}
{"error":"profile_limit_reached"}
{"error":"persistence_failed"}
```

## Configuración debug

### `GET /api/v1/config/debug`

Devuelve solo la sección debug.

Ejemplo de respuesta:

```json
{
	"debug": {
		"enabled": true,
		"heartbeatMs": 5000
	}
}
```

### `PATCH /api/v1/config/debug`

Acepta payload con raíz `debug` o payload directo con los campos.

Ejemplos válidos:

```json
{
	"debug": {
		"enabled": true,
		"heartbeatMs": 1000
	}
}
```

```json
{
	"enabled": true,
	"heartbeatMs": 1000
}
```

Ejemplo de respuesta:

```json
{
	"updated": true,
	"debug": {
		"enabled": true,
		"heartbeatMs": 1000
	}
}
```

Errores posibles:

```json
{"error":"invalid_payload"}
{"error":"invalid_json"}
{"error":"persistence_failed"}
{"error":"invalid_heartbeat_ms"}
```

## Configuración completa

### `GET /api/v1/config/all`

Exporta toda la configuración combinada (`network`, `debug`, `gpio`).

La salida se serializa con formato pretty-print.

### `POST /api/v1/config/all`

Importa la configuración completa y valida primero en candidatos antes de tocar la configuración real.

Comportamiento:

- valida `network` + `debug`
- valida `gpio`
- si algo falla, no aplica nada
- si todo es válido, persiste y aplica WiFi

Ejemplo de payload:

```json
{
	"network": {
		"wifi": {
			"mode": "ap_sta",
			"apAvailability": "always",
			"connection": {
				"ssid": "MiWifi",
				"password": "clave-segura"
			}
		},
		"ip": {
			"sta": {
				"mode": "dhcp"
			}
		},
		"dns": {
			"hostname": "duxman-led-c3"
		}
	},
	"debug": {
		"enabled": true,
		"heartbeatMs": 1000
	},
	"gpio": {
		"outputs": [
			{
				"id": 0,
				"pin": 8,
				"ledCount": 60,
				"ledType": "ws2812b",
				"colorOrder": "GRB"
			}
		]
	}
}
```

Ejemplo de respuesta:

```json
{
	"imported": true,
	"config": {
		"network": {},
		"debug": {},
		"gpio": {}
	}
}
```

Errores posibles:

```json
{"error":"invalid_payload"}
{"error":"invalid_json"}
{"error":"network_<codigo_de_validacion>"}
{"error":"gpio_<codigo_de_validacion>"}
{"error":"persistence_failed"}
```

## Release

### `GET /api/v1/release`

Devuelve metadatos de release compilados en binario.

Campos esperados:

- `version`
- `releaseDate`
- `branch`
- `board`
- `repositoryUrl`
- `repositoryGitUrl`

Ejemplo:

```json
{
	"version": "0.3.6-beta",
	"releaseDate": "2026-04-12",
	"branch": "main",
	"board": "esp32c3supermini",
	"repositoryUrl": "https://github.com/duxman/DUXMAN-LED-NEXT",
	"repositoryGitUrl": "https://github.com/duxman/DUXMAN-LED-NEXT.git"
}
```

## Hardware

### `GET /api/v1/hardware`

Devuelve información de hardware detectada en runtime a partir de la propia placa.

Campos actuales:

- `chipModel`
- `chipRevision`
- `cores`
- `cpuFreqMHz`
- `hasWifi`
- `hasBluetoothClassic`
- `hasBluetoothLe`
- `flashSizeBytes`
- `flashSizeMb`
- `flashSpeedHz`
- `mac`

Ejemplo de respuesta:

```json
{
	"chipModel": "ESP32-D0WD-V3",
	"chipRevision": 3,
	"cores": 2,
	"cpuFreqMHz": 240,
	"hasWifi": true,
	"hasBluetoothClassic": true,
	"hasBluetoothLe": true,
	"flashSizeBytes": 4194304,
	"flashSizeMb": 4,
	"flashSpeedHz": 80000000,
	"mac": "88:57:21:BB:C2:C4"
}
```

## OpenAPI-like embebido

### `GET /api/v1/openapi.json`

Devuelve una descripción resumida de la API generada por el firmware.

No reemplaza esta documentación, pero sirve como índice embebido para herramientas ligeras.

## Errores comunes

| Código | Significado |
|---|---|
| `invalid_payload` | Falta body o body vacío |
| `invalid_json` | JSON mal formado |
| `method_not_allowed` | Método HTTP no soportado |
| `not_found` | Ruta no encontrada |
| `persistence_failed` | Fallo al guardar en LittleFS |
| `wifi_apply_failed` | Fallo al reaplicar configuración WiFi |

Además, las validaciones de `network` y `gpio` devuelven errores específicos en formato string.

## Interfaz Serial

La interfaz Serial reutiliza la misma semántica de la API HTTP.

Ejemplos:

```text
GET /api/v1/state
PATCH /api/v1/state {"power":false}
GET /api/v1/config/network
PATCH /api/v1/config/network {"network":{"wifi":{"mode":"ap"}}}
GET /api/v1/config/microphone
PATCH /api/v1/config/microphone {"microphone":{"enabled":false,"source":"generic_i2c","profileId":"gledopto_gl_c_017wl_d","sampleRate":16000,"fftSize":512,"gainPercent":100,"noiseFloorPercent":8,"pins":{"din":26,"ws":5,"bclk":21}}}
GET /api/v1/config/gpio
PATCH /api/v1/config/gpio {"gpio":{"outputs":[{"id":0,"pin":8,"ledCount":60,"ledType":"ws2812b","colorOrder":"GRB"}]}}
GET /api/v1/profiles/gpio
POST /api/v1/profiles/gpio/save {"profile":{"id":"mi_perfil","name":"Mi perfil","gpio":{"outputs":[{"id":0,"pin":8,"ledCount":60,"ledType":"ws2812b","colorOrder":"GRB"}]},"autoApplyOnBoot":true}}
POST /api/v1/profiles/gpio/apply {"profile":{"id":"gledopto_gl_c_017wl_d","setDefault":true}}
POST /api/v1/profiles/gpio/default {"profile":{"id":"gledopto_gl_c_017wl_d"}}
POST /api/v1/profiles/gpio/delete {"profile":{"id":"mi_perfil"}}
GET /api/v1/config/debug
PATCH /api/v1/config/debug {"debug":{"enabled":true,"heartbeatMs":1000}}
GET /api/v1/config/all
POST /api/v1/config/all {"network":{},"debug":{},"microphone":{},"gpio":{}}
GET /api/v1/hardware
GET /api/v1/release
```

## Páginas HTML relacionadas

Además de la API JSON, el firmware expone páginas de ayuda y configuración:

- `/`
- `/config`
- `/config/network`
- `/config/microphone`
- `/config/gpio`
- `/config/profiles`
- `/config/debug`
- `/config/manual`
- `/api`
- `/api/state`
- `/api/config/network`
- `/api/config/microphone`
- `/api/config/gpio`
- `/api/profiles/gpio`
- `/api/config/debug`
- `/api/config/all`
- `/api/hardware`
- `/api/release`
- `/version`

## Estado de esta documentación

Este documento describe la implementación actual del firmware `v0.3.6-beta` (Fase 4A-4C + catalogo de efectos dinamicos + audio reactivo I2S base + Fase 3A de paletas predefinidas + editor de paletas de usuario). Debe actualizarse junto con `README.md` cuando se añadan o cambien endpoints, payloads o reglas de validación.
