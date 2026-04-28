/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/effects/EffectRegistry.cpp
 * Last commit: a67d822 - 2026-04-12
 */

#include "effects/EffectRegistry.h"

#include <ArduinoJson.h>

namespace {
constexpr EffectDescriptor kEffects[] = {
    {EffectRegistry::kEffectFixed, "fixed", "Color fijo",
  "Alterna los 3 colores por secciones fijas.", false, false},
    {EffectRegistry::kEffectGradient, "gradient", "Degradado fijo",
  "Genera un degradado estatico con los 3 colores dentro de cada seccion.", false, false},
   {EffectRegistry::kEffectBlinkFixed, "blink_fixed", "Parpadeo fijo",
  "Parpadea el patron fijo por secciones usando la velocidad configurada.", true, false},
   {EffectRegistry::kEffectBlinkGradient, "blink_gradient", "Parpadeo degradado",
  "Parpadea el degradado por secciones usando la velocidad configurada.", true, false},
   {EffectRegistry::kEffectDiagnostic, "diagnostic", "Diagnostico",
  "Activa solo la primera salida en rojo para aislar la linea principal.", false, false},
   {EffectRegistry::kEffectBreathFixed, "breath_fixed", "Respiracion fija",
  "Las secciones de color fijo respiran con una envolvente senoidal.", true, false},
   {EffectRegistry::kEffectBreathGradient, "breath_gradient", "Respiracion degradado",
  "El degradado completo respira con una envolvente senoidal.", true, false},
     {EffectRegistry::kEffectTripleChase, "triple_chase", "Triple chase",
  "Tren de color en movimiento con huecos y repeticiones por secciones.", true, false},
     {EffectRegistry::kEffectGradientMeteor, "gradient_meteor", "Meteorito degradado",
  "Cabeza y cola en movimiento con color degradado a lo largo de la tira.", true, false},
    {EffectRegistry::kEffectScanningPulse, "scanning_pulse", "Pulso barrido",
  "Pulso que recorre la tira de extremo a extremo con rebote.", true, false},
  {EffectRegistry::kEffectTrigRibbon, "trig_ribbon", "AUDIO · Cinta trigonometrica",
  "Patron ondulante con mezcla de sinusoides y gradiente dinamico guiado por el micro.", true, true},
    {EffectRegistry::kEffectLavaFlow, "lava_flow", "Lava flow",
  "Flujo organico con deformacion de ondas y mezcla calida.", true, false},
    {EffectRegistry::kEffectPolarIce, "polar_ice", "Polar ice",
  "Interferencia fria con ondas lentas y contraste polar.", true, false},
  {EffectRegistry::kEffectStellarTwinkle, "stellar_twinkle", "AUDIO · Stellar twinkle",
  "Destellos estelares pseudoaleatorios reforzados por beats y nivel del micro.", true, true},
    {EffectRegistry::kEffectRandomColorPop, "random_color_pop", "Random color pop",
  "Apariciones de color aleatorias con envolvente de atenuacion.", true, false},
  {EffectRegistry::kEffectBouncingPhysics, "bouncing_physics", "AUDIO · Bouncing physics",
  "Bolas luminosas que rebotan con energia variable y responden al micro.", true, true},
  {EffectRegistry::kEffectAudioPulse, "audio_pulse", "AUDIO · Audio Pulse",
  "VU metro simetrico reactivo: beat flash, peak hold y color dinamico al audio.", false, true},
};
}

namespace EffectRegistry {
const EffectDescriptor *all() {
  return kEffects;
}

size_t count() {
  return sizeof(kEffects) / sizeof(kEffects[0]);
}

const EffectDescriptor &defaultEffect() {
  return kEffects[0];
}

const EffectDescriptor *findById(uint8_t effectId) {
  for (size_t i = 0; i < count(); ++i) {
    if (kEffects[i].id == effectId) {
      return &kEffects[i];
    }
  }
  return nullptr;
}

const EffectDescriptor *findByKey(const String &effectKey) {
  for (size_t i = 0; i < count(); ++i) {
    if (effectKey == kEffects[i].key) {
      return &kEffects[i];
    }
  }

  if (effectKey == "fijo" || effectKey == "color_fijo") {
    return findById(kEffectFixed);
  }
  if (effectKey == "degradado" || effectKey == "degradado_fijo") {
    return findById(kEffectGradient);
  }
  if (effectKey == "parpadeo_fijo" || effectKey == "blink" || effectKey == "blink-fixed") {
    return findById(kEffectBlinkFixed);
  }
  if (effectKey == "parpadeo_degradado" || effectKey == "blink-gradient") {
    return findById(kEffectBlinkGradient);
  }
  if (effectKey == "diagnostic" || effectKey == "diagnostico") {
    return findById(kEffectDiagnostic);
  }
  if (effectKey == "triple-chase" || effectKey == "chase") {
    return findById(kEffectTripleChase);
  }
  if (effectKey == "meteor" || effectKey == "gradient-meteor" || effectKey == "meteorito") {
    return findById(kEffectGradientMeteor);
  }
  if (effectKey == "scan" || effectKey == "pulse" || effectKey == "scan-pulse") {
    return findById(kEffectScanningPulse);
  }
  if (effectKey == "ribbon" || effectKey == "trig-ribbon") {
    return findById(kEffectTrigRibbon);
  }
  if (effectKey == "lava") {
    return findById(kEffectLavaFlow);
  }
  if (effectKey == "ice" || effectKey == "polar") {
    return findById(kEffectPolarIce);
  }
  if (effectKey == "twinkle" || effectKey == "stellar") {
    return findById(kEffectStellarTwinkle);
  }
  if (effectKey == "pop" || effectKey == "color-pop") {
    return findById(kEffectRandomColorPop);
  }
  if (effectKey == "bouncing" || effectKey == "physics") {
    return findById(kEffectBouncingPhysics);
  }

  return nullptr;
}

const char *keyFor(uint8_t effectId) {
  const EffectDescriptor *effect = findById(effectId);
  return effect != nullptr ? effect->key : defaultEffect().key;
}

const char *labelFor(uint8_t effectId) {
  const EffectDescriptor *effect = findById(effectId);
  return effect != nullptr ? effect->label : defaultEffect().label;
}

bool usesAudio(uint8_t effectId) {
  const EffectDescriptor *effect = findById(effectId);
  return effect != nullptr ? effect->usesAudio : false;
}

uint8_t parseId(const String &value, uint8_t fallback) {
  if (value.isEmpty()) {
    return fallback;
  }

  bool isNumeric = true;
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isDigit(value[i])) {
      isNumeric = false;
      break;
    }
  }

  if (isNumeric) {
    const EffectDescriptor *effect = findById(static_cast<uint8_t>(value.toInt()));
    return effect != nullptr ? effect->id : fallback;
  }

  const EffectDescriptor *effect = findByKey(value);
  return effect != nullptr ? effect->id : fallback;
}

String toJsonArray() {
  JsonDocument doc;
  JsonArray effects = doc.to<JsonArray>();
  for (size_t i = 0; i < count(); ++i) {
    JsonObject item = effects.add<JsonObject>();
    item["id"] = kEffects[i].id;
    item["key"] = kEffects[i].key;
    item["label"] = kEffects[i].label;
    item["description"] = kEffects[i].description;
    item["usesSpeed"] = kEffects[i].usesSpeed;
    item["usesAudio"] = kEffects[i].usesAudio;
  }

  String json;
  serializeJson(doc, json);
  return json;
}

String buildHtmlOptions(uint8_t selectedEffectId) {
  String visualHtml;
  String audioHtml;
  for (size_t i = 0; i < count(); ++i) {
    String option = "<option value='";
    option += kEffects[i].key;
    option += "' data-audio='";
    option += kEffects[i].usesAudio ? "1" : "0";
    option += "'";
    if (kEffects[i].id == selectedEffectId) {
      option += " selected";
    }
    option += ">";
    option += kEffects[i].label;
    option += "</option>";

    if (kEffects[i].usesAudio) {
      audioHtml += option;
    } else {
      visualHtml += option;
    }
  }

  String html;
  if (!visualHtml.isEmpty()) {
    html += "<optgroup label='Visuales'>";
    html += visualHtml;
    html += "</optgroup>";
  }
  if (!audioHtml.isEmpty()) {
    html += "<optgroup label='Audio reactivos'>";
    html += audioHtml;
    html += "</optgroup>";
  }
  return html;
}
} // namespace EffectRegistry