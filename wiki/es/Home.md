# DUXMAN LED-NEXT Wiki

**Bienvenido a la documentación de DUXMAN LED-NEXT** — Un controlador LED profesional modular basado en ESP32 con soporte completo para múltiples protocolos de control y efectos de audio reactivos.

## 📚 Documentación por Categoría

### 🏗️ **Núcleo del Sistema** ([Core/](Core/))
Arquitectura técnica, especificaciones de API y decisiones de diseño:

- **[Arquitectura del Sistema](Core/Architecture.md)** — Descripción completa del diseño de firmware y componentes del sistema
- **[API REST v1](Core/API-v1.md)** — Especificación completa de endpoints REST para control remoto

### ⚙️ **Configuración** ([Configuration/](Configuration/))
Guía de almacenamiento, persistencia y gestión de configuración:

- **[Almacenamiento y Memoria](Configuration/Storage-and-Memory.md)** — Esquema de almacenamiento en LittleFS, distribución de RAM/Flash

### ✨ **Características** ([Features/](Features/))
Documentación de funciones principales del sistema:

- **[Efectos LED](Features/Effects.md)** — Catálogo de efectos disponibles, parámetros, modos
- **[Paletas de Colores](Features/Palettes.md)** — Sistema de paletas, creación personalizada, estructura
- **[Perfiles GPIO](Features/GPIO-Profiles.md)** — Configuración de pines, perfiles predefinidos, casos de uso

### 🚀 **Desarrollo** ([Development/](Development/))
Guía para desarrolladores, roadmap y planes de evolución:

- **[Roadmap General](Development/Roadmap.md)** — Plan de desarrollo a largo plazo, versiones futuras
- **[Roadmap de Efectos](Development/Effects-Implementation-Roadmap.md)** — Efectos planificados, estado de implementación
- **[Integración VoltageOptimizer](Development/VoltageOptimizer-Integration.md)** — Optimización de consumo de energía por software

### 📖 **Referencia** ([Reference/](Reference/))
Documentación técnica de hardware y controladores:

- **[Hardware: Controlador Gledopto](Reference/Hardware-Controller-Gledopto.md)** — Especificación pinout, compatibilidades

### 🎨 **Interfaz de Usuario** ([UI/](UI/))
Guía de la interfaz web:

- **[Guía de UI](UI/UI-Guide.md)** — Componentes, navegación, configuración desde web

## 🌍 Idiomas

- 🇬🇧 **[English Documentation](../en/)** — English version
- 🇪🇸 **[Documentación en Español](./)** — Versión en Español

## ❓ Preguntas Frecuentes

Para respuestas rapidas a preguntas comunes, consulta [FAQ.md](FAQ.md)

## 📋 Resumen de Consolidación

Última actualización de documentación: [Ver Resumen](../CONSOLIDATION_SUMMARY.md)

## 🎯 Primeros Pasos

1. **Nuevo usuario**: Lee [Arquitectura del Sistema](Core/Architecture.md) para comprender la estructura
2. **Desarrollador**: Comienza con [API REST v1](Core/API-v1.md) y [Roadmap General](Development/Roadmap.md)
3. **Configuración**: Consulta [Almacenamiento y Memoria](Configuration/Storage-and-Memory.md)
4. **Problemas**: Revisa [FAQ.md](FAQ.md)

---

**Ultima actualizacion:** 2026-05-02
**Version:** LED-NEXT v0.6.3-alpha

---

## 🔗 Enlaces utiles

- **Repositorio GitHub:** DUXMAN-LED-NEXT
- **Notas de la version:** [RELEASE_NOTES.md](../../RELEASE_NOTES.md)
- **Issues y bugs:** Abre un issue en GitHub con logs y contexto
- **Compilacion:** Ver `platformio.ini` en el repositorio
