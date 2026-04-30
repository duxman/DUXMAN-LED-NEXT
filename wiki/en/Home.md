# DUXMAN LED-NEXT Wiki

**Welcome to DUXMAN LED-NEXT Documentation** — A professional modular LED controller based on ESP32 with full support for multiple control protocols and audio-reactive effects.

## 📚 Documentation by Category

### 🏗️ **System Core** ([Core/](Core/))
Technical architecture, API specifications, and design decisions:

- **[System Architecture](Core/Architecture.md)** — Complete description of firmware design and system components
- **[REST API v1](Core/API-v1.md)** — Complete specification of REST endpoints for remote control

### ⚙️ **Configuration** ([Configuration/](Configuration/))
Storage, persistence, and configuration management guide:

- **[Storage and Memory](Configuration/Storage-and-Memory.md)** — LittleFS storage schema, RAM/Flash distribution

### ✨ **Features** ([Features/](Features/))
Documentation of main system features:

- **[LED Effects](Features/Effects.md)** — Catalog of available effects, parameters, modes
- **[Color Palettes](Features/Palettes.md)** — Palette system, custom creation, structure
- **[GPIO Profiles](Features/GPIO-Profiles.md)** — Pin configuration, predefined profiles, use cases

### 🚀 **Development** ([Development/](Development/))
Developer guide, roadmap and evolution plans:

- **[General Roadmap](Development/Roadmap.md)** — Long-term development plan, future versions
- **[Effects Implementation Roadmap](Development/Effects-Implementation-Roadmap.md)** — Planned effects, implementation status
- **[VoltageOptimizer Integration](Development/VoltageOptimizer-Integration.md)** — Software power consumption optimization

### 📖 **Reference** ([Reference/](Reference/))
Hardware and controller technical documentation:

- **[Hardware: Gledopto Controller](Reference/Hardware-Controller-Gledopto.md)** — Pinout specification, compatibility

### 🎨 **User Interface** ([UI/](UI/))
Web interface guide:

- **[UI Guide](UI/UI-Guide.md)** — Components, navigation, web configuration

## 🌍 Languages

- 🇬🇧 **[English Documentation](./)** — English version
- 🇪🇸 **[Documentación en Español](../es/)** — Spanish version

## ❓ Frequently Asked Questions

For quick answers to common questions, see [FAQ.md](../FAQ.md)

## 📋 Documentation Consolidation Summary

Last documentation update: [View Summary](../CONSOLIDATION_SUMMARY.md)

## 🎯 Getting Started

1. **New User**: Read [System Architecture](Core/Architecture.md) to understand the structure
2. **Developer**: Start with [REST API v1](Core/API-v1.md) and [General Roadmap](Development/Roadmap.md)
3. **Configuration**: Check [Storage and Memory](Configuration/Storage-and-Memory.md)
4. **Questions**: Review [FAQ.md](../FAQ.md)

---

**Last Updated:** 2024
**Version:** LED-NEXT v1.0+

---

## 📊 Documentación por Categoría

| Categoría | Archivos | Descripción |
|-----------|----------|-------------|
| **Core** | 2 | Arquitectura y API del sistema |
| **Features** | 3 | Efectos, paletas y configuración GPIO |
| **Configuration** | 1 | Almacenamiento y gestión de memoria |
| **UI** | 1 | Guía de interfaz de usuario |
| **Development** | 2 | Roadmap y estrategia de implementación |
| **Reference** | 1 | Documentación técnica de hardware |

---

## 🔗 Enlaces Útiles

- **Repositorio GitHub:** [DUXMAN-LED-NEXT](https://github.com/)
- **Issues y Bugs:** Abre un issue en GitHub con logs y contexto
- **Compilación:** Ver `platformio.ini` en el repositorio

---

## 📝 Notas

- Esta wiki es el único repositorio centralizado de documentación
- Toda la información relevante se mantiene aquí para facilitar el trabajo colaborativo
- La documentación legacy está archivada en `Archive/` (no sincronizada con GitHub)

Última actualización: 2026-04-30