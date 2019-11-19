// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for cr-radio-button-like elements.
 */

// clang-format off
// #import {PaperRippleBehavior} from 'chrome://resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js'
// clang-format on

/** @polymerBehavior */
const CrRadioButtonBehaviorImpl = {
  properties: {
    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      notify: true,
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
    blur: 'hideRipple_',
    focus: 'onFocus_',
    up: 'hideRipple_',
  },

  /** @private */
  onFocus_: function() {
    this.getRipple().showAndHoldDown();
    this.$.button.focus();
  },

  /** @private */
  hideRipple_: function() {
    this.getRipple().clear();
  },

  /** @private */
  getAriaChecked_: function() {
    return this.checked ? 'true' : 'false';
  },

  /** @private */
  getAriaDisabled_: function() {
    return this.disabled ? 'true' : 'false';
  },

  /**
   * When shift-tab is pressed, first bring the focus to the host element.
   * This accomplishes 2 things:
   * 1) Host doesn't get focused when the browser moves the focus backward.
   * 2) focus now escaped the shadow-dom of this element, so that it'll
   *    correctly obey non-zero tabindex ordering of the containing document.
   * @param {!Event} e
   * @private
   */
  onInputKeydown_: function(e) {
    if (e.shiftKey && e.key === 'Tab') {
      this.focus();
    }
  },

  // customize the element's ripple
  _createRipple: function() {
    this._rippleContainer = this.$$('.disc-wrapper');
    const ripple = Polymer.PaperRippleBehavior._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  },
};


/** @polymerBehavior */
/* #export */ const CrRadioButtonBehavior = [
  Polymer.PaperRippleBehavior,
  CrRadioButtonBehaviorImpl,
];
