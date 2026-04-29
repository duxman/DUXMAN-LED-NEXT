# Preguntas frecuentes (FAQ)

**¿Qué placas ESP32 están soportadas?**
- ESP32-C3-DevKitM-1, ESP32 DevKit, ESP32-S3-DevKitC-1

**¿Cuántas salidas LED puedo configurar?**
- Hasta 4 salidas independientes por perfil GPIO

**¿Puedo crear mis propias paletas de color?**
- Sí, desde la UI o la API (`UserPaletteService`)

**¿Cómo se actualiza el firmware?**
- Actualmente por USB/Serial (OTA en roadmap)

**¿Qué backend LED es preferente?**
- NeoPixelBus (DMA/I2S/RMT), FastLED como alternativo

**¿Puedo controlar el dispositivo por red y por Serial?**
- Sí, la API es idéntica en HTTP y Serial

**¿Dónde está la documentación técnica?**
- En esta wiki, el README y los docs del repositorio

**¿Cómo reporto un bug o solicito una feature?**
- Abre un issue en GitHub con logs y pasos claros