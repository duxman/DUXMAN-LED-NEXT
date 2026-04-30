# UI Guide

Guia funcional de pantallas y campos de DUXMAN-LED-NEXT.

## Pantallas principales

### Home

Objetivo:

- Cambiar rapidamente colores, paleta, efecto, parametros visuales y estado de salida.
- Gestionar persistencia del efecto de arranque y la lista de secuencia.
- Ver el estado runtime en JSON cuando se necesita diagnostico.

Bloques:

- Seleccion de colores: fondo/off, color 1, color 2, color 3, paleta predefinida, aplicar paleta.
- Seleccion de efecto: selector de efecto, aplicar, guardar arranque, recargar.
- Parametros y presets: numero de secciones, rapidez, nivel, brillo global, presets visuales.
- Configuracion playlist: estado de salida, duracion de nueva entrada, anadir a lista, recargar lista.
- Persistencia de efectos: resumen de startup, entradas guardadas de secuencia y salida JSON de depuracion.
- Estado runtime: bloque plegable con el estado actual.

Campos:

- Fondo / off: color de base o apagado perceptivo del efecto.
- Color 1 / 2 / 3: colores primarios del render cuando la paleta es manual.
- Paleta predefinida: seleccion de paletas del sistema o usuario.
- Efecto: render principal del motor.
- Numero de secciones: particion logica para patrones por bloques o segmentos.
- Rapidez del efecto: solo se usa en efectos con velocidad.
- Nivel del efecto: intensidad o escala del efecto.
- Brillo global: brillo general del dispositivo.
- Audio del microfono: indicador automatico segun el tipo de efecto.

### Configuracion

Objetivo:

- Entrar a los editores especializados de red, microfono, GPIO, perfiles, paletas, debug y JSON manual.

### Network

Objetivo:

- Configurar WiFi, hostname, NTP, IP AP e IP STA.

Campos:

- Modo WiFi: ap, sta o ap_sta.
- Disponibilidad AP: always o untilStaConnected.
- SSID / Password STA: credenciales de la red cliente.
- Hostname DNS: nombre visible en red local.
- Sincronizar en arranque / servidor NTP: configuracion de tiempo.
- Modo IP AP / STA: dhcp o static.
- Address / Gateway / Subnet / DNS: parametros de direccionamiento.

### Microfono

Objetivo:

- Ajustar sensibilidad y pines del microfono I2S.

Campos:

- enabled: activa captura.
- source: fuente soportada actualmente.
- profileId: perfil de pines/configuracion.
- sampleRate: frecuencia de muestreo.
- fftSize: tamano de ventana de configuracion.
- gainPercent: ganancia de entrada.
- noiseFloorPercent: ruido ambiente base.
- noiseGateKnee: umbral de activacion suave.
- agcResponsePercent: rapidez del ajuste automatico de ganancia.
- din / ws / bclk: pines I2S.

### GPIO

Objetivo:

- Definir salidas LED y limitacion de consumo por software.

Campos:

- GPIO Pin: pin de salida de la tira o LED digital.
- Cantidad LEDs: longitud de la salida direccionable.
- Tipo LED: chipset o tipo de salida.
- Orden Color: orden RGB o color fijo digital.
- Activar limite: habilita control de consumo estimado.
- Corriente maxima total: techo estimado del conjunto.
- Corriente por LED: consumo estimado por LED a blanco pleno.

### Profiles

Objetivo:

- Guardar snapshots completos y reusar configuraciones del dispositivo.

Operaciones:

- Guardar como perfil.
- Activar perfil.
- Editar JSON del perfil.
- Clonar perfil.
- Borrar perfil de usuario.

### Paletas

Objetivo:

- Gestionar paletas del sistema y de usuario.

Campos:

- ID: clave tecnica estable.
- Nombre visual: texto mostrado al usuario.
- Estilo: clasificacion visual.
- Descripcion: contexto opcional.
- Color 1 / 2 / 3: colores principales de la paleta.

### Debug

Objetivo:

- Controlar trazas y utilidades visuales de UI.

Campos:

- Debug habilitado: activa mensajes de depuracion.
- Heartbeat ms: intervalo de traza periodica serial.
- Mostrar respuestas JSON: preferencia guardada en navegador.

### Manual JSON

Objetivo:

- Exportar, validar, editar e importar toda la configuracion del dispositivo.

Operaciones:

- Cargar del dispositivo.
- Aplicar al dispositivo.
- Descargar JSON.
- Solo validar sintaxis local.
- Cargar archivo al editor.

## Notas operativas

- La importacion completa solo se aplica si todas las secciones del JSON son validas.
- Los cambios de red pueden provocar reconexion del dispositivo si alteran el modo WiFi o direccionamiento.
- Los efectos de audio activan automaticamente el uso del microfono a nivel runtime.
