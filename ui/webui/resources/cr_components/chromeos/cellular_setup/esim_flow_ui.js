// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellular_setup', function() {
  /** @enum{string} */
  /* #export */ const ESimPageName = {
    ESIM: 'activationCodePage',
    FINAL: 'finalPage',
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

    listeners: {'activation-code-updated': 'onActivationCodeUpdated_'},

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

    onActivationCodeUpdated_(event) {
      if (event.detail.activationCode) {
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_AND_ENABLED);
      } else {
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_BUT_DISABLED);
      }
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
  });

  // #cr_define_end
  return {ESimPageName: ESimPageName};
});