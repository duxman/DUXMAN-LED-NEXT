# Frequently Asked Questions (FAQ)

**Which ESP32 boards are supported?**
- ESP32-C3-DevKitM-1, ESP32 DevKit, and ESP32-S3-DevKitC-1.

**How many LED outputs can I configure?**
- Up to 4 independent outputs per GPIO profile.

**Can I create my own color palettes?**
- Yes, from either the UI or the API through `UserPaletteService`.

**How is firmware updated?**
- Currently over USB/Serial. OTA is still on the roadmap.

**Which LED backend is preferred?**
- NeoPixelBus is the primary backend. FastLED is available as an alternative.

**Can I control the device over the network and over Serial?**
- Yes. The API is mirrored across HTTP and Serial commands.

**Where is the technical documentation?**
- In this wiki, the repository README files, and the release/changelog documentation.

**How do I report a bug or request a feature?**
- Open a GitHub issue with logs, reproduction steps, and context.