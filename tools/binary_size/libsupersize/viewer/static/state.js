// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * Methods for manipulating the state and the DOM of the page
 */

/** @type {HTMLFormElement} Form with options and filters */
const form = /** @type {HTMLFormElement} */ (
    document.getElementById('options'));

/** @type {HTMLInputElement} */
const methodCountInput = /** @type {HTMLInputElement} */ (
    form.elements.namedItem('method_count'));

/** Utilities for working with the DOM */
const dom = {
  /**
   * Creates a document fragment from the given nodes.
   * @param {Iterable<Node>} nodes
   * @returns {DocumentFragment}
   */
  createFragment(nodes) {
    const fragment = document.createDocumentFragment();
    for (const node of nodes)
      fragment.appendChild(node);
    return fragment;
  },
  /**
   * Removes all the existing children of `parent` and inserts `newChild` in
   * their place.
   * @param {Node} parent
   * @param {Node | null} newChild
   */
  replace(parent, newChild) {
    while (parent.firstChild)
      parent.removeChild(parent.firstChild);
    if (newChild != null)
      parent.appendChild(newChild);
  },
  /**
   * Builds a text element in a single statement.
   * @param {string} tagName Type of the element, such as "span".
   * @param {string} text Text content for the element.
   * @param {string} [className] Class to apply to the element.
   */
  textElement(tagName, text, className) {
    const element = document.createElement(tagName);
    element.textContent = text;
    if (className)
      element.className = className;
    return element;
  },
};

/** Build utilities for working with the state. */
function _initState() {
  const _DEFAULT_FORM = new FormData(form);

  /**
   * State is represented in the query string and can be manipulated by this
   * object. Keys in the query match with input names.
   */

  /** @type {URLSearchParams} */
  let _filterParams = new URLSearchParams(location.search.slice(1));
  const typeList = _filterParams.getAll(_TYPE_STATE_KEY);
  _filterParams.delete(_TYPE_STATE_KEY);
  for (const type of types(typeList)) {
    _filterParams.append(_TYPE_STATE_KEY, type);
  }

  const state = Object.freeze({
    /**
     * Returns a string from the current query string state.
     * @param {string} key
     * @returns {string | null}
     */
    get(key) {
      return _filterParams.get(key);
    },

    /**
     * Checks if a key is present in the query string state.
     * @param {string} key
     * @returns {boolean}
     */
    has(key) {
      return _filterParams.has(key);
    },

    /**
     * Formats the filter state as a string.
     */
    toString() {
      const copy = new URLSearchParams(_filterParams);
      const types = [...new Set(copy.getAll(_TYPE_STATE_KEY))];
      if (types.length > 0) copy.set(_TYPE_STATE_KEY, types.join(''));

      const queryString = copy.toString();
      return queryString.length > 0 ? `?${queryString}` : '';
    },

    /**
     * Saves a key and value into a temporary state not displayed in the URL.
     * @param {string} key
     * @param {string | null} value
     */
    set(key, value) {
      if (value == null) {
        _filterParams.delete(key);
      } else {
        _filterParams.set(key, value);
      }
      // Passing empty `state` leads to no change, so use `location.pathname`.
      history.replaceState(null, null, state.toString() || location.pathname);
    },
  });

  // Update form inputs to reflect the state from URL.
  for (const element of
      /** @type {Array<HTMLInputElement>} */ (Array.from(form.elements))) {
    if (element.name) {
      const input = /** @type {HTMLInputElement} */ (element);
      const values = _filterParams.getAll(input.name);
      const [value] = values;
      if (value) {
        switch (input.type) {
          case 'checkbox':
            input.checked = values.includes(input.value);
            break;
          case 'radio':
            input.checked = value === input.value;
            break;
          default:
            input.value = value;
            break;
        }
      }
    }
  }

  /**
   * Yields only entries that have been modified relative to `_DEFAULT_FORM`.
   * @generator
   * @param {FormData} modifiedForm
   * @yields {{key: string, value: FormDataEntryValue}}
   */
  function* onlyChangedEntries(modifiedForm) {
    // Remove default values
    for (const key of modifiedForm.keys()) {
      const modifiedValues = modifiedForm.getAll(key);
      const defaultValues = _DEFAULT_FORM.getAll(key);

      const valuesChanged =
          (modifiedValues.length !== defaultValues.length) ||
          modifiedValues.some((v, i) => v !== defaultValues[i]);
      if (valuesChanged) {
        for (const value of modifiedValues) {
          yield {key, value};
        }
      }
    }
  }

  // Update the state when the form changes.
  function _updateStateFromForm() {
    _filterParams = new URLSearchParams();
    const modifiedForm = new FormData(form);
    for (const {key, value} of onlyChangedEntries(modifiedForm)) {
      _filterParams.append(key, value.toString());
    }
    // Passing empty `state` leads to no change, so use `location.pathname`.
    history.replaceState(null, null, state.toString() || location.pathname);
  }

  form.addEventListener('change', _updateStateFromForm);

  return state;
}

function _startListeners() {
  const _SHOW_OPTIONS_STORAGE_KEY = 'show-options';

  /** @type {HTMLFieldSetElement} */
  const typesFilterElement = /** @type {HTMLFieldSetElement} */ (
      document.getElementById('types-filter'));
  /** @type {HTMLFieldSetElement} */
  const byteunit = /** @type {HTMLFieldSetElement} */ (
      form.elements.namedItem('byteunit'));
  /** @type {RadioNodeList} */
  const typeCheckboxes = /** @type {RadioNodeList} */ (
      form.elements.namedItem(_TYPE_STATE_KEY));
  /** @type {HTMLSpanElement} */
  const sizeHeader = /** @type {HTMLSpanElement} */ (
      document.getElementById('size-header'));

  /**
   * The settings dialog on the side can be toggled on and off by elements with
   * a 'toggle-options' class.
   */
  function _toggleOptions() {
    const openedOptions = document.body.classList.toggle('show-options');
    localStorage.setItem(_SHOW_OPTIONS_STORAGE_KEY, openedOptions.toString());
  }
  for (const button of document.getElementsByClassName('toggle-options')) {
    button.addEventListener('click', _toggleOptions);
  }
  // Default to open if getItem returns null
  if (localStorage.getItem(_SHOW_OPTIONS_STORAGE_KEY) !== 'false') {
    document.body.classList.add('show-options');
  }

  /**
   * Disable some fields when method_count is set
   */
  function setMethodCountModeUI() {
    if (methodCountInput.checked) {
      byteunit.setAttribute('disabled', '');
      typesFilterElement.setAttribute('disabled', '');
      sizeHeader.textContent = 'Methods';
    } else {
      byteunit.removeAttribute('disabled');
      typesFilterElement.removeAttribute('disabled');
      sizeHeader.textContent = 'Size';
    }
  }
  setMethodCountModeUI();
  methodCountInput.addEventListener('change', setMethodCountModeUI);

  /**
   * Display error text on blur for regex inputs, if the input is invalid.
   * @param {Event} event
   */
  function checkForRegExError(event) {
    const input = /** @type {HTMLInputElement} */ (event.currentTarget);
    const errorBox = document.getElementById(
      input.getAttribute('aria-describedby')
    );
    try {
      new RegExp(input.value);
      errorBox.textContent = '';
      input.setAttribute('aria-invalid', 'false');
    } catch (err) {
      errorBox.textContent = err.message;
      input.setAttribute('aria-invalid', 'true');
    }
  }
  for (const input of document.getElementsByClassName('input-regex')) {
    input.addEventListener('blur', checkForRegExError);
    input.dispatchEvent(new Event('blur'));
  }

  document.getElementById('type-all').addEventListener('click', () => {
    for (const checkbox of typeCheckboxes) {
      /** @type {HTMLInputElement} */ (checkbox).checked = true;
    }
    form.dispatchEvent(new Event('change'));
  });
  document.getElementById('type-none').addEventListener('click', () => {
    for (const checkbox of typeCheckboxes) {
      /** @type {HTMLInputElement} */ (checkbox).checked = false;
    }
    form.dispatchEvent(new Event('change'));
  });
}

function _makeIconTemplateGetter() {
  const _icons = document.getElementById('icons');

  /**
   * @type {{[type:string]: SVGSVGElement}} Icon elements
   * that correspond to each symbol type.
   */
  const symbolIcons = {
    D: _icons.querySelector('.foldericon'),
    G: _icons.querySelector('.groupicon'),
    J: _icons.querySelector('.javaclassicon'),
    F: _icons.querySelector('.fileicon'),
    b: _icons.querySelector('.bssicon'),
    d: _icons.querySelector('.dataicon'),
    r: _icons.querySelector('.readonlyicon'),
    t: _icons.querySelector('.codeicon'),
    R: _icons.querySelector('.relroicon'),
    '*': _icons.querySelector('.generatedicon'),
    x: _icons.querySelector('.dexicon'),
    m: _icons.querySelector('.dexmethodicon'),
    p: _icons.querySelector('.localpakicon'),
    P: _icons.querySelector('.nonlocalpakicon'),
    o: _icons.querySelector('.othericon'),  // used as default icon
  };

  const _statuses = document.getElementById('symbol-diff-status-icons');
  const statusIcons = {
    added: _statuses.querySelector('.addedicon'),
    removed: _statuses.querySelector('.removedicon'),
    changed: _statuses.querySelector('.changedicon'),
    unchanged: _statuses.querySelector('.unchangedicon'),
  };


  /** @type {Map<string, {color:string,description:string}>} */
  const iconInfoCache = new Map();

  /**
   * Returns the SVG icon template element corresponding to the given type.
   * @param {string} type Symbol type character.
   * @param {boolean} readonly If true, the original template is returned.
   * If false, a copy is returned that can be modified.
   * @returns {SVGSVGElement}
   */
  function getIconTemplate(type, readonly = false) {
    const iconTemplate = symbolIcons[type] || symbolIcons[_OTHER_SYMBOL_TYPE];
    return /** @type {SVGSVGElement} */ (
        readonly ? iconTemplate : iconTemplate.cloneNode(true));
  }

  /**
   * Returns style info about SVG icon template element corresponding to the
   * given type.
   * @param {string} type Symbol type character.
   */
  function getIconStyle(type) {
    let info = iconInfoCache.get(type);
    if (info == null) {
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
   * @returns {SVGSVGElement}
   */
  function getDiffStatusTemplate(node) {
    const isLeaf = node.children && node.children.length === 0;
    const entries = Object.entries(node.childStats);
    let key = 'unchanged';
    if (isLeaf && entries.length != 0) {
      const statsEntry = entries[0][1];
      if (statsEntry.added) {
        key = 'added';
      } else if (statsEntry.removed) {
        key = 'removed';
      } else if (statsEntry.changed) {
        key = 'changed';
      }
    } else if (node.diffStatus == _DIFF_STATUSES.ADDED) {
      key = 'added';
    } else if (node.diffStatus == _DIFF_STATUSES.REMOVED) {
      key = 'removed';
    }
    return statusIcons[key].cloneNode(true);
  }

  return {getIconTemplate, getIconStyle, getDiffStatusTemplate};
}

function _makeSizeTextGetter() {
  /**
   * Create the contents for the size element of a tree node.
   * The unit to use is selected from the current state.
   *
   * If in method count mode, size instead represents the amount of methods in
   * the node. Otherwise, the original number of bytes will be displayed.
   *
   * @param {TreeNode} node Node whose size is the number of bytes to use for
   * the size text
   * @returns {GetSizeResult} Object with hover text title and size element
   * body.
   */
  function getSizeContents(node) {
    if (state.has('method_count')) {
      const {count: methodCount = 0} =
        node.childStats[_DEX_METHOD_SYMBOL_TYPE] || {};
      const methodStr = formatNumber(methodCount);
      return {
        description: `${methodStr} method${methodCount === 1 ? '' : 's'}`,
        element: document.createTextNode(methodStr),
        value: methodCount,
      };

    } else {
      const bytes = node.size;
      const descriptionToks = [];
      if ('beforeSize' in node) {
        const before = formatNumber(node.beforeSize);
        const after = formatNumber(node.beforeSize + bytes);
        descriptionToks.push(`(${before} → ${after})`);  // '→' is '\u2192'.
      }
      descriptionToks.push(`${formatNumber(bytes)} bytes`);
      if (node.numAliases && node.numAliases > 1) {
        descriptionToks.push(`for 1 of ${node.numAliases} aliases`);
      }

      const unit = state.get('byteunit') || 'KiB';
      const suffix = _BYTE_UNITS[unit];
      // Format |bytes| as a number with 2 digits after the decimal point
      const text = formatNumber(bytes / suffix, 2, 2);
      const textNode = document.createTextNode(`${text} `);
      // Display the suffix with a smaller font
      const suffixElement = dom.textElement('small', unit);

      return {
        description: descriptionToks.join(' '),
        element: dom.createFragment([textNode, suffixElement]),
        value: bytes,
      };
    }
  }

  /**
   * Set classes on an element based on the size it represents.
   * @param {HTMLElement} sizeElement
   * @param {number} value
   */
  function setSizeClasses(sizeElement, value) {
    const cutOff = methodCountInput.checked ? 10 : 50000;
    const shouldHaveStyle =
      state.has('diff_mode') && Math.abs(value) > cutOff;

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

  return {getSizeContents, setSizeClasses};
}

/** Utilities for working with the state */
const state = _initState();
const {getIconTemplate, getIconStyle, getDiffStatusTemplate} =
    _makeIconTemplateGetter();
const {getSizeContents, setSizeClasses} = _makeSizeTextGetter();
_startListeners();
