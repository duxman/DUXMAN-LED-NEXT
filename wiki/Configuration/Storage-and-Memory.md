# Storage & Memory

Diseno de persistencia y uso de memoria para DUXMAN-LED-NEXT (estado v0.4.2-beta).

## Objetivos

- Evitar corrupcion de configuracion ante reinicios inesperados.
- Reducir desgaste de flash con escrituras diferidas.
- Mantener latencia baja en control y render evitando I/O bloqueante en ruta critica.

## Almacenamiento

Backend principal:

- LittleFS para configuraciones, perfiles, paletas de usuario y recursos UI.

Estrategia actual de UI:

- Plantillas HTML/CSS en data/ui desplegadas en LittleFS.
- Fallback embebido en firmware para tolerancia a archivos faltantes.

Requisito de build:

- platformio.ini define board_build.filesystem = littlefs para que uploadfs genere littlefs.bin correcto.

## Politica de persistencia

Persistencia diferida:

- Los cambios se aplican en RAM y se encolan por PersistenceSchedulerService.
- Se realiza coalescing de cambios para evitar escritura por cada ajuste de slider.

Persistencia gestionada por servicios:

- StorageService: serializacion/deserializacion de configuracion base.
- ProfileService: perfiles completos y perfil por defecto.
- UserPaletteService: CRUD y almacenamiento de paletas de usuario.
- EffectPersistenceService: startup effect y secuencias.

## Atomicidad y robustez

Principios aplicados:

- Validacion previa de payload/config antes de comprometer cambios globales.
- En configuraciones de red, respuesta HTTP antes de reaplicar WiFi para minimizar reset de conexion percibido por cliente.
- Export de config/all ensamblado por secciones para bajar pico de memoria y reducir riesgo de reset por presion RAM.

## Memoria RAM

Fuentes principales de consumo:

- Buffers de render LED por salida/tipo de backend.
- Buffer de captura de AudioService y estado compartido de audio.
- Documentos JSON temporales en API y configuracion.

Medidas actuales:

- Evitar JsonDocument agregado grande en GET /api/v1/config/all.
- AudioService ajustado a buffers mas cortos y proceso frecuente para respuesta en vivo con uso controlado.
- Tareas separadas control/render para evitar bloqueos cruzados.

## Recomendaciones operativas

- Antes de desplegar cambios de UI en data/ui, ejecutar uploadfs ademas del upload de firmware.
- Mantener heartbeat y logs de debug desactivados en operacion normal para reducir ruido serial.
- Validar configuracion de microfono y red en hardware real tras cambios de pipeline.

## Riesgos abiertos

- Ajustes de sensibilidad de audio dependen del microfono y del ruido ambiente.
- Posible contencion puntual de mutex en estados de alta actividad API + render.
- Pendiente estrategia OTA final compatible con particiones actuales y crecimiento del firmware.
