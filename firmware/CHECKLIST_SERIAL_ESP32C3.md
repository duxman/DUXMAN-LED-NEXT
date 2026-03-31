# Checklist de diagnostico serial (ESP32-C3)

## 1) Cable USB con datos
- Sintoma: la placa enciende pero no aparece puerto COM nuevo.
- Accion: prueba otro cable USB (muchos son solo carga).

## 2) Puerto correcto
- Comando: `pio device list`
- Esperado: un COM con VID:PID de Espressif (por ejemplo 303A:1001).

## 3) Puerto libre
- Cierra monitor de PlatformIO, Arduino IDE, Putty, etc.
- Sintoma tipico: `Access denied` o `port busy`.

## 4) Subida forzando puerto
- Comando: `pio run -e upload_esp32c3supermini -t upload --upload-port COM6`
- Cambia COM6 por el que tengas activo.

## 5) Monitor a 115200
- Comando: `pio device monitor -p COM6 -b 115200`
- Esperado: logs `[boot]` al reiniciar.

## 6) Reinicio con monitor abierto
- Pulsa RST o reconecta USB.
- Si ves logs de boot, el canal serial esta bien.

## 7) Boot manual si falla upload
1. Mantener BOOT pulsado.
2. Pulsar RST una vez.
3. Soltar RST.
4. Soltar BOOT tras 1-2 segundos.
5. Reintentar upload.

## 8) Entorno correcto de compilacion
- Usa `esp32c3supermini` o `upload_esp32c3supermini`.

## 9) USB CDC habilitado en build
- Verifica flags:
  - `ARDUINO_USB_MODE=1`
  - `ARDUINO_USB_CDC_ON_BOOT=1`

## 10) Descarte rapido de hardware
- Cambiar puerto USB del PC.
- Probar en otro PC.
- Revisar conector/soldaduras de la placa.

## Prueba funcional rapida
1. Ver `[boot] serial ready` y pasos de arranque.
2. Enviar por serial: `GET /api/v1/state`
3. Enviar: `PATCH /api/v1/config/network {"debug":{"enabled":true}}`
4. Ver logs `[api][serial][debug]` y `[wifi][debug]`.
