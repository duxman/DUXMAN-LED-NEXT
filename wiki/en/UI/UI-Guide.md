# UI Guide

Functional guide to the main screens and fields in DUXMAN-LED-NEXT.

## Main Screens

### Home

Goal:

- Quickly change colors, palette, effect, visual parameters, and output state.
- Manage startup effect persistence and the sequence list.
- Inspect runtime JSON when diagnostics are needed.

Blocks:

- Color selection: background/off, color 1, color 2, color 3, predefined palette, apply palette.
- Effect selection: effect selector, apply, save startup, reload.
- Parameters and presets: section count, speed, level, global brightness, visual presets.
- Playlist configuration: output state, new entry duration, add to list, reload list.
- Effect persistence: startup summary, stored sequence entries, and diagnostic JSON output.
- Runtime state: collapsible block with current status.

Fields:

- Background / off: base color or perceived blackout state.
- Color 1 / 2 / 3: primary render colors when palette mode is manual.
- Predefined palette: system or user palette selection.
- Effect: main rendering engine output.
- Section count: logical partitioning for block/segment patterns.
- Effect speed: only used on effects that support speed.
- Effect level: intensity or structural scale of the effect.
- Global brightness: overall device brightness.
- Microphone audio: automatic indicator depending on the active effect type.

### Configuration

Goal:

- Access specialized editors for network, microphone, GPIO, profiles, palettes, debug, and manual JSON.

### Network

Goal:

- Configure WiFi, hostname, NTP, AP IP, and STA IP.

Fields:

- WiFi mode: `ap`, `sta`, or `ap_sta`.
- AP availability: `always` or `untilStaConnected`.
- STA SSID / password: client-network credentials.
- DNS hostname: name exposed on the local network.
- Sync at boot / NTP server: time configuration.
- AP / STA IP mode: `dhcp` or `static`.
- Address / Gateway / Subnet / DNS: addressing parameters.

### Microphone

Goal:

- Adjust sensitivity and I2S microphone pins.

Fields:

- `enabled`: enables capture.
- `source`: currently supported source.
- `profileId`: pin/configuration profile.
- `sampleRate`: sampling rate.
- `fftSize`: analysis window size.
- `gainPercent`: input gain.
- `noiseFloorPercent`: base ambient noise level.
- `noiseGateKnee`: soft activation threshold.
- `agcResponsePercent`: auto-gain response speed.
- `din / ws / bclk`: I2S pins.

### GPIO

Goal:

- Define LED outputs and software power limiting.

Fields:

- GPIO Pin: strip or digital LED output pin.
- LED Count: length of the addressable output.
- LED Type: chipset or output type.
- Color Order: RGB order or fixed digital color.
- Enable limit: enables estimated power limiting.
- Max total current: estimated budget cap for the whole setup.
- Current per LED: estimated consumption per LED at full white.

### Profiles

Goal:

- Save full snapshots and reuse device configurations.

Operations:

- Save as profile.
- Apply profile.
- Edit profile JSON.
- Clone profile.
- Delete user profile.

### Palettes

Goal:

- Manage system and user palettes.

Fields:

- ID: stable technical key.
- Display name: text shown to the user.
- Style: visual classification.
- Description: optional context.
- Color 1 / 2 / 3: main palette colors.

### Debug

Goal:

- Control debug traces and UI helper utilities.

Fields:

- Debug enabled: enables diagnostic messages.
- Heartbeat ms: serial trace interval.
- Show JSON responses: browser-stored UI preference.

### Manual JSON

Goal:

- Export, validate, edit, and import the entire device configuration.

Operations:

- Load from device.
- Apply to device.
- Download JSON.
- Validate local syntax only.
- Load file into the editor.

## Operational Notes

- Full import is applied only if every JSON section is valid.
- Network changes may trigger a device reconnect if they modify WiFi mode or addressing.
- Audio effects automatically enable microphone usage at runtime.
