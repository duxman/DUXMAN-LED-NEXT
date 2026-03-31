# Tools

Scripts utilitarios para build, checks y automatizaciones futuras.

## flash.ps1

Script para compilar, subir y monitorizar firmware con deteccion automatica de puerto serie.

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
