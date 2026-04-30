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
      { es: 'No hay efectos guardados en la secuencia.', en: 'No saved effects in sequence.' },
      { es: 'Persistencia de efectos', en: 'Effect persistence' },
      { es: 'Guarda el efecto de arranque y prepara la secuencia futura con duracion por entrada.', en: 'Save startup effect and prepare the future sequence with per-entry duration.' },
      { es: 'Persistencia cargada', en: 'Persistence loaded' },
      { es: 'Configuracion de Red', en: 'Network Configuration' },
      { es: 'Configuracion de Microfono', en: 'Microphone Configuration' },
      { es: 'Configuracion LED (GPIO)', en: 'LED Configuration (GPIO)' },
      { es: 'Configuracion Manual', en: 'Manual Configuration' },
      { es: 'Perfiles de Configuracion', en: 'Configuration Profiles' },
      { es: 'Configurar microfono generic_i2c (SD/WS/SCK) y perfil.', en: 'Configure generic_i2c microphone (SD/WS/SCK) and profile.' },
      { es: 'Crear, editar y guardar paletas de colores personalizadas.', en: 'Create, edit and save custom color palettes.' },
      { es: 'Editar, exportar e importar toda la configuracion JSON.', en: 'Edit, export and import complete JSON configuration.' },
      { es: 'Guardar, aplicar y fijar perfiles GPIO por defecto.', en: 'Save, apply and set default GPIO profiles.' },
      { es: 'Habilitar debug y ajustar intervalo de heartbeat.', en: 'Enable debug and adjust heartbeat interval.' },
      { es: 'Pin LED, cantidad, tipo de LED y orden de color.', en: 'LED pin, count, LED type and color order.' },
      { es: 'Selecciona la seccion de configuracion que quieres editar.', en: 'Select the configuration section you want to edit.' },
      { es: 'WiFi, AP/STA, hostname, IP y DNS.', en: 'WiFi, AP/STA, hostname, IP and DNS.' },
      { es: 'Cargar del dispositivo', en: 'Load from device' },
      { es: 'Solo validar', en: 'Validate only' },
      { es: 'Aplicar al dispositivo', en: 'Apply to device' },
      { es: 'Descargar .json', en: 'Download .json' },
      { es: 'Importar archivo', en: 'Import file' },
      { es: 'Cargar en editor', en: 'Load into editor' },
      { es: 'Resultado', en: 'Result' },
      { es: 'Guardar Configuracion', en: 'Save Configuration' },
      { es: 'Payload / Respuesta', en: 'Payload / Response' },
      { es: 'Payload Enviado', en: 'Sent Payload' },
      { es: 'Respuesta API', en: 'API Response' },
      { es: 'Modo', en: 'Mode' },
      { es: 'Disponibilidad AP', en: 'AP Availability' },
      { es: 'SSID STA', en: 'STA SSID' },
      { es: 'Password STA', en: 'STA Password' },
      { es: 'Modo IP AP', en: 'AP IP Mode' },
      { es: 'IP AP', en: 'AP IP' },
      { es: 'Gateway AP', en: 'AP Gateway' },
      { es: 'Subnet AP', en: 'AP Subnet' },
      { es: 'Modo IP STA', en: 'STA IP Mode' },
      { es: 'IP STA', en: 'STA IP' },
      { es: 'Gateway STA', en: 'STA Gateway' },
      { es: 'Subnet STA', en: 'STA Subnet' },
      { es: 'DNS 1 STA', en: 'STA DNS 1' },
      { es: 'DNS 2 STA', en: 'STA DNS 2' },
      { es: 'Hostname DNS', en: 'DNS Hostname' },
      { es: 'Tiempo / NTP', en: 'Time / NTP' },
      { es: 'Sincronizar en arranque', en: 'Sync on boot' },
      { es: 'Servidor NTP', en: 'NTP Server' },
      { es: 'Edita WiFi/AP/STA y guarda en el dispositivo. Los cambios se aplican al momento.', en: 'Edit WiFi/AP/STA and save to device. Changes are applied immediately.' },
      { es: 'Al arrancar y al conectar STA, el equipo intentara sincronizar la hora con el servidor NTP configurado.', en: 'On boot and STA connect, the device will try to sync time with the configured NTP server.' },
      { es: 'Microfono habilitado', en: 'Microphone enabled' },
      { es: 'Aplicar pines GLEDOPTO (26/5/21)', en: 'Apply GLEDOPTO pins (26/5/21)' },
      { es: 'Aplicar sensibilidad en tiempo real al mover controles', en: 'Apply sensitivity in real time while moving controls' },
      { es: 'Pines', en: 'Pins' },
      { es: 'Perfil actual soportado: generic_i2c con pines GLEDOPTO (SD=26, WS=5, SCK=21). Los cambios se aplican en caliente y se guardan en la configuracion persistente.', en: 'Current supported profile: generic_i2c with GLEDOPTO pins (SD=26, WS=5, SCK=21). Changes apply live and are saved to persistent config.' },
      { es: 'Configura las salidas LED del controlador. Max __MAX_OUTPUTS__ salidas.', en: 'Configure controller LED outputs. Max __MAX_OUTPUTS__ outputs.' },
      { es: 'Salidas LED', en: 'LED Outputs' },
      { es: '+ Agregar salida', en: '+ Add output' },
      { es: 'Limite de consumo (software)', en: 'Power limit (software)' },
      { es: 'Control de corriente estimada para evitar picos de consumo. Si se activa, el firmware escala brillo global automaticamente.', en: 'Estimated current control to avoid consumption spikes. If enabled, firmware scales global brightness automatically.' },
      { es: 'Activar limite', en: 'Enable limit' },
      { es: 'Corriente maxima total (mA)', en: 'Max total current (mA)' },
      { es: 'Corriente estimada por LED (mA)', en: 'Estimated current per LED (mA)' },
      { es: 'Referencia de Pines', en: 'Pin reference' },
      { es: 'Opciones Generales', en: 'General Settings' },
      { es: 'Idioma / Language', en: 'Language' },
      { es: 'Activa trazas detalladas por puerto serial.', en: 'Enable detailed serial debug traces.' },
      { es: 'Afecta el formato de numeros, fechas y unidades.', en: 'Affects number, date and unit formatting.' },
      { es: 'Intervalo de latido serial. 0 = desactivado.', en: 'Serial heartbeat interval. 0 = disabled.' },
      { es: 'Cargando...', en: 'Loading...' },
      { es: 'Paletas del Sistema', en: 'System Palettes' },
      { es: 'Paletas de fabrica. Solo lectura.', en: 'Factory palettes. Read-only.' },
      { es: 'Mis Paletas', en: 'My Palettes' },
      { es: 'Paletas propias. Edita o elimina desde cada tarjeta.', en: 'Custom palettes. Edit or delete from each card.' },
      { es: 'Nueva Paleta', en: 'New Palette' },
      { es: '+ Nueva', en: '+ New' },
      { es: 'Ultima respuesta API', en: 'Last API response' },
      { es: 'Guardar configuracion actual como perfil', en: 'Save current configuration as profile' },
      { es: 'Nombre', en: 'Name' },
      { es: 'Descripcion', en: 'Description' },
      { es: 'Descripcion (opcional)', en: 'Description (optional)' },
      { es: 'ID del perfil *', en: 'Profile ID *' },
      { es: 'Perfiles disponibles', en: 'Available profiles' },
      { es: 'Haz clic en Activar para cargar un perfil y aplicarlo al dispositivo. Ver config muestra el JSON completo guardado.', en: 'Click Activate to load a profile and apply it to the device. View config shows full stored JSON.' },
      { es: 'Editar perfil:', en: 'Edit profile:' },
      { es: 'JSON completo del perfil (editable)', en: 'Full profile JSON (editable)' },
      { es: 'Modifica el JSON y pulsa Guardar cambios. Solo se sobrescriben los campos que edites.', en: 'Edit JSON and press Save changes. Only edited fields are overwritten.' },
      { es: 'Captura un snapshot completo de toda la configuracion activa (red, GPIO, microfono, debug) y lo guarda con el nombre indicado. No tienes que escribir ningun JSON.', en: 'Captures a full snapshot of active configuration (network, GPIO, microphone, debug) and saves it with the given name. No JSON writing needed.' },
      { es: 'Un perfil guarda toda la configuracion del dispositivo: red Wi-Fi, GPIO, microfono y debug. El perfil default siempre refleja la configuracion activa en el dispositivo.', en: 'A profile stores full device configuration: Wi-Fi network, GPIO, microphone and debug. The default profile always reflects active device config.' },
      { es: 'Leyendo configuracion...', en: 'Reading configuration...' },
      { es: 'Configuracion cargada.', en: 'Configuration loaded.' },
      { es: 'Configuracion cargada (HTTP', en: 'Configuration loaded (HTTP' },
      { es: 'Error leyendo configuracion.', en: 'Error reading configuration.' },
      { es: 'Guardando configuracion...', en: 'Saving configuration...' },
      { es: 'Configuracion guardada y aplicada', en: 'Configuration saved and applied' },
      { es: 'Error al guardar', en: 'Error saving' },
      { es: 'Error de red al guardar.', en: 'Network error while saving.' },
      { es: 'JSON valido (sintaxis correcta).', en: 'Valid JSON (correct syntax).' },
      { es: 'JSON invalido:', en: 'Invalid JSON:' },
      { es: 'Error al aplicar', en: 'Error applying' },
      { es: 'Archivo descargado.', en: 'File downloaded.' },
      { es: 'Selecciona un archivo primero.', en: 'Select a file first.' },
      { es: 'Nada que descargar.', en: 'Nothing to download.' },
      { es: 'El editor esta vacio.', en: 'Editor is empty.' },
      { es: 'Aplicando configuracion...', en: 'Applying configuration...' },
      { es: 'Configuracion aplicada correctamente.', en: 'Configuration applied successfully.' },
      { es: 'Error de red al aplicar.', en: 'Network error while applying.' }
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
      .normalize('NFD')
      .replace(/[\u0300-\u036f]/g, '')
      .replace(/[^a-zA-Z0-9%/().:+_\-\s]+/g, ' ')
      .replace(/\s+/g, ' ')
      .trim()
      .toLowerCase();
  }

  patchRuntimeUiFunctions() {
    if (typeof window === 'undefined') return;
    if (window.__duxI18nPatched) return;

    const self = this;
    if (typeof window.duxShowToast === 'function') {
      const baseToast = window.duxShowToast;
      window.duxShowToast = function(message, variant, durationMs) {
        const translated = self.translateLiteral(message);
        return baseToast.call(this, translated, variant, durationMs);
      };
    }
    if (typeof window.duxUpdateStatus === 'function') {
      const baseStatus = window.duxUpdateStatus;
      window.duxUpdateStatus = function(target, message, isError) {
        const translated = self.translateLiteral(message);
        return baseStatus.call(this, target, translated, isError);
      };
    }
    window.__duxI18nPatched = true;
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

    // Keep UI language consistent: if we cannot translate most strings,
    // avoid ending in a mixed-language page.
    const changedNodes = [];
    let candidateCount = 0;
    let translatedCount = 0;

    nodes.forEach((node) => {
      const parent = node.parentElement;
      if (!parent) return;
      if (blockedParents.has(parent.tagName)) return;
      if (parent.closest('[data-i18n]')) return;

      const raw = node.nodeValue;
      const trimmed = String(raw || '').trim();
      if (!trimmed || trimmed.length < 2) return;

      candidateCount += 1;

      const translated = this.translateLiteral(trimmed);
      if (translated && translated !== trimmed) {
        changedNodes.push({ node, oldValue: raw });
        node.nodeValue = this.replaceKeepingWhitespace(raw, translated);
        translatedCount += 1;
      }
    });

    // If coverage is low, rollback to avoid Spanglish.
    if (this.currentLang !== 'es' && candidateCount > 0) {
      const coverage = translatedCount / candidateCount;
      const minCoverage = 0.78;
      if (coverage < minCoverage) {
        changedNodes.forEach((entry) => {
          entry.node.nodeValue = entry.oldValue;
        });
        return;
      }
    }

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

    const controls = document.querySelectorAll('button, input[type="button"], input[type="submit"], option');
    controls.forEach((el) => {
      if (el.hasAttribute('data-i18n')) return;
      if (el.tagName === 'OPTION' || el.tagName === 'BUTTON') {
        const raw = (el.textContent || '').trim();
        if (!raw) return;
        const translated = this.translateLiteral(raw);
        if (translated && translated !== raw) el.textContent = translated;
      } else if (el.tagName === 'INPUT') {
        const raw = (el.value || '').trim();
        if (!raw) return;
        const translated = this.translateLiteral(raw);
        if (translated && translated !== raw) el.value = translated;
      }
    });
  }

  /**
   * Translate all DOM elements with data-i18n attribute
   */
  translatePage() {
    this.patchRuntimeUiFunctions();

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
  const browserLang = navigator.language?.split('-')[0] || 'en';

  // Always prefer language configured in firmware (server-side config).
  fetch('/api/v1/config/general')
    .then(r => r.ok ? r.json() : Promise.reject(r.status))
    .then(data => {
      const serverLang = data.general?.language;
      const startLang = serverLang || localLang || browserLang || 'en';
      return i18n.load(startLang);
    })
    .catch(() => {
      const fallback = localLang || browserLang || 'en';
      return i18n.load(fallback);
    })
    .catch(err => console.error('[i18n] Initialization failed:', err));
});
