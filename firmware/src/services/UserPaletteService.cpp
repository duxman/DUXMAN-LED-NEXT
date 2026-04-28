/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/UserPaletteService.cpp
 * Last commit: 6edd14b - 2026-04-28
 */

#include "services/UserPaletteService.h"
#include "core/PaletteRegistry.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
constexpr const char *kUserPalettesPath = "/user-palettes.json";
} // namespace

UserPaletteService::UserPaletteService(
    PersistenceSchedulerService &persistenceSchedulerService)
    : persistenceSchedulerService_(persistenceSchedulerService) {}

void UserPaletteService::begin() {
  load();
}

void UserPaletteService::processPendingPersistence() {
  if (!pendingPersistence_) {
    return;
  }
  pendingPersistence_ = false;
  if (!save()) {
    Serial.println("[palettes][async] save failed");
  }
}

void UserPaletteService::requestSave() {
  pendingPersistence_ = true;
}

// ---------- listado ----------

String UserPaletteService::listAllJson() const {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  JsonArray arr = root["palettes"].to<JsonArray>();
  appendSystemJson(arr);
  appendUserJson(arr);
  String out;
  serializeJson(doc, out);
  return out;
}

String UserPaletteService::listUserJson() const {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  JsonArray arr = root["palettes"].to<JsonArray>();
  appendUserJson(arr);
  String out;
  serializeJson(doc, out);
  return out;
}

// ---------- guardar ----------

bool UserPaletteService::saveFromJson(const String &payload, String *response,
                                       String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    if (error) *error = "invalid_json";
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst pObj = root["palette"].isNull() ? root : root["palette"].as<JsonObjectConst>();

  // Acepta tanto 'id' como 'key' como identificador
  String key = pObj["id"] | String("");
  if (key.isEmpty()) key = pObj["key"] | String("");
  if (!isValidKey(key)) {
    if (error) *error = "invalid_palette_id";
    return false;
  }

  // No se puede sobreescribir paletas de sistema
  if (PaletteRegistry::findByKey(key) != nullptr) {
    if (error) *error = "readonly_system_palette";
    return false;
  }

  // Acepta primaryColors[] o colors.{color1,color2,color3}
  String c1, c2, c3;
  if (!pObj["primaryColors"].isNull()) {
    JsonArrayConst colorsArr = pObj["primaryColors"].as<JsonArrayConst>();
    if (colorsArr.isNull() || colorsArr.size() < 3) {
      if (error) *error = "missing_primary_colors";
      return false;
    }
    c1 = colorsArr[0] | String("");
    c2 = colorsArr[1] | String("");
    c3 = colorsArr[2] | String("");
  } else if (!pObj["colors"].isNull()) {
    JsonObjectConst colorsObj = pObj["colors"].as<JsonObjectConst>();
    c1 = colorsObj["color1"] | String("");
    c2 = colorsObj["color2"] | String("");
    c3 = colorsObj["color3"] | String("");
  } else {
    if (error) *error = "missing_colors";
    return false;
  }

  UserPalette palette;
  palette.id = key;
  palette.label = pObj["label"] | key;
  palette.style = pObj["style"] | String("custom");
  palette.description = pObj["description"] | String("");
  palette.color1 = parseHexColor(c1);
  palette.color2 = parseHexColor(c2);
  palette.color3 = parseHexColor(c3);

  // Upsert
  UserPalette *existing = findByKeyMutable(key);
  if (existing != nullptr) {
    *existing = palette;
  } else {
    if (userCount_ >= kMaxUserPalettes) {
      if (error) *error = "palette_limit_reached";
      return false;
    }
    userPalettes_[userCount_++] = palette;
  }

  requestSave();

  if (response) {
    JsonDocument resp;
    resp["saved"] = true;
    resp["id"] = key;
    serializeJson(resp, *response);
  }
  return true;
}

// ---------- eliminar ----------

bool UserPaletteService::deleteFromJson(const String &payload, String *response,
                                         String *error) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    if (error) *error = "invalid_json";
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  JsonObjectConst pObj = root["palette"].isNull() ? root : root["palette"].as<JsonObjectConst>();

  // Acepta tanto 'id' como 'key' como identificador
  String key = pObj["id"] | String("");
  if (key.isEmpty()) key = pObj["key"] | String("");
  if (key.isEmpty()) {
    if (error) *error = "invalid_palette_id";
    return false;
  }

  if (PaletteRegistry::findByKey(key) != nullptr) {
    if (error) *error = "readonly_system_palette";
    return false;
  }

  const int idx = findIndexByKey(key);
  if (idx < 0) {
    if (error) *error = "palette_not_found";
    return false;
  }

  // Compact array
  for (int i = idx; i < static_cast<int>(userCount_) - 1; ++i) {
    userPalettes_[i] = userPalettes_[i + 1];
  }
  userCount_--;
  requestSave();

  if (response) {
    JsonDocument resp;
    resp["deleted"] = true;
    resp["id"] = key;
    serializeJson(resp, *response);
  }
  return true;
}

// ---------- resolución de colores ----------

bool UserPaletteService::resolveColors(const String &value,
                                        uint32_t (&colorsOut)[3]) const {
  if (value.isEmpty()) return false;

  // Intentar como clave de sistema
  const ColorPaletteDescriptor *sys = PaletteRegistry::findByKey(value);
  if (sys != nullptr) {
    colorsOut[0] = sys->color1;
    colorsOut[1] = sys->color2;
    colorsOut[2] = sys->color3;
    return true;
  }

  // Intentar como id numérico
  bool isNumeric = true;
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (!(isDigit(c) || (i == 0 && c == '-'))) {
      isNumeric = false;
      break;
    }
  }
  if (isNumeric) {
    const int16_t id = static_cast<int16_t>(value.toInt());
    return resolveColorsById(id, colorsOut);
  }

  // Intentar como clave de usuario
  const UserPalette *up = findByKey(value);
  if (up != nullptr) {
    colorsOut[0] = up->color1;
    colorsOut[1] = up->color2;
    colorsOut[2] = up->color3;
    return true;
  }
  return false;
}

bool UserPaletteService::resolveColorsById(int16_t encodedId,
                                            uint32_t (&colorsOut)[3]) const {
  if (encodedId >= 0) {
    // Paleta del sistema
    const ColorPaletteDescriptor *sys = PaletteRegistry::findById(encodedId);
    if (sys == nullptr) return false;
    colorsOut[0] = sys->color1;
    colorsOut[1] = sys->color2;
    colorsOut[2] = sys->color3;
    return true;
  }
  if (isUserEncodedId(encodedId)) {
    const uint8_t idx = decodeUserIndex(encodedId);
    if (idx >= userCount_) return false;
    colorsOut[0] = userPalettes_[idx].color1;
    colorsOut[1] = userPalettes_[idx].color2;
    colorsOut[2] = userPalettes_[idx].color3;
    return true;
  }
  return false;
}

// ---------- persistencia ----------

bool UserPaletteService::load() {
  String raw;
  if (!readFile(kUserPalettesPath, raw)) {
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, raw)) {
    Serial.println("[palettes] invalid user-palettes.json");
    return false;
  }

  JsonArrayConst arr = doc["palettes"].as<JsonArrayConst>();
  userCount_ = 0;
  for (JsonObjectConst item : arr) {
    if (userCount_ >= kMaxUserPalettes) break;

    UserPalette p;
    p.id = item["id"].isNull() ? "" : item["id"].as<String>();
    if (!isValidKey(p.id)) continue;

    p.label = item["label"].isNull() ? p.id : item["label"].as<String>();
    p.style = item["style"].isNull() ? "custom" : item["style"].as<String>();
    p.description = item["description"].isNull() ? "" : item["description"].as<String>();

    JsonArrayConst colors = item["primaryColors"].as<JsonArrayConst>();
    if (!colors.isNull() && colors.size() >= 3) {
      p.color1 = parseHexColor(colors[0].as<String>());
      p.color2 = parseHexColor(colors[1].as<String>());
      p.color3 = parseHexColor(colors[2].as<String>());
    }
    userPalettes_[userCount_++] = p;
  }

  Serial.printf("[palettes] loaded %u user palettes\n", userCount_);
  return true;
}

bool UserPaletteService::save() const {
  JsonDocument doc;
  JsonArray arr = doc["palettes"].to<JsonArray>();
  for (uint8_t i = 0; i < userCount_; ++i) {
    JsonObject obj = arr.add<JsonObject>();
    serializeUserPalette(obj, userPalettes_[i], i);
  }

  String out;
  serializeJson(doc, out);
  return writeFile(kUserPalettesPath, out);
}

// ---------- búsqueda ----------

const UserPalette *UserPaletteService::findByKey(const String &key) const {
  for (uint8_t i = 0; i < userCount_; ++i) {
    if (userPalettes_[i].id == key) return &userPalettes_[i];
  }
  return nullptr;
}

UserPalette *UserPaletteService::findByKeyMutable(const String &key) {
  for (uint8_t i = 0; i < userCount_; ++i) {
    if (userPalettes_[i].id == key) return &userPalettes_[i];
  }
  return nullptr;
}

int UserPaletteService::findIndexByKey(const String &key) const {
  for (uint8_t i = 0; i < userCount_; ++i) {
    if (userPalettes_[i].id == key) return static_cast<int>(i);
  }
  return -1;
}

// ---------- serialización ----------

void UserPaletteService::appendSystemJson(JsonArray arr) const {
  const ColorPaletteDescriptor *sys = PaletteRegistry::all();
  const size_t n = PaletteRegistry::count();
  for (size_t i = 0; i < n; ++i) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = sys[i].id;
    obj["key"] = sys[i].key;
    obj["label"] = sys[i].label;
    obj["style"] = sys[i].style;
    obj["description"] = sys[i].description;
    obj["source"] = "system";
    obj["readOnly"] = true;
    JsonArray colors = obj["primaryColors"].to<JsonArray>();
    colors.add(colorToHex(sys[i].color1));
    colors.add(colorToHex(sys[i].color2));
    colors.add(colorToHex(sys[i].color3));
  }
}

void UserPaletteService::appendUserJson(JsonArray arr) const {
  for (uint8_t i = 0; i < userCount_; ++i) {
    JsonObject obj = arr.add<JsonObject>();
    serializeUserPalette(obj, userPalettes_[i], i);
  }
}

void UserPaletteService::serializeUserPalette(JsonObject obj,
                                               const UserPalette &p,
                                               uint8_t index) const {
  obj["id"] = encodeUserIndex(index);
  obj["key"] = p.id;
  obj["label"] = p.label;
  obj["style"] = p.style;
  obj["description"] = p.description;
  obj["source"] = "user";
  obj["readOnly"] = false;
  JsonArray colors = obj["primaryColors"].to<JsonArray>();
  colors.add(colorToHex(p.color1));
  colors.add(colorToHex(p.color2));
  colors.add(colorToHex(p.color3));
}

// ---------- utilidades ----------

bool UserPaletteService::isValidKey(const String &id) {
  if (id.isEmpty() || id.length() > 40) return false;
  for (size_t i = 0; i < id.length(); ++i) {
    const char c = id[i];
    if (!(isLowerCase(c) || isDigit(c) || c == '_' || c == '-')) return false;
  }
  return true;
}

uint32_t UserPaletteService::parseHexColor(const String &hex, uint32_t fallback) {
  String h = hex;
  if (h.startsWith("#")) h = h.substring(1);
  if (h.length() != 6) return fallback;
  return static_cast<uint32_t>(strtoul(h.c_str(), nullptr, 16)) & 0xFFFFFFUL;
}

String UserPaletteService::colorToHex(uint32_t color) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%06lX",
           static_cast<unsigned long>(color & 0xFFFFFFUL));
  return String(buf);
}

bool UserPaletteService::readFile(const char *path, String &content) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  content = f.readString();
  f.close();
  return !content.isEmpty();
}

bool UserPaletteService::writeFile(const char *path, const String &content) {
  String tmp = String(path) + ".tmp";
  File f = LittleFS.open(tmp.c_str(), "w");
  if (!f) return false;
  f.print(content);
  f.close();
  LittleFS.remove(path);
  return LittleFS.rename(tmp.c_str(), path);
}
