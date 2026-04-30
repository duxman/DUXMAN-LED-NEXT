/**
 * Lightweight Internationalization (i18n) Engine for Embedded Web UI
 * Supports runtime language switching with LocalStorage persistence
 * 
 * Usage:
 *   i18n.load('es')                    // Load Spanish
 *   i18n.t('button.save')              // Get translated string
 *   i18n.setFormatter('custom', fn)    // Add custom formatter
 *   i18n.on('change', callback)        // Listen for language changes
 */

class I18nEngine {
  constructor() {
    this.currentLang = 'en';
    this.strings = {};
    this.fallbackLang = 'en';
    this.listeners = [];
    this.formatters = {
      number: (val) => parseInt(val).toLocaleString(),
      percent: (val) => val + '%',
      date: (val) => new Date(val).toLocaleDateString(),
    };
    this.loadStoredLanguage();
  }

  /**
   * Load language pack from JSON file
   */
  async load(langCode) {
    try {
      const response = await fetch(`/ui/i18n/${langCode}.json`);
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      
      const data = await response.json();
      this.strings = data;
      this.currentLang = langCode || data.lang || 'en';
      
      // Store language preference
      localStorage.setItem('i18n_language', this.currentLang);
      
      // Notify listeners
      this.notifyListeners(this.currentLang);
      
      // Translate all elements with data-i18n attribute
      this.translatePage();
      
      console.log(`[i18n] Loaded language: ${this.currentLang}`);
      return true;
    } catch (error) {
      console.error(`[i18n] Failed to load ${langCode}:`, error);
      if (langCode !== this.fallbackLang) {
        return this.load(this.fallbackLang);
      }
      return false;
    }
  }

  /**
   * Translate a single key with support for nested paths
   * Example: t('ui.button.save') -> 'Save'
   */
  t(key, options = {}) {
    if (!key) return '';
    
    const keys = key.split('.');
    let value = this.strings;
    
    // Navigate through nested structure
    for (const k of keys) {
      if (value && typeof value === 'object' && k in value) {
        value = value[k];
      } else {
        // Return key itself if not found (signal of missing translation)
        return options.default || key;
      }
    }
    
    // Apply formatters if specified
    if (options.format && typeof value === 'string') {
      const formatter = this.formatters[options.format];
      if (formatter) {
        return formatter(value);
      }
    }
    
    return String(value);
  }

  /**
   * Get all strings for debugging
   */
  getAll() {
    return { lang: this.currentLang, strings: this.strings };
  }

  /**
   * Register custom formatter
   */
  setFormatter(name, fn) {
    this.formatters[name] = fn;
  }

  /**
   * Translate all DOM elements with data-i18n attribute
   */
  translatePage() {
    const elements = document.querySelectorAll('[data-i18n]');
    
    elements.forEach(el => {
      const key = el.getAttribute('data-i18n');
      const translated = this.t(key);
      
      if (el.tagName === 'INPUT' || el.tagName === 'BUTTON' || el.tagName === 'TEXTAREA') {
        // For input elements, use placeholder or value
        if (el.hasAttribute('data-i18n-placeholder')) {
          el.placeholder = translated;
        } else {
          el.value = translated;
        }
      } else if (el.tagName === 'LABEL' || el.tagName === 'SPAN' || el.tagName === 'DIV') {
        // For label/div, set textContent
        el.textContent = translated;
      } else {
        // Generic approach
        el.textContent = translated;
      }
      
      // Handle title attribute
      if (el.hasAttribute('data-i18n-title')) {
        el.title = this.t(el.getAttribute('data-i18n-title'));
      }
    });
  }

  /**
   * Get current language
   */
  getCurrentLanguage() {
    return this.currentLang;
  }

  /**
   * Get language name
   */
  getLanguageName(lang = this.currentLang) {
    const names = {
      en: 'English',
      es: 'Español',
      fr: 'Français',
      de: 'Deutsch',
      it: 'Italiano'
    };
    return names[lang] || lang;
  }

  /**
   * List available languages
   */
  getAvailableLanguages() {
    return [
      { code: 'en', name: 'English' },
      { code: 'es', name: 'Español' },
      { code: 'fr', name: 'Français' },
      { code: 'de', name: 'Deutsch' },
      { code: 'it', name: 'Italiano' }
    ];
  }

  /**
   * Load language from localStorage or browser preference
   */
  loadStoredLanguage() {
    const stored = localStorage.getItem('i18n_language');
    if (stored) {
      this.currentLang = stored;
    } else {
      // Try browser language
      const browserLang = navigator.language?.split('-')[0] || 'en';
      this.currentLang = ['en', 'es', 'fr', 'de', 'it'].includes(browserLang) 
        ? browserLang 
        : 'en';
    }
  }

  /**
   * Listen for language changes
   */
  onChange(callback) {
    this.listeners.push(callback);
  }

  /**
   * Notify all listeners of language change
   */
  notifyListeners(lang) {
    this.listeners.forEach(callback => {
      try {
        callback(lang);
      } catch (e) {
        console.error('[i18n] Listener error:', e);
      }
    });
  }

  /**
   * Format number according to locale
   */
  formatNumber(num, options = {}) {
    return num.toLocaleString(this.getLookupLocale());
  }

  /**
   * Format percentage
   */
  formatPercent(num) {
    return `${num}%`;
  }

  /**
   * Format bytes to human readable
   */
  formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return (bytes / Math.pow(k, i)).toFixed(2) + ' ' + sizes[i];
  }

  /**
   * Format time
   */
  formatTime(seconds) {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
  }

  /**
   * Get locale code for toLocaleString() calls
   */
  getLookupLocale() {
    const locales = {
      en: 'en-US',
      es: 'es-ES',
      fr: 'fr-FR',
      de: 'de-DE',
      it: 'it-IT'
    };
    return locales[this.currentLang] || 'en-US';
  }

  /**
   * Check if a key exists in current language
   */
  has(key) {
    const keys = key.split('.');
    let value = this.strings;
    for (const k of keys) {
      if (value && typeof value === 'object' && k in value) {
        value = value[k];
      } else {
        return false;
      }
    }
    return true;
  }
}

// Global instance
const i18n = new I18nEngine();

// Auto-initialize on page load
document.addEventListener('DOMContentLoaded', () => {
  const localLang = localStorage.getItem('i18n_language');
  if (localLang) {
    // LocalStorage has a preference — use it directly
    i18n.load(localLang).catch(err => console.error('[i18n] Init failed:', err));
  } else {
    // No local preference — sync from server config
    fetch('/api/v1/config/general')
      .then(r => r.ok ? r.json() : Promise.reject(r.status))
      .then(data => {
        const lang = data.general?.language || navigator.language?.split('-')[0] || 'en';
        return i18n.load(lang);
      })
      .catch(() => i18n.load('en'));
  }
});
