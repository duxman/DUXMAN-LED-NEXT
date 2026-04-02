# Tools

Scripts utilitarios para build, checks y automatizaciones futuras.

## flash.ps1

Script para compilar, subir y monitorizar firmware con deteccion automatica de puerto serie.
La deteccion combina `Win32_SerialPort` y `Get-PnpDevice -Class Ports`, porque algunos adaptadores USB-serie pueden no aparecer siempre en ambas fuentes. La autodeteccion prioriza puertos con estado `OK` y adaptadores reconocibles como `CH340`, `CP210x`, `FTDI` o `UART`.

Ejemplos desde la raiz del repo:

1. Listar puertos:
	powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -ListPorts
2. Compilar perfil C3:
	powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -Profile esp32c3supermini -Action build
3. Subir detectando puerto automaticamente:
	powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -Profile esp32dev -Action upload
4. Subir y abrir monitor:
	powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -Profile esp32s3 -Action upload-monitor
5. Forzar puerto manual:
	powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1 -Profile esp32dev -Action upload -Port COM12

## esptool

Tambien se puede consultar la placa conectada directamente con `esptool` usando el Python del sistema.

Ejemplos:

1. Identificar chip:
	python -m esptool --chip auto --port COM7 chip-id
2. Leer flash detectada:
	python -m esptool --chip auto --port COM7 flash-id
