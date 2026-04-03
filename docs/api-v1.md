# API v1

Proyecto: `DUXMAN-LED-NEXT`.

Documentación de la API real implementada actualmente en firmware.

## Resumen

- Base HTTP: `http://<ip-dispositivo>/api/v1/*`
- Puerto HTTP: `80`
- Interfaz Serial: mismos comandos que HTTP, una línea por petición
- Formato: JSON
- Métodos soportados:
	- `GET` para lectura
	- `PATCH` para actualización parcial
	- `POST` como alias de `PATCH` en algunos endpoints

## Endpoints disponibles

| Método | Ruta | Descripción |
|---|---|---|
| `GET` | `/api/v1/state` | Obtener estado runtime actual |
| `PATCH` | `/api/v1/state` | Actualizar estado runtime |
| `POST` | `/api/v1/state` | Alias de `PATCH` |
| `GET` | `/api/v1/config/network` | Obtener configuración de red |
| `PATCH` | `/api/v1/config/network` | Actualizar configuración de red |
| `POST` | `/api/v1/config/network` | Alias de `PATCH` |
| `GET` | `/api/v1/config/microphone` | Obtener configuración de micrófono (`generic_i2c`) |
| `PATCH` | `/api/v1/config/microphone` | Actualizar configuración de micrófono |
| `POST` | `/api/v1/config/microphone` | Alias de `PATCH` |
| `GET` | `/api/v1/config/gpio` | Obtener configuración de salidas LED |
| `PATCH` | `/api/v1/config/gpio` | Actualizar configuración GPIO/LED |
| `POST` | `/api/v1/config/gpio` | Alias de `PATCH` |
| `GET` | `/api/v1/profiles/gpio` | Listar perfiles GPIO integrados y guardados |
| `POST` | `/api/v1/profiles/gpio/save` | Guardar o actualizar un perfil GPIO de usuario |
| `PATCH` | `/api/v1/profiles/gpio/save` | Alias de `POST` |
| `POST` | `/api/v1/profiles/gpio/apply` | Aplicar un perfil GPIO al runtime |
| `PATCH` | `/api/v1/profiles/gpio/apply` | Alias de `POST` |
| `POST` | `/api/v1/profiles/gpio/default` | Fijar o limpiar perfil GPIO por defecto |
| `PATCH` | `/api/v1/profiles/gpio/default` | Alias de `POST` |
| `POST` | `/api/v1/profiles/gpio/delete` | Eliminar un perfil GPIO de usuario |
| `PATCH` | `/api/v1/profiles/gpio/delete` | Alias de `POST` |
| `GET` | `/api/v1/config/debug` | Obtener configuración debug |
| `PATCH` | `/api/v1/config/debug` | Actualizar configuración debug |
| `POST` | `/api/v1/config/debug` | Alias de `PATCH` |
| `GET` | `/api/v1/config/all` | Exportar configuración completa |
| `POST` | `/api/v1/config/all` | Importar configuración completa validando todo antes de aplicar |
| `GET` | `/api/v1/hardware` | Obtener capacidades de hardware detectadas en runtime |
| `GET` | `/api/v1/release` | Obtener versión/release compilada en binario |
| `GET` | `/api/v1/openapi.json` | OpenAPI-like embebido |

## Estado runtime

### `GET /api/v1/state`

Devuelve el estado actual del motor.

Ejemplo de respuesta:

```json
{
	"power": true,
	"brightness": 128,
	"effectId": 0,
	"effect": "fixed",
	"sectionCount": 3,
	"primaryColors": [
		"#FF4D00",
		"#FFD400",
		"#00B8D9"
	],
	"backgroundColor": "#000000"
}
```

### `PATCH /api/v1/state`

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
- `effectId`: `0` fijo, `1` degradado
- `effect`: alias string (`fixed`, `gradient`)
- `sectionCount`: número de secciones repartidas sobre cada salida
- `primaryColors`: array de 3 colores hex `#RRGGBB`
- `backgroundColor`: color de fondo/off `#RRGGBB`

Errores típicos:

```json
{"error":"invalid_payload"}
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
	"version": "0.3.3-beta",
	"releaseDate": "2026-04-03",
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

Este documento describe la implementación actual del firmware `v0.3.3-beta` (Fase 4A-4C + catalogo de efectos dinamicos + audio reactivo I2S base + calibrado de speed/level). Debe actualizarse junto con `README.md` cuando se añadan o cambien endpoints, payloads o reglas de validación.
