// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Root element for the cellular setup flow. This element wraps
 * the psim setup flow, esim setup flow, and setup flow selection page.
 */
Polymer({
  is: 'cellular-setup',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!cellular_setup.CellularSetupDelegate} */
    delegate: Object,

    flowTitle: {
      type: String,
      notify: true,
    },

    /**
     * Name of the currently displayed sub-page.
     * @private {!cellularSetup.CellularSetupPageName|null}
     */
    currentPageName: String,

    /**
     * Current user selected setup flow page name.
     * @private {!cellularSetup.CellularSetupPageName|null}
     */
    selectedFlow_: {
      type: String,
      value: null,
    },

    /**
     * Button bar button state.
     * @private {!cellularSetup.ButtonBarState}
     */
    buttonState_: {
      type: Object,
      notify: true,
    },

    /**
     * DOM Element corresponding to the visible page.
     *
     * @private {!SetupSelectionFlowElement|!PsimFlowUiElement|
     *           !EsimFlowUiElement}
     */
    currentPage_: {
      type: Object,
      observer: 'onPageChange_',
    },

    /**
     * Text for the button_bar's 'Forward' button.
     * @private {string}
     */
    forwardButtonLabel_: {
      type: String,
    }
  },

  listeners: {
    'backward-nav-requested': 'onBackwardNavRequested_',
    'retry-requested': 'onRetryRequested_',
    'forward-nav-requested': 'onForwardNavRequested_',
    'cancel-requested': 'onCancelRequested_',
    'focus-default-button': 'onFocusDefaultButton_',
  },


  /** @override */
  attached() {
    if (!this.currentPageName) {
      this.setCurrentPage_();
    }
  },

  /**
   * Sets current cellular setup flow, one of eSIM flow, pSIM flow or
   * selection flow, depending on available pSIM and eSIM slots.
   * @private
   */
  setCurrentPage_() {
    const networkConfig = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
    networkConfig.getDeviceStateList().then(response => {
      const deviceStateList = response.result;

      const deviceState = deviceStateList.find(
          (device) => device.type ===
              chromeos.networkConfig.mojom.NetworkType.kCellular);

      if (!deviceState) {
        this.currentPageName =
            cellularSetup.CellularSetupPageName.SETUP_FLOW_SELECTION;
        return;
      }

      const {pSimSlots, eSimSlots} = getSimSlotCount(deviceState);

      if (pSimSlots > 0 && eSimSlots === 0) {
        this.currentPageName = cellularSetup.CellularSetupPageName.PSIM_FLOW_UI;
        return;
      } else if (pSimSlots === 0 && eSimSlots > 0) {
        this.currentPageName = cellularSetup.CellularSetupPageName.ESIM_FLOW_UI;
        return;
      }
      this.currentPageName =
          cellularSetup.CellularSetupPageName.SETUP_FLOW_SELECTION;
    });
  },

  /** @private */
  onPageChange_() {
    if (this.currentPage_) {
      this.flowTitle = '';
      this.currentPage_.initSubflow();
    }
  },

  /** @private */
  onBackwardNavRequested_() {
    const isNavHandled = this.currentPage_.attemptBackwardNavigation();

    // Subflow returns false in a state where it cannot perform backward
    // navigation any more. Switch back to the selection flow in this case so
    // that the user can select a flow again.
    if (!isNavHandled) {
      this.currentPageName =
          cellularSetup.CellularSetupPageName.SETUP_FLOW_SELECTION;
    }
  },

  onCancelRequested_() {
    this.fire('exit-cellular-setup');
  },

  /** @private */
  onRetryRequested_() {
    // TODO(crbug.com/1093185): Add try again logic.
  },

  /** @private */
  onForwardNavRequested_() {
    // Switch current page to user selected flow when navigating forward from
    // setup selection.
    if (this.currentPageName ===
        cellularSetup.CellularSetupPageName.SETUP_FLOW_SELECTION) {
      this.currentPageName = this.selectedFlow_;
      return;
    }
    this.currentPage_.navigateForward();
  },

  /** @private */
  onFocusDefaultButton_() {
    this.$.buttonBar.focusDefaultButton();
  },

  /**
   * @param {string} currentPage
   * @private
   */
  isPSimSelected_(currentPage) {
    return currentPage === cellularSetup.CellularSetupPageName.PSIM_FLOW_UI;
  },

  /**
   * @param {string} currentPage
   * @private
   */
  isESimSelected_(currentPage) {
    return currentPage === cellularSetup.CellularSetupPageName.ESIM_FLOW_UI;
  }
});
