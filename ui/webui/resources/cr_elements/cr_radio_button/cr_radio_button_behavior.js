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

    /**
     * Whether the radio button should be focusable or not. Toggling this
     * property sets the corresponding tabindex of the button itself as well
     * as any links in the button description.
     */
    focusable: {
      type: Boolean,
      value: false,
      observer: 'onFocusableChanged_',
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

    /**
     * Holds the tabIndex for the radio button.
     * @private {number}
     */
    buttonTabIndex_: {
      type: Number,
      computed: 'getTabIndex_(focusable)',
    },
  },

  listeners: {
    blur: 'hideRipple_',
    focus: 'onFocus_',
    up: 'hideRipple_',
  },

  focus() {
    this.$.button.focus();
  },

  /** @private */
  onFocusableChanged_() {
    const links = this.querySelectorAll('a');
    links.forEach((link) => {
      // Remove the tab stop on any links when the row is unchecked. Since the
      // row is not tabbable, any links within the row should not be either.
      link.tabIndex = this.checked ? 0 : -1;
    });
  },

  /** @private */
  onFocus_() {
    this.getRipple().showAndHoldDown();
  },

  /** @private */
  hideRipple_() {
    this.getRipple().clear();
  },

  /**
   * @return {string}
   * @private
   */
  getAriaChecked_() {
    return this.checked ? 'true' : 'false';
  },

  /**
   * @return {string}
   * @private
   */
  getAriaDisabled_() {
    return this.disabled ? 'true' : 'false';
  },

  /**
   * @return {number}
   * @private
   */
  getTabIndex_() {
    return this.focusable ? 0 : -1;
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
  onInputKeydown_(e) {
    if (e.shiftKey && e.key === 'Tab') {
      this.focus();
    }
  },

  // customize the element's ripple
  _createRipple() {
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
/* #ignore */ console.warn('crbug/1173575, non-JS module files deprecated.');
