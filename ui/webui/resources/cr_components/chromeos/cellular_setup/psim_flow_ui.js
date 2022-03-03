// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellularSetup', function() {
  /** @enum{string} */
  /* #export */ const PSimPageName = {
    SIM_DETECT: 'simDetectPage',
    PROVISIONING: 'provisioningPage',
    FINAL: 'finalPage',
  };

  /** @enum{string} */
  /* #export */ const PSimUIState = {
    IDLE: 'idle',
    STARTING_ACTIVATION: 'starting-activation',
    WAITING_FOR_ACTIVATION_TO_START: 'waiting-for-activation-to-start',
    TIMEOUT_START_ACTIVATION: 'timeout-start-activation',
    FINAL_TIMEOUT_START_ACTIVATION: 'final-timeout-start-activation',
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
   * The reason that caused the user to exit the PSim Setup flow.
   * These values are persisted to logs. Entries should not be renumbered
   * and numeric values should never be reused.
   * @enum{number}
   */
  /* #export */ const PSimSetupFlowResult = {
    SUCCESS: 0,
    CANCELLED: 1,
    CANCELLED_NO_SIM: 2,
    CANCELLED_COLD_SIM_DEFER: 3,
    CANCELLED_CARRIER_PORTAL: 4,
    CANCELLED_PORTAL_ERROR: 5,
    CARRIER_PORTAL_TIMEOUT: 6,
    NETWORK_ERROR: 7,
  };

  /**
   * @param {!cellularSetup.PSimUIState} state
   * @return {?number} The time delta, in ms, for the timeout corresponding to
   *     |state|. If no timeout is applicable for this state, null is returned.
   */
  function getTimeoutMsForPSimUIState(state) {
    // In some cases, starting activation may require power-cycling the device's
    // modem, a process that can take several seconds.
    if (state === PSimUIState.STARTING_ACTIVATION) {
      return 10000;  // 10 seconds.
    }

    // The portal is a website served by the mobile carrier.
    if (state === PSimUIState.WAITING_FOR_PORTAL_TO_LOAD) {
      return 10000;  // 10 seconds.
    }

    // Finishing activation only requires sending a D-Bus message to Shill.
    if (state === PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH) {
      return 1000;  // 1 second.
    }

    // No other states require timeouts.
    return null;
  }

  /**
   * The maximum tries allowed to detect the SIM.
   * @private {number}
   */
  const MAX_START_ACTIVATION_ATTEMPTS = 3;

  /* #export */ const PSIM_SETUP_RESULT_METRIC_NAME =
      'Network.Cellular.PSim.SetupFlowResult';

  /* #export */ const SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME =
      'Network.Cellular.PSim.CellularSetup.Success.Duration';

  /* #export */ const FAILED_PSIM_SETUP_DURATION_METRIC_NAME =
      'Network.Cellular.PSim.CellularSetup.Failure.Duration';

  /**
   * Root element for the pSIM cellular setup flow. This element interacts with
   * the CellularSetup service to carry out the psim activation flow. It
   * contains navigation buttons and sub-pages corresponding to each step of the
   * flow.
   */
  Polymer({
    is: 'psim-flow-ui',

    behaviors: [
      I18nBehavior,
      SubflowBehavior,
    ],

    properties: {
      /** @type {!cellular_setup.CellularSetupDelegate} */
      delegate: Object,

      /**
       * Carrier name; used in dialog title to show the current carrier
       * name being setup
       * @type {string}
       */
      nameOfCarrierPendingSetup: {
        type: String,
        notify: true,
        computed: 'getCarrierText(' +
            'selectedPSimPageName_, cellularMetadata_.*)',
      },

      forwardButtonLabel: {
        type: String,
        notify: true,
      },

      /**
       * @type {!cellularSetup.PSimUIState}
       * @private
       */
      state_: {
        type: String,
        value: PSimUIState.IDLE,
      },

      /**
       * Element name of the current selected sub-page.
       * @type {!cellularSetup.PSimPageName}
       * @private
       */
      selectedPSimPageName_: {
        type: String,
        value: PSimPageName.SIM_DETECT,
        notify: true,
      },

      /**
       * DOM Element for the current selected sub-page.
       * @private {!SetupLoadingPageElement|!ProvisioningPageElement|
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
       * @private {?ash.cellularSetup.mojom.CellularMetadata}
       */
      cellularMetadata_: {
        type: Object,
        value: null,
      },

      /**
       * The current number of tries to detect the SIM.
       * @private {number}
       */
      startActivationAttempts_: {
        type: Number,
        value: 0,
      },
    },

    observers: [
      'updateShowError_(state_)',
      'updateSelectedPage_(state_)',
      'handlePSimUIStateChange_(state_)',
      'updateButtonBarState_(state_)',
    ],

    /**
     * Provides an interface to the CellularSetup Mojo service.
     * @private {?ash.cellularSetup.mojom.CellularSetupRemote}
     */
    cellularSetupRemote_: null,

    /**
     * Delegate responsible for routing activation started/finished events.
     * @private {?ash.cellularSetup.mojom.ActivationDelegateReceiver}
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
     * @private {?ash.cellularSetup.mojom.CarrierPortalHandlerRemote}
     */
    carrierPortalHandler_: null,

    /**
     * Whether there was a carrier portal error.
     * @private {boolean}
     */
    didCarrierPortalResultFail_: false,

    /**
     * The function used to initiate a timer. Can be overwritten in tests.
     * @private {function(Function, number)}
     */
    setTimeoutFunction_: setTimeout.bind(window),

    /**
     * The time at which the PSim flow is attached.
     * @private {?Date}
     */
    timeOnAttached_: null,

    /** @override */
    created() {
      this.cellularSetupRemote_ = cellular_setup.getCellularSetupRemote();
    },

    /** @override */
    attached() {
      this.timeOnAttached_ = new Date();
    },

    /** @override */
    detached() {
      let resultCode = null;
      switch (this.state_) {
        case PSimUIState.IDLE:
        case PSimUIState.STARTING_ACTIVATION:
          resultCode = PSimSetupFlowResult.CANCELLED;
          break;
        case PSimUIState.WAITING_FOR_ACTIVATION_TO_START:
          resultCode = PSimSetupFlowResult.CANCELLED_COLD_SIM_DEFER;
          break;
        case PSimUIState.TIMEOUT_START_ACTIVATION:
        case PSimUIState.FINAL_TIMEOUT_START_ACTIVATION:
          resultCode = PSimSetupFlowResult.CANCELLED_NO_SIM;
          break;
        case PSimUIState.WAITING_FOR_PORTAL_TO_LOAD:
          resultCode = PSimSetupFlowResult.CANCELLED;
          break;
        case PSimUIState.TIMEOUT_PORTAL_LOAD:
          resultCode = PSimSetupFlowResult.CARRIER_PORTAL_TIMEOUT;
          break;
        case PSimUIState.WAITING_FOR_USER_PAYMENT:
          resultCode = PSimSetupFlowResult.CANCELLED_CARRIER_PORTAL;
          break;
        case PSimUIState.ACTIVATION_SUCCESS:
        case PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH:
        case PSimUIState.TIMEOUT_FINISH_ACTIVATION:
        case PSimUIState.ALREADY_ACTIVATED:
          resultCode = PSimSetupFlowResult.SUCCESS;
          break;
        case PSimUIState.ACTIVATION_FAILURE:
          resultCode = this.didCarrierPortalResultFail_ ?
              PSimSetupFlowResult.CANCELLED_PORTAL_ERROR :
              PSimSetupFlowResult.NETWORK_ERROR;
          break;
        default:
          assertNotReached();
      }

      assert(resultCode !== null);
      chrome.metricsPrivate.recordEnumerationValue(
          PSIM_SETUP_RESULT_METRIC_NAME, resultCode,
          Object.keys(PSimSetupFlowResult).length);

      const elapsedTimeMs = new Date() - this.timeOnAttached_;
      if (resultCode === PSimSetupFlowResult.SUCCESS) {
        chrome.metricsPrivate.recordLongTime(
            SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
        return;
      }

      chrome.metricsPrivate.recordLongTime(
          FAILED_PSIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
    },

    /**
     * Overrides ash.cellularSetup.mojom.ActivationDelegateInterface.
     * @param {!ash.cellularSetup.mojom.CellularMetadata} metadata
     * @private
     */
    onActivationStarted(metadata) {
      this.clearTimer_();
      this.cellularMetadata_ = metadata;
      this.state_ = PSimUIState.WAITING_FOR_PORTAL_TO_LOAD;
    },

    initSubflow() {
      this.state_ = PSimUIState.STARTING_ACTIVATION;
      this.startActivationAttempts_ = 0;
      this.updateButtonBarState_();
      this.fire('focus-default-button');
    },

    navigateForward() {
      switch (this.state_) {
        case PSimUIState.WAITING_FOR_PORTAL_TO_LOAD:
        case PSimUIState.TIMEOUT_PORTAL_LOAD:
        case PSimUIState.WAITING_FOR_USER_PAYMENT:
        case PSimUIState.ACTIVATION_SUCCESS:
          this.state_ = PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH;
          break;
        case PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH:
        case PSimUIState.TIMEOUT_FINISH_ACTIVATION:
        case PSimUIState.FINAL_TIMEOUT_START_ACTIVATION:
        case PSimUIState.ALREADY_ACTIVATED:
          this.fire('exit-cellular-setup');
          break;
        case PSimUIState.TIMEOUT_START_ACTIVATION:
          this.state_ = PSimUIState.STARTING_ACTIVATION;
          break;
        default:
          assertNotReached();
          break;
      }
    },

    /**
     * Sets the function used to initiate a timer.
     * @param {function(Function, number)}
     *     timerFunction
     */
    setTimerFunctionForTest(timerFunction) {
      this.setTimeoutFunction_ = timerFunction;
    },

    /** @private */
    updateButtonBarState_() {
      let buttonState;
      switch (this.state_) {
        case PSimUIState.IDLE:
        case PSimUIState.STARTING_ACTIVATION:
        case PSimUIState.WAITING_FOR_ACTIVATION_TO_START:
        case PSimUIState.WAITING_FOR_PORTAL_TO_LOAD:
        case PSimUIState.TIMEOUT_PORTAL_LOAD:
        case PSimUIState.WAITING_FOR_USER_PAYMENT:
          this.forwardButtonLabel = this.i18n('next');
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cellularSetup.ButtonState.ENABLED,
            forward: cellularSetup.ButtonState.DISABLED,
          };
          break;
        case PSimUIState.TIMEOUT_START_ACTIVATION:
          this.forwardButtonLabel = this.i18n('tryAgain');
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cellularSetup.ButtonState.ENABLED,
            forward: cellularSetup.ButtonState.ENABLED,
          };
          break;
        case PSimUIState.ACTIVATION_SUCCESS:
          this.forwardButtonLabel = this.i18n('next');
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cellularSetup.ButtonState.ENABLED,
            forward: cellularSetup.ButtonState.ENABLED,
          };
          break;
        case PSimUIState.ALREADY_ACTIVATED:
        case PSimUIState.ACTIVATION_FAILURE:
        case PSimUIState.FINAL_TIMEOUT_START_ACTIVATION:
          this.forwardButtonLabel = this.i18n('done');
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cellularSetup.ButtonState.ENABLED,
            forward: cellularSetup.ButtonState.ENABLED,
          };
          break;
        case PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH:
        case PSimUIState.TIMEOUT_FINISH_ACTIVATION:
          this.forwardButtonLabel = this.i18n('done');
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cellularSetup.ButtonState.HIDDEN,
            forward: cellularSetup.ButtonState.ENABLED,
          };
          break;
        default:
          assertNotReached();
      }
      this.set('buttonState', buttonState);
    },

    /**
     * Overrides ash.cellularSetup.mojom.ActivationDelegateInterface.
     * @param {!ash.cellularSetup.mojom.ActivationResult} result
     * @private
     */
    onActivationFinished(result) {
      this.closeActivationConnection_();

      const ActivationResult = ash.cellularSetup.mojom.ActivationResult;
      switch (result) {
        case ActivationResult.kSuccessfullyStartedActivation:
          this.state_ = PSimUIState.ACTIVATION_SUCCESS;
          break;
        case ActivationResult.kAlreadyActivated:
          this.state_ = PSimUIState.ALREADY_ACTIVATED;
          break;
        case ActivationResult.kFailedToActivate:
          this.state_ = PSimUIState.ACTIVATION_FAILURE;
          break;
        default:
          assertNotReached();
      }
    },

    /** @private */
    getCarrierText() {
      if (this.selectedPSimPageName_ === PSimPageName.PROVISIONING &&
          this.cellularMetadata_) {
        return this.cellularMetadata_.carrier;
      }
      return '';
    },

    /** @private */
    updateShowError_() {
      switch (this.state_) {
        case PSimUIState.TIMEOUT_PORTAL_LOAD:
        case PSimUIState.TIMEOUT_FINISH_ACTIVATION:
        case PSimUIState.ACTIVATION_FAILURE:
          this.showError_ = true;
          return;
        default:
          this.showError_ = false;
          return;
      }
    },

    /** @private */
    updateSelectedPage_() {
      switch (this.state_) {
        case PSimUIState.IDLE:
        case PSimUIState.STARTING_ACTIVATION:
        case PSimUIState.WAITING_FOR_ACTIVATION_TO_START:
        case PSimUIState.TIMEOUT_START_ACTIVATION:
        case PSimUIState.FINAL_TIMEOUT_START_ACTIVATION:
          this.selectedPSimPageName_ = PSimPageName.SIM_DETECT;
          return;
        case PSimUIState.WAITING_FOR_PORTAL_TO_LOAD:
        case PSimUIState.TIMEOUT_PORTAL_LOAD:
        case PSimUIState.WAITING_FOR_USER_PAYMENT:
        case PSimUIState.ACTIVATION_SUCCESS:
          this.selectedPSimPageName_ = PSimPageName.PROVISIONING;
          return;
        case PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH:
        case PSimUIState.TIMEOUT_FINISH_ACTIVATION:
        case PSimUIState.ALREADY_ACTIVATED:
        case PSimUIState.ACTIVATION_FAILURE:
          this.selectedPSimPageName_ = PSimPageName.FINAL;
          return;
        default:
          assertNotReached();
      }
    },

    /** @private */
    handlePSimUIStateChange_() {
      // Since the state has changed, the previous state did not time out, so
      // clear any active timeout.
      this.clearTimer_();

      // If the new state has an associated timeout, set it.
      const timeoutMs = getTimeoutMsForPSimUIState(this.state_);
      if (timeoutMs !== null) {
        this.currentTimeoutId_ =
            this.setTimeoutFunction_(this.onTimeout_.bind(this), timeoutMs);
      }

      if (this.state_ === PSimUIState.STARTING_ACTIVATION) {
        this.startActivation_();
        return;
      }
    },

    /** @private */
    onTimeout_() {
      // The activation attempt failed, so close the connection to the service.
      this.closeActivationConnection_();

      switch (this.state_) {
        case PSimUIState.STARTING_ACTIVATION:
          this.startActivationAttempts_++;
          if (this.startActivationAttempts_ < MAX_START_ACTIVATION_ATTEMPTS) {
            this.state_ = PSimUIState.TIMEOUT_START_ACTIVATION;
          } else {
            this.state_ = PSimUIState.FINAL_TIMEOUT_START_ACTIVATION;
          }
          return;
        case PSimUIState.WAITING_FOR_PORTAL_TO_LOAD:
          this.state_ = PSimUIState.TIMEOUT_PORTAL_LOAD;
          return;
        case PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH:
          this.state_ = PSimUIState.TIMEOUT_FINISH_ACTIVATION;
          return;
        default:
          // Only the above states are expected to time out.
          assertNotReached();
      }
    },

    /** @private */
    startActivation_() {
      assert(!this.activationDelegateReceiver_);
      this.activationDelegateReceiver_ =
          new ash.cellularSetup.mojom.ActivationDelegateReceiver(
              /**
               * @type {!ash.cellularSetup.mojom.ActivationDelegateInterface}
               */
              (this));

      this.cellularSetupRemote_
          .startActivation(
              this.activationDelegateReceiver_.$.bindNewPipeAndPassRemote())
          .then(
              /**
               * @param {!ash.cellularSetup.
               *             mojom.CellularSetup_StartActivation_ResponseParams}
               *                 params
               */
              (params) => {
                this.carrierPortalHandler_ = params.observer;
              });
    },

    /** @private */
    closeActivationConnection_() {
      assert(!!this.activationDelegateReceiver_);
      this.activationDelegateReceiver_.$.close();
      this.activationDelegateReceiver_ = null;
      this.carrierPortalHandler_ = null;
      this.cellularMetadata_ = null;
    },

    /** @private */
    clearTimer_() {
      if (this.currentTimeoutId_) {
        clearTimeout(this.currentTimeoutId_);
      }
      this.currentTimeoutId_ = null;
    },

    /** @private */
    onCarrierPortalLoaded_() {
      this.state_ = PSimUIState.WAITING_FOR_USER_PAYMENT;
      this.carrierPortalHandler_.onCarrierPortalStatusChange(
          ash.cellularSetup.mojom.CarrierPortalStatus
              .kPortalLoadedWithoutPaidUser);
    },

    /**
     * @param {!CustomEvent<boolean>} event
     * @private
     */
    onCarrierPortalResult_(event) {
      const success = event.detail;
      this.didCarrierPortalResultFail_ = !success;
      this.state_ = success ? PSimUIState.ACTIVATION_SUCCESS :
                              PSimUIState.ACTIVATION_FAILURE;
    },

    /** @return {string} */
    getLoadingMessage_() {
      if (this.state_ === PSimUIState.TIMEOUT_START_ACTIVATION) {
        return this.i18n('simDetectPageErrorMessage');
      } else if (this.state_ === PSimUIState.FINAL_TIMEOUT_START_ACTIVATION) {
        return this.i18n('simDetectPageFinalErrorMessage');
      }
      return this.i18n('establishNetworkConnectionMessage');
    },

    /** @return {boolean} */
    isSimDetectError_() {
      return this.state_ === PSimUIState.TIMEOUT_START_ACTIVATION ||
          this.state_ === PSimUIState.FINAL_TIMEOUT_START_ACTIVATION;
    },

    /** @return {string} */
    getLoadingTitle_() {
      if (this.delegate.shouldShowPageTitle() && this.isSimDetectError_()) {
        return this.i18n('simDetectPageErrorTitle');
      }
      return '';
    },
  });

  // #cr_define_end
  return {
    PSimPageName: PSimPageName,
    PSimUIState: PSimUIState,
    getTimeoutMsForPSimUIState: getTimeoutMsForPSimUIState
  };
});
