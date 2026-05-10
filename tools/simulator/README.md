# DUXMAN-LED-NEXT — PC LED Simulator

Visualiza los efectos del firmware ESP32 directamente desde tu PC, sin necesidad de hardware.

## Instalación

```bash
cd tools/simulator
pip install -r requirements.txt
```

## Uso

### Modo standalone (sin hardware)
```bash
python led_simulator.py
python led_simulator.py --leds 60    # cambiar número de LEDs
```

### Modo API (sincroniza con el ESP32 real)
```bash
python led_simulator.py --host 192.168.1.50
```
El simulador consulta `/api/state` cada segundo y actualiza efecto, colores y brillo automáticamente.

## Controles

| Tecla / Acción | Función |
|---|---|
| `SPACE` | Encender / apagar |
| `←` / `→` | Cambiar efecto |
| `↑` / `↓` | Brillo +/- 10 |
| `R` | Colores primarios aleatorios |
| Clic en swatch de color | Color primario aleatorio |
| Clic en botón de efecto | Seleccionar efecto |
| Arrastrar slider | Ajustar brillo, velocidad, intensidad, secciones, LEDs |

## Efectos implementados

| ID | Nombre |
|----|--------|
| 0  | Fixed |
| 1  | Gradient |
| 3  | Blink Fixed |
| 4  | Blink Gradient |
| 5  | Breath Fixed |
| 6  | Breath Gradient |
| 7  | Triple Chase |
| 8  | Gradient Meteor |
| 9  | Scanning Pulse |
| 11 | Lava Flow |
| 12 | Polar Ice |
| 14 | Random Color Pop |

## Arquitectura

El simulador es un port directo de `firmware/src/effects/` a Python.  
Cada función `_effect_*` replica exactamente los mismos cálculos matemáticos que el C++ del ESP32:
- `speed01()`, `level01()`, `clamp01()`, `smoothstep()`
- `scaleColor()`, `gradientColor()`, `lerpColor()`, `addColor()`
- `normalizedTimeSec()`, `normalizedX()`

Esto garantiza que lo que ves en el simulador es lo que verás en el hardware.
