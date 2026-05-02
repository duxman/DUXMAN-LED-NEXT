# DUXMAN LED-NEXT Wiki

**Welcome to DUXMAN LED-NEXT documentation** — a modular ESP32-based LED controller with support for multiple control protocols, embedded web UI, and audio-reactive effects.

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
- 🇪🇸 **[Spanish Documentation](../es/)** — Spanish version

## ❓ Frequently Asked Questions

For quick answers to common questions, see [FAQ.md](FAQ.md)

## 📋 Documentation Consolidation Summary

Last documentation update: [View Summary](CONSOLIDATION_SUMMARY.md)

## 🎯 Getting Started

1. **New User**: Read [System Architecture](Core/Architecture.md) to understand the structure
2. **Developer**: Start with [REST API v1](Core/API-v1.md) and [General Roadmap](Development/Roadmap.md)
3. **Configuration**: Check [Storage and Memory](Configuration/Storage-and-Memory.md)
4. **Questions**: Review [FAQ.md](FAQ.md)

---

**Last Updated:** 2026-05-02
**Version:** LED-NEXT v0.6.3-alpha

---

## 📊 Documentation by Category

| Category | Files | Description |
|----------|------:|-------------|
| **Core** | 2 | System architecture and API |
| **Features** | 3 | Effects, palettes, and GPIO configuration |
| **Configuration** | 1 | Storage and memory management |
| **UI** | 1 | Web user interface guide |
| **Development** | 3 | Roadmap and implementation strategy |
| **Reference** | 1 | Hardware reference |

---

## 🔗 Useful Links

- **GitHub repository:** DUXMAN-LED-NEXT
- **Release notes:** [RELEASE_NOTES.en.md](../../RELEASE_NOTES.en.md)
- **Issues and bugs:** Open a GitHub issue with logs and context
- **Build config:** See `platformio.ini` in the repository

---

## 📝 Notes

- This wiki is the centralized source of project documentation.
- Active documentation is maintained here to simplify collaboration and GitHub publishing.
- Legacy material is kept under `Archive/` for local reference only.

Last updated: 2026-05-02