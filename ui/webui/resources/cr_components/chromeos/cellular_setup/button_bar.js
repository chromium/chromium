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
    buttonState: {type: Object, value: {}},

    /**
     * @type {!cellularSetup.Button}
     */
    Button: {
      type: Object,
      value: cellularSetup.Button,
    },
  },

  /**
   * @param {!cellularSetup.Button} buttonName
   * @return {boolean}
   * @private
   */
  isButtonHidden_(buttonName) {
    const state = this.getButtonBarState_(buttonName);
    return state === cellularSetup.ButtonState.HIDDEN;
  },

  /**
   * @param {!cellularSetup.Button} buttonName
   * @return {boolean}
   * @private
   */
  isButtonDisabled_(buttonName) {
    const state = this.getButtonBarState_(buttonName);
    return state === cellularSetup.ButtonState.SHOWN_BUT_DISABLED;
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
  onDoneButtonClicked_() {
    this.fire('complete-flow-requested');
  },

  /** @private */
  onNextButtonClicked_() {
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
      case cellularSetup.Button.DONE:
        return this.buttonState.done;
      case cellularSetup.Button.NEXT:
        return this.buttonState.next;
      case cellularSetup.Button.TRY_AGAIN:
        return this.buttonState.tryAgain;
      default:
        assertNotReached();
        return cellularSetup.ButtonState.SHOWN_AND_ENABLED;
    }
  }
});
