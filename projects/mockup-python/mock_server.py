#!/usr/bin/env python3
"""
DUXMAN-LED-NEXT — Servidor Mockup
==================================
Simula todos los endpoints REST del firmware (ESP32) sin necesitar hardware real.
El estado inicial se carga desde mock_state.json y se mantiene en memoria.

Uso:
    pip install flask
    python mock_server.py [--port 8080] [--host 0.0.0.0]

La UI estará disponible en http://localhost:8080/
"""

import argparse
import copy
import html
import json
import logging
import os
import sys
import time
import uuid
from logging.handlers import RotatingFileHandler
from pathlib import Path

from flask import Flask, Response, g, jsonify, redirect, request, send_from_directory

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
HERE = Path(__file__).parent.resolve()
PROJECTS_ROOT = HERE.parent
WORKSPACE_ROOT = PROJECTS_ROOT.parent
FIRMWARE_ROOT = PROJECTS_ROOT / "firmware-platformio"
UI_DIR = FIRMWARE_ROOT / "data" / "ui"
HELP_DIR = FIRMWARE_ROOT / "data" / "help"
LEGACY_HELP_DIR = WORKSPACE_ROOT / "docs referencia" / "pantallazos"
MOCK_HELP_DIR = HERE / "help"
STATE_FILE = HERE / "mock_state.json"
DEFAULT_LOG_FILE = HERE / "mock_interactions.log"
DEFAULT_LOG_LEVEL = "INFO"
MAX_LOG_BODY_CHARS = 4096
MAX_LOG_FILE_BYTES = 2 * 1024 * 1024
LOG_BACKUP_COUNT = 3

LOG_FILE_PATH: Path = DEFAULT_LOG_FILE
INTERACTION_LOGGER = logging.getLogger("duxman.mock.interactions")


def _truncate_text(value: str, max_len: int = MAX_LOG_BODY_CHARS) -> str:
    if len(value) <= max_len:
        return value
    return value[:max_len] + f" ...[truncated:{len(value) - max_len}]"


def _setup_interaction_logger(log_file: Path, level_name: str) -> None:
    global LOG_FILE_PATH
    LOG_FILE_PATH = log_file
    LOG_FILE_PATH.parent.mkdir(parents=True, exist_ok=True)

    INTERACTION_LOGGER.setLevel(getattr(logging, level_name.upper(), logging.INFO))
    INTERACTION_LOGGER.handlers.clear()
    INTERACTION_LOGGER.propagate = False

    handler = RotatingFileHandler(
        LOG_FILE_PATH,
        maxBytes=MAX_LOG_FILE_BYTES,
        backupCount=LOG_BACKUP_COUNT,
        encoding="utf-8",
    )
    handler.setFormatter(logging.Formatter("%(message)s"))
    INTERACTION_LOGGER.addHandler(handler)


def _append_interaction_log(entry: dict) -> None:
    if not INTERACTION_LOGGER.handlers:
        return
    INTERACTION_LOGGER.info(json.dumps(entry, ensure_ascii=False))


def _read_log_tail(limit: int = 200) -> list[dict]:
    if not LOG_FILE_PATH.exists():
        return []

    lines = LOG_FILE_PATH.read_text(encoding="utf-8", errors="replace").splitlines()
    tail = lines[-limit:]
    rows = []
    for line in tail:
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            rows.append({"raw": line})
    return rows

# ---------------------------------------------------------------------------
# Catálogos estáticos (espejo de EffectRegistry y PaletteRegistry del firmware)
# ---------------------------------------------------------------------------
EFFECTS = [
    {"id": 0,  "key": "fixed",                "label": "Color fijo",               "description": "Alterna los 3 colores por secciones fijas.",               "usesSpeed": False, "usesAudio": False},
    {"id": 1,  "key": "gradient",             "label": "Degradado fijo",            "description": "Genera un degradado estático con los 3 colores.",           "usesSpeed": False, "usesAudio": False},
    {"id": 3,  "key": "blink_fixed",          "label": "Parpadeo fijo",             "description": "Parpadea el patrón fijo por secciones.",                    "usesSpeed": True,  "usesAudio": False},
    {"id": 4,  "key": "blink_gradient",       "label": "Parpadeo degradado",        "description": "Parpadea el degradado por secciones.",                      "usesSpeed": True,  "usesAudio": False},
    {"id": 2,  "key": "diagnostic",           "label": "Diagnóstico",               "description": "Activa solo la primera salida en rojo.",                    "usesSpeed": False, "usesAudio": False},
    {"id": 5,  "key": "breath_fixed",         "label": "Respiración fija",          "description": "Las secciones respiran con envolvente senoidal.",            "usesSpeed": True,  "usesAudio": False},
    {"id": 6,  "key": "breath_gradient",      "label": "Respiración degradado",     "description": "El degradado completo respira con envolvente senoidal.",     "usesSpeed": True,  "usesAudio": False},
    {"id": 7,  "key": "triple_chase",         "label": "Triple chase",              "description": "Tren de color en movimiento con huecos y repeticiones.",     "usesSpeed": True,  "usesAudio": False},
    {"id": 8,  "key": "gradient_meteor",      "label": "Meteorito degradado",       "description": "Cabeza y cola en movimiento con color degradado.",           "usesSpeed": True,  "usesAudio": False},
    {"id": 9,  "key": "scanning_pulse",       "label": "Pulso barrido",             "description": "Pulso que recorre la tira de extremo a extremo.",            "usesSpeed": True,  "usesAudio": False},
    {"id": 10, "key": "trig_ribbon",          "label": "AUDIO · Cinta trigonométrica", "description": "Patrón ondulante guiado por micrófono.",                "usesSpeed": True,  "usesAudio": True},
    {"id": 11, "key": "lava_flow",            "label": "Lava flow",                 "description": "Flujo orgánico con deformación de ondas.",                  "usesSpeed": True,  "usesAudio": False},
    {"id": 12, "key": "polar_ice",            "label": "Polar ice",                 "description": "Interferencia fría con ondas lentas.",                      "usesSpeed": True,  "usesAudio": False},
    {"id": 13, "key": "stellar_twinkle",      "label": "AUDIO · Stellar twinkle",   "description": "Destellos estelares pseudoaleatorios reactivos al audio.",   "usesSpeed": True,  "usesAudio": True},
    {"id": 14, "key": "random_color_pop",     "label": "Random color pop",          "description": "Apariciones de color aleatorias con envolvente.",            "usesSpeed": True,  "usesAudio": False},
    {"id": 15, "key": "bouncing_physics",     "label": "AUDIO · Bouncing physics",  "description": "Bolas luminosas que rebotan y responden al micrófono.",      "usesSpeed": True,  "usesAudio": True},
    {"id": 16, "key": "audio_pulse",          "label": "AUDIO · Audio Pulse",       "description": "VU metro simétrico reactivo al audio.",                     "usesSpeed": False, "usesAudio": True},
    {"id": 17, "key": "audio_spectrum",       "label": "AUDIO · Spectrum VU",       "description": "VU-meter de 3 bandas con colores primarios.",                "usesSpeed": False, "usesAudio": True},
    {"id": 18, "key": "audio_neon_eq",        "label": "AUDIO · Neon EQ",           "description": "Ecualizador neon de 3 bandas con beat flash.",               "usesSpeed": True,  "usesAudio": True},
    {"id": 19, "key": "audio_rainbow_wave",   "label": "AUDIO · Rainbow Wave",      "description": "Onda de arcoíris que cambia de color con el audio.",         "usesSpeed": True,  "usesAudio": True},
    {"id": 20, "key": "audio_spectrum_chase", "label": "AUDIO · Spectrum Chase",    "description": "Chase tipo Knight Rider con colores reactivos al audio.",    "usesSpeed": True,  "usesAudio": True},
    {"id": 21, "key": "audio_section_strobe", "label": "AUDIO · Section Strobe",    "description": "Secciones de LEDs que flashean por turno en cada beat.",    "usesSpeed": False, "usesAudio": True},
]

PALETTES = [
    {"id": -1,  "key": "manual",              "label": "Manual",              "style": "manual",        "color1": "#FF4D00", "color2": "#FFD400", "color3": "#00B8D9"},
    {"id": 0,   "key": "ember_citrus_cyan",   "label": "Ember Citrus Cyan",   "style": "high-contrast", "color1": "#FF4D00", "color2": "#FFD400", "color3": "#00B8D9"},
    {"id": 1,   "key": "sunset_drive",        "label": "Sunset Drive",        "style": "warm",          "color1": "#FF5E5B", "color2": "#FFA552", "color3": "#FFD166"},
    {"id": 2,   "key": "ocean_neon",          "label": "Ocean Neon",          "style": "neon",          "color1": "#00E5FF", "color2": "#00FFA3", "color3": "#0A84FF"},
    {"id": 3,   "key": "aurora_soft",         "label": "Aurora Soft",         "style": "pastel",        "color1": "#A3E1DC", "color2": "#B5B9FF", "color3": "#FFC6D9"},
    {"id": 4,   "key": "vaporwave",           "label": "Vaporwave",           "style": "party",         "color1": "#FF3CAC", "color2": "#784BA0", "color3": "#2B86C5"},
    {"id": 5,   "key": "forest_pop",          "label": "Forest Pop",          "style": "cold",          "color1": "#00A86B", "color2": "#6BD66B", "color3": "#B8F2E6"},
    {"id": 6,   "key": "arctic_pulse",        "label": "Arctic Pulse",        "style": "cold",          "color1": "#B8F3FF", "color2": "#66D9FF", "color3": "#1C8DFF"},
    {"id": 7,   "key": "golden_hour",         "label": "Golden Hour",         "style": "warm",          "color1": "#FF7F11", "color2": "#FFB627", "color3": "#FFE17D"},
    {"id": 8,   "key": "synthwave",           "label": "Synthwave",           "style": "party",         "color1": "#F72585", "color2": "#7209B7", "color3": "#3A0CA3"},
    {"id": 9,   "key": "mint_lavender",       "label": "Mint Lavender",       "style": "pastel",        "color1": "#B8F2E6", "color2": "#CDB4DB", "color3": "#A9DEF9"},
    {"id": 10,  "key": "lava_ice",            "label": "Lava Ice",            "style": "high-contrast", "color1": "#FF3D00", "color2": "#00B4D8", "color3": "#F9C80E"},
    {"id": 11,  "key": "club_rgb",            "label": "Club RGB",            "style": "neon",          "color1": "#FF006E", "color2": "#3A86FF", "color3": "#8AC926"},
]

# ---------------------------------------------------------------------------
# Estado en memoria
# ---------------------------------------------------------------------------
def _load_state() -> dict:
    with open(STATE_FILE, encoding="utf-8") as f:
        data = json.load(f)
    # Eliminar claves de documentación
    data.pop("_comment", None)
    return data


_state: dict = _load_state()

# Paletas de usuario (extra, además de las del catálogo)
_user_palettes: list = []


def _build_effect_options(selected_id: int) -> str:
    visual_opts = []
    audio_opts = []
    for effect in EFFECTS:
        selected = " selected" if effect["id"] == selected_id else ""
        option = (
            f"<option value='{effect['key']}' data-audio='{'1' if effect['usesAudio'] else '0'}'{selected}>"
            f"{effect['label']}"
            "</option>"
        )
        if effect["usesAudio"]:
            audio_opts.append(option)
        else:
            visual_opts.append(option)

    html = ""
    if visual_opts:
        html += "<optgroup label='Visuales'>" + "".join(visual_opts) + "</optgroup>"
    if audio_opts:
        html += "<optgroup label='Audio reactivos'>" + "".join(audio_opts) + "</optgroup>"
    return html


def _render_ui_template(filename: str) -> str:
    template_path = UI_DIR / filename
    raw = template_path.read_text(encoding="utf-8")

    css = ""
    css_path = UI_DIR / "common.css.html"
    if css_path.exists():
        css = css_path.read_text(encoding="utf-8")

    state = _state.get("state", {})
    gpio = _state.get("gpio", {})
    outputs = gpio.get("outputs") or []
    primary = outputs[0] if outputs else {}

    replacements = {
        "__CSS__": css,
        "__FW_VERSION__": "v0.4.2-beta-mock",
        "__BOOTED_AT__": "Arranque: mockup local",
        "__EFFECT_OPTIONS__": _build_effect_options(int(state.get("effectId", 0))),
        "__MAX_OUTPUTS__": "4",
        "__BUILD_PIN__": str(primary.get("pin", 2)),
        "__BUILD_COUNT__": str(primary.get("ledCount", 60)),
    }

    rendered = raw
    for key, value in replacements.items():
        rendered = rendered.replace(key, value)
    return rendered


def _resolve_help_dir(filename: str) -> Path | None:
    candidates = [HELP_DIR, LEGACY_HELP_DIR, MOCK_HELP_DIR]
    for folder in candidates:
        if (folder / filename).exists():
            return folder
    return None


def _help_placeholder_svg(filename: str) -> str:
    safe_name = html.escape(Path(filename).name)
    return (
        "<svg xmlns='http://www.w3.org/2000/svg' width='1200' height='700' viewBox='0 0 1200 700'>"
        "<defs>"
        "<linearGradient id='g' x1='0' y1='0' x2='1' y2='1'>"
        "<stop offset='0%' stop-color='#10222d'/>"
        "<stop offset='100%' stop-color='#163443'/>"
        "</linearGradient>"
        "</defs>"
        "<rect width='1200' height='700' fill='url(#g)'/>"
        "<rect x='70' y='70' width='1060' height='560' rx='24' fill='#0d1b22' stroke='#1e3a48' stroke-width='3'/>"
        "<text x='600' y='300' text-anchor='middle' fill='#d8edf5' font-family='Segoe UI, sans-serif' font-size='42' font-weight='700'>"
        "Captura no disponible"
        "</text>"
        "<text x='600' y='350' text-anchor='middle' fill='#7aacbf' font-family='Consolas, monospace' font-size='24'>"
        f"{safe_name}"
        "</text>"
        "<text x='600' y='405' text-anchor='middle' fill='#0fd0e0' font-family='Segoe UI, sans-serif' font-size='20'>"
        "Agrega la imagen en data/help o docs referencia/pantallazos"
        "</text>"
        "</svg>"
    )


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def _ok(payload=None):
    if payload is None:
        payload = {"ok": True}
    resp = jsonify(payload)
    resp.headers["Access-Control-Allow-Origin"] = "*"
    return resp


def _err(message: str, code: int = 400):
    resp = jsonify({"ok": False, "error": message})
    resp.headers["Access-Control-Allow-Origin"] = "*"
    return resp, code


def _effect_by_id(eid: int) -> dict | None:
    return next((e for e in EFFECTS if e["id"] == eid), None)


def _palette_by_id(pid: int) -> dict | None:
    if pid == -1:
        return PALETTES[0]
    all_palettes = PALETTES + _user_palettes
    return next((p for p in all_palettes if p["id"] == pid), None)


def _full_state() -> dict:
    s = copy.deepcopy(_state["state"])
    # Rellenar campos calculados
    eff = _effect_by_id(s.get("effectId", 0)) or EFFECTS[0]
    s["effect"] = eff["key"]
    s["effectLabel"] = eff["label"]
    s["effectUsesAudio"] = eff["usesAudio"]
    pal = _palette_by_id(s.get("paletteId", -1)) or PALETTES[0]
    s["palette"] = pal["key"]
    s["paletteLabel"] = pal["label"]
    s["paletteStyle"] = pal["style"]
    s["availableEffects"] = EFFECTS
    s["availablePalettes"] = PALETTES + _user_palettes
    return s


def _apply_patch(target: dict, patch: dict) -> None:
    """Aplica un PATCH shallow sobre target (primer nivel)."""
    for key, value in patch.items():
        if key.startswith("_"):
            continue
        target[key] = value


def _network_for_ui() -> dict:
    network = _state.get("network", {})
    wifi = network.get("wifi", {})
    wifi_conn = wifi.get("connection", {})
    ap = network.get("ap", {})
    sta = network.get("sta", {})
    return {
        "network": {
            "wifi": {
                "mode": wifi.get("mode", "ap"),
                "apAvailability": wifi.get("apAvailability", "always"),
                "connection": {
                    "ssid": wifi_conn.get("ssid", wifi.get("ssid", "")),
                    "password": wifi_conn.get("password", wifi.get("password", "")),
                },
            },
            "ip": {
                "ap": {
                    "mode": ap.get("mode", "dhcp"),
                    "address": ap.get("address", ""),
                    "gateway": ap.get("gateway", ""),
                    "subnet": ap.get("subnet", ""),
                },
                "sta": {
                    "mode": sta.get("mode", "dhcp"),
                    "address": sta.get("address", ""),
                    "gateway": sta.get("gateway", ""),
                    "subnet": sta.get("subnet", ""),
                    "primaryDns": sta.get("primaryDns", ""),
                    "secondaryDns": sta.get("secondaryDns", ""),
                },
            },
            "dns": network.get("dns", {}),
            "time": network.get("time", {}),
        }
    }


def _apply_network_payload(payload: dict) -> None:
    section = payload.get("network", payload) if isinstance(payload, dict) else {}
    if not isinstance(section, dict):
        return

    network = _state.setdefault("network", {})
    wifi = network.setdefault("wifi", {})
    ap = network.setdefault("ap", {})
    sta = network.setdefault("sta", {})

    incoming_wifi = section.get("wifi", {}) if isinstance(section.get("wifi", {}), dict) else {}
    if "mode" in incoming_wifi:
        wifi["mode"] = incoming_wifi["mode"]
    if "apAvailability" in incoming_wifi:
        wifi["apAvailability"] = incoming_wifi["apAvailability"]

    conn = incoming_wifi.get("connection", {}) if isinstance(incoming_wifi.get("connection", {}), dict) else {}
    if "ssid" in conn:
        wifi["ssid"] = conn["ssid"]
    elif "ssid" in incoming_wifi:
        wifi["ssid"] = incoming_wifi["ssid"]

    if "password" in conn:
        wifi["password"] = conn["password"]
    elif "password" in incoming_wifi:
        wifi["password"] = incoming_wifi["password"]

    ip_section = section.get("ip", {}) if isinstance(section.get("ip", {}), dict) else {}
    incoming_ap = ip_section.get("ap", section.get("ap", {}))
    incoming_sta = ip_section.get("sta", section.get("sta", {}))

    if isinstance(incoming_ap, dict):
        _apply_patch(ap, incoming_ap)
    if isinstance(incoming_sta, dict):
        _apply_patch(sta, incoming_sta)

    if isinstance(section.get("dns", {}), dict):
        _apply_patch(network.setdefault("dns", {}), section["dns"])
    if isinstance(section.get("time", {}), dict):
        _apply_patch(network.setdefault("time", {}), section["time"])


def _microphone_for_ui() -> dict:
    return {"microphone": copy.deepcopy(_state.get("microphone", {}))}


def _apply_microphone_payload(payload: dict) -> None:
    section = payload.get("microphone", payload) if isinstance(payload, dict) else {}
    if not isinstance(section, dict):
        return
    mic = _state.setdefault("microphone", {})
    pins = section.pop("pins", None)
    _apply_patch(mic, section)
    if isinstance(pins, dict):
        _apply_patch(mic.setdefault("pins", {}), pins)


def _gpio_for_ui() -> dict:
    gpio = _state.get("gpio", {})
    outputs = copy.deepcopy(gpio.get("outputs", []))
    power_limit = gpio.get("powerLimit", {})
    power_legacy = gpio.get("power", {})
    merged_limit = {
        "enabled": bool(power_limit.get("enabled", power_legacy.get("powerLimitEnabled", False))),
        "maxCurrentmA": int(power_limit.get("maxCurrentmA", power_legacy.get("maxTotalCurrentmA", 2500))),
        "milliAmpsPerLed": int(power_limit.get("milliAmpsPerLed", power_legacy.get("milliAmpsPerLedBase", 60))),
    }
    return {
        "gpio": {
            "outputCount": len(outputs),
            "outputs": outputs,
            "powerLimit": merged_limit,
        }
    }


def _apply_gpio_payload(payload: dict) -> None:
    section = payload.get("gpio", payload) if isinstance(payload, dict) else {}
    if not isinstance(section, dict):
        return

    gpio = _state.setdefault("gpio", {})

    outputs = section.get("outputs")
    if isinstance(outputs, list):
        gpio["outputs"] = outputs
        gpio["outputCount"] = len(outputs)

    power_limit = section.get("powerLimit")
    if isinstance(power_limit, dict):
        _apply_patch(gpio.setdefault("powerLimit", {}), power_limit)
        power = gpio.setdefault("power", {})
        if "enabled" in power_limit:
            power["powerLimitEnabled"] = bool(power_limit["enabled"])
        if "maxCurrentmA" in power_limit:
            power["maxTotalCurrentmA"] = int(power_limit["maxCurrentmA"])
        if "milliAmpsPerLed" in power_limit:
            power["milliAmpsPerLedBase"] = int(power_limit["milliAmpsPerLed"])

    if isinstance(section.get("power", {}), dict):
        _apply_patch(gpio.setdefault("power", {}), section["power"])


def _debug_for_ui() -> dict:
    general = _state.get("general", {})
    return {
        "debug": {
            "enabled": bool(general.get("debugEnabled", False)),
            "heartbeatMs": int(general.get("heartbeatMs", 5000)),
        }
    }


def _apply_debug_payload(payload: dict) -> None:
    section = payload.get("debug", payload.get("general", payload)) if isinstance(payload, dict) else {}
    if not isinstance(section, dict):
        return
    general = _state.setdefault("general", {})
    if "enabled" in section:
        general["debugEnabled"] = bool(section["enabled"])
    if "debugEnabled" in section:
        general["debugEnabled"] = bool(section["debugEnabled"])
    if "heartbeatMs" in section:
        general["heartbeatMs"] = int(section["heartbeatMs"])


def _sync_mode_enabled(mode: str) -> bool:
    return mode not in ("", "off", "disabled", "none")


def _sync_for_ui() -> dict:
    sync = _state.get("sync", {})
    mode = str(sync.get("mode", "off"))
    role = sync.get("role", "client")
    return {
        "sync": {
            "enabled": _sync_mode_enabled(mode),
            "mode": mode,
            "role": role,
            "inputProtocol": sync.get("inputProtocol", "ddp"),
            "ddpPort": int(sync.get("ddpPort", 4048)),
            "udpSyncPort": int(sync.get("udpSyncPort", 21324)),
            "sourceTimeoutMs": int(sync.get("sourceTimeoutMs", 1500)),
            "groupMask": int(sync.get("groupMask", 1)),
            "clockSmoothing": sync.get("clockSmoothing", "soft"),
            "e131UniverseStart": int(sync.get("e131UniverseStart", 1)),
            "e131UniverseCount": int(sync.get("e131UniverseCount", 1)),
        }
    }


def _apply_sync_payload(payload: dict) -> None:
    section = payload.get("sync", payload) if isinstance(payload, dict) else {}
    if not isinstance(section, dict):
        return

    sync = _state.setdefault("sync", {})
    role = section.get("role", sync.get("role", "client"))
    enabled = section.get("enabled")

    if "mode" in section:
        sync["mode"] = section["mode"]
    elif enabled is not None:
        sync["mode"] = "off" if not enabled else ("cluster_sync" if role == "server" else "ledfx_realtime")

    fields = (
        "role",
        "inputProtocol",
        "ddpPort",
        "udpSyncPort",
        "sourceTimeoutMs",
        "groupMask",
        "clockSmoothing",
        "e131UniverseStart",
        "e131UniverseCount",
    )
    for field in fields:
        if field in section:
            sync[field] = section[field]


def _sync_state_for_ui() -> dict:
    sync_cfg = _sync_for_ui()["sync"]
    enabled = bool(sync_cfg.get("enabled"))
    input_protocol = sync_cfg.get("inputProtocol", "ddp")
    now_ms = int(time.time() * 1000)
    packets = (now_ms // 1000) % 5000 if enabled else 0
    return {
        "sync": {
            "connected": enabled,
            "degraded": False,
            "activeProtocol": input_protocol if enabled else "off",
            "stats": {
                "sourceAlive": enabled,
                "sourceIp": "127.0.0.1" if enabled else "-",
                "packetsReceived": packets,
                "packetsDropped": 0,
                "inputFps": 44 if enabled else 0,
                "framesApplied": packets,
                "lastSequence": packets,
                "clockOffsetMs": 0,
                "lastSyncEpochMs": now_ms,
                "timeoutEvents": 0,
                "pollSaturationEvents": 0,
                "syncStateSent": packets,
                "syncStateApplied": packets,
            },
        }
    }


# ---------------------------------------------------------------------------
# App Flask
# ---------------------------------------------------------------------------
app = Flask(__name__, static_folder=None)
app.config["JSON_SORT_KEYS"] = False


@app.before_request
def _track_request_start() -> None:
    g.request_started_at = time.perf_counter()


@app.after_request
def _cors(response):
    response.headers["Access-Control-Allow-Origin"] = "*"
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PATCH, OPTIONS"
    response.headers["Access-Control-Allow-Headers"] = "Content-Type"

    duration_ms = int((time.perf_counter() - g.get("request_started_at", time.perf_counter())) * 1000)
    request_body = request.get_data(cache=True, as_text=True) or ""

    response_body = ""
    content_type = (response.content_type or "").lower()
    is_log_endpoint = request.path.startswith("/api/v1/mock/logs")
    if request.path.startswith("/api/") and "application/json" in content_type and not is_log_endpoint:
        response_body = response.get_data(as_text=True) or ""

    entry = {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()),
        "durationMs": duration_ms,
        "remoteAddr": request.remote_addr,
        "method": request.method,
        "path": request.path,
        "query": request.query_string.decode("utf-8", errors="replace"),
        "status": response.status_code,
        "requestBody": "" if is_log_endpoint else _truncate_text(request_body),
        "responseBody": _truncate_text(response_body),
    }
    _append_interaction_log(entry)

    return response


@app.route("/", methods=["GET"])
def _root():
    return redirect("/ui/home.html")


# ---------------------------------------------------------------------------
# Servir UI estática
# ---------------------------------------------------------------------------
@app.route("/ui/<path:filename>", methods=["GET"])
def _ui(filename):
    if not UI_DIR.exists():
        return _err("UI directory not found. Expected: " + str(UI_DIR), 404)

    if filename.endswith(".html"):
        file_path = UI_DIR / filename
        if not file_path.exists():
            return _err("ui template not found: " + filename, 404)
        return _render_ui_template(filename), 200, {"Content-Type": "text/html; charset=utf-8"}

    return send_from_directory(UI_DIR, filename)


@app.route("/help/<path:filename>", methods=["GET"])
def _help_assets(filename):
    resolved_dir = _resolve_help_dir(filename)
    if resolved_dir:
        return send_from_directory(resolved_dir, filename)

    # Fallback visual para mockup: evita iconos rotos cuando faltan capturas.
    return Response(_help_placeholder_svg(filename), status=200, mimetype="image/svg+xml")


# ---------------------------------------------------------------------------
# /api/v1/state
# ---------------------------------------------------------------------------
@app.route("/api/v1/state", methods=["GET"])
def _get_state():
    return _ok(_full_state())


@app.route("/api/v1/state", methods=["POST", "PATCH", "OPTIONS"])
def _patch_state():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    # Validar effectId si viene
    if "effectId" in body:
        eid = body["effectId"]
        if isinstance(eid, str):
            match = next((e for e in EFFECTS if e["key"] == eid), None)
            body["effectId"] = match["id"] if match else _state["state"]["effectId"]
        if not _effect_by_id(body.get("effectId", 0)):
            return _err("invalid effectId")
    # Validar paletteId si viene
    if "paletteId" in body:
        pid = body["paletteId"]
        if isinstance(pid, str):
            match = next((p for p in PALETTES + _user_palettes if p["key"] == pid), None)
            body["paletteId"] = match["id"] if match else -1
    _apply_patch(_state["state"], body)
    return _ok(_full_state())


# ---------------------------------------------------------------------------
# /api/v1/config/*
# ---------------------------------------------------------------------------
@app.route("/api/v1/config/network", methods=["GET"])
def _get_network():
    return _ok(_network_for_ui())


@app.route("/api/v1/config/network", methods=["POST", "PATCH", "OPTIONS"])
def _patch_network():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_network_payload(body)
    return _ok({"ok": True, **_network_for_ui()})


@app.route("/api/v1/config/microphone", methods=["GET"])
def _get_microphone():
    return _ok(_microphone_for_ui())


@app.route("/api/v1/config/microphone", methods=["POST", "PATCH", "OPTIONS"])
def _patch_microphone():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_microphone_payload(body)
    return _ok({"ok": True, **_microphone_for_ui()})


@app.route("/api/v1/config/gpio", methods=["GET"])
def _get_gpio():
    return _ok(_gpio_for_ui())


@app.route("/api/v1/config/gpio", methods=["POST", "PATCH", "OPTIONS"])
def _patch_gpio():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_gpio_payload(body)
    return _ok({"ok": True, **_gpio_for_ui()})


@app.route("/api/v1/config/debug", methods=["GET"])
def _get_debug():
    return _ok(_debug_for_ui())


@app.route("/api/v1/config/debug", methods=["POST", "PATCH", "OPTIONS"])
def _patch_debug():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_debug_payload(body)
    return _ok({"ok": True, **_debug_for_ui()})


@app.route("/api/v1/config/general", methods=["GET"])
def _get_general():
    return _ok({"general": _state["general"]})


@app.route("/api/v1/config/general", methods=["POST", "PATCH", "OPTIONS"])
def _patch_general():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    section = body.get("general", body)
    if isinstance(section, dict):
        _apply_patch(_state.setdefault("general", {}), section)
    return _ok({"ok": True, "general": _state["general"]})


@app.route("/api/v1/config/all", methods=["GET"])
def _get_config_all():
    return _ok({
        "network":    _network_for_ui()["network"],
        "gpio":       _gpio_for_ui()["gpio"],
        "microphone": _microphone_for_ui()["microphone"],
        "general":    _state["general"],
        "sync":       _sync_for_ui()["sync"],
    })


@app.route("/api/v1/config/all", methods=["POST", "OPTIONS"])
def _post_config_all():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_network_payload(body)
    _apply_gpio_payload(body)
    _apply_microphone_payload(body)
    _apply_debug_payload(body.get("general", {}))
    _apply_sync_payload(body)

    general_section = body.get("general")
    if isinstance(general_section, dict):
        _apply_patch(_state.setdefault("general", {}), general_section)
    return _ok({"ok": True})


# ---------------------------------------------------------------------------
# /api/v1/sync/*  (estado de sincronización)
# ---------------------------------------------------------------------------
@app.route("/api/v1/config/sync", methods=["GET"])
def _get_sync():
    return _ok(_sync_for_ui())


@app.route("/api/v1/config/sync", methods=["POST", "PATCH", "OPTIONS"])
def _patch_sync():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_sync_payload(body)
    return _ok({"ok": True, **_sync_for_ui()})


@app.route("/api/v1/sync/config", methods=["GET", "POST", "PATCH", "OPTIONS"])
def _sync_config():
    if request.method == "OPTIONS":
        return _ok()
    if request.method in ("POST", "PATCH"):
        body = request.get_json(silent=True) or {}
        _apply_sync_payload(body)
        return _ok({"ok": True, **_sync_for_ui()})
    return _ok(_sync_for_ui())


@app.route("/api/v1/sync/state", methods=["GET"])
def _sync_state():
    return _ok(_sync_state_for_ui())


@app.route("/api/v1/sync/connected", methods=["GET"])
def _sync_connected():
    sync_cfg = _sync_for_ui()["sync"]
    return _ok({"connected": bool(sync_cfg.get("enabled")), "mode": sync_cfg.get("mode", "off")})


# ---------------------------------------------------------------------------
# /api/v1/effects
# ---------------------------------------------------------------------------
@app.route("/api/v1/effects", methods=["GET"])
def _get_effects():
    return _ok({
        "effects": EFFECTS,
        "startupEffect": _state.get("startup_effect", {}),
        "sequence": _state.get("effect_sequence", []),
    })


@app.route("/api/v1/effects/startup/save", methods=["POST", "PATCH", "OPTIONS"])
def _save_startup_effect():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _state["startup_effect"] = body
    return _ok({"ok": True})


@app.route("/api/v1/effects/sequence/add", methods=["POST", "PATCH", "OPTIONS"])
def _add_sequence():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _state.setdefault("effect_sequence", []).append(body)
    return _ok({"ok": True, "sequence": _state["effect_sequence"]})


@app.route("/api/v1/effects/sequence/delete", methods=["POST", "PATCH", "OPTIONS"])
def _delete_sequence():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    idx = body.get("index")
    seq = _state.get("effect_sequence", [])
    if idx is not None and 0 <= idx < len(seq):
        seq.pop(idx)
    else:
        _state["effect_sequence"] = []
    return _ok({"ok": True, "sequence": _state.get("effect_sequence", [])})


# ---------------------------------------------------------------------------
# /api/v1/palettes
# ---------------------------------------------------------------------------
@app.route("/api/v1/palettes", methods=["GET"])
def _get_palettes():
    return _ok({"palettes": PALETTES + _user_palettes})


@app.route("/api/v1/palettes/apply", methods=["POST", "PATCH", "OPTIONS"])
def _apply_palette():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    pid = body.get("paletteId", -1)
    if isinstance(pid, str):
        match = next((p for p in PALETTES + _user_palettes if p["key"] == pid), None)
        pid = match["id"] if match else -1
    _state["state"]["paletteId"] = pid
    pal = _palette_by_id(pid) or PALETTES[0]
    if pid != -1:
        _state["state"]["primaryColors"] = [pal["color1"], pal["color2"], pal["color3"]]
    return _ok({"ok": True, "paletteId": pid})


@app.route("/api/v1/palettes/save", methods=["POST", "PATCH", "OPTIONS"])
def _save_palette():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    pid_new = max((p["id"] for p in _user_palettes), default=11) + 1
    palette = {
        "id": body.get("id", pid_new),
        "key": body.get("key", f"user_palette_{pid_new}"),
        "label": body.get("label", f"Paleta {pid_new}"),
        "style": body.get("style", "custom"),
        "color1": body.get("color1", "#FF0000"),
        "color2": body.get("color2", "#00FF00"),
        "color3": body.get("color3", "#0000FF"),
    }
    # Upsert
    for i, p in enumerate(_user_palettes):
        if p["id"] == palette["id"] or p["key"] == palette["key"]:
            _user_palettes[i] = palette
            return _ok({"ok": True, "palette": palette})
    _user_palettes.append(palette)
    return _ok({"ok": True, "palette": palette})


@app.route("/api/v1/palettes/delete", methods=["POST", "PATCH", "OPTIONS"])
def _delete_palette():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    pid = body.get("paletteId") or body.get("id")
    before = len(_user_palettes)
    _user_palettes[:] = [p for p in _user_palettes if p["id"] != pid and p["key"] != pid]
    deleted = len(_user_palettes) < before
    return _ok({"ok": deleted})


# ---------------------------------------------------------------------------
# /api/v1/profiles
# ---------------------------------------------------------------------------
def _find_profile(pid: str) -> dict | None:
    return next((p for p in _state["profiles"] if p["id"] == pid), None)


@app.route("/api/v1/profiles", methods=["GET"])
def _get_profiles():
    default_id = next((p["id"] for p in _state["profiles"] if p.get("isDefault")), "default")
    return _ok({
        "profiles": _state["profiles"],
        "defaultProfileId": default_id,
    })


@app.route("/api/v1/profiles/get", methods=["GET"])
def _get_profile():
    pid = request.args.get("id", "default")
    profile = _find_profile(pid)
    if not profile:
        return _err(f"profile not found: {pid}", 404)
    full = copy.deepcopy(profile)
    full.update({
        "network":    _state["network"],
        "gpio":       _state["gpio"],
        "microphone": _state["microphone"],
        "general":    _state["general"],
    })
    return _ok(full)


@app.route("/api/v1/profiles/save", methods=["POST", "PATCH", "OPTIONS"])
def _save_profile():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    pid = body.get("id", str(uuid.uuid4())[:8])
    existing = _find_profile(pid)
    if existing:
        existing.update({k: v for k, v in body.items() if k not in ("readOnly", "builtIn")})
    else:
        _state["profiles"].append({
            "id": pid,
            "name": body.get("name", pid),
            "description": body.get("description", ""),
            "readOnly": False,
            "builtIn": False,
            "isDefault": False,
        })
    return _ok({"ok": True, "id": pid})


@app.route("/api/v1/profiles/clone", methods=["POST", "PATCH", "OPTIONS"])
def _clone_profile():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    src_id = body.get("sourceId") or body.get("id")
    src = _find_profile(src_id)
    if not src:
        return _err("source profile not found", 404)
    new_id = body.get("newId", str(uuid.uuid4())[:8])
    new_profile = copy.deepcopy(src)
    new_profile["id"] = new_id
    new_profile["name"] = body.get("name", src["name"] + " (copia)")
    new_profile["readOnly"] = False
    new_profile["builtIn"] = False
    new_profile["isDefault"] = False
    _state["profiles"].append(new_profile)
    return _ok({"ok": True, "id": new_id})


@app.route("/api/v1/profiles/apply", methods=["POST", "PATCH", "OPTIONS"])
def _apply_profile():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    pid = body.get("id")
    profile = _find_profile(pid)
    if not profile:
        return _err(f"profile not found: {pid}", 404)
    # En el mockup solo marcamos que se aplicó (no hay hardware que reconfigurar)
    return _ok({"ok": True, "applied": pid})


@app.route("/api/v1/profiles/default", methods=["POST", "PATCH", "OPTIONS"])
def _set_default_profile():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    pid = body.get("id")
    if not _find_profile(pid):
        return _err(f"profile not found: {pid}", 404)
    for p in _state["profiles"]:
        p["isDefault"] = p["id"] == pid
    return _ok({"ok": True, "defaultProfileId": pid})


@app.route("/api/v1/profiles/delete", methods=["POST", "PATCH", "OPTIONS"])
def _delete_profile():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    pid = body.get("id")
    profile = _find_profile(pid)
    if not profile:
        return _err(f"profile not found: {pid}", 404)
    if profile.get("readOnly") or profile.get("builtIn"):
        return _err("cannot delete a built-in or read-only profile", 403)
    _state["profiles"] = [p for p in _state["profiles"] if p["id"] != pid]
    return _ok({"ok": True})


# ---------------------------------------------------------------------------
# /api/v1/hardware  &  /api/v1/release  &  /api/v1/diag
# ---------------------------------------------------------------------------
@app.route("/api/v1/hardware", methods=["GET"])
def _get_hardware():
    return _ok({
        "chipModel": "ESP32-MOCK",
        "chipRevision": 3,
        "cores": 2,
        "cpuFreqMHz": 240,
        "hasWifi": True,
        "hasBluetoothClassic": True,
        "hasBluetoothLe": True,
        "flashSizeBytes": 4194304,
        "flashSizeMb": 4,
        "flashSpeedHz": 80000000,
        "mac": "DE:AD:BE:EF:00:01",
        "_mockup": True,
    })


@app.route("/api/v1/release", methods=["GET"])
def _get_release():
    return _ok({
        "version": "0.6.3-alpha",
        "releaseDate": "2026-05-03",
        "branch": "main",
        "board": "mockup",
        "repositoryUrl": "https://github.com/duxman/DUXMAN-LED-NEXT",
        "repositoryGitUrl": "https://github.com/duxman/DUXMAN-LED-NEXT.git",
        "_mockup": True,
    })


@app.route("/api/v1/diag", methods=["GET"])
def _get_diag():
    return _ok({
        "uptime_ms": int(time.time() * 1000) % (1000 * 60 * 60 * 24),
        "freeHeap": 200000,
        "minFreeHeap": 150000,
        "totalHeap": 327680,
        "wifiRssi": -65,
        "wifiConnected": False,
        "ip": "192.168.4.1",
        "_mockup": True,
    })


# ---------------------------------------------------------------------------
# /api/v1/system/restart
# ---------------------------------------------------------------------------
@app.route("/api/v1/system/restart", methods=["POST", "OPTIONS"])
def _restart():
    if request.method == "OPTIONS":
        return _ok()
    # En el mockup recargamos el estado desde fichero (simula un reinicio)
    global _state
    _state = _load_state()
    return _ok({"ok": True, "restarted": True, "_mockup": "state reloaded from file"})


# ---------------------------------------------------------------------------
# /api/v1/openapi.json  (esqueleto mínimo)
# ---------------------------------------------------------------------------
@app.route("/api/v1/openapi.json", methods=["GET"])
def _openapi():
    spec = {
        "openapi": "3.0.3",
        "info": {"title": "DUXMAN-LED-NEXT API", "version": "1.0.0"},
        "paths": {
            "/api/v1/state": {"get": {}, "patch": {}},
            "/api/v1/config/network": {"get": {}, "patch": {}},
            "/api/v1/config/microphone": {"get": {}, "patch": {}},
            "/api/v1/config/gpio": {"get": {}, "patch": {}},
            "/api/v1/config/debug": {"get": {}, "patch": {}},
            "/api/v1/config/all": {"get": {}, "post": {}},
            "/api/v1/profiles": {"get": {}},
            "/api/v1/effects": {"get": {}},
            "/api/v1/palettes": {"get": {}},
            "/api/v1/hardware": {"get": {}},
            "/api/v1/release": {"get": {}},
            "/api/v1/diag": {"get": {}},
            "/api/v1/system/restart": {"post": {}},
        },
    }
    return _ok(spec)


@app.route("/api/v1/mock/logs", methods=["GET"])
def _get_mock_logs():
    limit = request.args.get("limit", default=200, type=int)
    bounded_limit = min(max(limit, 1), 1000)
    return _ok({
        "ok": True,
        "logFile": str(LOG_FILE_PATH),
        "count": bounded_limit,
        "entries": _read_log_tail(bounded_limit),
    })


@app.route("/api/v1/mock/logs/clear", methods=["POST", "OPTIONS"])
def _clear_mock_logs():
    if request.method == "OPTIONS":
        return _ok()

    for path in [LOG_FILE_PATH, *LOG_FILE_PATH.parent.glob(LOG_FILE_PATH.name + ".*")]:
        if path.exists():
            path.unlink()
    return _ok({"ok": True, "cleared": True})


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="DUXMAN-LED-NEXT Mockup Server")
    parser.add_argument("--host", default="127.0.0.1", help="Host a escuchar (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8080, help="Puerto (default: 8080)")
    parser.add_argument("--log-file", default=str(DEFAULT_LOG_FILE), help="Ruta de log JSONL del mock")
    parser.add_argument("--log-level", default=DEFAULT_LOG_LEVEL, help="Nivel de log (DEBUG, INFO, WARNING, ERROR)")
    args = parser.parse_args()

    _setup_interaction_logger(Path(args.log_file), args.log_level)

    if not UI_DIR.exists():
        print(f"⚠️  Directorio UI no encontrado: {UI_DIR}")
        print("   La API REST funcionará igualmente, pero las páginas web no se servirán.")

    print(f"""
╔══════════════════════════════════════════════════╗
║        DUXMAN-LED-NEXT — Mockup Server           ║
╠══════════════════════════════════════════════════╣
║  UI:    http://{args.host}:{args.port}/                      
║  API:   http://{args.host}:{args.port}/api/v1/state         
║  Estado guardado en: mock_state.json             ║
║  Log JSONL: {str(LOG_FILE_PATH)} ║
║                                                  ║
║  POST /api/v1/system/restart → recarga el estado ║
║  Ctrl+C para detener                             ║
╚══════════════════════════════════════════════════╝
""")

    app.run(host=args.host, port=args.port, debug=False)
