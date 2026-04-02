# 📋 Guía de Configuración: Gledopto GL-C-017WL-D (ESP32)

Este documento contiene la asignación definitiva de pines (Pinout) para el controlador Gledopto diseñado para **WLED**.

---

## 1. Salidas de Datos (LED Outputs)
Configura estos pines en **Settings > LED Preferences**. La placa soporta hasta 4 salidas independientes.

| Salida | Pin GPIO | Notas |
| :--- | :--- | :--- |
| **LED Output 1** | **16** | Salida principal (recomendada) |
| **LED Output 2** | **4** | Salida secundaria |
| **LED Output 3** | **2** | Salida terciaria |
| **LED Output 4** | **1** | Usar solo si es necesario (Pin TX) |

---

## 2. Micrófono Integrado (Audio Reactive)
Para activar la reactividad al sonido, ve a **Settings > Sync Interfaces > Audio Input**.

* **Microphone Type:** `Generic I2S (Philips)`
* **I2S SD Pin:** `26`
* **I2S WS Pin:** `5`
* **I2S SCK Pin:** `21`

---

## 3. Botones y Entradas Físicas
Configura estos pines en **Settings > LED Preferences > Buttons**.

| Control | Pin GPIO | Tipo (Type) | Función |
| :--- | :--- | :--- | :--- |
| **Botón en Placa** | **0** | `Pushbutton` | On/Off y Reset Wi-Fi (10s) |
| **Bornera IO33** | **33** | `Pushbutton` | Para pulsador externo de pared |

---

## 4. Notas Técnicas y Seguridad

### ⚡ Alimentación
* **Voltaje:** Soporta de **5V a 24V DC**.
* **¡Atención!**: El voltaje de la fuente debe ser el mismo que el de la tira LED. El controlador no convierte el voltaje, solo lo gestiona.
* **Fusible:** Incluye un fusible tipo coche de **20A**.

### 🛠️ Configuración de Botón Externo (IO33)
* Para instalar un botón físico de pared, conecta los dos cables del pulsador a los terminales **IO33** y **GND** de la bornera pequeña.
* **Comportamiento:**
    * *Click corto:* Encender / Apagar.
    * *Mantener 1s:* Cambiar color.
    * *Mantener 10s:* Resetear configuración Wi-Fi.

---
*Documento generado para controlador Gledopto ESP32 WLED.*