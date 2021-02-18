// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Element containing navigation buttons for the Cellular Setup flow. */
Polymer({
  is: 'button-bar',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * Sets the states of all buttons
     * @type {!cellularSetup.ButtonBarState}
     */
    buttonState: {
      type: Object,
      value: {},
    },

    /**
     * @type {!cellularSetup.Button}
     */
    Button: {
      type: Object,
      value: cellularSetup.Button,
    },

    forwardButtonLabel: {
      type: String,
      value: '',
    }
  },

  /**
   * @param {!cellularSetup.Button} buttonName
   * @return {boolean}
   * @private
   */
  isButtonHidden_(buttonName) {
    return !this.getButtonBarState_(buttonName);
  },

  /**
   * @param {!cellularSetup.Button} buttonName
   * @return {boolean}
   * @private
   */
  isButtonDisabled_(buttonName) {
    const state = this.getButtonBarState_(buttonName);
    return state === cellularSetup.ButtonState.DISABLED;
  },

  focusDefaultButton() {
    const buttons = this.shadowRoot.querySelectorAll('cr-button');
    // Focus the first non-disabled, non-hidden button from the end.
    for (let i = buttons.length - 1; i >= 0; i--) {
      const button = buttons.item(i);
      if (!button.disabled && !button.hidden) {
        cr.ui.focusWithoutInk(button);
        return;
      }
    }
  },

  /** @private */
  onBackwardButtonClicked_() {
    this.fire('backward-nav-requested');
  },

  /** @private */
  onCancelButtonClicked_() {
    this.fire('cancel-requested');
  },

  /** @private */
  onTryAgainButtonClicked_() {
    this.fire('retry-requested');
  },

  /** @private */
  onForwardButtonClicked_() {
    this.fire('forward-nav-requested');
  },

  /**
   * @param {!cellularSetup.Button} button
   * @returns {!cellularSetup.ButtonState|undefined}
   * @private
   */
  getButtonBarState_(button) {
    assert(this.buttonState);
    switch (button) {
      case cellularSetup.Button.BACKWARD:
        return this.buttonState.backward;
      case cellularSetup.Button.CANCEL:
        return this.buttonState.cancel;
      case cellularSetup.Button.FORWARD:
        return this.buttonState.forward;
      case cellularSetup.Button.TRY_AGAIN:
        return this.buttonState.tryAgain;
      default:
        assertNotReached();
        return cellularSetup.ButtonState.ENABLED;
    }
  }
});
