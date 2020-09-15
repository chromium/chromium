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
    this.buttonState = {
      backward: cellularSetup.ButtonState.HIDDEN,
      cancel: cellularSetup.ButtonState.SHOWN_AND_ENABLED,
      done: cellularSetup.ButtonState.HIDDEN,
      next: cellularSetup.ButtonState.SHOWN_BUT_DISABLED,
      tryAgain: cellularSetup.ButtonState.HIDDEN
    };
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSetupFlowRadioSelectedChange_(event) {
    switch (event.detail.value) {
      case cellularSetup.CellularSetupPageName.PSIM_FLOW_UI:
        this.selectedPage = cellularSetup.CellularSetupPageName.PSIM_FLOW_UI;
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_AND_ENABLED);
        break;
      case cellularSetup.CellularSetupPageName.ESIM_FLOW_UI:
        this.selectedPage = cellularSetup.CellularSetupPageName.ESIM_FLOW_UI;
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_AND_ENABLED);
        break;
      default:
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_BUT_DISABLED);
    }
  },
});