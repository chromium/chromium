// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellular_setup', function() {
  /** @enum{string} */
  /* #export */ const ESimPageName = {
    PROFILE_LOADING: 'profileLoadingPage',
    PROFILE_DISCOVERY: 'profileDiscoveryPage',
    ACTIVATION_CODE: 'activationCodePage',
    ACTIVATION_VERIFCATION: 'activationVerificationPage',
    CONFIRMATION_CODE: 'confirmationCodePage',
    FINAL: 'finalPage',
  };

  /** @enum{string} */
  /* #export */ const ESimUiState = {
    PROFILE_SEARCH: 'profile-search',
    ACTIVATION_CODE_ENTRY: 'activation-code-entry',
    ACTIVATION_CODE_ENTRY_READY: 'activation-code-entry-ready',
    ACTIVATION_CODE_ENTRY_INSTALLING: 'activation-code-entry-installing',
    CONFIRMATION_CODE_ENTRY: 'confirmation-code-entry',
    CONFIRMATION_CODE_ENTRY_READY: 'confirmation-code-entry-ready',
    CONFIRMATION_CODE_ENTRY_INSTALLING: 'confirmation-code-entry-installing',
    PROFILE_SELECTION: 'profile-selection',
    PROFILE_SELECTION_INSTALLING: 'profile-selection-installing',
    SETUP_FINISH: 'setup-finish',
  };

  /**
   * The reason that caused the user to exit the ESim Setup flow.
   * These values are persisted to logs. Entries should not be renumbered
   * and numeric values should never be reused.
   * @enum{number}
   */
  /* #export */ const ESimSetupFlowResult = {
    SUCCESS: 0,
    INSTALL_FAIL: 1,
    CANCELLED_NEEDS_CONFIRMATION_CODE: 2,
    CANCELLED_INVALID_ACTIVATION_CODE: 3,
    ERROR_FETCHING_PROFILES: 4,
    CANCELLED_WITHOUT_ERROR: 5,
    CANCELLED_NO_PROFILES: 6,
    NO_NETWORK: 7,
  };

  /* #export */ const ESIM_SETUP_RESULT_METRIC_NAME =
      'Network.Cellular.ESim.SetupFlowResult';

  /* #export */ const SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME =
      'Network.Cellular.ESim.CellularSetup.Success.Duration';

  /* #export */ const FAILED_ESIM_SETUP_DURATION_METRIC_NAME =
      'Network.Cellular.ESim.CellularSetup.Failure.Duration';

  /**
   * Root element for the eSIM cellular setup flow. This element interacts with
   * the CellularSetup service to carry out the esim activation flow.
   */
  Polymer({
    is: 'esim-flow-ui',

    behaviors: [
      I18nBehavior,
      NetworkListenerBehavior,
      SubflowBehavior,
    ],

    properties: {
      /** @type {!cellular_setup.CellularSetupDelegate} */
      delegate: Object,

      /**
       * Header shown at the top of the flow. No header shown if the string is
       * empty.
       */
      header: {
        type: String,
        notify: true,
        computed: 'computeHeader_(selectedESimPageName_, showError_)',
      },

      forwardButtonLabel: {
        type: String,
        notify: true,
      },

      /**
       * @type {!cellular_setup.ESimUiState}
       * @private
       */
      state_: {
        type: String,
        value: ESimUiState.PROFILE_SEARCH,
        observer: 'onStateChanged_',
      },

      /**
       * Element name of the current selected sub-page.
       * This is set in updateSelectedPage_ on initialization.
       * @type {?cellular_setup.ESimPageName}
       * @private
       */
      selectedESimPageName_: String,

      /**
       * Whether error state should be shown for the current page.
       * @private {boolean}
       */
      showError_: {
        type: Boolean,
        value: false,
      },

      /**
       * Profiles fetched that have status kPending.
       * @type {!Array<!ash.cellularSetup.mojom.ESimProfileRemote>}
       * @private
       */
      pendingProfiles_: {
        type: Array,
      },

      /**
       * Profile selected to be installed.
       * @type {?ash.cellularSetup.mojom.ESimProfileRemote}
       * @private
       */
      selectedProfile_: {
        type: Object,
      },

      /** @private */
      activationCode_: {
        type: String,
        value: '',
      },

      /** @private */
      confirmationCode_: {
        type: String,
        value: '',
        observer: 'onConfirmationCodeUpdated_',
      },

      /** @private */
      hasHadActiveCellularNetwork_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      isActivationCodeFromQrCode_: {
        type: Boolean,
      },
    },

    /**
     * Provides an interface to the ESimManager Mojo service.
     * @private {?ash.cellularSetup.mojom.ESimManagerRemote}
     */
    eSimManagerRemote_: null,

    /** @private {?ash.cellularSetup.mojom.EuiccRemote} */
    euicc_: null,

    /** @private {boolean} */
    hasFailedFetchingProfiles_: false,

    /** @private {?ash.cellularSetup.mojom.ProfileInstallResult} */
    lastProfileInstallResult_: null,

    /**
     * If there are no active network connections of any type.
     * @private {boolean}
     */
    isOffline_: false,

    /**
     * The time at which the ESim flow is attached.
     * @private {?Date}
     */
    timeOnAttached_: null,

    listeners: {
      'activation-code-updated': 'onActivationCodeUpdated_',
      'forward-navigation-requested': 'onForwardNavigationRequested_',
    },

    observers: ['onSelectedProfileChanged_(selectedProfile_)'],

    /** @override */
    created() {
      this.eSimManagerRemote_ = cellular_setup.getESimManagerRemote();
      const networkConfig =
          network_config.MojoInterfaceProviderImpl.getInstance()
              .getMojoServiceRemote();

      const filter = {
        filter: chromeos.networkConfig.mojom.FilterType.kActive,
        limit: chromeos.networkConfig.mojom.NO_LIMIT,
        networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
      };
      networkConfig.getNetworkStateList(filter).then(response => {
        this.onActiveNetworksChanged(response.result);
      });
    },

    /** @override */
    attached() {
      this.timeOnAttached_ = new Date();
    },

    /** @override */
    detached() {
      let resultCode = null;

      const ProfileInstallResult = ash.cellularSetup.mojom.ProfileInstallResult;

      switch (this.lastProfileInstallResult_) {
        case ProfileInstallResult.kSuccess:
          resultCode = ESimSetupFlowResult.SUCCESS;
          break;
        case ProfileInstallResult.kFailure:
          resultCode = ESimSetupFlowResult.INSTALL_FAIL;
          break;
        case ProfileInstallResult.kErrorNeedsConfirmationCode:
          resultCode = ESimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE;
          break;
        case ProfileInstallResult.kErrorInvalidActivationCode:
          resultCode = ESimSetupFlowResult.CANCELLED_INVALID_ACTIVATION_CODE;
          break;
        default:
          // Handles case when no profile installation was attempted.
          if (this.hasFailedFetchingProfiles_) {
            resultCode = ESimSetupFlowResult.ERROR_FETCHING_PROFILES;
          } else if (this.pendingProfiles_ && !this.pendingProfiles_.length) {
            resultCode = ESimSetupFlowResult.CANCELLED_NO_PROFILES;
          } else {
            resultCode = ESimSetupFlowResult.CANCELLED_WITHOUT_ERROR;
          }
          break;
      }

      if (this.isOffline_ && resultCode !== ProfileInstallResult.kSuccess) {
        resultCode = ESimSetupFlowResult.NO_NETWORK;
      }

      assert(resultCode !== null);
      chrome.metricsPrivate.recordEnumerationValue(
          ESIM_SETUP_RESULT_METRIC_NAME, resultCode,
          Object.keys(ESimSetupFlowResult).length);

      const elapsedTimeMs = new Date() - this.timeOnAttached_;
      if (resultCode === ESimSetupFlowResult.SUCCESS) {
        chrome.metricsPrivate.recordLongTime(
            SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
        return;
      }

      chrome.metricsPrivate.recordLongTime(
          FAILED_ESIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
    },

    /**
     * NetworkListenerBehavior override
     * Used to determine if there is an online network connection.
     * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
     *     activeNetworks
     */
    onActiveNetworksChanged(activeNetworks) {
      this.isOffline_ = !activeNetworks.some(
          (network) => network.connectionState ===
              chromeos.networkConfig.mojom.ConnectionStateType.kOnline);
    },

    initSubflow() {
      this.fetchProfiles_();
      this.onNetworkStateListChanged();
    },

    /** @private */
    async fetchProfiles_() {
      const euicc = await cellular_setup.getEuicc();
      if (!euicc) {
        this.hasFailedFetchingProfiles_ = true;
        this.showError_ = true;
        this.state_ = ESimUiState.SETUP_FINISH;
        console.warn('No Euiccs found');
        return;
      }
      this.euicc_ = euicc;
      const requestPendingProfilesResponse =
          await euicc.requestPendingProfiles();
      if (requestPendingProfilesResponse.result ===
          ash.cellularSetup.mojom.ESimOperationResult.kFailure) {
        this.hasFailedFetchingProfiles_ = true;
        console.warn(
            'Error requesting pending profiles: ',
            requestPendingProfilesResponse);
      }
      this.pendingProfiles_ =
          await cellular_setup.getPendingESimProfiles(euicc);
      switch (this.pendingProfiles_.length) {
        case 0:
          this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
          break;
        case 1:
          this.selectedProfile_ = this.pendingProfiles_[0];
          // Assume installing the profile doesn't require a confirmation
          // code, send an empty string.
          this.selectedProfile_.installProfile('').then(
              this.handleProfileInstallResponse_.bind(this));
          break;
        default:
          this.state_ = ESimUiState.PROFILE_SELECTION;
          break;
      }
    },

    /**
     * @private
     * @param {{result: ash.cellularSetup.mojom.ProfileInstallResult}} response
     */
    handleProfileInstallResponse_(response) {
      this.lastProfileInstallResult_ = response.result;
      if (response.result ===
          ash.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode) {
        this.state_ = ESimUiState.CONFIRMATION_CODE_ENTRY;
        return;
      }
      this.showError_ = response.result !==
          ash.cellularSetup.mojom.ProfileInstallResult.kSuccess;
      if (response.result ===
              ash.cellularSetup.mojom.ProfileInstallResult.kFailure &&
          this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING) {
        this.state_ = ESimUiState.CONFIRMATION_CODE_ENTRY_READY;
        return;
      }
      if (response.result ===
          ash.cellularSetup.mojom.ProfileInstallResult
              .kErrorInvalidActivationCode) {
        this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY_READY;
        return;
      }
      if (response.result ===
              ash.cellularSetup.mojom.ProfileInstallResult.kSuccess ||
          response.result ===
              ash.cellularSetup.mojom.ProfileInstallResult.kFailure) {
        this.state_ = ESimUiState.SETUP_FINISH;
      }
    },

    /** @private */
    onStateChanged_(newState, oldState) {
      this.updateButtonBarState_();
      this.updateSelectedPage_();
      this.initializePageState_(newState, oldState);
    },

    /** @private */
    updateSelectedPage_() {
      const oldSelectedESimPageName = this.selectedESimPageName_;
      switch (this.state_) {
        case ESimUiState.PROFILE_SEARCH:
          this.selectedESimPageName_ = ESimPageName.PROFILE_LOADING;
          break;
        case ESimUiState.ACTIVATION_CODE_ENTRY:
        case ESimUiState.ACTIVATION_CODE_ENTRY_READY:
          this.selectedESimPageName_ = ESimPageName.ACTIVATION_CODE;
          break;
        case ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING:
          this.selectedESimPageName_ = ESimPageName.ACTIVATION_VERIFCATION;
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY:
        case ESimUiState.CONFIRMATION_CODE_ENTRY_READY:
        case ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING:
          this.selectedESimPageName_ = ESimPageName.CONFIRMATION_CODE;
          break;
        case ESimUiState.PROFILE_SELECTION:
        case ESimUiState.PROFILE_SELECTION_INSTALLING:
          this.selectedESimPageName_ = ESimPageName.PROFILE_DISCOVERY;
          break;
        case ESimUiState.SETUP_FINISH:
          this.selectedESimPageName_ = ESimPageName.FINAL;
          break;
        default:
          assertNotReached();
          break;
      }
      // If there is a page change, fire focus event.
      if (oldSelectedESimPageName !== this.selectedESimPageName_) {
        this.fire('focus-default-button');
      }
    },

    /**
     * @param {boolean} enableForwardBtn
     * @param {!cellularSetup.ButtonState} cancelButtonStateIfEnabled
     * @param {boolean} isInstalling
     * @return {!cellularSetup.ButtonBarState}
     * @private
     */
    generateButtonStateForActivationPage_(
        enableForwardBtn, cancelButtonStateIfEnabled, isInstalling) {
      this.forwardButtonLabel = this.i18n('next');
      let backBtnState = cellularSetup.ButtonState.HIDDEN;
      if (this.pendingProfiles_.length > 1) {
        backBtnState = isInstalling ? cellularSetup.ButtonState.DISABLED :
                                      cellularSetup.ButtonState.ENABLED;
      }
      return {
        backward: backBtnState,
        cancel: cancelButtonStateIfEnabled,
        forward: enableForwardBtn ? cellularSetup.ButtonState.ENABLED :
                                    cellularSetup.ButtonState.DISABLED,
      };
    },

    /**
     * @param {boolean} enableForwardBtn
     * @param {!cellularSetup.ButtonState} cancelButtonStateIfEnabled
     * @param {boolean} isInstalling
     * @return {!cellularSetup.ButtonBarState}
     * @private
     */
    generateButtonStateForConfirmationPage_(
        enableForwardBtn, cancelButtonStateIfEnabled, isInstalling) {
      this.forwardButtonLabel = this.i18n('confirm');
      let backBtnState = cellularSetup.ButtonState.ENABLED;
      if (this.pendingProfiles_.length === 1) {
        backBtnState = cellularSetup.ButtonState.HIDDEN;
      } else if (isInstalling) {
        backBtnState = cellularSetup.ButtonState.DISABLED;
      }
      return {
        backward: backBtnState,
        cancel: cancelButtonStateIfEnabled,
        forward: enableForwardBtn ? cellularSetup.ButtonState.ENABLED :
                                    cellularSetup.ButtonState.DISABLED,
      };
    },

    /** @private */
    updateButtonBarState_() {
      let buttonState;
      const cancelButtonStateIfEnabled =
          this.delegate.shouldShowCancelButton() ?
          cellularSetup.ButtonState.ENABLED :
          cellularSetup.ButtonState.HIDDEN;
      const cancelButtonStateIfDisabled =
          this.delegate.shouldShowCancelButton() ?
          cellularSetup.ButtonState.DISABLED :
          cellularSetup.ButtonState.HIDDEN;
      switch (this.state_) {
        case ESimUiState.PROFILE_SEARCH:
          this.forwardButtonLabel = this.i18n('next');
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cancelButtonStateIfEnabled,
            forward: cellularSetup.ButtonState.DISABLED,
          };
          break;
        case ESimUiState.ACTIVATION_CODE_ENTRY:
          buttonState = this.generateButtonStateForActivationPage_(
              /*enableForwardBtn*/ false, cancelButtonStateIfEnabled,
              /*isInstalling*/ false);
          break;
        case ESimUiState.ACTIVATION_CODE_ENTRY_READY:
          buttonState = this.generateButtonStateForActivationPage_(
              /*enableForwardBtn*/ true, cancelButtonStateIfEnabled,
              /*isInstalling*/ false);
          break;
        case ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING:
          buttonState = this.generateButtonStateForActivationPage_(
              /*enableForwardBtn*/ false, cancelButtonStateIfDisabled,
              /*isInstalling*/ true);
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY:
          buttonState = this.generateButtonStateForConfirmationPage_(
              /*enableForwardBtn*/ false, cancelButtonStateIfEnabled,
              /*isInstalling*/ false);
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY_READY:
          buttonState = this.generateButtonStateForConfirmationPage_(
              /*enableForwardBtn*/ true, cancelButtonStateIfEnabled,
              /*isInstalling*/ false);
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING:
          buttonState = this.generateButtonStateForConfirmationPage_(
              /*enableForwardBtn*/ false, cancelButtonStateIfDisabled,
              /*isInstalling*/ true);
          break;
        case ESimUiState.PROFILE_SELECTION:
          this.forwardButtonLabel = this.selectedProfile_ ?
              this.i18n('next') :
              this.i18n('skipDiscovery');
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cancelButtonStateIfEnabled,
            forward: cellularSetup.ButtonState.ENABLED,
          };
          break;
        case ESimUiState.PROFILE_SELECTION_INSTALLING:
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cancelButtonStateIfDisabled,
            forward: cellularSetup.ButtonState.DISABLED,
          };
          break;
        case ESimUiState.SETUP_FINISH:
          this.forwardButtonLabel = this.i18n('done');
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cellularSetup.ButtonState.HIDDEN,
            forward: cellularSetup.ButtonState.ENABLED,
          };
          break;
        default:
          assertNotReached();
          break;
      }
      this.set('buttonState', buttonState);
    },

    /** @private */
    initializePageState_(newState, oldState) {
      if (newState === ESimUiState.CONFIRMATION_CODE_ENTRY &&
          oldState !== ESimUiState.CONFIRMATION_CODE_ENTRY_READY) {
        this.confirmationCode_ = '';
      }
      if (newState === ESimUiState.ACTIVATION_CODE_ENTRY &&
          oldState !== ESimUiState.ACTIVATION_CODE_ENTRY_READY) {
        this.activationCode_ = '';
      }
    },

    /** @private */
    onActivationCodeUpdated_(event) {
      // initializePageState_() may cause this observer to fire and update the
      // buttonState when we're not on the activation code page. Check we're on
      // the activation code page before proceeding.
      if (this.state_ !== ESimUiState.ACTIVATION_CODE_ENTRY &&
          this.state_ !== ESimUiState.ACTIVATION_CODE_ENTRY_READY) {
        return;
      }
      this.state_ = event.detail.activationCode ?
          ESimUiState.ACTIVATION_CODE_ENTRY_READY :
          ESimUiState.ACTIVATION_CODE_ENTRY;
    },

    /** @private */
    onSelectedProfileChanged_() {
      // initializePageState_() may cause this observer to fire and update the
      // buttonState when we're not on the profile selection page. Check we're
      // on the profile selection page before proceeding.
      if (this.state_ !== ESimUiState.PROFILE_SELECTION) {
        return;
      }
      this.forwardButtonLabel = this.selectedProfile_ ?
          this.i18n('next') :
          this.i18n('skipDiscovery');
    },

    /** @private */
    onConfirmationCodeUpdated_() {
      // initializePageState_() may cause this observer to fire and update the
      // buttonState when we're not on the confirmation code page. Check we're
      // on the confirmation code page before proceeding.
      if (this.state_ !== ESimUiState.CONFIRMATION_CODE_ENTRY &&
          this.state_ !== ESimUiState.CONFIRMATION_CODE_ENTRY_READY) {
        return;
      }
      this.state_ = this.confirmationCode_ ?
          ESimUiState.CONFIRMATION_CODE_ENTRY_READY :
          ESimUiState.CONFIRMATION_CODE_ENTRY;
    },

    /** SubflowBehavior override */
    navigateForward() {
      this.showError_ = false;
      switch (this.state_) {
        case ESimUiState.ACTIVATION_CODE_ENTRY_READY:
          // Assume installing the profile doesn't require a confirmation
          // code, send an empty string.
          this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING;
          this.euicc_
              .installProfileFromActivationCode(
                  this.activationCode_, /*confirmationCode=*/ '',
                  this.isActivationCodeFromQrCode_)
              .then(this.handleProfileInstallResponse_.bind(this));
          break;
        case ESimUiState.PROFILE_SELECTION:
          if (this.selectedProfile_) {
            this.state_ = ESimUiState.PROFILE_SELECTION_INSTALLING;
            // Assume installing the profile doesn't require a confirmation
            // code, send an empty string.
            this.selectedProfile_.installProfile('').then(
                this.handleProfileInstallResponse_.bind(this));
          } else {
            this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
          }
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY_READY:
          this.state_ = ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING;
          if (this.selectedProfile_) {
            this.selectedProfile_.installProfile(this.confirmationCode_)
                .then(this.handleProfileInstallResponse_.bind(this));
          } else {
            this.euicc_
                .installProfileFromActivationCode(
                    this.activationCode_, this.confirmationCode_,
                    this.isActivationCodeFromQrCode_)
                .then(this.handleProfileInstallResponse_.bind(this));
          }
          break;
        case ESimUiState.SETUP_FINISH:
          this.fire('exit-cellular-setup');
          break;
        default:
          assertNotReached();
          break;
      }
    },

    /** SubflowBehavior override */
    navigateBackward() {
      if ((this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY ||
           this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_READY) &&
          this.pendingProfiles_.length > 1) {
        this.state_ = ESimUiState.PROFILE_SELECTION;
        return;
      }

      if (this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY ||
          this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_READY) {
        if (this.activationCode_) {
          this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY_READY;
          return;
        } else if (this.pendingProfiles_.length > 1) {
          this.state_ = ESimUiState.PROFILE_SELECTION;
          return;
        }
      }
      console.error(
          'Navigate backward faled for : ' + this.state_ +
          ' this state does not support backward navigation.');
      assertNotReached();
    },

    /** @private */
    onForwardNavigationRequested_() {
      if (this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_READY ||
          this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_READY) {
        this.navigateForward();
      }
    },

    /** NetworkListenerBehavior override */
    onNetworkStateListChanged() {
      hasActiveCellularNetwork().then((hasActive) => {
        // If hasHadActiveCellularNetwork_ has been set to true, don't set to
        // false again as we should show the cellular disconnect warning for the
        // duration of the flow's lifecycle.
        if (hasActive) {
          this.hasHadActiveCellularNetwork_ = hasActive;
        }
      });
    },

    /** @private */
    shouldShowSubpageBusy_() {
      return this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING ||
          this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING ||
          this.state_ === ESimUiState.PROFILE_SELECTION_INSTALLING;
    },

    /** @private */
    getLoadingMessage_() {
      return this.hasHadActiveCellularNetwork_ ?
          this.i18n('eSimProfileDetectDuringActiveCellularConnectionMessage') :
          this.i18n('eSimProfileDetectMessage');
    },

    /**
     * @return {string}
     * @private
     */
    computeHeader_() {
      if (this.selectedESimPageName_ === ESimPageName.FINAL &&
          !this.showError_) {
        return this.i18n('eSimFinalPageSuccessHeader');
      }

      return '';
    },
  });

  // #cr_define_end
  return {ESimPageName: ESimPageName, ESimUiState: ESimUiState};
});
