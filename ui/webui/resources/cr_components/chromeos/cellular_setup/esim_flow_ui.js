// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellular_setup', function() {
  /** @enum{string} */
  /* #export */ const ESimPageName = {
    ESIM: 'qr-code-page',
    FINAL: 'final-page',
  };
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
      /** @type {!cellular_setup.CellularSetupDelegate} */
      delegate: Object,

      /**
       * @type {string}
       * @private
       */
      activationCode_: {
        type: String,
        value: '',
        observer: 'onActivationCodeChanged_',
      },

      /**
       * Element name of the current selected sub-page.
       * @type {!cellular_setup.ESimPageName}
       * @private
       */
      selectedESimPageName_: {
        type: String,
        value: ESimPageName.ESIM,
      },

      /**
       * Whether error state should be shown for the current page.
       * @private {boolean}
       */
      showError_: {
        type: Boolean,
        value: false,
      },
    },

    initSubflow() {
      this.buttonState = {
        backward: cellularSetup.ButtonState.SHOWN_AND_ENABLED,
        cancel: this.delegate.shouldShowCancelButton() ?
            cellularSetup.ButtonState.SHOWN_AND_ENABLED :
            cellularSetup.ButtonState.HIDDEN,
        done: cellularSetup.ButtonState.HIDDEN,
        next: cellularSetup.ButtonState.SHOWN_BUT_DISABLED,
        tryAgain: cellularSetup.ButtonState.HIDDEN
      };
    },

    navigateForward() {
      this.selectedESimPageName_ = ESimPageName.FINAL;
    },

    /**
     * @returns {boolean} true if backward navigation was handled
     */
    attemptBackwardNavigation() {
      // TODO(crbug.com/1093185): Handle state when camera is used
      return false;
    },

    /** @private */
    onActivationCodeChanged_() {
      if (!this.activationCode_) {
        this.buttonState.next = cellularSetup.ButtonState.SHOWN_BUT_DISABLED;
        return;
      }

      this.buttonState.next = cellularSetup.ButtonState.SHOWN_AND_ENABLED;
    }
  });

  // #cr_define_end
  return {ESimPageName: ESimPageName};
});