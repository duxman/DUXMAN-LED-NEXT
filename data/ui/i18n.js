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
    this.fallbackLang = 'en';
    this.strings = {};
    this.listeners = [];
    this.availableLangs = ['en', 'es'];
  }

  async init() {
    const localLang = localStorage.getItem('i18n_language');
    const browserLang = navigator.language?.split('-')[0] || 'en';

    try {
      const response = await fetch('/api/v1/config/general');
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      const serverLang = data.general?.language;
      const startLang = this.normalizeLanguage(serverLang || localLang || browserLang || this.fallbackLang);
      await this.load(startLang);
    } catch (_) {
      await this.load(this.normalizeLanguage(localLang || browserLang || this.fallbackLang));
    }
  }

  normalizeLanguage(langCode) {
    return this.availableLangs.includes(langCode) ? langCode : this.fallbackLang;
  }

  async load(langCode) {
    const normalized = this.normalizeLanguage(langCode);
    try {
      const response = await fetch(`/ui/i18n/${normalized}.json`);
      if (!response.ok) throw new Error(`HTTP ${response.status}`);

      this.strings = await response.json();
      this.currentLang = normalized;
      localStorage.setItem('i18n_language', normalized);
      this.translatePage();
      this.notifyListeners(normalized);
      return true;
    } catch (error) {
      console.error('[i18n] Failed to load language:', normalized, error);
      if (normalized !== this.fallbackLang) {
        return this.load(this.fallbackLang);
      }
      return false;
    }
  }

  resolve(key) {
    if (!key) return undefined;
    const path = String(key).split('.');
    let value = this.strings;
    for (const segment of path) {
      if (!value || typeof value !== 'object' || !(segment in value)) {
        return undefined;
      }
      value = value[segment];
    }
    return value;
  }

  interpolate(value, params) {
    if (!params || typeof value !== 'string') return value;
    return value.replace(/\{([a-zA-Z0-9_]+)\}/g, (match, token) => {
      return Object.prototype.hasOwnProperty.call(params, token) ? String(params[token]) : match;
    });
  }

  t(key, options = {}) {
    const value = this.resolve(key);
    if (typeof value === 'undefined') {
      return options.default || key;
    }
    if (typeof value !== 'string') {
      return String(value);
    }
    return this.interpolate(value, options.params);
  }

  translateAttribute(element, attributeName, keyAttributeName) {
    if (!element.hasAttribute(keyAttributeName)) return;
    const key = element.getAttribute(keyAttributeName);
    element.setAttribute(attributeName, this.t(key));
  }

  translateText(element) {
    if (!element.hasAttribute('data-i18n')) return;
    const key = element.getAttribute('data-i18n');
    element.textContent = this.t(key);
  }

  translateHtml(element) {
    if (!element.hasAttribute('data-i18n-html')) return;
    const key = element.getAttribute('data-i18n-html');
    element.innerHTML = this.t(key);
  }

  translateValue(element) {
    if (!element.hasAttribute('data-i18n-value')) return;
    const key = element.getAttribute('data-i18n-value');
    element.value = this.t(key);
  }

  translatePage(root = document) {
    if (!root) return;
    const elements = root.querySelectorAll('[data-i18n], [data-i18n-html], [data-i18n-placeholder], [data-i18n-title], [data-i18n-value], [data-i18n-aria-label]');
    elements.forEach((element) => {
      this.translateText(element);
      this.translateHtml(element);
      this.translateValue(element);
      this.translateAttribute(element, 'placeholder', 'data-i18n-placeholder');
      this.translateAttribute(element, 'title', 'data-i18n-title');
      this.translateAttribute(element, 'aria-label', 'data-i18n-aria-label');
    });
    document.documentElement.lang = this.currentLang;
  }

  /**
   * Get current language
   */
  getCurrentLanguage() {
    return this.currentLang;
  }

  onChange(callback) {
    this.listeners.push(callback);
  }

  notifyListeners(lang) {
    this.listeners.forEach(callback => {
      try {
        callback(lang);
      } catch (e) {
        console.error('[i18n] Listener error:', e);
      }
    });
  }

  has(key) {
    return typeof this.resolve(key) !== 'undefined';
  }

  formatNumber(value) {
    const locale = this.currentLang === 'es' ? 'es-ES' : 'en-US';
    return Number(value).toLocaleString(locale);
  }
}

const i18n = new I18nEngine();
window.i18n = i18n;
window.duxT = function(key, params, fallback) {
  return i18n.t(key, { params: params || undefined, default: fallback });
};

document.addEventListener('DOMContentLoaded', () => {
  i18n.init().catch((error) => {
    console.error('[i18n] Initialization failed:', error);
  });
});
