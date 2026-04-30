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
    this.catalogs = {};
    this.reverseLiteralIndex = {};
    this.baseCatalogLoaded = false;
    this.fallbackLang = 'en';
    this.listeners = [];
    this.literalPairs = [
      { es: 'Sin llamadas aun.', en: 'No calls yet.' },
      { es: 'Sin datos aun.', en: 'No data yet.' },
      { es: 'Respuesta', en: 'Response' },
      { es: 'Payload', en: 'Payload' },
      { es: 'Guardar', en: 'Save' },
      { es: 'Recargar', en: 'Reload' },
      { es: 'Guardar cambios', en: 'Save changes' },
      { es: 'Cancelar', en: 'Cancel' },
      { es: 'Configuracion', en: 'Configuration' },
      { es: 'Configuracion General', en: 'General Settings' },
      { es: 'Region / Locale', en: 'Region / Locale' },
      { es: 'Idioma de la interfaz', en: 'Interface language' },
      { es: 'El idioma se aplica inmediatamente al guardar.', en: 'Language is applied immediately after saving.' },
      { es: 'Preferencias UI', en: 'UI Preferences' },
      { es: 'Mostrar respuestas JSON en las paginas de configuracion', en: 'Show JSON responses in configuration pages' },
      { es: 'Se guarda en el navegador (localStorage). Por defecto desactivado.', en: 'Stored in the browser (localStorage). Disabled by default.' },
      { es: 'Debug habilitado', en: 'Debug enabled' },
      { es: 'Home', en: 'Home' },
      { es: 'Config', en: 'Config' },
      { es: 'Network', en: 'Network' },
      { es: 'Microfono', en: 'Microphone' },
      { es: 'Paletas', en: 'Palettes' },
      { es: 'General', en: 'General' },
      { es: 'Manual JSON', en: 'Manual JSON' },
      { es: 'Sobre', en: 'About' },
      { es: 'Ayuda', en: 'Help' },
      { es: 'Version', en: 'Version' },
      { es: 'Hardware', en: 'Hardware' },
      { es: 'Seleccion de colores', en: 'Color selection' },
      { es: 'Define el color de fondo y los tres colores de primer plano del efecto.', en: 'Define the background color and the three foreground colors of the effect.' },
      { es: 'Fondo / off', en: 'Background / off' },
      { es: 'Color 1', en: 'Color 1' },
      { es: 'Color 2', en: 'Color 2' },
      { es: 'Color 3', en: 'Color 3' },
      { es: 'Paleta predefinida', en: 'Preset palette' },
      { es: 'Manual (colores libres)', en: 'Manual (free colors)' },
      { es: 'Vista previa de paleta', en: 'Palette preview' },
      { es: 'Aplicar paleta', en: 'Apply palette' },
      { es: 'Aplicar y guardar', en: 'Apply and save' },
      { es: 'Seleccion de efecto', en: 'Effect selection' },
      { es: 'Seleccion y acciones', en: 'Selection and actions' },
      { es: 'Efecto', en: 'Effect' },
      { es: 'Aplicar', en: 'Apply' },
      { es: 'Guardar arranque', en: 'Save startup' },
      { es: 'Parametros y presets', en: 'Parameters and presets' },
      { es: 'Numero de secciones', en: 'Number of sections' },
      { es: 'Rapidez del efecto', en: 'Effect speed' },
      { es: 'Solo se usa en efectos animados.', en: 'Only used in animated effects.' },
      { es: 'Nivel del efecto', en: 'Effect level' },
      { es: 'Brillo global', en: 'Global brightness' },
      { es: 'No hay efectos guardados en la secuencia.', en: 'No saved effects in sequence.' }
    ];
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
      await this.preloadBaseCatalogs();
      const response = await fetch(`/ui/i18n/${langCode}.json`);
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      
      const data = await response.json();
      this.strings = data;
      this.currentLang = langCode || data.lang || 'en';
      this.catalogs[this.currentLang] = data;
      this.buildReverseIndex();
      
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

  async preloadBaseCatalogs() {
    if (this.baseCatalogLoaded) return;
    const baseLangs = ['en', 'es'];
    await Promise.all(baseLangs.map(async (lang) => {
      try {
        const res = await fetch(`/ui/i18n/${lang}.json`);
        if (!res.ok) return;
        this.catalogs[lang] = await res.json();
      } catch (_) {
        // Ignore preload errors; normal load fallback still applies.
      }
    }));
    this.baseCatalogLoaded = true;
    this.buildReverseIndex();
  }

  buildReverseIndex() {
    const index = {};
    const add = (text, key) => {
      if (typeof text !== 'string') return;
      const normalized = this.normalizeText(text);
      if (!normalized) return;
      if (!index[normalized]) index[normalized] = key;
    };

    const walk = (obj, path = '') => {
      if (!obj || typeof obj !== 'object') return;
      Object.keys(obj).forEach((k) => {
        const value = obj[k];
        const nextPath = path ? `${path}.${k}` : k;
        if (typeof value === 'string') add(value, nextPath);
        else if (value && typeof value === 'object') walk(value, nextPath);
      });
    };

    Object.keys(this.catalogs).forEach((lang) => walk(this.catalogs[lang]));
    this.reverseLiteralIndex = index;
  }

  normalizeText(text) {
    return String(text || '')
      .replace(/\s+/g, ' ')
      .trim()
      .toLowerCase();
  }

  translateByLiteralPair(rawText) {
    const normalized = this.normalizeText(rawText);
    if (!normalized) return null;
    const targetField = this.currentLang === 'es' ? 'es' : 'en';

    for (const pair of this.literalPairs) {
      if (this.normalizeText(pair.es) === normalized || this.normalizeText(pair.en) === normalized) {
        return pair[targetField];
      }
    }
    return null;
  }

  translateLiteral(rawText) {
    const fromPair = this.translateByLiteralPair(rawText);
    if (fromPair) return fromPair;

    const key = this.reverseLiteralIndex[this.normalizeText(rawText)];
    if (!key) return rawText;
    return this.t(key, { default: rawText });
  }

  replaceKeepingWhitespace(original, translatedCore) {
    const match = String(original).match(/^(\s*)([\s\S]*?)(\s*)$/);
    if (!match) return translatedCore;
    return `${match[1]}${translatedCore}${match[3]}`;
  }

  autoTranslateStaticText() {
    if (!document.body) return;

    const blockedParents = new Set(['SCRIPT', 'STYLE', 'NOSCRIPT', 'CODE', 'PRE', 'TEXTAREA']);
    const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, null);
    const nodes = [];
    while (walker.nextNode()) nodes.push(walker.currentNode);

    nodes.forEach((node) => {
      const parent = node.parentElement;
      if (!parent) return;
      if (blockedParents.has(parent.tagName)) return;
      if (parent.closest('[data-i18n]')) return;

      const raw = node.nodeValue;
      const trimmed = String(raw || '').trim();
      if (!trimmed || trimmed.length < 2) return;

      const translated = this.translateLiteral(trimmed);
      if (translated && translated !== trimmed) {
        node.nodeValue = this.replaceKeepingWhitespace(raw, translated);
      }
    });

    const attrs = document.querySelectorAll('input[placeholder], textarea[placeholder], [title]');
    attrs.forEach((el) => {
      if (el.hasAttribute('placeholder')) {
        const translated = this.translateLiteral(el.getAttribute('placeholder'));
        if (translated) el.setAttribute('placeholder', translated);
      }
      if (el.hasAttribute('title')) {
        const translated = this.translateLiteral(el.getAttribute('title'));
        if (translated) el.setAttribute('title', translated);
      }
    });
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

    // Best-effort global pass: translates static texts that are not yet tagged with data-i18n.
    this.autoTranslateStaticText();
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
