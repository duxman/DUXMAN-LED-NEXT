# Decisiones y lecciones aprendidas

## 2026-03-31
- El proyecto arrancó con un scaffold modular limpio antes de subir a GitHub
- La config GPIO es el siguiente milestone tras tener network/debug completos
- El usuario prefiere comunicación en español
- El usuario quiere memoria local del proyecto (.copilot-memory/) excluida de git
- Flujo de trabajo: el usuario itera conmigo en sesiones, quiere continuidad entre sesiones
- GPIO Config implementado: struct GpioConfig en Config.h/cpp, API REST + Serial, UI HTML, persistencia LittleFS
- Descubiertos archivos duplicados NetworkConfig.h/cpp (legacy), eliminados — Config.h/cpp es el único fuente de verdad
- GpioConfig usa BuildProfile como defaults: el pin/count de compilación se usa como valor inicial
- Validación GPIO incluye: rango de pin, detección de pines input-only (34-39), rango de ledCount (1-1500), tipos de LED válidos
- Patrón: cada sección config tiene su propio archivo JSON en LittleFS (gpio-config.json)

## 2026-04-01
- GPIO rediseñado: de config única a colección de salidas LED (LedOutput[kMaxLedOutputs=4])
- Cada LedOutput tiene: id, pin, ledCount, ledType, colorOrder
- Tipos LED ampliados: ws2812b, ws2811, ws2813, ws2815, sk6812, apa102, tm1814
- Color orders ampliados: GRB, RGB, BRG, RBG, GBR, BGR, RGBW, GRBW
- Validación añadida: detección de pines duplicados entre salidas
- UI dinámica: add/remove de salidas LED, dropdowns para tipo y color order
- JSON format cambiado: `{ "gpio": { "outputs": [...] } }` en vez de campos planos
- Build OK: RAM 12.1%, Flash 69.8% (esp32c3supermini)
- Config manual (import/export): nueva seccion para editar toda la config JSON, descargar .json, importar archivo
- API /api/v1/config/all: GET exporta, POST importa con validación completa previa (candidatos)
- Si el JSON es inválido en cualquier sección, se rechaza TODO sin modificar nada
- Build OK tras manual config: RAM 12.1%, Flash 71.2%
- Tipo LED digital añadido: ledCount=1 forzado, colorOrder=R/G/B/W, UI adapta dropdowns

## 2026-03-31 (sesión versión + partición)
- Versión movida de LittleFS (release-info.json mutable) a BuildProfile.h (constexpr compile-time)
- ReleaseInfo: de struct mutable a namespace read-only con toJson()
- Eliminado release-info.json de StorageService (no se persiste)
- Macros -D en platformio.ini: DUX_FW_VERSION, DUX_FW_DATE, DUX_FW_BRANCH
- Partición cambiada de default (1280 KB app x2 + OTA) a huge_app (3072 KB app, sin OTA)
- Motivo: Flash al 71% con poco implementado → ahora 29.6% con espacio de sobra
- TODO registrado: OTA a futuro
- TODO registrado: Arquitectura hub/nodo (Raspberry Pi hub + ESP32 nodo ejecutor)
- Regla: SIEMPRE actualizar README.md y roadmap.md con cada cambio

## 2026-04-02
- Regla reforzada por el usuario: con cada cambio relevante hay que actualizar README.md, la documentación afectada dentro de docs/ y la memoria local en .copilot-memory
- docs/api-v1.md deja de estar vacío y pasa a formar parte del mantenimiento obligatorio cuando cambie la API
- docs/architecture.md se actualiza para reflejar la arquitectura real actual: flujo de arranque, servicios, persistencia y jerarquía de LedDriver
- Se introduce ProfileService para gestionar perfiles GPIO reutilizables sin mezclar todavía audio I2S ni botones en GpioConfig
- Semántica acordada: si existe startup-profile, se aplica en cada arranque y sobrescribe la gpio-config activa persistida
- Preset integrado inicial añadido para Gledopto GL-C-017WL-D con GPIO seguros por defecto (16, 4, 2); GPIO1/TX queda fuera del preset automático
- Se instala `esptool` en el Python del sistema para poder consultar chips/flash sin depender del entorno interno de PlatformIO
- `tools/flash.ps1` deja de depender solo de `Win32_SerialPort` y pasa a combinar tambien `Get-PnpDevice -Class Ports` para detectar adaptadores como CH340K en COM7
- La autodeteccion de `tools/flash.ps1` ahora prioriza puertos `OK` y adaptadores USB-serie reconocidos para evitar que COM residuales `Unknown` ganen frente al puerto real de trabajo
- Se añade `GET /api/v1/hardware` para exponer chip, flash y MAC en runtime desde firmware mediante el nuevo namespace `HardwareInfo`
- La UI embebida se amplía para cubrir tambien perfiles GPIO y hardware runtime; objetivo: que toda la API implementada tenga acceso visible desde navegacion HTML
