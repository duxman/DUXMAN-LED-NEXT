# DUXMAN-LED-NEXT

Proyecto base para la evolución de un controlador LED moderno en `ESP32-S3`, tomando `WLED` como referencia funcional.

## Objetivo

- Construir una arquitectura modular y mantenible.
- Exponer API versionada desde el inicio (`/api/v1/*`).
- Mantener persistencia robusta con control de desgaste de flash.

## Estado actual

- Estructura inicial del repositorio creada.
- Firmware base con `PlatformIO` y `main.cpp` mínimo.
- Espacios preparados para módulos `core`, `api`, `services`, `effects` y `drivers`.

## API embebida (help)

Con el dispositivo conectado por WiFi, abre en navegador:

1. `http://<ip-dispositivo>/` para ver la pagina de ayuda con endpoints y ejemplos.
2. `http://<ip-dispositivo>/api/v1/openapi.json` para obtener una especificacion OpenAPI-like en JSON.
3. `http://<ip-dispositivo>/api/v1/release` para ver version, fecha de release, rama y URL del repositorio.

## Estructura del repositorio

- `firmware/`: firmware principal (PlatformIO para `ESP32-S3`).
- `web/`: futura interfaz web (MVP de control).
- `docs/`: documentación activa del proyecto.
- `tools/`: scripts y utilidades de soporte.
- `docs referencia/`: documentación de consulta heredada.

## Arranque rápido

1. Abrir el workspace en la carpeta raíz del proyecto (`duxman-led-next`).
2. Instalar `PlatformIO` (CLI o extensión de VS Code).
3. Compilar firmware desde la raíz del repositorio.

## Perfiles de compilación

Perfiles disponibles en `platformio.ini`:

1. `esp32c3supermini` (por defecto)
2. `esp32dev`
3. `esp32s3`

Comandos útiles (desde la raíz del repositorio):

1. `pio run` (usa el perfil por defecto)
2. `pio run -e esp32c3supermini`
3. `pio run -e esp32dev`
4. `pio run -e esp32s3`
5. `pio run -t upload -e esp32c3supermini`
6. `pio run -t upload -e esp32dev`
7. `pio run -t upload -e esp32s3`

### Perfiles de subida por puerto

También hay perfiles dedicados para subir firmware con puerto fijo:

1. `upload_esp32c3supermini` (COM6)
2. `upload_esp32dev` (COM8)
3. `upload_esp32s3` (COM9)

Comandos:

1. `pio run -e upload_esp32c3supermini -t upload`
2. `pio run -e upload_esp32dev -t upload`
3. `pio run -e upload_esp32s3 -t upload`

Si tu puerto es distinto, ajusta `upload_port` y `monitor_port` en `platformio.ini`.

## Debug en VS Code (PlatformIO)

Existe una configuracion de debug interna en `firmware/.vscode/launch.json`.

- Es un archivo autogenerado por PlatformIO.
- Esta ignorado por git en `firmware/.gitignore`.
- Puede quedar desactualizado cuando cambia la estructura del proyecto.

Flujo recomendado:

1. Abrir el proyecto en la raiz (`duxman-led-next`).
2. Ejecutar una compilacion previa (`pio run -e esp32c3supermini`).
3. Lanzar `PIO Debug` desde Run and Debug para que PlatformIO regenere/actualice la configuracion.

Nota: si mantienes el flujo interno de `firmware/`, revisa que el campo `executable` de `firmware/.vscode/launch.json` apunte al `.elf` real generado en tu entorno.

## Opciones WiFi permitidas

La API de red valida estos valores en `PATCH /api/v1/config/network`:

- `network.wifi.mode`: `ap` | `sta` | `ap_sta`
- `network.wifi.apAvailability`: `always` | `untilStaConnected`
- `network.ip.ap.mode`: `dhcp` | `static`
- `network.ip.sta.mode`: `dhcp` | `static`
- `network.dns.hostname`: 1-63 caracteres, solo `[a-zA-Z0-9-]`, sin guion al inicio ni al final
- `debug.enabled`: `true`/`false` (tambien acepta `1`/`0`)

Comportamiento importante:

- Para mantener el AP siempre activo como respaldo, usa `network.wifi.mode=ap_sta` con `network.wifi.apAvailability=always`.
- En modo `sta` o `ap_sta`, `network.wifi.connection.ssid` debe venir informado para iniciar STA.
- Si `network.ip.ap.mode=static`, son obligatorios IPv4 validos en `address`, `gateway` y `subnet` de AP.
- Si `network.ip.sta.mode=static`, son obligatorios IPv4 validos en `address`, `gateway` y `subnet` de STA; `primaryDns` y `secondaryDns` son opcionales pero, si se informan, deben ser IPv4 validas.
- Si `mode=ap_sta` y `apAvailability=untilStaConnected`, el AP se apaga al conectar STA y se vuelve a encender si STA cae.

Ejemplo de payload valido:

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
		"enabled": true
	}
}
```

### Script automatico de flash

Tambien puedes usar el helper de [tools/flash.ps1](tools/flash.ps1) para evitar tocar puertos manualmente:

1. Listar puertos detectados:
	powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -ListPorts
2. Subir con autodeteccion de puerto:
	powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -Profile esp32c3supermini -Action upload
3. Subir y abrir monitor:
	powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -Profile esp32dev -Action upload-monitor

## Próximo paso sugerido

Implementar Sprint 1:
- `CoreState` como fuente única de verdad.
- `GET /api/v1/state`.
- `PATCH /api/v1/state` con validación de payload.

## Agradecimientos

Este proyecto reconoce y agradece especialmente al equipo y la comunidad de `WLED` por su enorme trabajo.
Su enfoque técnico y evolución constante son una inspiración directa para la visión de este proyecto.
