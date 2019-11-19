// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-checkbox' is a component similar to native checkbox. It
 * fires a 'change' event *only* when its state changes as a result of a user
 * interaction. By default it assumes there will be child(ren) passed in to be
 * used as labels. If no label will be provided, a .no-label class should be
 * added to hide the spacing between the checkbox and the label container.
 */
Polymer({
  is: 'cr-checkbox',

  behaviors: [
    Polymer.PaperRippleBehavior,
  ],

  properties: {
    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'checkedChanged_',
      notify: true,
    },

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'disabledChanged_',
    },

    ariaDescription: String,

    tabIndex: {
      type: Number,
      value: 0,
      observer: 'onTabIndexChanged_',
    },
  },

  listeners: {
    blur: 'hideRipple_',
    click: 'onClick_',
    focus: 'showRipple_',
    up: 'hideRipple_',
  },

  /** @override */
  ready: function() {
    this.removeAttribute('unresolved');
  },

  focus: function() {
    this.$.checkbox.focus();
  },

  /** @return {!Element} */
  getFocusableElement: function() {
    return this.$.checkbox;
  },

  /** @private */
  checkedChanged_: function() {
    this.$.checkbox.setAttribute(
        'aria-checked', this.checked ? 'true' : 'false');
  },

  /**
   * @param {boolean} current
   * @param {boolean} previous
   * @private
   */
  disabledChanged_: function(current, previous) {
    if (previous === undefined && !this.disabled) {
      return;
    }

    this.tabIndex = this.disabled ? -1 : 0;
    this.$.checkbox.setAttribute(
        'aria-disabled', this.disabled ? 'true' : 'false');
  },

  /** @private */
  showRipple_: function() {
    this.getRipple().showAndHoldDown();
  },

  /** @private */
  hideRipple_: function() {
    this.getRipple().clear();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onClick_: function(e) {
    if (this.disabled || e.target.tagName == 'A') {
      return;
    }

    // Prevent |click| event from bubbling. It can cause parents of this
    // elements to erroneously re-toggle this control.
    e.stopPropagation();
    e.preventDefault();

    this.checked = !this.checked;
    this.fire('change', this.checked);
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_: function(e) {
    if (e.key != ' ' && e.key != 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    if (e.repeat) {
      return;
    }

    if (e.key == 'Enter') {
      this.click();
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyUp_: function(e) {
    if (e.key == ' ' || e.key == 'Enter') {
      e.preventDefault();
      e.stopPropagation();
    }

    if (e.key == ' ') {
      this.click();
    }
  },

  /** @private */
  onTabIndexChanged_: function() {
    // :host shouldn't have a tabindex because it's set on #checkbox.
    this.removeAttribute('tabindex');
  },

  // customize the element's ripple
  _createRipple: function() {
    this._rippleContainer = this.$.checkbox;
    const ripple = Polymer.PaperRippleBehavior._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  },
});
