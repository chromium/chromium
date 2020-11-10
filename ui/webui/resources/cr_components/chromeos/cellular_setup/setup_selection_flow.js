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
     * @private {!cellularSetup.CellularSetupPageName}
     */
    selectedPage: {
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
    this.buttonState = {
      backward: cellularSetup.ButtonState.HIDDEN,
      cancel: cellularSetup.ButtonState.SHOWN_AND_ENABLED,
      done: cellularSetup.ButtonState.HIDDEN,
      tryAgain: cellularSetup.ButtonState.HIDDEN,
      skipDiscovery: cellularSetup.ButtonState.HIDDEN,
    };
    if (selectedPage === cellularSetup.CellularSetupPageName.PSIM_FLOW_UI ||
        selectedPage === cellularSetup.CellularSetupPageName.ESIM_FLOW_UI) {
      this.buttonState.next = cellularSetup.ButtonState.SHOWN_AND_ENABLED;
    } else {
      this.buttonState.next = cellularSetup.ButtonState.SHOWN_BUT_DISABLED;
    }
  }
});