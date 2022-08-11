// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_radio_button/cr_radio_button.m.js';
import '../shared_vars_css.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EventTracker} from '../../js/event_tracker.m.js';

/**
 * @param {!Element} radio
 * @return {boolean}
 */
function isEnabled(radio) {
  return radio.matches(':not([disabled]):not([hidden])') &&
      radio.style.display !== 'none' && radio.style.visibility !== 'hidden';
}

export class CrRadioGroupElement extends PolymerElement {
  static get is() {
    return 'cr-radio-group';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'update_',
      },

      selected: {
        type: String,
        notify: true,
        observer: 'update_',
      },

      selectableElements: {
        type: String,
        value: 'cr-radio-button, cr-card-radio-button, controlled-radio-button',
      },

      /**
       * @type {!RegExp}
       * @private
       */
      selectableRegExp_: {
        value: Object,
        computed: 'computeSelectableRegExp_(selectableElements)',
      },
    };
  }

  constructor() {
    super();
    /** @private {?Array<!CrRadioButtonElement>} */
    this.buttons_ = null;

    /** @private {?EventTracker} */
    this.buttonEventTracker_ = null;

    /** @private {?Map<string, number>} */
    this.deltaKeyMap_ = null;

    /** @private {boolean} */
    this.isRtl_ = false;

    /** @private {Function} */
    this.populateBound_ = null;
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
    this.addEventListener('click', this.onClick_.bind(this));

    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'radiogroup');
    }
    this.setAttribute('aria-disabled', 'false');
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.isRtl_ = this.matches(':host-context([dir=rtl]) cr-radio-group');
    this.deltaKeyMap_ = new Map([
      ['ArrowDown', 1],
      ['ArrowLeft', this.isRtl_ ? 1 : -1],
      ['ArrowRight', this.isRtl_ ? -1 : 1],
      ['ArrowUp', -1],
      ['PageDown', 1],
      ['PageUp', -1],
    ]);
    this.buttonEventTracker_ = new EventTracker();

    this.populateBound_ = () => this.populate_();
    this.shadowRoot.querySelector('slot').addEventListener(
        'slotchange', this.populateBound_);

    this.populate_();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.shadowRoot.querySelector('slot').removeEventListener(
        'slotchange', this.populateBound_);
    this.buttonEventTracker_.removeAll();
  }

  /** @override */
  focus() {
    if (this.disabled || !this.buttons_) {
      return;
    }

    const radio =
        this.buttons_.find(radio => this.isButtonEnabledAndSelected_(radio));
    if (radio) {
      radio.focus();
    }
  }

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeyDown_(event) {
    if (this.disabled) {
      return;
    }

    if (event.ctrlKey || event.shiftKey || event.metaKey || event.altKey) {
      return;
    }

    const targetElement = /** @type {!CrRadioButtonElement} */ (event.target);
    if (!this.buttons_.includes(targetElement)) {
      return;
    }

    if (event.key === ' ' || event.key === 'Enter') {
      event.preventDefault();
      this.select_(/** @type {!CrRadioButtonElement} */ (event.target));
      return;
    }

    const enabledRadios = this.buttons_.filter(isEnabled);
    if (enabledRadios.length === 0) {
      return;
    }

    let selectedIndex;
    const max = enabledRadios.length - 1;
    if (event.key === 'Home') {
      selectedIndex = 0;
    } else if (event.key === 'End') {
      selectedIndex = max;
    } else if (this.deltaKeyMap_.has(event.key)) {
      const delta = this.deltaKeyMap_.get(event.key);
      // If nothing selected, start from the first radio then add |delta|.
      const lastSelection = enabledRadios.findIndex(radio => radio.checked);
      selectedIndex = Math.max(0, lastSelection) + delta;
      // Wrap the selection, if needed.
      if (selectedIndex > max) {
        selectedIndex = 0;
      } else if (selectedIndex < 0) {
        selectedIndex = max;
      }
    } else {
      return;
    }

    const radio = enabledRadios[selectedIndex];
    const name = `${radio.name}`;
    if (this.selected !== name) {
      event.preventDefault();
      this.selected = name;
      radio.focus();
    }
  }

  /**
   * @return {!RegExp}
   * @private
   */
  computeSelectableRegExp_() {
    const tags = this.selectableElements.split(', ').join('|');
    return new RegExp(`^(${tags})$`, 'i');
  }

  /**
   * @param {!Event} event
   * @private
   */
  onClick_(event) {
    const path = event.composedPath();
    if (path.some(target => /^a$/i.test(target.tagName))) {
      return;
    }
    const target = /** @type {!CrRadioButtonElement} */ (
        path.find(n => this.selectableRegExp_.test(n.tagName)));
    if (target && this.buttons_.includes(target)) {
      this.select_(/** @type {!CrRadioButtonElement} */ (target));
    }
  }

  /** @private */
  populate_() {
    const nodes =
        this.shadowRoot.querySelector('slot').assignedNodes({flatten: true});
    this.buttons_ = Array.from(nodes).filter(
        node => node.nodeType === Node.ELEMENT_NODE &&
            node.matches(this.selectableElements));
    this.buttonEventTracker_.removeAll();
    this.buttons_.forEach(el => {
      this.buttonEventTracker_.add(
          el, 'disabled-changed', () => this.populate_());
      this.buttonEventTracker_.add(el, 'name-changed', () => this.populate_());
    });
    this.update_();
  }

  /**
   * @param {!CrRadioButtonElement} button
   * @private
   */
  select_(button) {
    if (!isEnabled(button)) {
      return;
    }

    const name = `${button.name}`;
    if (this.selected !== name) {
      this.selected = name;
    }
  }

  /**
   * @param {!Element} button
   * @return {boolean}
   * @private
   */
  isButtonEnabledAndSelected_(button) {
    return !this.disabled && button.checked && isEnabled(button);
  }

  /** @private */
  update_() {
    if (!this.buttons_) {
      return;
    }
    let noneMadeFocusable = true;
    this.buttons_.forEach(radio => {
      radio.checked =
          this.selected !== undefined && `${radio.name}` === `${this.selected}`;
      const disabled = this.disabled || !isEnabled(radio);
      const canBeFocused = radio.checked && !disabled;
      if (canBeFocused) {
        radio.focusable = true;
        noneMadeFocusable = false;
      } else {
        radio.focusable = false;
      }
      radio.setAttribute('aria-disabled', `${disabled}`);
    });
    this.setAttribute('aria-disabled', `${this.disabled}`);
    if (noneMadeFocusable && !this.disabled) {
      const radio = this.buttons_.find(isEnabled);
      if (radio) {
        radio.focusable = true;
      }
    }
  }
}

customElements.define(CrRadioGroupElement.is, CrRadioGroupElement);
