#ifndef LANGUAGE_MANAGER_H
#define LANGUAGE_MANAGER_H

#include <Arduino.h>
#include <unordered_map>

/**
 * @file LanguageManager.h
 * @brief Central internationalization (i18n) service for firmware
 * 
 * Manages language packs and translation strings. Supports:
 * - Multiple language packs (EN, ES, FR, DE, IT)
 * - Nested JSON translation keys
 * - Fallback to English if key not found
 * - Dynamic language switching
 * - LittleFS and PROGMEM storage
 */

class LanguageManager {
public:
  static constexpr const char* DEFAULT_LANGUAGE = "en";
  static constexpr const char* FALLBACK_LANGUAGE = "en";
  
  enum class Language {
    ENGLISH = 0,
    SPANISH = 1,
    FRENCH = 2,
    GERMAN = 3,
    ITALIAN = 4,
    UNKNOWN = 255
  };
  
  LanguageManager();
  ~LanguageManager() = default;
  
  // Initialize with default language
  void begin(const String& language = DEFAULT_LANGUAGE);
  
  // Set current language
  void setLanguage(const String& languageCode);
  String getCurrentLanguage() const { return currentLanguage_; }
  Language getLanguageEnum(const String& code) const;
  
  // Get translation string
  // Key format: "section.key" or "section.subsection.key"
  // Returns key itself if not found
  String t(const char* key) const;
  String t(const String& key) const { return t(key.c_str()); }
  
  // Get localized effect/palette name
  String getEffectName(uint8_t effectId) const;
  String getPaletteName(uint8_t paletteId) const;
  
  // Format functions
  String formatNumber(int32_t value, bool includeSign = false) const;
  String formatPercent(uint8_t percent) const;
  String formatTime(uint32_t seconds) const;  // "HH:MM:SS"
  String formatFileSize(uint32_t bytes) const;  // "1.2 KB"
  
  // Get all strings for current language (for UI)
  String getAllStringsJson() const;
  
  // ISO language/region codes
  String getLanguageName(const String& code) const;
  String getLanguageCode(Language lang) const;
  
private:
  String currentLanguage_;
  Language currentLanguageEnum_;
  
  // Load language pack from PROGMEM (embedded in firmware)
  bool loadEmbeddedPack_(const String& languageCode);
  
  // Load language pack from LittleFS (optional override)
  bool loadFromLittleFS_(const String& languageCode);
  
  // Parse JSON and extract string
  String getFromJson_(const String& jsonData, const String& key) const;
};

extern LanguageManager gLanguageManager;

#endif // LANGUAGE_MANAGER_H
