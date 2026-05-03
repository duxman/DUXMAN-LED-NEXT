# DUXMAN LED Next — App Android

App Android para controlar y monitorizar los controladores DUXMAN LED Next desde la red local.

## Funcionalidades

- **Lista de controladores**: muestra todos los controladores configurados con su estado (en línea / fuera de línea).
- **Comprobación de estado**: al abrir la app y al arrastrar para actualizar, consulta `/api/v1/state` de cada controlador para verificar si está accesible.
- **Visor web a pantalla completa**: al pulsar sobre un controlador se abre su interfaz web integrada en la app.
- **Añadir controladores**: botón flotante (＋) → introducir nombre e IP/hostname.
- **Eliminar controladores**: botón de papelera en cada tarjeta → confirmación.

## Requisitos

- Android 7.0+ (API 24)
- Red local WiFi compartida con los controladores
- Los controladores deben estar accesibles en HTTP (puerto 80)

## Compilar

Requiere Android Studio (Hedgehog o superior) o Gradle 8.4+ con JDK 17.

```bash
cd projects/android-app
./gradlew assembleDebug
```

El APK de debug se genera en `app/build/outputs/apk/debug/`.

## Estructura del proyecto

```
projects/android-app/
├── app/src/main/
│   ├── AndroidManifest.xml
│   ├── java/com/duxman/lednext/
│   │   ├── data/Controller.kt          # Modelo de datos
│   │   ├── network/StatusChecker.kt    # Comprobación de estado HTTP
│   │   ├── repository/ControllerRepository.kt  # Persistencia (SharedPreferences)
│   │   └── ui/
│   │       ├── MainActivity.kt         # Lista de controladores
│   │       ├── MainViewModel.kt        # ViewModel (coroutines)
│   │       ├── ControllerAdapter.kt    # RecyclerView adapter
│   │       └── WebViewActivity.kt      # Visor web a pantalla completa
│   └── res/
│       ├── layout/                     # Layouts XML
│       ├── values/                     # Colores, strings, temas
│       ├── drawable/                   # Icono de estado, launcher
│       └── menu/                       # Menú de acción (actualizar)
└── build.gradle / settings.gradle
```

## API del firmware

La app usa el endpoint `GET /api/v1/state` para determinar si el controlador está en línea.
Al abrir el visor web, carga `http://<ip>/` donde se sirve la interfaz completa del firmware.
