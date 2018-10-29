// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Repeat Button.
 *
 * This is for repeat button in Control Panel for Audio Player.
 */
Polymer({
  is: 'repeat-button',

  hostAttributes: {
    role: 'button',
    tabindex: 0,
  },

  behaviors: [
    Polymer.IronButtonState,
    Polymer.IronControlState,
  ],

  properties: {
    'repeatMode': {
      type: String,
      notify: true,
      reflectToAttribute: true,
    }
  },

  listeners: {
    click: '_clickHandler',
  },

  observers: [
    '_focusedChanged(receivedFocusFromKeyboard)',
  ],

  _focusedChanged: function(receivedFocusFromKeyboard) {
    if (receivedFocusFromKeyboard) {
      this.classList.add('keyboard-focus');
    } else {
      this.classList.remove('keyboard-focus');
    }
  },

  /**
   * Initialize member variables.
   */
  created: function() {
    /**
     * @private {Array<string>}
     */
    this.modeName_ = [
      "no-repeat",
      "repeat-all",
      "repeat-one"
    ];
  },

  _clickHandler: function() {
    this.next_();
  },

  /**
   * Change the mode into next one.
   * @private
   */
  next_: function() {
    this.index_ = this.index_ || this.modeName_.indexOf(this.repeatMode);
    if(this.index_ === -1)
      return;

    var nextIndex = (this.index_ + 1) % this.modeName_.length;
    this.repeatMode = this.modeName_[nextIndex];
    this.index_ = nextIndex;
  },

  /**
   * Whether or not the button is active, which means it should be toggled.
   * @param {string} mode Current mode name
   * @return {boolean} True if the mode is repeat.
   */
  isActive: function(mode) {
    return mode === "repeat-all" || mode === "repeat-one";
  },
});
