// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-button' is a button which displays slotted elements. It can
 * be interacted with like a normal button using click as well as space and
 * enter to effectively click the button and fire a 'click' event.
 */
Polymer({
  is: 'cr-button',

  behaviors: [
    Polymer.PaperRippleBehavior,
  ],

  properties: {
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'disabledChanged_',
    },
  },

  hostAttributes: {
    'aria-disabled': 'false',
    role: 'button',
    tabindex: 0,
  },

  listeners: {
    click: 'onClick_',
    keydown: 'onKeyDown_',
    keyup: 'onKeyUp_',
    pointerdown: 'onPointerDown_',
    tap: 'onTap_',
  },

  /** @private {Set<number>} */
  timeoutIds_: null,

  /** @override */
  ready: function() {
    cr.ui.FocusOutlineManager.forDocument(document);
    this.timeoutIds_ = new Set();
  },

  /** @override */
  detached: function() {
    this.timeoutIds_.forEach(clearTimeout);
    this.timeoutIds_.clear();
  },

  /**
   * @param {!Function} fn
   * @param {number=} delay
   * @private
   */
  setTimeout_: function(fn, delay) {
    if (!this.isConnected) {
      return;
    }
    const id = setTimeout(() => {
      this.timeoutIds_.delete(id);
      fn();
    }, delay);
    this.timeoutIds_.add(id);
  },

  /**
   * @param {boolean} newValue
   * @param {boolean} oldValue
   * @private
   */
  disabledChanged_: function(newValue, oldValue) {
    if (!newValue && oldValue == undefined) {
      return;
    }
    if (this.disabled) {
      this.blur();
    }
    this.setAttribute('aria-disabled', Boolean(this.disabled));
    this.setAttribute('tabindex', this.disabled ? -1 : 0);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onClick_: function(e) {
    if (this.disabled) {
      e.stopImmediatePropagation();
    }
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

    this.getRipple().uiDownAction();
    if (e.key == 'Enter') {
      this.click();
      // Delay was chosen manually as a good time period for the ripple to be
      // visible.
      this.setTimeout_(() => this.getRipple().uiUpAction(), 100);
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyUp_: function(e) {
    if (e.key != ' ' && e.key != 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    if (e.key == ' ') {
      this.click();
      this.getRipple().uiUpAction();
    }
  },

  /** @private */
  onPointerDown_: function() {
    this.ensureRipple();
  },

  /**
   * Need to handle tap events to enable tap events for where they are still
   * used with |button.addEventListener('tap', handler)|.
   * TODO(crbug.com/812035): Remove function and listener after Chrome OS no
   *     longer uses tap event at least with addEventListener().
   * @private
   */
  onTap_: function() {}
});
