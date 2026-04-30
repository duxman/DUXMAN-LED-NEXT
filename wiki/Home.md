# 📚 DUXMAN-LED-NEXT Wiki

Bienvenido a la wiki oficial y centralizada de DUXMAN-LED-NEXT. Toda la documentación se encuentra aquí organizada por temas.

## 🚀 Inicio Rápido

- [README General](../README.md) — Descripción del proyecto
- [Changelog](../CHANGELOG.md) — Historial de cambios
- [FAQ](./FAQ.md) — Preguntas frecuentes

---

## 📖 Documentación Principal

### Core

Documentación fundamental sobre la arquitectura y API del sistema.

- [Arquitectura](./Core/Architecture.md) — Diseño general, tareas, servicios y componentes
- [API REST v1](./Core/API-v1.md) — Endpoints disponibles y detalles de integración

### Features

Características principales del firmware.

- [Catálogo de Efectos](./Features/Effects.md) — Todos los efectos visuales y audio-reactivos disponibles
- [Paletas de Color](./Features/Palettes.md) — Gestión de paletas predefinidas y de usuario
- [Perfiles GPIO](./Features/GPIO-Profiles.md) — Configuración de salidas LED y perfiles

### Configuración

- [Almacenamiento y Memoria](./Configuration/Storage-and-Memory.md) — Persistencia, LittleFS y gestión de RAM

### Interfaz de Usuario

- [Guía de Pantallas y Campos](./UI/UI-Guide.md) — Descripción funcional de todas las pantallas UI

### Desarrollo

Planificación y roadmap del proyecto.

- [Roadmap General](./Development/Roadmap.md) — Fases, decisiones técnicas clave y siguientes objetivos
- [Roadmap de Implementación de Efectos](./Development/Effects-Implementation-Roadmap.md) — Estrategia de implementación de nuevos efectos

### Referencia

Documentación técnica de referencia.

- [Hardware: Controlador Gledopto GL-C-017WL-D](./Reference/Hardware-Controller-Gledopto.md) — Pinout y configuración del controlador

---

## 📦 Estructura de Carpetas

```
wiki/
├── Home.md                          (Este archivo)
├── FAQ.md                           (Preguntas frecuentes)
├── Core/                            (Arquitectura y API)
├── Features/                        (Características principales)
├── Configuration/                   (Configuración del sistema)
├── UI/                              (Documentación de interfaz)
├── Development/                     (Roadmap y planificación)
├── Reference/                       (Documentación de referencia)
└── Archive/                         (Documentación obsoleta)
```

---

## 🔗 Enlaces Útiles

- **Repositorio GitHub:** [DUXMAN-LED-NEXT](https://github.com/)
- **Issues y Bugs:** Abre un issue en GitHub con logs y contexto
- **Compilación:** Ver `platformio.ini` en el repositorio

---

## 📝 Notas

Esta wiki es el único repositorio centralizado de documentación. Toda la información relevante se mantiene aquí para facilitar el trabajo colaborativo y garantizar que TODO sea accesible desde GitHub.

Última actualización: 2026-04-30