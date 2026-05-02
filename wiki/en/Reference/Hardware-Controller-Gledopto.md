# Gledopto GL-C-017WL-D (ESP32) Configuration Guide

This document describes the confirmed pinout for the Gledopto controller designed for **WLED**.

---

## 1. Data Outputs (LED Outputs)

Configure these pins under **Settings > LED Preferences**. The board supports up to 4 independent outputs.

| Salida | Pin GPIO | Notas |
| :--- | :--- | :--- |
| **LED Output 1** | **16** | Main output (recommended) |
| **LED Output 2** | **4** | Secondary output |
| **LED Output 3** | **2** | Tertiary output |
| **LED Output 4** | **1** | Use only if needed (TX pin) |

---

## 2. Integrated Microphone (Audio Reactive)

To enable audio reactivity, go to **Settings > Sync Interfaces > Audio Input**.

* **Microphone Type:** `Generic I2S (Philips)`
* **I2S SD Pin:** `26`
* **I2S WS Pin:** `5`
* **I2S SCK Pin:** `21`

---

## 3. Buttons and Physical Inputs

Configure these pins under **Settings > LED Preferences > Buttons**.

| Control | Pin GPIO | Tipo (Type) | Función |
| :--- | :--- | :--- | :--- |
| **On-board Button** | **0** | `Pushbutton` | On/Off and Wi-Fi Reset (10s) |
| **IO33 Terminal** | **33** | `Pushbutton` | For an external wall switch |

---

## 4. Technical Notes and Safety

### Power

- **Voltage:** Supports **5V to 24V DC**.
- **Warning:** The power-supply voltage must match the LED strip voltage. The controller does not convert voltage; it only distributes and controls it.
- **Fuse:** Includes a **20A automotive-style fuse**.

### External Button Setup (IO33)

* To install a physical wall button, connect the two switch wires to **IO33** and **GND** on the small terminal block.
* **Behavior:**
    * *Short press:* Power on / off.
    * *Hold 1s:* Change color.
    * *Hold 10s:* Reset Wi-Fi configuration.

---

Generated for the Gledopto ESP32 WLED controller.
