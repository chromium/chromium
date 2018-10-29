// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for cr-radio-button-like elements.
 */

/** @polymerBehavior */
var CrRadioButtonBehaviorImpl = {
  properties: {
    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'checkedChanged_',
    },

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      notify: true,
      observer: 'disabledChanged_',
    },

    label: {
      type: String,
      value: '',  // Allows the hidden$= binding to run without being set.
    },

    name: {
      type: String,
      notify: true,
      reflectToAttribute: true,
    },
  },

  listeners: {
    blur: 'cancelRipple_',
    focus: 'onFocus_',
    pointerup: 'cancelRipple_',
  },

  hostAttributes: {
    'aria-disabled': 'false',
    'aria-checked': 'false',
    role: 'radio',
  },

  /** @private */
  checkedChanged_: function() {
    this.setAttribute('aria-checked', this.checked ? 'true' : 'false');
  },

  /**
   * @param {boolean} current
   * @param {boolean} previous
   * @private
   */
  disabledChanged_: function(current, previous) {
    if (previous === undefined && !this.disabled)
      return;

    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
  },

  /** @private */
  onFocus_: function() {
    this.ensureRipple();
    this.$$('paper-ripple').holdDown = true;
  },

  /** @private */
  cancelRipple_: function() {
    this.ensureRipple();
    this.$$('paper-ripple').holdDown = false;
  },

  // customize the element's ripple
  _createRipple: function() {
    this._rippleContainer = this.$$('.disc-wrapper');
    let ripple = Polymer.PaperRippleBehavior._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  },
};


/** @polymerBehavior */
const CrRadioButtonBehavior = [
  Polymer.PaperRippleBehavior,
  CrRadioButtonBehaviorImpl,
];