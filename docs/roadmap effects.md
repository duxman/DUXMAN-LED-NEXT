# 📂 ESPECIFICACIÓN TÉCNICA: MOTOR DE EFECTOS LED DINÁMICOS

Este documento contiene la lógica de renderizado para un sistema de tiras LED (1D). Todos los cálculos deben realizarse en el Core 1 del ESP32 usando aritmética de punto flotante (`float`) y coordenadas normalizadas (x de 0.0 a 1.0).

---

## 1. Trig-Ribbon (Baile de 3 Colores)
* **Descripción:** Tres ondas senoidales desfasadas 120° que se persiguen.
* **Lógica:** $Intensidad_n = \max(0.0, \sin(2\pi \cdot (x \cdot freq + t \cdot speed) + Phase_n))^{sharpness}$
* **Fases:** $0$, $2\pi/3$, $4\pi/3$.
* **Mezcla:** $(C1 \cdot I_A) + (C2 \cdot I_B) + (C3 \cdot I_C) + (BG \cdot (1 - \max(I)))$.

## 2. Gradient Meteor (Meteoro Tri-Cromático)
* **Descripción:** Cabeza brillante con estela que degrada: C1 (Cabeza) -> C2 -> C3 -> BG.
* **Lógica:** $dist = \text{fmod}(Pos_{cabeza} - x + 1.0, 1.0)$.
* **Tramo 1 (u < 0.5):** LERP(C1, C2).
* **Tramo 2 (u >= 0.5):** LERP(C2, C3).
* **Final:** Fundido lineal a `bg_color` según longitud de estela.

## 3. Bouncing Physics (Bolas Saltarinas)
* **Descripción:** Simulación física de esferas con rebote y mezcla aditiva (ADD).
* **Estado:** Requiere `pos` y `vel` persistentes por cada bola.
* **Física:** $pos = pos + (vel \cdot dt)$; invertir `vel` al tocar 0.0 o 1.0.
* **Render:** $Brillo = 1.0 - (\text{distancia}(x, pos) / ball\_size)$.

## 4. Stellar Twinkle (Centelleo Aleatorio)
* **Descripción:** Chispas que nacen y mueren con una curva de brillo senoidal.
* **Estado:** Array `life[NUM_LEDS]` (0.0 a 1.0).
* **Lógica:** Si `life == 0` y `rand < spawn_rate` -> activar y asignar color aleatorio.
* **Brillo:** $\sin(\pi \cdot life)$. Al llegar a 1.0, resetear vida a 0.

## 5. Triple Chase (El Trenecito)
* **Descripción:** Bloques de colores fijos desplazándose con espacios vacíos (gaps).
* **Lógica:** $P_{local} = \text{fmod}(x \cdot rep + t \cdot speed, 1.0)$.
* **Zonificación:** Dividir el ciclo 1.0 en 4 segmentos: [C1, C2, C3, BG] basados en `car_size`.
* **Suavizado:** Usar `smoothstep` en los bordes de cada vagón.

## 6. Triple Fixed Breathe (Segmentos con Respiración)
* **Descripción:** Tres bloques estáticos que hacen Fade In/Out simultáneamente.
* **Lógica:** $F_{fade} = (\sin(t \cdot speed \cdot 2\pi) + 1.0) / 2.0$.
* **Salida:** $Color_{final} = Color_{segmento} \cdot (F_{fade} \cdot max\_brightness)$.

## 7. Global Gradient Breathe (Degradado con Respiración)
* **Descripción:** Un degradado estático C1 -> C2 -> C3 que oscila en intensidad global.
* **Lógica:** Generar degradado según `x`, luego multiplicar todo el buffer por el $F_{fade}$ senoidal.

## 8. Polar Ice (Interferencia Glaciar)
* **Descripción:** Ondas lentas cruzadas que crean zonas de brillo cambiante y orgánico.
* **Lógica:** $Mix = (\sin(x \cdot 3 + t \cdot s) + \sin(x \cdot 5 - t \cdot s \cdot 1.2) + 2) / 4$.
* **Mapeo:** Usar `Mix` para transicionar entre azul profundo, cian y blanco.

## 9. Lava Flow (Magma Dinámico)
* **Descripción:** Fluido viscoso donde el degradado se deforma al avanzar.
* **Lógica:** $x_{def} = x + 0.1 \cdot \sin(x \cdot 10 + t)$.
* **Render:** Aplicar el efecto de degradado usando $x_{def}$ como índice de posición.

## 10. Random Color Pop (Flash Aleatorio)
* **Descripción:** Variación de Twinkle con colores puros y desvanecimiento rápido.
* **Lógica:** Selección de color aleatoria instantánea en el "nacimiento" del píxel.

## 11. Scanning Pulse (Cylon / KITT)
* **Descripción:** Un pulso con estela que rebota de lado a lado (Ping-Pong).
* **Lógica:** Movimiento físico de Bouncing Ball aplicado a la estructura de Gradient Meteor.

---

### 🛠 DIRECTRICES PARA EL PROGRAMADOR
1. **FPU Optimización:** Usar versiones de funciones para float (`sinf`, `fmodf`, `powf`).
2. **Color Blending:** Implementar una función `blend(colorA, colorB, ratio)` robusta.
3. **Gamma Correction:** Aplicar tabla de corrección Gamma (valor 2.8) antes del envío final a la tira.
4. **FPS:** El motor debe apuntar a un mínimo de 60 frames por segundo para suavidad en los degradados.