// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(() => {

  /**
   * @param {!Element} radio
   * @return {boolean}
   */
  function isEnabled(radio) {
    return radio.matches(':not([disabled]):not([hidden])') &&
        radio.style.display != 'none' && radio.style.visibility != 'hidden';
  }

  Polymer({
    is: 'cr-radio-group',

    properties: {
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
        value: 'cr-radio-button, controlled-radio-button',
      },

      /**
       * @type {!RegExp}
       * @private
       */
      selectableRegExp_: {
        value: Object,
        computed: 'computeSelectableRegExp_(selectableElements)',
      },
    },

    listeners: {
      keydown: 'onKeyDown_',
      click: 'onClick_',
    },

    hostAttributes: {
      'aria-disabled': 'false',
      role: 'radiogroup',
    },

    /** @private {Array<!Element>} */
    buttons_: null,

    /** @private {EventTracker} */
    buttonEventTracker_: null,

    /** @private {Map<string, number>} */
    deltaKeyMap_: null,

    /** @private {boolean} */
    isRtl_: false,

    /** @private  {PolymerDomApi.ObserveHandle} */
    observer_: null,

    /** @private {Function} */
    populateBound_: null,

    /** @override */
    attached: function() {
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
      // Needed for when the radio buttons change when using dom-repeat or
      // dom-if.
      // TODO(crbug.com/738611): After migration to Polymer 2, remove Polymer 1
      // references.
      if (Polymer.DomIf) {
        this.$$('slot').addEventListener('slotchange', this.populateBound_);
      } else {
        this.observer_ = Polymer.dom(this).observeNodes(this.populateBound_);
      }

      this.populate_();
    },

    /** @override */
    detached: function() {
      if (Polymer.DomIf) {
        this.$$('slot').removeEventListener('slotchange', this.populateBound_);
      } else if (this.observer_) {
        Polymer.dom(this).unobserveNodes(
            /** @type {!PolymerDomApi.ObserveHandle} */ (this.observer_));
      }
      this.buttonEventTracker_.removeAll();
    },

    /** @override */
    focus: function() {
      if (this.disabled || !this.buttons_) {
        return;
      }

      const radio =
          this.buttons_.find(radio => radio.getAttribute('tabindex') == '0');
      if (radio) {
        radio.focus();
      }
    },

    /**
     * @param {!KeyboardEvent} event
     * @private
     */
    onKeyDown_: function(event) {
      if (this.disabled) {
        return;
      }

      if (event.ctrlKey || event.shiftKey || event.metaKey || event.altKey) {
        return;
      }

      const targetElement = /** @type {!Element} */ (event.target);
      if (!this.buttons_.includes(targetElement)) {
        return;
      }

      if (event.key == ' ' || event.key == 'Enter') {
        event.preventDefault();
        this.select_(/** @type {!Element} */ (event.target));
        return;
      }

      const enabledRadios = this.buttons_.filter(isEnabled);
      if (enabledRadios.length == 0) {
        return;
      }

      let selectedIndex;
      const max = enabledRadios.length - 1;
      if (event.key == 'Home') {
        selectedIndex = 0;
      } else if (event.key == 'End') {
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
      if (this.selected != name) {
        event.preventDefault();
        this.selected = name;
        radio.focus();
      }
    },

    /**
     * @return {!RegExp}
     * @private
     */
    computeSelectableRegExp_: function() {
      const tags = this.selectableElements.split(', ').join('|');
      return new RegExp(`^(${tags})$`, 'i');
    },

    /**
     * @param {!Event} event
     * @private
     */
    onClick_: function(event) {
      const path = event.composedPath();
      if (path.some(target => /^a$/i.test(target.tagName))) {
        return;
      }
      const target = /** @type {!Element} */ (
          path.find(n => this.selectableRegExp_.test(n.tagName)));
      if (target && this.buttons_.includes(target)) {
        this.select_(/** @type {!Element} */ (target));
      }
    },

    /** @private */
    populate_: function() {
      // TODO(crbug.com/738611): After migration to Polymer 2, remove
      // Polymer 1 references.
      this.buttons_ = Polymer.DomIf ?
          this.$$('slot')
              .assignedNodes({flatten: true})
              .filter(n => this.selectableRegExp_.test(n.tagName)) :
          this.queryAllEffectiveChildren(this.selectableElements);
      this.buttonEventTracker_.removeAll();
      this.buttons_.forEach(el => {
        this.buttonEventTracker_.add(
            el, 'disabled-changed', () => this.populate_());
        this.buttonEventTracker_.add(
            el, 'name-changed', () => this.populate_());
      });
      this.update_();
    },

    /**
     * @param {!Element} button
     * @private
     */
    select_: function(button) {
      if (!isEnabled(button)) {
        return;
      }

      const name = `${button.name}`;
      if (this.selected != name) {
        this.selected = name;
      }
    },

    /** @private */
    update_: function() {
      if (!this.buttons_) {
        return;
      }
      let noneMadeFocusable = true;
      this.buttons_.forEach(radio => {
        radio.checked = this.selected != undefined &&
            radio.name == this.selected;
        const disabled = this.disabled || !isEnabled(radio);
        const canBeFocused = radio.checked && !disabled;
        noneMadeFocusable &= !canBeFocused;
        radio.setAttribute('tabindex', canBeFocused ? '0' : '-1');
        radio.setAttribute('aria-disabled', `${disabled}`);
      });
      this.setAttribute('aria-disabled', `${this.disabled}`);
      if (noneMadeFocusable && !this.disabled) {
        const focusable = this.buttons_.find(isEnabled);
        if (focusable) {
          focusable.setAttribute('tabindex', '0');
        }
      }
    },
  });
})();
