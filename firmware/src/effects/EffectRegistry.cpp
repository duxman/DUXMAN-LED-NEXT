#include "effects/EffectRegistry.h"

#include <ArduinoJson.h>

namespace {
constexpr EffectDescriptor kEffects[] = {
    {EffectRegistry::kEffectFixed, "fixed", "Color fijo",
    "Alterna los 3 colores por secciones fijas.", false},
    {EffectRegistry::kEffectGradient, "gradient", "Degradado fijo",
    "Genera un degradado estatico con los 3 colores dentro de cada seccion.", false},
   {EffectRegistry::kEffectBlinkFixed, "blink_fixed", "Parpadeo fijo",
    "Parpadea el patron fijo por secciones usando la velocidad configurada.", true},
   {EffectRegistry::kEffectBlinkGradient, "blink_gradient", "Parpadeo degradado",
    "Parpadea el degradado por secciones usando la velocidad configurada.", true},
   {EffectRegistry::kEffectDiagnostic, "diagnostic", "Diagnostico",
    "Activa solo la primera salida en rojo para aislar la linea principal.", false},
   {EffectRegistry::kEffectBreathFixed, "breath_fixed", "Respiracion fija",
    "Las secciones de color fijo respiran con una envolvente senoidal.", true},
   {EffectRegistry::kEffectBreathGradient, "breath_gradient", "Respiracion degradado",
    "El degradado completo respira con una envolvente senoidal.", true},
     {EffectRegistry::kEffectTripleChase, "triple_chase", "Triple chase",
    "Tren de color en movimiento con huecos y repeticiones por secciones.", true},
     {EffectRegistry::kEffectGradientMeteor, "gradient_meteor", "Meteorito degradado",
    "Cabeza y cola en movimiento con color degradado a lo largo de la tira.", true},
    {EffectRegistry::kEffectScanningPulse, "scanning_pulse", "Pulso barrido",
    "Pulso que recorre la tira de extremo a extremo con rebote.", true},
    {EffectRegistry::kEffectTrigRibbon, "trig_ribbon", "Cinta trigonometrica",
    "Patron ondulante con mezcla de sinusoides y gradiente dinamico.", true},
    {EffectRegistry::kEffectLavaFlow, "lava_flow", "Lava flow",
    "Flujo organico con deformacion de ondas y mezcla calida.", true},
    {EffectRegistry::kEffectPolarIce, "polar_ice", "Polar ice",
    "Interferencia fria con ondas lentas y contraste polar.", true},
    {EffectRegistry::kEffectStellarTwinkle, "stellar_twinkle", "Stellar twinkle",
    "Destellos estelares pseudoaleatorios sobre fondo base.", true},
    {EffectRegistry::kEffectRandomColorPop, "random_color_pop", "Random color pop",
    "Apariciones de color aleatorias con envolvente de atenuacion.", true},
    {EffectRegistry::kEffectBouncingPhysics, "bouncing_physics", "Bouncing physics",
    "Bolas luminosas que rebotan con energia variable.", true},
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
  }

  String json;
  serializeJson(doc, json);
  return json;
}

String buildHtmlOptions(uint8_t selectedEffectId) {
  String html;
  for (size_t i = 0; i < count(); ++i) {
    html += "<option value='";
    html += kEffects[i].key;
    html += "'";
    if (kEffects[i].id == selectedEffectId) {
      html += " selected";
    }
    html += ">";
    html += kEffects[i].label;
    html += "</option>";
  }
  return html;
}
} // namespace EffectRegistry