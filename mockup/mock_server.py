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
import json
import os
import sys
import time
import uuid
from pathlib import Path

from flask import Flask, jsonify, redirect, request, send_from_directory

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
HERE = Path(__file__).parent.resolve()
REPO_ROOT = HERE.parent
UI_DIR = REPO_ROOT / "data" / "ui"
STATE_FILE = HERE / "mock_state.json"

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


# ---------------------------------------------------------------------------
# App Flask
# ---------------------------------------------------------------------------
app = Flask(__name__, static_folder=None)
app.config["JSON_SORT_KEYS"] = False


@app.after_request
def _cors(response):
    response.headers["Access-Control-Allow-Origin"] = "*"
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PATCH, OPTIONS"
    response.headers["Access-Control-Allow-Headers"] = "Content-Type"
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
    return send_from_directory(UI_DIR, filename)


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
    return _ok(_state["network"])


@app.route("/api/v1/config/network", methods=["POST", "PATCH", "OPTIONS"])
def _patch_network():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_patch(_state["network"], body)
    return _ok({"ok": True, "network": _state["network"]})


@app.route("/api/v1/config/microphone", methods=["GET"])
def _get_microphone():
    return _ok(_state["microphone"])


@app.route("/api/v1/config/microphone", methods=["POST", "PATCH", "OPTIONS"])
def _patch_microphone():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_patch(_state["microphone"], body)
    return _ok({"ok": True, "microphone": _state["microphone"]})


@app.route("/api/v1/config/gpio", methods=["GET"])
def _get_gpio():
    return _ok(_state["gpio"])


@app.route("/api/v1/config/gpio", methods=["POST", "PATCH", "OPTIONS"])
def _patch_gpio():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_patch(_state["gpio"], body)
    return _ok({"ok": True, "gpio": _state["gpio"]})


@app.route("/api/v1/config/debug", methods=["GET"])
def _get_debug():
    return _ok(_state["general"])


@app.route("/api/v1/config/debug", methods=["POST", "PATCH", "OPTIONS"])
def _patch_debug():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_patch(_state["general"], body)
    return _ok({"ok": True, "general": _state["general"]})


@app.route("/api/v1/config/general", methods=["GET"])
def _get_general():
    return _ok({"general": _state["general"]})


@app.route("/api/v1/config/all", methods=["GET"])
def _get_config_all():
    return _ok({
        "network":    _state["network"],
        "gpio":       _state["gpio"],
        "microphone": _state["microphone"],
        "general":    _state["general"],
        "sync":       _state["sync"],
    })


@app.route("/api/v1/config/all", methods=["POST", "OPTIONS"])
def _post_config_all():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    for section in ("network", "gpio", "microphone", "general", "sync"):
        if section in body:
            _apply_patch(_state[section], body[section])
    return _ok({"ok": True})


# ---------------------------------------------------------------------------
# /api/v1/sync/*  (estado de sincronización)
# ---------------------------------------------------------------------------
@app.route("/api/v1/config/sync", methods=["GET"])
def _get_sync():
    return _ok(_state["sync"])


@app.route("/api/v1/config/sync", methods=["POST", "PATCH", "OPTIONS"])
def _patch_sync():
    if request.method == "OPTIONS":
        return _ok()
    body = request.get_json(silent=True) or {}
    _apply_patch(_state["sync"], body)
    return _ok({"ok": True, "sync": _state["sync"]})


@app.route("/api/v1/sync/connected", methods=["GET"])
def _sync_connected():
    return _ok({"connected": False, "mode": _state["sync"]["mode"]})


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


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="DUXMAN-LED-NEXT Mockup Server")
    parser.add_argument("--host", default="127.0.0.1", help="Host a escuchar (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8080, help="Puerto (default: 8080)")
    args = parser.parse_args()

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
║                                                  ║
║  POST /api/v1/system/restart → recarga el estado ║
║  Ctrl+C para detener                             ║
╚══════════════════════════════════════════════════╝
""")

    app.run(host=args.host, port=args.port, debug=False)
