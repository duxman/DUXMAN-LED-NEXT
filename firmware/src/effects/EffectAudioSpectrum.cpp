/*
 * duxman-led next - v0.3.9-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectAudioSpectrum.cpp
 */

#include "effects/EffectAudioSpectrum.h"

#include "effects/EffectRegistry.h"

// ──────────────────────────────────────────────────────────────────────────
// Constantes de suavizado
// ──────────────────────────────────────────────────────────────────────────
// El suavizado es exponencial: nivel_n = nivel_{n-1} * (1 - k) + nuevo * k
// Rise rápido, decay lento → aspecto VU clásico.
static constexpr float kRiseK  = 0.35f;   // ataque (subida)
static constexpr float kDecayK = 0.08f;   // decaimiento (bajada)

// Umbral a partir del cual el audioLevel se clasifica como "medios".
// Por debajo de este umbral, el nivel alimenta principalmente los bajos.
static constexpr float kMidThreshold  = 0.35f;
// Umbral para los altos: el audioPeakHold escala en este rango.
static constexpr float kHighThreshold = 0.55f;

// Duración del flash de beat en los bajos (ms).
static constexpr unsigned long kBeatFlashMs = 120;

// ──────────────────────────────────────────────────────────────────────────

bool EffectAudioSpectrum::supports(uint8_t effectId) const {
  return effectId == EffectRegistry::kEffectAudioSpectrum;
}

void EffectAudioSpectrum::onActivate() {
  levelLow_  = 0.0f;
  levelMid_  = 0.0f;
  levelHigh_ = 0.0f;
  beatMs_    = millis() - kBeatFlashMs;
}

// ──────────────────────────────────────────────────────────────────────────
// Lógica de suavizado por banda
// ──────────────────────────────────────────────────────────────────────────
static float smoothBand(float current, float target) {
  const float k = (target > current) ? kRiseK : kDecayK;
  return current + (target - current) * k;
}

// ──────────────────────────────────────────────────────────────────────────
// Dibujo de un segmento: VU que crece desde los extremos hacia el centro
// ──────────────────────────────────────────────────────────────────────────
void EffectAudioSpectrum::drawBand(LedDriver &led, uint8_t outIdx,
                                   uint16_t startPx, uint16_t segLen,
                                   float bandLevel, float beatFlash,
                                   uint32_t color, float brightness) const {
  if (segLen == 0) return;

  // Número de píxeles encendidos en cada mitad del segmento.
  const uint16_t half = segLen / 2;
  const uint16_t fillPixels = static_cast<uint16_t>(bandLevel * static_cast<float>(half) + 0.5f);

  for (uint16_t i = 0; i < segLen; ++i) {
    const uint16_t px = startPx + i;

    // Normalizar posición dentro del segmento como distancia al borde más
    // cercano (0 = borde, 1 = centro).  El VU crece desde los bordes hacia el centro.
    const uint16_t distFromEdge = (i < half) ? i : (segLen - 1 - i);

    float intensity = 0.0f;

    if (distFromEdge < fillPixels) {
      // Degradado: máximo brillo en el borde activo, baja hacia el interior.
      const float t = static_cast<float>(distFromEdge + 1) / static_cast<float>(fillPixels + 1);
      // Curva: más brillante en la punta exterior.
      intensity = smoothstep(0.0f, 1.0f, 1.0f - t) * 0.5f + 0.5f;
    }

    // Overlay de beat flash.
    intensity = clamp01(intensity + beatFlash * 0.9f);

    // Color: mezcla con blanco durante el beat flash.
    uint32_t col = color;
    if (beatFlash > 0.0f) {
      col = lerpColor(col, 0xFFFFFF, beatFlash * 0.50f);
    }

    led.setPixelColor(outIdx, px, scaleColorFloat(col, clamp01(intensity) * brightness));
  }
}

// ──────────────────────────────────────────────────────────────────────────
// renderFrame
// ──────────────────────────────────────────────────────────────────────────
void EffectAudioSpectrum::renderFrame() {
  CoreState &s   = state();
  LedDriver  &led = driver();

  const float brightness = clamp01(static_cast<float>(s.brightness) / 255.0f);
  const unsigned long nowMs = millis();

  // ── Beat flash ─────────────────────────────────────────────────────────
  if (s.reactiveToAudio && s.beatDetected) {
    beatMs_ = nowMs;
  }
  const float beatFlash = clamp01(
      1.0f - static_cast<float>(nowMs - beatMs_) / static_cast<float>(kBeatFlashMs));

  // ── Derivación de niveles por banda desde audioLevel y audioPeakHold ───
  // audioLevel es el RMS instantáneo, audioPeakHold es el pico lento.
  // Bandas:
  //   Bajos  (B): se dispara con cada beat + nivel bajo de audio.
  //   Medios (M): sigue el audioLevel en el rango medio-alto.
  //   Altos  (H): sigue audioPeakHold (pico suavizado), representa transientes.
  const float rawAudio = clamp01(static_cast<float>(s.audioLevel) / 255.0f);
  const float rawPeak  = clamp01(static_cast<float>(s.audioPeakHold) / 255.0f);

  float targetLow, targetMid, targetHigh;
  if (s.reactiveToAudio) {
    // Bajos: escala el rango [0..kMidThreshold] de audioLevel + boost en beat.
    targetLow  = clamp01(rawAudio / max(kMidThreshold, 0.01f)) * 0.6f
                 + beatFlash * 0.4f;

    // Medios: rango completo del audioLevel pero rampa que arranca en kMidThreshold.
    const float midStart = clamp01((rawAudio - kMidThreshold) / (1.0f - kMidThreshold));
    targetMid  = 0.15f + midStart * 0.85f;   // nunca a cero para dar vida
    targetMid *= rawAudio;                    // proporcional al volumen real

    // Altos: audioPeakHold escala en [kHighThreshold..1.0].
    const float highStart = clamp01((rawPeak - kHighThreshold) / (1.0f - kHighThreshold));
    targetHigh = 0.10f + highStart * 0.90f;
    targetHigh *= rawPeak;
  } else {
    // Sin audio: estado estático en 80% para mostrar los colores claramente.
    targetLow = targetMid = targetHigh = 0.80f;
  }

  levelLow_  = smoothBand(levelLow_,  targetLow);
  levelMid_  = smoothBand(levelMid_,  targetMid);
  levelHigh_ = smoothBand(levelHigh_, targetHigh);

  // Sin audio reactivo no hay flash de beat.
  const float beatFlashApplied = s.reactiveToAudio ? beatFlash : 0.0f;

  // ── Distribución en salidas ─────────────────────────────────────────────
  // Mapeamos 3 bandas a las salidas disponibles con dos estrategias:
  //   A) ≥3 salidas activas con soporte per-píxel → cada salida = una banda.
  //   B) <3 salidas → dividimos cada salida en sectionCount segmentos,
  //      segmento i → banda (i % 3).

  // Contamos salidas per-pixel habilitadas.
  uint8_t ppOutputs = 0;
  for (uint8_t o = 0; o < led.outputCount(); ++o) {
    if (led.outputConfig(o).enabled && led.supportsPerPixelColor(o)) ++ppOutputs;
  }

  const float bands[3]  = { levelLow_, levelMid_, levelHigh_ };
  const uint32_t cols[3] = { s.primaryColors[0], s.primaryColors[1], s.primaryColors[2] };

  if (ppOutputs >= 3) {
    // ── Modo A: una salida por banda ────────────────────────────────────
    uint8_t bandIdx = 0;
    for (uint8_t o = 0; o < led.outputCount() && bandIdx < 3; ++o) {
      const LedDriverOutputConfig &out = led.outputConfig(o);
      if (!out.enabled) continue;

      if (!led.supportsPerPixelColor(o)) {
        // Salida digital: brillo proporcional a la banda.
        const float g = bands[bandIdx] * (beatFlashApplied > 0.0f && bandIdx == 0
                                          ? clamp01(1.0f + beatFlashApplied) : 1.0f);
        led.setOutputColor(o, scaleColorFloat(cols[bandIdx], clamp01(g) * brightness));
        ++bandIdx;
        continue;
      }

      // VU band completa en la salida.
      const float bf = (bandIdx == 0) ? beatFlashApplied : 0.0f;
      drawBand(led, o, 0, out.ledCount, bands[bandIdx], bf, cols[bandIdx], brightness);
      ++bandIdx;
    }
  } else {
    // ── Modo B: segmentos dentro de cada salida ──────────────────────────
    for (uint8_t o = 0; o < led.outputCount(); ++o) {
      const LedDriverOutputConfig &out = led.outputConfig(o);
      if (!out.enabled) continue;

      if (!led.supportsPerPixelColor(o)) {
        // Sin per-píxel: alterna color por beat/sección.
        const uint8_t bi = static_cast<uint8_t>((millis() / 300UL) % 3);
        led.setOutputColor(o, scaleColorFloat(cols[bi], bands[bi] * brightness));
        continue;
      }

      const uint8_t sections = max<uint8_t>(1, s.sectionCount);
      const uint16_t segLen  = resolveSectionSize(out.ledCount, sections);

      for (uint8_t seg = 0; seg < sections; ++seg) {
        const uint8_t  bi      = seg % 3;
        const uint16_t startPx = static_cast<uint16_t>(seg) * segLen;
        // Evitar sobrepasar el total de píxeles.
        const uint16_t available = (startPx < out.ledCount)
                                   ? static_cast<uint16_t>(out.ledCount - startPx)
                                   : 0;
        const uint16_t len = (segLen < available) ? segLen : available;
        if (len == 0) break;

        const float bf = (bi == 0) ? beatFlashApplied : 0.0f;
        drawBand(led, o, startPx, len, bands[bi], bf, cols[bi], brightness);
      }
    }
  }

  led.show();
}
