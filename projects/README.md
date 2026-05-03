# Proyectos del workspace

Estructura desacoplada por tipo de aplicacion:

- `firmware-platformio/`: firmware ESP32 con PlatformIO, UI LittleFS y scripts de flash.
- `mockup-python/`: servidor mock API/UI para pruebas sin hardware.
- `android-app/`: aplicacion Android (Gradle, Java/Kotlin).

## Comandos rapidos

Firmware:

```bash
cd projects/firmware-platformio
pio run -e esp32dev
powershell -ExecutionPolicy Bypass -File tools/flash.ps1 -Profile esp32dev -Action upload
```

Mockup:

```bash
cd projects/mockup-python
python -m pip install -r requirements.txt
python mock_server.py --host 127.0.0.1 --port 8080
```

Android:

```bash
cd projects/android-app
./gradlew assembleDebug
```
