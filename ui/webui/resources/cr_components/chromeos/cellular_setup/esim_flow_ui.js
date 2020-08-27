// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Root element for the eSIM cellular setup flow. This element interacts with
 * the CellularSetup service to carry out the esim activation flow.
 */
Polymer({
  is: 'esim-flow-ui',

  behaviors: [
    I18nBehavior,
    SubflowBehavior,
  ],

  properties: {
    /**
     * @type {string}
     * @private
     */
    activationCode_: String,
  },

  initSubflow() {
    this.buttonState = {
      backward: cellularSetup.ButtonState.HIDDEN,
      cancel: cellularSetup.ButtonState.SHOWN_AND_ENABLED,
      finish: cellularSetup.ButtonState.HIDDEN,
      next: cellularSetup.ButtonState.SHOWN_AND_ENABLED,
      tryAgain: cellularSetup.ButtonState.HIDDEN
    };
  },
});
