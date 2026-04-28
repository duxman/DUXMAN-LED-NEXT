/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/UserPaletteService.h
 * Last commit: ec3d96f - 2026-04-28
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "services/PersistenceSchedulerService.h"

constexpr uint8_t kMaxUserPalettes = 20;

struct UserPalette {
  String id;          // clave slug única [a-z0-9_-]
  String label;       // nombre legible
  String style;       // warm|cold|neon|pastel|high-contrast|party|custom
  String description;
  uint32_t color1 = 0xFFFFFF;
  uint32_t color2 = 0xFFFFFF;
  uint32_t color3 = 0xFFFFFF;
};

class UserPaletteService {
public:
  explicit UserPaletteService(PersistenceSchedulerService &persistenceSchedulerService);

  void begin();
  void processPendingPersistence();

  // Listado completo (sistema + usuario) como JSON.
  // Las de sistema llevan "source":"system" y "readOnly":true.
  // Las de usuario llevan "source":"user" y "readOnly":false.
  String listAllJson() const;

  // Solo paletas de usuario.
  String listUserJson() const;

  bool saveFromJson(const String &payload, String *response = nullptr, String *error = nullptr);
  bool deleteFromJson(const String &payload, String *response = nullptr, String *error = nullptr);

  // Resuelve una paleta por id (>= 0 sistema, < 0 usuario id codificado) o clave.
  // Devuelve true si se encontró y rellena colorsOut[3].
  bool resolveColors(const String &value, uint32_t (&colorsOut)[3]) const;
  bool resolveColorsById(int16_t encodedId, uint32_t (&colorsOut)[3]) const;

  // Id codificado para paletas de usuario: offset negativo desde -100.
  // kSystemOffset = 0+ (ids del PaletteRegistry), kUserOffset empieza en -100.
  static constexpr int16_t kUserIdBase = -100;
  static int16_t encodeUserIndex(uint8_t index) {
    return static_cast<int16_t>(kUserIdBase - static_cast<int16_t>(index));
  }
  static bool isUserEncodedId(int16_t id) { return id <= kUserIdBase; }
  static uint8_t decodeUserIndex(int16_t id) {
    return static_cast<uint8_t>(kUserIdBase - id);
  }

  uint8_t userCount() const { return userCount_; }
  const UserPalette &userAt(uint8_t index) const { return userPalettes_[index]; }

private:
  PersistenceSchedulerService &persistenceSchedulerService_;
  UserPalette userPalettes_[kMaxUserPalettes];
  uint8_t userCount_ = 0;
  bool pendingPersistence_ = false;

  void requestSave();
  bool load();
  bool save() const;

  const UserPalette *findByKey(const String &key) const;
  UserPalette *findByKeyMutable(const String &key);
  int findIndexByKey(const String &key) const;

  void appendSystemJson(JsonArray arr) const;
  void appendUserJson(JsonArray arr) const;
  void serializeUserPalette(JsonObject obj, const UserPalette &p, uint8_t index) const;

  static bool isValidKey(const String &id);
  static uint32_t parseHexColor(const String &hex, uint32_t fallback = 0xFFFFFF);
  static String colorToHex(uint32_t color);
  static bool readFile(const char *path, String &content);
  static bool writeFile(const char *path, const String &content);
};
