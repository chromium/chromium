// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellularSetup', function() {
  /** @enum{string} */
  const PageName = {
    SIM_DETECT: 'sim-detect-page',
    PROVISIONING: 'provisioning-page',
    FINAL: 'final-page',
  };

  /** @enum{string} */
  const State = {
    IDLE: 'idle',
    STARTING_ACTIVATION: 'starting-activation',
    WAITING_FOR_ACTIVATION_TO_START: 'waiting-for-activation-to-start',
    TIMEOUT_START_ACTIVATION: 'timeout-start-activation',
    WAITING_FOR_PORTAL_TO_LOAD: 'waiting-for-portal-to-load',
    TIMEOUT_PORTAL_LOAD: 'timeout-portal-load',
    WAITING_FOR_USER_PAYMENT: 'waiting-for-user-payment',
    WAITING_FOR_ACTIVATION_TO_FINISH: 'waiting-for-activation-to-finish',
    TIMEOUT_FINISH_ACTIVATION: 'timeout-finish-activation',
    ACTIVATION_SUCCESS: 'activation-success',
    ALREADY_ACTIVATED: 'already-activated',
    ACTIVATION_FAILURE: 'activation-failure',
  };

  /**
   * @param {!cellularSetup.State} state
   * @return {?number} The time delta, in ms, for the timeout corresponding to
   *     |state|. If no timeout is applicable for this state, null is returned.
   */
  function getTimeoutMsForState(state) {
    // In some cases, starting activation may require power-cycling the device's
    // modem, a process that can take several seconds.
    if (state === State.STARTING_ACTIVATION) {
      return 10000;  // 10 seconds.
    }

    // The portal is a website served by the mobile carrier.
    if (state === State.WAITING_FOR_PORTAL_TO_LOAD) {
      return 10000;  // 10 seconds.
    }

    // Finishing activation only requires sending a D-Bus message to Shill.
    if (state === State.WAITING_FOR_ACTIVATION_TO_FINISH) {
      return 1000;  // 1 second.
    }

    // No other states require timeouts.
    return null;
  }

  return {
    PageName: PageName,
    State: State,
    getTimeoutMsForState: getTimeoutMsForState
  };
});

/**
 * Root element for the cellular setup flow. This element interacts with the
 * CellularSetup service to carry out the activation flow. It contains
 * navigation buttons and sub-pages corresponding to each step of the flow.
 */
Polymer({
  is: 'cellular-setup',

  behaviors: [I18nBehavior],

  properties: {
    /** @private {!cellularSetup.State} */
    state_: {
      type: String,
      value: cellularSetup.State.IDLE,
    },

    /**
     * Element name of the current selected sub-page.
     * @private {!cellularSetup.PageName}
     */
    selectedPageName_: {
      type: String,
      value: cellularSetup.PageName.SIM_DETECT,
      notify: true,
    },

    /**
     * DOM Element for the current selected sub-page.
     * @private {!SimDetectPageElement|!ProvisioningPageElement|
     *           !FinalPageElement}
     */
    selectedPage_: Object,

    /**
     * Whether error state should be shown for the current page.
     * @private {boolean}
     */
    showError_: {type: Boolean, value: false},

    /**
     * Cellular metadata received via the onActivationStarted() callback. If
     * that callback has not occurred, this field is null.
     * @private {?chromeos.cellularSetup.mojom.CellularMetadata}
     */
    cellularMetadata_: {
      type: Object,
      value: null,
    },

    /**
     * Whether try again should be shown in the button bar.
     * @private {boolean}
     */
    showTryAgainButton_: {type: Boolean, value: false},

    /**
     * Whether finish button should be shown in the button bar.
     * @private {boolean}
     */
    showFinishButton_: {type: Boolean, value: false},

    /**
     * Whether cancel button should be shown in the button bar.
     * @private {boolean}
     */
    showCancelButton_: {type: Boolean, value: false}
  },

  observers: [
    'updateShowError_(state_)',
    'updateSelectedPage_(state_)',
    'handleStateChange_(state_)',
  ],

  listeners: {
    'backward-nav-requested': 'onBackwardNavRequested_',
    'retry-requested': 'onRetryRequested_',
    'complete-flow-requested': 'onCompleteFlowRequested_',
  },

  /**
   * Provides an interface to the CellularSetup Mojo service.
   * @private {?cellular_setup.MojoInterfaceProvider}
   */
  mojoInterfaceProvider_: null,

  /**
   * Delegate responsible for routing activation started/finished events.
   * @private {?chromeos.cellularSetup.mojom.ActivationDelegateReceiver}
   */
  activationDelegateReceiver_: null,

  /**
   * The timeout ID corresponding to a timeout for the current state. If no
   * timeout is active, this value is null.
   * @private {?number}
   */
  currentTimeoutId_: null,

  /**
   * Handler used to communicate state updates back to the CellularSetup
   * service.
   * @private {?chromeos.cellularSetup.mojom.CarrierPortalHandlerRemote}
   */
  carrierPortalHandler_: null,

  /** @override */
  created: function() {
    this.mojoInterfaceProvider_ =
        cellular_setup.MojoInterfaceProviderImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.state_ = cellularSetup.State.STARTING_ACTIVATION;
  },

  /**
   * Overrides chromeos.cellularSetup.mojom.ActivationDelegateInterface.
   * @param {!chromeos.cellularSetup.mojom.CellularMetadata} metadata
   * @private
   */
  onActivationStarted(metadata) {
    this.clearTimer_();
    this.cellularMetadata_ = metadata;
    this.state_ = cellularSetup.State.WAITING_FOR_PORTAL_TO_LOAD;
  },

  /**
   * Overrides chromeos.cellularSetup.mojom.ActivationDelegateInterface.
   * @param {!chromeos.cellularSetup.mojom.ActivationResult} result
   * @private
   */
  onActivationFinished(result) {
    this.closeActivationConnection_();

    const ActivationResult = chromeos.cellularSetup.mojom.ActivationResult;
    switch (result) {
      case ActivationResult.kSuccessfullyStartedActivation:
        this.state_ = cellularSetup.State.ALREADY_ACTIVATED;
        break;
      case ActivationResult.kAlreadyActivated:
        this.state_ = cellularSetup.State.ACTIVATION_SUCCESS;
        break;
      case ActivationResult.kFailedToActivate:
        this.state_ = cellularSetup.State.ACTIVATION_FAILURE;
        break;
      default:
        assertNotReached();
    }
  },

  /** @private */
  updateShowError_: function() {
    switch (this.state_) {
      case cellularSetup.State.TIMEOUT_START_ACTIVATION:
      case cellularSetup.State.TIMEOUT_PORTAL_LOAD:
      case cellularSetup.State.TIMEOUT_FINISH_ACTIVATION:
      case cellularSetup.State.ACTIVATION_FAILURE:
        this.showError_ = true;
        return;
      default:
        this.showError_ = false;
        return;
    }
  },

  /** @private */
  updateSelectedPage_: function() {
    switch (this.state_) {
      case cellularSetup.State.IDLE:
      case cellularSetup.State.STARTING_ACTIVATION:
      case cellularSetup.State.WAITING_FOR_ACTIVATION_TO_START:
      case cellularSetup.State.TIMEOUT_START_ACTIVATION:
        this.selectedPageName_ = cellularSetup.PageName.SIM_DETECT;
        return;
      case cellularSetup.State.WAITING_FOR_PORTAL_TO_LOAD:
      case cellularSetup.State.TIMEOUT_PORTAL_LOAD:
      case cellularSetup.State.WAITING_FOR_USER_PAYMENT:
        this.selectedPageName_ = cellularSetup.PageName.PROVISIONING;
        return;
      case cellularSetup.State.WAITING_FOR_ACTIVATION_TO_FINISH:
      case cellularSetup.State.TIMEOUT_FINISH_ACTIVATION:
      case cellularSetup.State.ACTIVATION_SUCCESS:
      case cellularSetup.State.ALREADY_ACTIVATED:
      case cellularSetup.State.ACTIVATION_FAILURE:
        this.selectedPageName_ = cellularSetup.PageName.FINAL;
        return;
      default:
        assertNotReached();
    }
  },

  /** @private */
  handleStateChange_: function() {
    // Since the state has changed, the previous state did not time out, so
    // clear any active timeout.
    this.clearTimer_();

    // If the new state has an associated timeout, set it.
    const timeoutMs = cellularSetup.getTimeoutMsForState(this.state_);
    if (timeoutMs !== null) {
      this.currentTimeoutId_ =
          setTimeout(this.onTimeout_.bind(this), timeoutMs);
    }

    if (this.state_ === cellularSetup.State.STARTING_ACTIVATION) {
      this.startActivation_();
      return;
    }
  },

  /** @private */
  onTimeout_: function() {
    // The activation attempt failed, so close the connection to the service.
    this.closeActivationConnection_();

    switch (this.state_) {
      case cellularSetup.State.STARTING_ACTIVATION:
        this.state_ = cellularSetup.State.TIMEOUT_START_ACTIVATION;
        return;
      case cellularSetup.State.WAITING_FOR_PORTAL_TO_LOAD:
        this.state_ = cellularSetup.State.TIMEOUT_PORTAL_LOAD;
        return;
      case cellularSetup.State.WAITING_FOR_ACTIVATION_TO_FINISH:
        this.state_ = cellularSetup.State.TIMEOUT_FINISH_ACTIVATION;
        return;
      default:
        // Only the above states are expected to time out.
        assertNotReached();
    }
  },

  /** @private */
  startActivation_: function() {
    assert(!this.activationDelegateReceiver_);
    this.activationDelegateReceiver_ =
        new chromeos.cellularSetup.mojom.ActivationDelegateReceiver(
            /**
             * @type {!chromeos.cellularSetup.mojom.ActivationDelegateInterface}
             */
            (this));

    this.mojoInterfaceProvider_.getMojoServiceRemote()
        .startActivation(
            this.activationDelegateReceiver_.$.bindNewPipeAndPassRemote())
        .then(
            /**
             * @param {!chromeos.cellularSetup.
             *             mojom.CellularSetup_StartActivation_ResponseParams}
             *                 params
             */
            (params) => {
              this.carrierPortalHandler_ = params.observer;
            });
  },

  /** @private */
  closeActivationConnection_: function() {
    assert(!!this.activationDelegateReceiver_);
    this.activationDelegateReceiver_.$.close();
    this.activationDelegateReceiver_ = null;
    this.carrierPortalHandler_ = null;
    this.cellularMetadata_ = null;
  },

  /** @private */
  clearTimer_: function() {
    if (this.currentTimeoutId_) {
      clearTimeout(this.currentTimeoutId_);
    }
    this.currentTimeoutId_ = null;
  },

  /** @private */
  onCarrierPortalLoaded_: function() {
    this.state_ = cellularSetup.State.WAITING_FOR_USER_PAYMENT;
    this.carrierPortalHandler_.onCarrierPortalStatusChange(
        chromeos.cellularSetup.mojom.CarrierPortalStatus
            .kPortalLoadedWithoutPaidUser);
  },

  /**
   * @param {!CustomEvent<boolean>} event
   * @private
   */
  onCarrierPortalResult_: function(event) {
    const success = event.detail;
    this.state_ = success ? cellularSetup.State.ACTIVATION_SUCCESS :
                            cellularSetup.State.ACTIVATION_FAILURE;
  },

  /** @private */
  onBackwardNavRequested_: function() {
    // TODO(azeemarshad): Add back navigation.
  },

  /** @private */
  onRetryRequested_: function() {
    // TODO(azeemarshad): Add try again logic.
  },

  /** @private */
  onCompleteFlowRequested__: function() {
    // TODO(azeemarshad): Add completion logic.
  },
});
