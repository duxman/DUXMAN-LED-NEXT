"""
DUXMAN-LED-NEXT — PC LED Simulator
===================================
Visualiza en tiempo real los efectos del firmware ESP32 desde el PC.

Modos:
  - Standalone : renderiza efectos localmente (sin hardware)
  - API mode   : sincroniza estado con el ESP32 vía HTTP (/api/state)

Dependencias:  pip install pygame requests
Python:        3.9+

Uso:
  python led_simulator.py                    # standalone
  python led_simulator.py --host 192.168.1.x # conectar al ESP32
"""

from __future__ import annotations

import argparse
import math
import random
import sys
import time
from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Dependencias opcionales
# ---------------------------------------------------------------------------
try:
    import pygame
except ImportError:
    sys.exit("ERROR: pygame no instalado.  Ejecuta:  pip install pygame")

try:
    import requests as _requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

# ---------------------------------------------------------------------------
# Configuración de ventana
# ---------------------------------------------------------------------------
WIN_W        = 1100
WIN_H        = 420
LED_AREA_H   = 200     # altura de la tira LED
PANEL_X      = 0
PANEL_W      = WIN_W
FPS          = 60

# Colores UI
C_BG         = (18, 18, 24)
C_PANEL      = (28, 28, 36)
C_ACCENT     = (255, 160,  40)
C_TEXT       = (220, 220, 220)
C_DIMTEXT    = (120, 120, 130)
C_SLIDER_BG  = (60,  60,  70)
C_SLIDER_FG  = (255, 160,  40)
C_BTN_ON     = (60, 180,  80)
C_BTN_OFF    = (60,  60,  70)
C_BTN_TEXT   = (240, 240, 240)

# ---------------------------------------------------------------------------
# Estado del simulador (equivalente a CoreState)
# ---------------------------------------------------------------------------
@dataclass
class SimState:
    power: bool           = True
    brightness: int       = 180        # 0-255
    effect_id: int        = 5          # BreathFixed por defecto
    section_count: int    = 3          # 1-8
    effect_speed: int     = 5          # 1-10
    effect_level: int     = 5          # 1-10
    primary_colors: list  = field(default_factory=lambda: [0xFF4D00, 0xFFD400, 0x00B8D9])
    background_color: int = 0x000000
    led_count: int        = 144        # LEDs visibles en el simulador


# Mapa id → nombre
EFFECTS: Dict[int, str] = {
    0:  "Fixed",
    1:  "Gradient",
    3:  "Blink Fixed",
    4:  "Blink Gradient",
    5:  "Breath Fixed",
    6:  "Breath Gradient",
    7:  "Triple Chase",
    8:  "Gradient Meteor",
    9:  "Scanning Pulse",
    11: "Lava Flow",
    12: "Polar Ice",
    14: "Random Color Pop",
}
EFFECT_IDS = list(EFFECTS.keys())

# ---------------------------------------------------------------------------
# Helpers matemáticos (espejo de EffectEngine.h)
# ---------------------------------------------------------------------------
_SIM_START = time.monotonic()

def _time_sec() -> float:
    return time.monotonic() - _SIM_START

def _speed01(speed_scale: int) -> float:
    return max(0.0, min(1.0, (speed_scale - 1) / 9.0))

def _level01(level_scale: int) -> float:
    return max(0.0, min(1.0, (level_scale - 1) / 9.0))

def _clamp01(v: float) -> float:
    return max(0.0, min(1.0, v))

def _smoothstep(edge0: float, edge1: float, x: float) -> float:
    t = _clamp01((x - edge0) / (edge1 - edge0 + 1e-9))
    return t * t * (3.0 - 2.0 * t)

def _normalized_x(px: int, count: int) -> float:
    return px / max(1, count - 1)

def _section_size(led_count: int, section_count: int) -> int:
    return max(1, led_count // max(1, section_count))

# Color helpers (0xRRGGBB)
def _split(c: int) -> Tuple[int, int, int]:
    return (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF

def _join(r: int, g: int, b: int) -> int:
    return (int(r) << 16) | (int(g) << 8) | int(b)

def _scale(c: int, gain: float) -> int:
    r, g, b = _split(c)
    return _join(r * gain, g * gain, b * gain)

def _scale_brightness(c: int, brightness: int) -> int:
    return _scale(c, brightness / 255.0)

def _lerp_color(cA: int, cB: int, t: float) -> int:
    t = _clamp01(t)
    rA, gA, bA = _split(cA)
    rB, gB, bB = _split(cB)
    return _join(rA + (rB - rA) * t, gA + (gB - gA) * t, bA + (bB - bA) * t)

def _add_color(cA: int, cB: int) -> int:
    rA, gA, bA = _split(cA)
    rB, gB, bB = _split(cB)
    return _join(min(255, rA + rB), min(255, gA + gB), min(255, bA + bB))

def _gradient_color(cA: int, cB: int, cC: int, idx: int, count: int) -> int:
    if count <= 1:
        return cA
    half = (count - 1) / 2.0
    t = idx / max(1, count - 1)
    if t <= 0.5:
        return _lerp_color(cA, cB, t * 2.0)
    return _lerp_color(cB, cC, (t - 0.5) * 2.0)

def _to_pygame(c: int) -> Tuple[int, int, int]:
    return _split(c)

# ---------------------------------------------------------------------------
# Motores de efecto (renderFrame → list[int] de colores 0xRRGGBB)
# ---------------------------------------------------------------------------
EffectFn = Callable[[SimState, int], List[int]]

def _effect_fixed(s: SimState, n: int) -> List[int]:
    buf = [0] * n
    ss = _section_size(n, s.section_count)
    gain = s.brightness / 255.0
    for px in range(n):
        idx = min(px // ss, 2)
        buf[px] = _scale(s.primary_colors[idx], gain)
    return buf

def _effect_gradient(s: SimState, n: int) -> List[int]:
    buf = [0] * n
    bg = _scale_brightness(s.background_color, s.brightness)
    ss = _section_size(n, s.section_count)
    gain = s.brightness / 255.0
    for px in range(n):
        sec = px // ss
        if sec >= s.section_count:
            buf[px] = bg
            continue
        sec_start = sec * ss
        sec_end = min(n, sec_start + ss)
        sec_len = max(1, sec_end - sec_start)
        buf[px] = _scale(
            _gradient_color(s.primary_colors[0], s.primary_colors[1], s.primary_colors[2],
                            px - sec_start, sec_len),
            gain
        )
    return buf

def _effect_blink_fixed(s: SimState, n: int) -> List[int]:
    t = _time_sec()
    period = 2.0 - 1.8 * _speed01(s.effect_speed)
    on = (t % period) < (period * 0.5)
    gain = (s.brightness / 255.0) if (s.power and on) else 0.0
    ss = _section_size(n, s.section_count)
    return [_scale(s.primary_colors[min(px // ss, 2)], gain) for px in range(n)]

def _effect_blink_gradient(s: SimState, n: int) -> List[int]:
    t = _time_sec()
    period = 2.0 - 1.8 * _speed01(s.effect_speed)
    on = (t % period) < (period * 0.5)
    gain = (s.brightness / 255.0) if (s.power and on) else 0.0
    ss = _section_size(n, s.section_count)
    buf = [0] * n
    for px in range(n):
        sec = px // ss
        if sec >= s.section_count:
            continue
        sec_start = sec * ss
        sec_len = max(1, min(n, sec_start + ss) - sec_start)
        buf[px] = _scale(
            _gradient_color(s.primary_colors[0], s.primary_colors[1], s.primary_colors[2],
                            px - sec_start, sec_len),
            gain
        )
    return buf

def _effect_breath_fixed(s: SimState, n: int) -> List[int]:
    t = _time_sec()
    period = 8.0 - 7.5 * _speed01(s.effect_speed)
    breathe = 0.5 + 0.5 * math.sin(2.0 * math.pi * t / max(0.1, period))
    depth = _level01(s.effect_level)
    envelope = (1.0 - depth) + depth * breathe
    gain = _clamp01(envelope * (s.brightness / 255.0))
    ss = _section_size(n, s.section_count)
    return [_scale(s.primary_colors[min(px // ss, 2)], gain) for px in range(n)]

def _effect_breath_gradient(s: SimState, n: int) -> List[int]:
    t = _time_sec()
    period = 8.0 - 7.5 * _speed01(s.effect_speed)
    breathe = 0.5 + 0.5 * math.sin(2.0 * math.pi * t / max(0.1, period))
    depth = _level01(s.effect_level)
    envelope = (1.0 - depth) + depth * breathe
    gain = _clamp01(envelope * (s.brightness / 255.0))
    ss = _section_size(n, s.section_count)
    buf = [0] * n
    for px in range(n):
        sec = px // ss
        sec_start = (sec if sec < s.section_count else s.section_count - 1) * ss
        sec_len = max(1, min(n, sec_start + ss) - sec_start)
        buf[px] = _scale(
            _gradient_color(s.primary_colors[0], s.primary_colors[1], s.primary_colors[2],
                            px - sec_start, sec_len),
            gain
        )
    return buf

def _effect_triple_chase(s: SimState, n: int) -> List[int]:
    t = _time_sec()
    speed_norm = _speed01(s.effect_speed)
    level_norm = _level01(s.effect_level)
    level_curve = level_norm ** 1.25
    repeats = max(1, s.section_count)
    speed_hz = 0.08 + speed_norm * 3.2
    phase = t * speed_hz
    train_width = 0.05 + level_curve * 0.30
    global_gain = s.brightness / 255.0

    buf = [0] * n
    for px in range(n):
        x = _normalized_x(px, n)
        pattern_pos = math.fmod(x * repeats - phase, 1.0)
        local_pos = pattern_pos if pattern_pos >= 0.0 else pattern_pos + 1.0

        intensity = 0.0
        if local_pos < train_width:
            ramp = 1.0 - (local_pos / max(1e-6, train_width))
            intensity = _smoothstep(0.0, 1.0, ramp)

        color_idx = int(x * repeats) % 3
        base = _scale(s.background_color, global_gain * (0.10 + 0.25 * (1.0 - level_norm)))
        if intensity > 0.0:
            chase = _scale(s.primary_colors[color_idx],
                           intensity * global_gain * (0.55 + 0.45 * level_norm))
            buf[px] = _add_color(base, chase)
        else:
            buf[px] = base
    return buf

def _effect_gradient_meteor(s: SimState, n: int) -> List[int]:
    t = _time_sec()
    speed_norm = _speed01(s.effect_speed)
    level_norm = _level01(s.effect_level)
    speed_hz = 0.12 + speed_norm * 2.8
    tail = 0.08 + level_norm * 0.40
    repeats = max(1, s.section_count)
    gain = s.brightness / 255.0

    buf = [0] * n
    for px in range(n):
        x = _normalized_x(px, n)
        phase = math.fmod(x * repeats - t * speed_hz, 1.0)
        local_pos = phase if phase >= 0.0 else phase + 1.0
        head_pos = 1.0 - local_pos
        fade = math.exp(-head_pos / max(1e-4, tail)) if head_pos >= 0.0 else 0.0
        sec = int(x * repeats) % 3
        color = _gradient_color(s.primary_colors[0], s.primary_colors[1],
                                s.primary_colors[2], sec, 3)
        glow = _scale(color, fade * gain * (0.5 + 0.5 * level_norm))
        bg = _scale(s.background_color, gain * 0.05)
        buf[px] = _add_color(bg, glow)
    return buf

def _effect_scanning_pulse(s: SimState, n: int) -> List[int]:
    t = _time_sec()
    speed_norm = _speed01(s.effect_speed)
    level_norm = _level01(s.effect_level)
    level_curve = level_norm ** 1.15
    speed_hz = 0.06 + speed_norm * 3.1
    gain = s.brightness / 255.0

    phase = math.fmod(t * speed_hz, 2.0)
    ping_pong = phase if phase <= 1.0 else 2.0 - phase
    center = ping_pong * (n - 1)
    half_width = max(1.0, n * (0.03 + level_curve * 0.24))

    buf = [0] * n
    for px in range(n):
        dist = abs(px - center)
        intensity = 0.0
        if dist <= half_width:
            norm = 1.0 - dist / half_width
            intensity = _smoothstep(0.0, 1.0, norm)
        x = _normalized_x(px, n)
        color_idx = int(x * max(1, s.section_count) * 3) % 3
        base = _scale(s.background_color, gain * (0.10 + 0.22 * (1.0 - level_norm)))
        if intensity > 0.0:
            pulse = _scale(s.primary_colors[color_idx],
                           intensity * gain * (0.60 + 0.40 * level_norm))
            buf[px] = _add_color(base, pulse)
        else:
            buf[px] = base
    return buf

def _effect_lava_flow(s: SimState, n: int) -> List[int]:
    t = _time_sec()
    speed_norm = _speed01(s.effect_speed)
    level_norm = _level01(s.effect_level)
    level_curve = level_norm ** 1.2
    speed = 0.10 + speed_norm * 2.2
    deform = 0.06 + level_curve * 0.56
    repeats = max(1, s.section_count)
    gain = s.brightness / 255.0

    buf = [0] * n
    for px in range(n):
        x = _normalized_x(px, n)
        w1 = math.sin((x * repeats + t * speed) * 2.0 * math.pi)
        w2 = math.sin((x * repeats * 2.3 - t * speed * 1.3) * 2.0 * math.pi)
        mix = _clamp01(0.5 + 0.5 * (w1 * (1.0 - deform) + w2 * deform))
        hot = _lerp_color(s.primary_colors[0], s.primary_colors[1], mix)
        lava = _lerp_color(hot, s.primary_colors[2], _clamp01(0.2 + 0.8 * mix))
        base = _scale(s.background_color, gain * (0.08 + 0.20 * (1.0 - level_norm)))
        glow = _scale(lava, gain * (0.60 + 0.40 * level_norm))
        buf[px] = _add_color(base, glow)
    return buf

def _effect_polar_ice(s: SimState, n: int) -> List[int]:
    t = _time_sec()
    speed_norm = _speed01(s.effect_speed)
    level_norm = _level01(s.effect_level)
    speed = 0.05 + speed_norm * 1.2
    repeats = max(1, s.section_count)
    gain = s.brightness / 255.0

    buf = [0] * n
    for px in range(n):
        x = _normalized_x(px, n)
        crystal = 0.5 + 0.5 * math.sin((x * repeats + t * speed) * 2.0 * math.pi)
        shimmer = 0.5 + 0.5 * math.sin((x * repeats * 3.1 - t * speed * 0.7) * 2.0 * math.pi)
        mix = _clamp01(crystal * (1.0 - level_norm * 0.5) + shimmer * level_norm * 0.5)
        cold = _lerp_color(s.primary_colors[0], s.primary_colors[1], mix)
        deep = _lerp_color(cold, s.primary_colors[2], _clamp01(mix * 0.6 + 0.2))
        base = _scale(s.background_color, gain * 0.05)
        glow = _scale(deep, gain * (0.4 + 0.6 * mix))
        buf[px] = _add_color(base, glow)
    return buf

class _RandomColorPop:
    def __init__(self):
        self._pops: List[Dict] = []
        self._last_spawn = 0.0

    def render(self, s: SimState, n: int) -> List[int]:
        t = _time_sec()
        speed_norm = _speed01(s.effect_speed)
        level_norm = _level01(s.effect_level)
        rate = 0.05 + speed_norm * 0.5
        decay = 0.3 + level_norm * 1.5
        gain = s.brightness / 255.0

        if t - self._last_spawn > rate:
            self._last_spawn = t
            px_idx = random.randint(0, n - 1)
            col = s.primary_colors[random.randint(0, 2)]
            self._pops.append({"px": px_idx, "born": t, "color": col, "decay": decay})

        self._pops = [p for p in self._pops if (t - p["born"]) < p["decay"] * 3.0]

        buf = [_scale(s.background_color, gain * 0.05)] * n
        for p in self._pops:
            age = t - p["born"]
            fade = math.exp(-age / max(1e-4, p["decay"]))
            radius = int(1 + level_norm * 4)
            for d in range(-radius, radius + 1):
                idx = p["px"] + d
                if 0 <= idx < n:
                    dist_fade = 1.0 - abs(d) / max(1, radius + 1)
                    col = _scale(p["color"], fade * dist_fade * gain)
                    buf[idx] = _add_color(buf[idx], col)
        return buf


_RANDOM_POP_INSTANCE = _RandomColorPop()

EFFECT_RENDERERS: Dict[int, EffectFn] = {
    0:  _effect_fixed,
    1:  _effect_gradient,
    3:  _effect_blink_fixed,
    4:  _effect_blink_gradient,
    5:  _effect_breath_fixed,
    6:  _effect_breath_gradient,
    7:  _effect_triple_chase,
    8:  _effect_gradient_meteor,
    9:  _effect_scanning_pulse,
    11: _effect_lava_flow,
    12: _effect_polar_ice,
    14: _RANDOM_POP_INSTANCE.render,
}

# ---------------------------------------------------------------------------
# UI Helpers
# ---------------------------------------------------------------------------
def _draw_text(surf: pygame.Surface, text: str, x: int, y: int,
               color=C_TEXT, size=14, bold=False):
    font = pygame.font.SysFont("segoeui", size, bold=bold)
    s = font.render(text, True, color)
    surf.blit(s, (x, y))
    return s.get_width()

def _draw_slider(surf: pygame.Surface, label: str, value: int, vmin: int, vmax: int,
                 x: int, y: int, w: int = 180) -> pygame.Rect:
    """Dibuja un slider horizontal. Devuelve el Rect del track para hit-testing."""
    _draw_text(surf, label, x, y, C_DIMTEXT, 12)
    track_y = y + 16
    track = pygame.Rect(x, track_y, w, 6)
    pygame.draw.rect(surf, C_SLIDER_BG, track, border_radius=3)
    frac = (value - vmin) / max(1, vmax - vmin)
    filled = pygame.Rect(x, track_y, int(w * frac), 6)
    pygame.draw.rect(surf, C_SLIDER_FG, filled, border_radius=3)
    knob_x = x + int(w * frac)
    pygame.draw.circle(surf, C_ACCENT, (knob_x, track_y + 3), 7)
    _draw_text(surf, str(value), x + w + 6, track_y - 4, C_TEXT, 12)
    return track

def _draw_button(surf: pygame.Surface, label: str, x: int, y: int,
                 w: int, h: int, active: bool) -> pygame.Rect:
    rect = pygame.Rect(x, y, w, h)
    color = C_BTN_ON if active else C_BTN_OFF
    pygame.draw.rect(surf, color, rect, border_radius=5)
    font = pygame.font.SysFont("segoeui", 12)
    ts = font.render(label, True, C_BTN_TEXT)
    surf.blit(ts, (x + (w - ts.get_width()) // 2, y + (h - ts.get_height()) // 2))
    return rect

def _draw_color_swatch(surf: pygame.Surface, color: int, x: int, y: int,
                       w: int = 24, h: int = 24) -> pygame.Rect:
    rect = pygame.Rect(x, y, w, h)
    pygame.draw.rect(surf, _to_pygame(color), rect, border_radius=4)
    pygame.draw.rect(surf, (80, 80, 90), rect, 1, border_radius=4)
    return rect

# ---------------------------------------------------------------------------
# Sincronización con ESP32 (opcional)
# ---------------------------------------------------------------------------
class ApiSync:
    def __init__(self, host: str):
        self._url = f"http://{host}/api/state"
        self._last_fetch = 0.0
        self._interval = 1.0   # cada 1 s

    def maybe_sync(self, s: SimState) -> None:
        if not HAS_REQUESTS:
            return
        now = time.monotonic()
        if now - self._last_fetch < self._interval:
            return
        self._last_fetch = now
        try:
            resp = _requests.get(self._url, timeout=0.8)
            if resp.status_code == 200:
                data = resp.json()
                self._apply(s, data)
        except Exception:
            pass

    def _apply(self, s: SimState, data: dict) -> None:
        if "power" in data:
            s.power = bool(data["power"])
        if "brightness" in data:
            s.brightness = int(data["brightness"])
        if "effectId" in data:
            eid = int(data["effectId"])
            if eid in EFFECTS:
                s.effect_id = eid
        if "effectSpeed" in data:
            s.effect_speed = int(data["effectSpeed"])
        if "effectLevel" in data:
            s.effect_level = int(data["effectLevel"])
        if "sectionCount" in data:
            s.section_count = max(1, min(8, int(data["sectionCount"])))
        if "primaryColors" in data:
            for i, c in enumerate(data["primaryColors"][:3]):
                s.primary_colors[i] = int(c, 16) if isinstance(c, str) else int(c)
        if "backgroundColor" in data:
            bc = data["backgroundColor"]
            s.background_color = int(bc, 16) if isinstance(bc, str) else int(bc)


# ---------------------------------------------------------------------------
# Clase principal del simulador
# ---------------------------------------------------------------------------
class LedSimulator:
    def __init__(self, host: Optional[str] = None):
        pygame.init()
        pygame.display.set_caption("DUXMAN-LED-NEXT — Simulator")
        self._screen = pygame.display.set_mode((WIN_W, WIN_H), pygame.RESIZABLE)
        self._clock  = pygame.time.Clock()
        self._state  = SimState()
        self._api    = ApiSync(host) if host else None
        self._dragging: Optional[str] = None   # nombre del slider activo
        self._slider_rects: Dict[str, pygame.Rect] = {}
        self._effect_btn_rects: List[Tuple[pygame.Rect, int]] = []
        self._color_rects: List[pygame.Rect] = []

    # ------------------------------------------------------------------ Loop
    def run(self) -> None:
        while True:
            self._handle_events()
            if self._api:
                self._api.maybe_sync(self._state)
            self._render()
            self._clock.tick(FPS)

    # --------------------------------------------------------------- Eventos
    def _handle_events(self) -> None:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()
            elif event.type == pygame.KEYDOWN:
                self._handle_key(event.key)
            elif event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1:
                    self._handle_click(event.pos)
                    self._dragging = self._find_slider(event.pos)
            elif event.type == pygame.MOUSEBUTTONUP:
                if event.button == 1:
                    self._dragging = None
            elif event.type == pygame.MOUSEMOTION:
                if self._dragging:
                    self._drag_slider(self._dragging, event.pos[0])

    def _handle_key(self, key: int) -> None:
        s = self._state
        if key == pygame.K_SPACE:
            s.power = not s.power
        elif key == pygame.K_RIGHT:
            idx = EFFECT_IDS.index(s.effect_id) if s.effect_id in EFFECT_IDS else 0
            s.effect_id = EFFECT_IDS[(idx + 1) % len(EFFECT_IDS)]
        elif key == pygame.K_LEFT:
            idx = EFFECT_IDS.index(s.effect_id) if s.effect_id in EFFECT_IDS else 0
            s.effect_id = EFFECT_IDS[(idx - 1) % len(EFFECT_IDS)]
        elif key == pygame.K_UP:
            s.brightness = min(255, s.brightness + 10)
        elif key == pygame.K_DOWN:
            s.brightness = max(0, s.brightness - 10)
        elif key == pygame.K_r:
            self._randomize_colors()

    def _handle_click(self, pos: Tuple[int, int]) -> None:
        # Botones de efectos
        for rect, eid in self._effect_btn_rects:
            if rect.collidepoint(pos):
                self._state.effect_id = eid
                return
        # Swatches de color (click → color aleatorio)
        for i, rect in enumerate(self._color_rects):
            if rect.collidepoint(pos):
                self._state.primary_colors[i] = random.randint(0x100000, 0xFFFFFF)
                return

    def _find_slider(self, pos: Tuple[int, int]) -> Optional[str]:
        px, py = pos
        for name, rect in self._slider_rects.items():
            hit = pygame.Rect(rect.x - 10, rect.y - 8, rect.w + 20, rect.h + 16)
            if hit.collidepoint(px, py):
                return name
        return None

    def _drag_slider(self, name: str, mouse_x: int) -> None:
        rect = self._slider_rects.get(name)
        if not rect:
            return
        frac = _clamp01((mouse_x - rect.x) / max(1, rect.w))
        s = self._state
        if name == "brightness":
            s.brightness = int(frac * 255)
        elif name == "speed":
            s.effect_speed = max(1, min(10, round(1 + frac * 9)))
        elif name == "level":
            s.effect_level = max(1, min(10, round(1 + frac * 9)))
        elif name == "sections":
            s.section_count = max(1, min(8, round(1 + frac * 7)))
        elif name == "led_count":
            s.led_count = max(10, min(300, round(10 + frac * 290)))

    def _randomize_colors(self) -> None:
        for i in range(3):
            self._state.primary_colors[i] = random.randint(0x100000, 0xFFFFFF)

    # ----------------------------------------------------------------- Render
    def _render(self) -> None:
        self._screen.fill(C_BG)
        self._draw_led_strip()
        self._draw_controls()
        pygame.display.flip()

    def _draw_led_strip(self) -> None:
        s = self._state
        n = s.led_count
        renderer = EFFECT_RENDERERS.get(s.effect_id, _effect_fixed)
        pixels = renderer(s, n) if s.power else [0] * n

        # Área de la tira: ocupa todo el ancho
        strip_margin = 20
        strip_h      = LED_AREA_H
        strip_y      = 10
        avail_w      = WIN_W - 2 * strip_margin
        led_w        = max(2, avail_w // n)
        led_h        = strip_h - 20
        total_w      = led_w * n
        start_x      = strip_margin + (avail_w - total_w) // 2

        # Fondo de la tira
        strip_rect = pygame.Rect(strip_margin, strip_y, avail_w, strip_h)
        pygame.draw.rect(self._screen, (10, 10, 14), strip_rect, border_radius=8)

        for i, c in enumerate(pixels):
            r, g, b = _split(c)
            x = start_x + i * led_w
            y = strip_y + 10
            led_rect = pygame.Rect(x, y, max(1, led_w - 1), led_h)
            pygame.draw.rect(self._screen, (r, g, b), led_rect,
                             border_radius=min(3, led_w // 2))
            # Halo de brillo
            if r + g + b > 60:
                halo = pygame.Surface((led_w + 4, led_h + 4), pygame.SRCALPHA)
                halo.fill((r // 3, g // 3, b // 3, 60))
                self._screen.blit(halo, (x - 2, y - 2))

        # Etiqueta del efecto y FPS
        eff_name = EFFECTS.get(s.effect_id, "???")
        _draw_text(self._screen, f"{eff_name}  |  {n} LEDs  |  {self._clock.get_fps():.0f} fps",
                   strip_margin + 6, strip_y + strip_h - 16, C_DIMTEXT, 11)

    def _draw_controls(self) -> None:
        s = self._state
        panel_y = LED_AREA_H + 20
        pygame.draw.rect(self._screen, C_PANEL, pygame.Rect(0, panel_y, WIN_W, WIN_H - panel_y))
        pygame.draw.line(self._screen, (60, 60, 80), (0, panel_y), (WIN_W, panel_y), 1)

        # --- Título + conexión ---
        lbl = "DUXMAN-LED-NEXT Simulator"
        if self._api:
            lbl += f"  [API: {self._api._url}]"
        _draw_text(self._screen, lbl, 12, panel_y + 6, C_ACCENT, 13, bold=True)
        _draw_text(self._screen, "SPACE=power  ←/→=efecto  ↑/↓=brillo  R=colores aleatorios",
                   12, panel_y + 22, C_DIMTEXT, 11)

        col1_x = 12
        col2_x = 230
        col3_x = 450
        col4_x = 680
        row1_y = panel_y + 44
        row2_y = panel_y + 90

        # --- Sliders ---
        self._slider_rects = {}

        r = _draw_slider(self._screen, "Brillo", s.brightness, 0, 255,
                         col1_x, row1_y, 190)
        self._slider_rects["brightness"] = r

        r = _draw_slider(self._screen, "Velocidad", s.effect_speed, 1, 10,
                         col1_x, row2_y, 190)
        self._slider_rects["speed"] = r

        r = _draw_slider(self._screen, "Intensidad (level)", s.effect_level, 1, 10,
                         col2_x, row1_y, 190)
        self._slider_rects["level"] = r

        r = _draw_slider(self._screen, "Secciones", s.section_count, 1, 8,
                         col2_x, row2_y, 190)
        self._slider_rects["sections"] = r

        r = _draw_slider(self._screen, "Número de LEDs", s.led_count, 10, 300,
                         col3_x, row1_y, 190)
        self._slider_rects["led_count"] = r

        # --- Power button ---
        pw_rect = _draw_button(self._screen, "POWER  ON" if s.power else "POWER  OFF",
                               col3_x, row2_y, 100, 26, s.power)
        self._effect_btn_rects = []  # limpiar
        if pw_rect.collidepoint(pygame.mouse.get_pos()):
            pygame.draw.rect(self._screen, C_ACCENT, pw_rect, 1, border_radius=5)
        # Añadimos manejo de clic para power
        # (se maneja mediante el teclado también, aquí lo registramos)

        # --- Swatches de color ---
        _draw_text(self._screen, "Colores primarios (clic = aleatorio)",
                   col4_x, row1_y - 2, C_DIMTEXT, 11)
        self._color_rects = []
        for i in range(3):
            cx = col4_x + i * 36
            cr = _draw_color_swatch(self._screen, s.primary_colors[i], cx, row1_y + 14, 28, 28)
            self._color_rects.append(cr)
            hex_val = f"#{s.primary_colors[i]:06X}"
            _draw_text(self._screen, hex_val, cx - 4, row1_y + 44, C_DIMTEXT, 9)

        # Fondo
        _draw_text(self._screen, "BG:", col4_x + 115, row1_y + 14, C_DIMTEXT, 11)
        _draw_color_swatch(self._screen, s.background_color, col4_x + 136, row1_y + 14, 28, 28)

        # --- Botones de efecto ---
        _draw_text(self._screen, "Efectos:", col1_x, row2_y + 46, C_DIMTEXT, 11)
        btn_x = col1_x
        btn_y = row2_y + 62
        btn_w = 95
        btn_h = 22
        cols_per_row = 9
        for idx, eid in enumerate(EFFECT_IDS):
            bx = btn_x + (idx % cols_per_row) * (btn_w + 4)
            by = btn_y + (idx // cols_per_row) * (btn_h + 4)
            r2 = _draw_button(self._screen, EFFECTS[eid], bx, by, btn_w, btn_h,
                              eid == s.effect_id)
            self._effect_btn_rects.append((r2, eid))


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(
        description="DUXMAN-LED-NEXT PC LED Simulator"
    )
    parser.add_argument(
        "--host", metavar="IP",
        help="IP del ESP32 para sincronizar estado vía HTTP (ej: 192.168.1.50)"
    )
    parser.add_argument(
        "--leds", type=int, default=144,
        help="Número de LEDs a simular (default: 144)"
    )
    args = parser.parse_args()

    sim = LedSimulator(host=args.host)
    sim._state.led_count = max(10, min(300, args.leds))
    sim.run()


if __name__ == "__main__":
    main()
