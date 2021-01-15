// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Cellular setup selection subflow. This element allows user to select
 * between eSim and pSim setup subflows.
 */
Polymer({
  is: 'setup-selection-flow',

  behaviors: [
    I18nBehavior,
    SubflowBehavior,
  ],

  properties: {
    /**
     * Element name of the current selected sub-page.
     * @type {!cellularSetup.CellularSetupPageName}
     */
    selectedPage: {
      type: String,
      notify: true,
    },

    forwardButtonLabel: {
      type: String,
      notify: true,
    }
  },

  initSubflow() {
    this.updateButtonState_(this.selectedPage);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSetupFlowRadioSelectedChange_(event) {
    this.selectedPage = event.detail.value;
    this.updateButtonState_(this.selectedPage);
  },

  /**
   * @param {!cellularSetup.CellularSetupPageName} selectedPage
   * @private
   */
  updateButtonState_(selectedPage) {
    this.forwardButtonLabel = this.i18n('next');
    this.buttonState = {
      cancel: cellularSetup.ButtonState.ENABLED,
    };
    if (selectedPage === cellularSetup.CellularSetupPageName.PSIM_FLOW_UI ||
        selectedPage === cellularSetup.CellularSetupPageName.ESIM_FLOW_UI) {
      this.set('buttonState.forward', cellularSetup.ButtonState.ENABLED);
    } else {
      this.set('buttonState.forward', cellularSetup.ButtonState.DISABLED);
    }
  }
});