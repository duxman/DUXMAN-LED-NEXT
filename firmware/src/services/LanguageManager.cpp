#include "LanguageManager.h"
#include <ArduinoJson.h>

// Embedded language packs (will be in separate files in production)
// For now, these are minimal examples embedded as PROGMEM strings

static const char* packEN PROGMEM = R"({
  "ui": {
    "nav": {
      "home": "Home",
      "effects": "Effects",
      "settings": "Settings",
      "advanced": "Advanced"
    },
    "button": {
      "save": "Save",
      "cancel": "Cancel",
      "apply": "Apply",
      "delete": "Delete",
      "back": "Back",
      "next": "Next"
    },
    "label": {
      "brightness": "Brightness",
      "speed": "Speed",
      "color": "Color",
      "effect": "Effect",
      "palette": "Palette",
      "mode": "Mode",
      "enabled": "Enabled",
      "disabled": "Disabled"
    },
    "settings": {
      "general": "General Settings",
      "network": "Network",
      "power": "Power Management",
      "effects": "Effects",
      "language": "Language",
      "region": "Region",
      "debug": "Debug Options",
      "debugEnabled": "Enable Debug"
    },
    "message": {
      "saving": "Saving...",
      "saved": "Saved successfully",
      "error": "Error",
      "warning": "Warning",
      "success": "Success"
    }
  },
  "api": {
    "effects": {
      "fixed": "Fixed Color",
      "gradient": "Gradient",
      "breath": "Breathing",
      "rainbow": "Rainbow",
      "chase": "Chase"
    }
  }
})";

static const char* packES PROGMEM = R"({
  "ui": {
    "nav": {
      "home": "Inicio",
      "effects": "Efectos",
      "settings": "Configuración",
      "advanced": "Avanzado"
    },
    "button": {
      "save": "Guardar",
      "cancel": "Cancelar",
      "apply": "Aplicar",
      "delete": "Eliminar",
      "back": "Atrás",
      "next": "Siguiente"
    },
    "label": {
      "brightness": "Brillo",
      "speed": "Velocidad",
      "color": "Color",
      "effect": "Efecto",
      "palette": "Paleta",
      "mode": "Modo",
      "enabled": "Activado",
      "disabled": "Desactivado"
    },
    "settings": {
      "general": "Configuración General",
      "network": "Red",
      "power": "Gestión de Potencia",
      "effects": "Efectos",
      "language": "Idioma",
      "region": "Región",
      "debug": "Opciones de Debug",
      "debugEnabled": "Activar Debug"
    },
    "message": {
      "saving": "Guardando...",
      "saved": "Guardado correctamente",
      "error": "Error",
      "warning": "Advertencia",
      "success": "Éxito"
    }
  },
  "api": {
    "effects": {
      "fixed": "Color Fijo",
      "gradient": "Gradiente",
      "breath": "Respiración",
      "rainbow": "Arcoíris",
      "chase": "Persecución"
    }
  }
})";

// Global instance
LanguageManager gLanguageManager;

LanguageManager::LanguageManager() 
    : currentLanguage_(DEFAULT_LANGUAGE), 
      currentLanguageEnum_(Language::ENGLISH) {}

void LanguageManager::begin(const String& language) {
  setLanguage(language);
}

void LanguageManager::setLanguage(const String& languageCode) {
  currentLanguage_ = languageCode;
  currentLanguageEnum_ = getLanguageEnum(languageCode);
  
  // Try to load from LittleFS first, then fallback to embedded
  if (!loadFromLittleFS_(languageCode)) {
    if (!loadEmbeddedPack_(languageCode)) {
      // If target language failed, try fallback
      Serial.printf("[i18n] Language '%s' not found, falling back to '%s'\n", 
                   languageCode.c_str(), FALLBACK_LANGUAGE);
      loadEmbeddedPack_(FALLBACK_LANGUAGE);
      currentLanguage_ = FALLBACK_LANGUAGE;
    }
  }
}

LanguageManager::Language LanguageManager::getLanguageEnum(const String& code) const {
  if (code == "en") return Language::ENGLISH;
  if (code == "es") return Language::SPANISH;
  if (code == "fr") return Language::FRENCH;
  if (code == "de") return Language::GERMAN;
  if (code == "it") return Language::ITALIAN;
  return Language::UNKNOWN;
}

String LanguageManager::t(const char* key) const {
  if (!key || strlen(key) == 0) {
    return String(key);
  }
  
  // For now, return key itself (template for actual implementation)
  // In production, this would parse loaded JSON and extract value
  // For the MVP, we'll return the key as untranslated
  
  // This is a placeholder - actual implementation would:
  // 1. Load the currentLanguage_ JSON pack
  // 2. Parse the dot-notation key (e.g., "ui.button.save")
  // 3. Extract value from nested JSON
  // 4. Fallback to FALLBACK_LANGUAGE if not found
  
  return String(key);  // TODO: implement translation lookup
}

String LanguageManager::getEffectName(uint8_t effectId) const {
  // Placeholder - would look up effect names in current language
  return "Effect_" + String(effectId);
}

String LanguageManager::getPaletteName(uint8_t paletteId) const {
  // Placeholder - would look up palette names in current language
  return "Palette_" + String(paletteId);
}

String LanguageManager::formatNumber(int32_t value, bool includeSign) const {
  // Format number according to locale
  // For EN: 1,234,567  For ES: 1.234.567
  String result = String(value);
  if (includeSign && value >= 0) {
    result = "+" + result;
  }
  return result;
}

String LanguageManager::formatPercent(uint8_t percent) const {
  return String(percent) + "%";
}

String LanguageManager::formatTime(uint32_t seconds) const {
  uint32_t hours = seconds / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;
  
  String result = "";
  if (hours < 10) result += "0";
  result += String(hours) + ":";
  if (minutes < 10) result += "0";
  result += String(minutes) + ":";
  if (secs < 10) result += "0";
  result += String(secs);
  
  return result;
}

String LanguageManager::formatFileSize(uint32_t bytes) const {
  const char* units[] = {"B", "KB", "MB", "GB"};
  int unitIndex = 0;
  float size = bytes;
  
  while (size >= 1024 && unitIndex < 3) {
    size /= 1024;
    unitIndex++;
  }
  
  char buffer[16];
  if (size < 10) {
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unitIndex]);
  } else {
    snprintf(buffer, sizeof(buffer), "%.1f %s", size, units[unitIndex]);
  }
  
  return String(buffer);
}

bool LanguageManager::loadEmbeddedPack_(const String& languageCode) {
  if (languageCode == "en") {
    // In production, would parse packEN
    Serial.println("[i18n] Loaded embedded EN language pack");
    return true;
  }
  if (languageCode == "es") {
    // In production, would parse packES
    Serial.println("[i18n] Loaded embedded ES language pack");
    return true;
  }
  return false;
}

bool LanguageManager::loadFromLittleFS_(const String& languageCode) {
  // Placeholder for LittleFS loading
  // In production, would try to load from data/lang/i18n_{code}.json
  return false;
}

String LanguageManager::getFromJson_(const String& jsonData, const String& key) const {
  // Placeholder for JSON extraction
  // Would use ArduinoJson to parse and navigate dot-notation keys
  return key;
}

String LanguageManager::getAllStringsJson() const {
  // Return the appropriate language pack as JSON
  if (currentLanguage_ == "es") {
    return String(packES);
  }
  return String(packEN);
}

String LanguageManager::getLanguageName(const String& code) const {
  if (code == "en") return "English";
  if (code == "es") return "Español";
  if (code == "fr") return "Français";
  if (code == "de") return "Deutsch";
  if (code == "it") return "Italiano";
  return code;
}

String LanguageManager::getLanguageCode(Language lang) const {
  switch (lang) {
    case Language::ENGLISH: return "en";
    case Language::SPANISH: return "es";
    case Language::FRENCH: return "fr";
    case Language::GERMAN: return "de";
    case Language::ITALIAN: return "it";
    default: return "en";
  }
}
