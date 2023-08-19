// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * Methods for manipulating the state and the DOM of the page
 */

/**
 * @typedef {string|number|boolean} StateValue
 */

/**
 * @typedef {Object} HasValue
 * @property {StateValue} value - Readable and writable value.
 */

/**
 * Encapsulation of an UI state, supporting observers on setting values.
 */
class UiState {
  /** @param {!StateValue} defaultValue */
  constructor(defaultValue) {
    /** @protected {!StateValue} */
    this.defaultValue = defaultValue;

    /** @protected {!StateValue} */
    this.value = this.defaultValue;

    /** @private {!Array<!function(): *>} */
    this.observers = [];
  }

  /**
   * @param {!function(): *} observer
   * @public
   */
  addObserver(observer) {
    this.observers.push(observer);
  }

  /** @protected */
  notifyObservers() {
    for (const observer of this.observers) {
      observer();
    }
  }

  /**
   * @return {!StateValue}
   * @public
   */
  get() {
    return this.value;
  }

  /**
   * @param {!StateValue} v
   * @public
   */
  set(v) {
    this.value = v;
    this.notifyObservers();
  }
}

/**
 * UiState supporting reads from / writes to a provided query param.
 */
class QueryParamUiState extends UiState {
  /**
   * @param {string} name
   * @param {!StateValue} defaultValue
   * @param {?function(string): StateValue} parser
   * @param {boolean} isHash
   */
  constructor(name, defaultValue, parser, isHash) {
    super(defaultValue);

    /** @public @const {string} */
    this.name = name;

    /** @private @const {?function(string): StateValue} null = identity. */
    this.parser = parser;

    /** @public @const {boolean} */
    this.isHash = isHash;

    /** @public {string} */
    this.hidden = false;
  }

  /**
   * @param {!URLSearchParams} params
   * @public
   */
  readFromSearchParams(params) {
    if (params.has(this.name)) {
      const s = params.get(this.name);
      this.value = this.parser ? this.parser(s) : s;
    } else {
      this.value = this.defaultValue;
    }
    this.notifyObservers();
  }

  /**
   * @param {!URLSearchParams} params
   * @public
   */
  writeToSearchParams(params) {
    if (this.hidden || this.value === this.defaultValue) {
      params.delete(this.name);
    } else {
      const s = (this.value === true) ? 'on' : this.value.toString();
      params.set(this.name, s);
    }
  }
}

/**
 * QueryParamUiState that syncs with UI elements.
 */
class ElementUiState extends QueryParamUiState {
  /**
   * @param {string} name
   * @param {!HasValue} elt
   * @param {boolean} isHash
   */
  constructor(name, elt, isHash) {
    let parser = null;
    let readElt = () => elt.value;
    let writeElt = (v) => {
      elt.value = v;
    };

    // Define Element specific adapters for data access to concentrate the mess,
    // and reduce boilerplate from defining stateful adapter classes.
    if (elt instanceof HTMLInputElement) {
      const input = /** @type {HTMLInputElement} */ (elt);
      if (input.type === 'number') {
        parser = (s) => {
          const ret = parseInt(s, 10);
          return isNaN(ret) ? this.defaultValue : ret;
        };
      } else if (input.type === 'checkbox') {
        parser = (s) => s === 'on' || s === 'true' || s === '1';
        readElt = () => elt.checked;
        writeElt = (v) => {
          elt.checked = v;
        };
      }
    } else if (elt instanceof HTMLSelectElement) {
      const sel = /** @type {!HTMLSelectElement} */ (elt);
      const values =
          new Set(Array.from(sel.querySelectorAll('option'), e => e.value));
      parser = (s) => values.has(s) ? s : this.defaultValue;
    } else if (elt instanceof RadioNodeList) {
      const inputs = Array.from(
          /** @type {!RadioNodeList} */ (elt),
          e => /** @type {HTMLInputElement} */ (e));
      assert(inputs.length > 0);
      if (inputs[0].type === 'radio') {
        const values = new Set(Array.from(inputs, e => e.value));
        parser = (s) => values.has(s) ? s : this.defaultValue;
      } else if (inputs[0].type === 'checkbox') {
        readElt = () => {
          return Array.from(inputs, e => e.checked ? e.value : '').join('');
        };
        writeElt = (s) => {
          const values = new Set(Array.from(s));
          for (const e of inputs) {
            e.checked = values.has(e.value);
          }
        };
      } else {
        throw new Error(`Unknown RadioNodeList type: ${inputs[0].type}.`);
      }
    } else {
      throw new Error('Unknown element type.');
    }

    super(name, readElt(), parser, isHash);

    /** @private @const {function(): StateValue} */
    this.readElt = readElt;

    /** @private @const {function(): StateValue} */
    this.writeElt = writeElt;
  }

  /**
   * @param {!StateValue} v
   * @public @override
   */
  set(v) {
    super.set(v);
    this.writeElt(/** @type {StateValue} */ (this.value));
  }

  /**
   * @param {!URLSearchParams} params
   * @public @override
   */
  readFromSearchParams(params) {
    super.readFromSearchParams(params);
    this.writeElt(/** @type {StateValue} */ (this.value));
  }

  /** @public */
  syncFromElt() {
    // Calling super.set() to avoid redundant writeElt() call in this.set().
    super.set(/** @type {StateValue} */ (this.readElt()));
  }
}

/** Build utilities for working with the state. */
class MainState {
  constructor() {
    /** @private @const {!Array<!QueryParamUiState>} */
    this.uiStates = [];

    /**
     * Instantiation helper that also pushes object to |uiStates|.
     * @param {string} name
     * @param {?HasValue} elt
     * @param {boolean=} isHash
     */
    const newUiState = (name, elt, isHash = false) => {
      if (!elt) {
        // Assume string value with defaultValue == ''.
        this.uiStates.push(new QueryParamUiState(name, '', null, isHash));
      } else {
        this.uiStates.push(new ElementUiState(name, elt, isHash));
      }
      return this.uiStates[this.uiStates.length - 1];
    };

    /**
     * @public @const {!QueryParamUiState} Active "load" URL that gets updated
     *   on "Upload data".
     */
    this.stLoadUrl = newUiState(STATE_KEY.LOAD_URL, null);

    /**
     * @public @const {!QueryParamUiState} "Before" URL that gets cleared on
     *   "Upload data".
     */
    this.stBeforeUrl = newUiState(STATE_KEY.BEFORE_URL, null);

    /** @public @const {!ElementUiState} */
    this.stMethodCount = newUiState(STATE_KEY.METHOD_COUNT, g_el.cbMethodCount);

    /** @public @const {!ElementUiState} */
    this.stByteUnit = newUiState(STATE_KEY.BYTE_UNIT, g_el.selByteUnit);

    /** @public @const {!ElementUiState} */
    this.stGroupBy = newUiState(STATE_KEY.GROUP_BY, g_el.rnlGroupBy);

    /** @public @const {!ElementUiState} */
    this.stMinSize = newUiState(STATE_KEY.MIN_SIZE, g_el.nbMinSize);

    /** @public @const {!ElementUiState} */
    this.stInclude = newUiState(STATE_KEY.INCLUDE, g_el.tbIncludeRegex);

    /** @public @const {!ElementUiState} */
    this.stExclude = newUiState(STATE_KEY.EXCLUDE, g_el.tbExcludeRegex);

    /** @public @const {!ElementUiState} */
    this.stType = newUiState(STATE_KEY.TYPE, g_el.rnlType);

    /** @public @const {!ElementUiState} */
    this.stFlagFilter = newUiState(STATE_KEY.FLAG_FILTER, g_el.rnlFlagFilter);

    /** @public @const {!QueryParamUiState} */
    this.stFocus = newUiState(STATE_KEY.FOCUS, null, true);

    /** @private {boolean} */
    this.diffMode = false;
  }

  /**
   * Formats the filter state as a string.
   * @return {string}
   * @private
   */
  toString() {
    const queryParams = new URLSearchParams();
    const hashParams = new URLSearchParams();
    for (const st of this.uiStates) {
      st.writeToSearchParams(st.isHash ? hashParams : queryParams);
    }
    const queryString = queryParams.toString();
    const hashString = hashParams.toString();
    return (queryString.length > 0 ? `?${queryString}` : '') +
        (hashString.length > 0 ? `#${hashString}` : '');
  }

  /** @private */
  updateUrlParams() {
    // Passing empty `state` leads to no change, so use `location.pathname`.
    history.replaceState(null, null, this.toString() || location.pathname);
  }

  /**
   * @return {?function(string): boolean}
   * @public
   */
  getFilter() {
    const getRegExpOrNull = (s) => {
      if (s) {
        try {
          return new RegExp(s);
        } catch (err) {
        }
      }
      return null;
    };

    const includeRE = getRegExpOrNull(this.stInclude.get());
    const excludeRE = getRegExpOrNull(this.stExclude.get());
    if (includeRE) {
      return excludeRE ? (s) => includeRE.test(s) && !excludeRE.test(s) :
                         (s) => includeRE.test(s);
    }
    return excludeRE ? (s) => !excludeRE.test(s) : null;
  }

  /**
   * @return {boolean}
   * @public
   */
  getDiffMode() {
    return this.diffMode;
  }

  /**
   * @param {boolean} diffMode
   * @public
   */
  setDiffMode(diffMode) {
    this.diffMode = diffMode;
  }

  /**
   * @return {!BuildOptions}
   * @public
   */
  exportToBuildOptions() {
    const ret = /** @type {BuildOptions} */ ({});
    ret.loadUrl = /** @type {string} */ (this.stLoadUrl.get());
    ret.beforeUrl = /** @type {string} */ (this.stBeforeUrl.get());
    ret.methodCountMode = /** @type {boolean} */ (this.stMethodCount.get());
    // Skipping |this.stByteUnit|.
    ret.minSymbolSize = /** @type {number} */ (this.stMinSize.value);
    ret.groupBy = /** @type {string} */ (this.stGroupBy.get());
    ret.includeRegex = /** @type {string} */ (this.stInclude.get());
    ret.excludeRegex = /** @type {string} */ (this.stExclude.get());
    if (ret.methodCountMode) {
      ret.includeSections = _DEX_METHOD_SYMBOL_TYPE;
    } else {
      ret.includeSections = /** @type {string} */ (this.stType.get());
    }
    const flagToFilterStr = /** @type {string} */ (this.stFlagFilter.get());
    ret.flagToFilter = _NAMES_TO_FLAGS[flagToFilterStr] ?? 0;
    ret.nonOverhead = flagToFilterStr === 'nonoverhead';
    ret.disassemblyMode = flagToFilterStr === 'disassembly';
    return ret;
  }

  /** @public */
  init() {
    const queryParams = new URLSearchParams(location.search.slice(1));
    const hashParams = new URLSearchParams(location.hash.slice(1));
    for (const st of this.uiStates) {
      st.readFromSearchParams(st.isHash ? hashParams : queryParams);
    }
    // At this point it's possible to update the URL to fix mistakes and
    // canonicalize (e.g., param ordering). However, we choose to NOT do this
    // since it's disconcerting for the user, as they might want to continue
    // editing or tweaking the URL.

    const loadUrl = /** @type {string} */ (this.stLoadUrl.get());
    const beforeUrl = /** @type {string} */ (this.stBeforeUrl.get());
    this.setDiffMode(
        Boolean(loadUrl && (loadUrl.endsWith('.sizediff') || beforeUrl)));

    // If load_url changes beyond initial load, clear before_url and hide both
    // in query params.
    this.stLoadUrl.addObserver(() => {
      this.stBeforeUrl.set('');
      if (!this.stLoadUrl.hidden) {
        this.stLoadUrl.hidden = true;
        this.stBeforeUrl.hidden = true;
        this.updateUrlParams();
      }
    });

    // Update states on form change.
    g_el.frmOptions.addEventListener('change', (e) => {
      for (const st of this.uiStates) {
        if (st instanceof ElementUiState)
          st.syncFromElt();
      }
      this.updateUrlParams();
    });

    this.stFocus.addObserver(() => this.updateUrlParams());
  }
}

function _startListeners() {
  const _SHOW_OPTIONS_STORAGE_KEY = 'show-options';

  /** @type {RadioNodeList} */
  const typeCheckboxes = /** @type {RadioNodeList} */ (
      g_el.frmOptions.elements.namedItem(STATE_KEY.TYPE));

  /**
   * The settings dialog on the side can be toggled on and off by elements with
   * a 'toggle-options' class.
   */
  function _toggleOptions() {
    const openedOptions = document.body.classList.toggle('show-options');
    localStorage.setItem(_SHOW_OPTIONS_STORAGE_KEY, openedOptions.toString());
  }
  for (const node of g_el.nlShowOptions) {
    node.addEventListener('click', _toggleOptions);
  }
  // Default to open if getItem returns null
  if (localStorage.getItem(_SHOW_OPTIONS_STORAGE_KEY) !== 'false') {
    document.body.classList.add('show-options');
  }

  /** Disables some fields when method_count is set. */
  function setMethodCountModeUI() {
    if (state.stMethodCount.get()) {
      g_el.selByteUnit.setAttribute('disabled', '');
      g_el.fsTypesFilter.setAttribute('disabled', '');
      g_el.spanSizeHeader.textContent = 'Methods';
    } else {
      g_el.selByteUnit.removeAttribute('disabled');
      g_el.fsTypesFilter.removeAttribute('disabled');
      g_el.spanSizeHeader.textContent = 'Size';
    }
  }
  setMethodCountModeUI();
  state.stMethodCount.addObserver(setMethodCountModeUI);

  /**
   * Displays error text on blur for regex inputs, if the input is invalid.
   * @param {Event} event
   */
  function checkForRegExError(event) {
    const input = /** @type {HTMLInputElement} */ (event.currentTarget);
    const errorBox = g_el.getAriaDescribedBy(input);
    try {
      new RegExp(input.value);
      errorBox.textContent = '';
      input.setAttribute('aria-invalid', 'false');
    } catch (err) {
      errorBox.textContent = err.message;
      input.setAttribute('aria-invalid', 'true');
    }
  }
  for (const input of [g_el.tbIncludeRegex, g_el.tbExcludeRegex]) {
    input.addEventListener('blur', checkForRegExError);
    input.dispatchEvent(new Event('blur'));
  }

  g_el.btnTypeAll.addEventListener('click', () => {
    for (const checkbox of typeCheckboxes) {
      /** @type {HTMLInputElement} */ (checkbox).checked = true;
    }
    g_el.frmOptions.dispatchEvent(new Event('change'));
  });
  g_el.btnTypeNone.addEventListener('click', () => {
    for (const checkbox of typeCheckboxes) {
      /** @type {HTMLInputElement} */ (checkbox).checked = false;
    }
    g_el.frmOptions.dispatchEvent(new Event('change'));
  });

  // Outside of input, make pressing "?" open FAQ page.
  window.addEventListener('keydown', event => {
    if (event.key === '?' &&
        /** @type {HTMLElement} */ (event.target).tagName !== 'INPUT') {
      // Open help when "?" is pressed.
      g_el.linkFaq.click();
    }
  });
}

function _makeIconTemplateGetter() {
  const getSymbolIcon = (q) => assertNotNull(g_el.divIcons.querySelector(q));

  /**
   * @type {{[type:string]: SVGSVGElement}} Icon elements
   * that correspond to each symbol type.
   */
  const symbolIcons = {
    D: getSymbolIcon('.foldericon'),
    G: getSymbolIcon('.groupicon'),
    J: getSymbolIcon('.javaclassicon'),
    F: getSymbolIcon('.fileicon'),
    b: getSymbolIcon('.bssicon'),
    d: getSymbolIcon('.dataicon'),
    r: getSymbolIcon('.readonlyicon'),
    t: getSymbolIcon('.codeicon'),
    R: getSymbolIcon('.relroicon'),
    x: getSymbolIcon('.dexothericon'),
    m: getSymbolIcon('.dexmethodicon'),
    p: getSymbolIcon('.localpakicon'),
    P: getSymbolIcon('.nonlocalpakicon'),
    a: getSymbolIcon('.arscicon'),
    o: getSymbolIcon('.othericon'),  // used as default icon
    '*': null,
  };

  const getDiffStatusIcon = (q) => {
    return assertNotNull(g_el.divDiffStatusIcons.querySelector(q));
  };
  const statusIcons = {
    added: getDiffStatusIcon('.addedicon'),
    removed: getDiffStatusIcon('.removedicon'),
    changed: getDiffStatusIcon('.changedicon'),
    unchanged: getDiffStatusIcon('.unchangedicon'),
  };

  const getMiscIcon = (q) => {
    return assertNotNull(g_el.divMiscIcons.querySelector(q))
  };
  const metricsIcons = {
    group: getSymbolIcon('.groupicon'),  // Reuse.
    elf: getMiscIcon('.elficon'),
    dex: getMiscIcon('.dexicon'),
    arsc: getSymbolIcon('.arscicon'),  // Reuse.
    metrics: getMiscIcon('.metricsicon'),
    other: getSymbolIcon('.othericon'),  // Reuse.
  };

  const metadataIcons = {
    root: getMiscIcon('.metadataicon'),
    group: getSymbolIcon('.groupicon'),  // Reuse.
  };

  /** @type {Map<string, {color:string, description:string}>} */
  const iconInfoCache = new Map();

  /**
   * Returns the SVG icon template element corresponding to the given type.
   * @param {string} type Symbol type character.
   * @param {boolean} readonly If true, the original template is returned.
   * If false, a copy is returned that can be modified.
   * @return {SVGSVGElement}
   */
  function getIconTemplate(type, readonly = false) {
    const iconTemplate = symbolIcons[type] || symbolIcons[_OTHER_SYMBOL_TYPE];
    return /** @type {SVGSVGElement} */ (
        readonly ? iconTemplate : iconTemplate.cloneNode(true));
  }

  /**
   * @param {string} type Symbol type character.
   * @param {?string} fill If non-null, fill color of icon.
   */
  function getIconTemplateWithFill(type, fill) {
    const icon = getIconTemplate(type);
    if (fill)
      icon.setAttribute('fill', fill);
    return icon;
  }

  /**
   * Returns style info about SVG icon template element corresponding to the
   * given type.
   * @param {string} type Symbol type character.
   */
  function getIconStyle(type) {
    let info = iconInfoCache.get(type);
    if (!info) {
      const icon = getIconTemplate(type, true);
      info = {
        color: icon.getAttribute('fill'),
        description: icon.querySelector('title').textContent,
      };
      iconInfoCache.set(type, info);
    }
    return info;
  }

  /**
   * Returns the SVG status icon template element corresponding to the diff
   * status of the node. Only valid for leaf nodes.
   * @param {TreeNode} node Leaf node whose diff status is used to select
   * template.
   * @return {SVGSVGElement}
   */
  function getDiffStatusTemplate(node) {
    const isLeaf = node.children && node.children.length === 0;
    const entries = Object.entries(node.childStats);
    let key = 'unchanged';
    if (isLeaf && entries.length !== 0) {
      const statsEntry = entries[0][1];
      if (statsEntry.added) {
        key = 'added';
      } else if (statsEntry.removed) {
        key = 'removed';
      } else if (statsEntry.changed) {
        key = 'changed';
      }
    } else if (node.diffStatus === _DIFF_STATUSES.ADDED) {
      key = 'added';
    } else if (node.diffStatus === _DIFF_STATUSES.REMOVED) {
      key = 'removed';
    }
    return statusIcons[key].cloneNode(true);
  }

  /**
   * @param {string} key
   * @return {SVGSVGElement}
   */
  function getMetricsIconTemplate(key) {
    return metricsIcons[key].cloneNode(true);
  }

  /**
   * @param {string} key
   * @return {SVGSVGElement}
   */
  function getMetadataIconTemplate(key) {
    return metadataIcons[key].cloneNode(true);
  }

  return {
    getIconTemplate,
    getIconTemplateWithFill,
    getIconStyle,
    getDiffStatusTemplate,
    getMetricsIconTemplate,
    getMetadataIconTemplate,
  };
}

function _makeSizeTextGetter() {
  /**
   * @param {number} bytes
   * @return {!DocumentFragment}
   */
  function makeBytesElement(bytes) {
    const unit = /** @type {string} */ (state.stByteUnit.get());
    const suffix = _BYTE_UNITS[unit];
    // Format |bytes| as a number with 2 digits after the decimal point
    const text = formatNumber(bytes / suffix, 2, 2);
    const textNode = document.createTextNode(`${text} `);
    // Display the suffix with a smaller font
    const suffixElement = dom.textElement('small', unit);

    return dom.createFragment([textNode, suffixElement]);
  }

  /**
   * Create the contents for the size element of a tree node.
   * The unit to use is selected from the current state.
   *
   * If in method count mode, size instead represents the amount of methods in
   * the node. Otherwise, the original number of bytes will be displayed.
   *
   * @param {TreeNode} node Node whose size is the number of bytes to use for
   * the size text
   * @return {GetSizeResult} Object with hover text title and size element
   * body.
   */
  function getSizeContents(node) {
    if (state.stMethodCount.get()) {
      const {count: methodCount = 0} =
        node.childStats[_DEX_METHOD_SYMBOL_TYPE] || {};
      const methodStr = formatNumber(methodCount);
      return {
        description: `${methodStr} method${methodCount === 1 ? '' : 's'}`,
        element: document.createTextNode(methodStr),
        value: methodCount,
      };

    } else {
      const isLeaf = node.children && node.children.length === 0;
      const bytes = node.size;
      const descriptionToks = [];
      // Show "before → after" only for leaf nodes, since group nodes'
      // |beforeSize| would miss contributions from unchanged symbols.
      if (isLeaf && ('beforeSize' in node)) {
        const before = formatNumber(node.beforeSize);
        const after = formatNumber(node.beforeSize + bytes);
        descriptionToks.push(`(${before} → ${after})`);  // '→' is '\u2192'.
      }
      descriptionToks.push(`${formatNumber(bytes)} bytes`);
      if (node.numAliases && node.numAliases > 1) {
        descriptionToks.push(`for 1 of ${node.numAliases} aliases`);
      }

      return {
        description: descriptionToks.join(' '),
        element: makeBytesElement(bytes),
        value: bytes,
      };
    }
  }

  /**
   * Set classes on an element based on the size it represents.
   * @param {HTMLElement} sizeElement
   * @param {number} value
   * @param {boolean} isCount Whether |value| is count (true) or byte (false).
   */
  function setSizeClasses(sizeElement, value, isCount) {
    const cutOff = isCount ? 10 : 50000;
    const shouldHaveStyle = state.getDiffMode() && Math.abs(value) > cutOff;

    if (shouldHaveStyle) {
      if (value < 0) {
        sizeElement.classList.add('shrunk');
        sizeElement.classList.remove('grew');
      } else {
        sizeElement.classList.remove('shrunk');
        sizeElement.classList.add('grew');
      }
    } else {
      sizeElement.classList.remove('shrunk', 'grew');
    }
  }

  return {makeBytesElement, getSizeContents, setSizeClasses};
}

/** Global UI State. */
const state = new MainState();
state.init();

/** Utilities for working with the state */
const {
  getIconTemplate,
  getIconTemplateWithFill,
  getIconStyle,
  getDiffStatusTemplate,
  getMetricsIconTemplate,
  getMetadataIconTemplate,
} = _makeIconTemplateGetter();
const {makeBytesElement, getSizeContents, setSizeClasses} =
    _makeSizeTextGetter();
_startListeners();
