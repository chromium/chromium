// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellular_setup', function() {
  /** @enum{string} */
  /* #export */ const ESimPageName = {
    PROFILE_LOADING: 'profileLoadingPage',
    PROFILE_DISCOVERY: 'profileDiscoveryPage',
    ACTIVATION_CODE: 'activationCodePage',
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
       * @type {!Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>}
       * @private
       */
      pendingProfiles_: {
        type: Array,
      },

      /**
       * Profile selected to be installed.
       * @type {?chromeos.cellularSetup.mojom.ESimProfileRemote}
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
      hasActivePSimNetwork_: {
        type: Boolean,
        value: false,
      },
    },

    /**
     * Provides an interface to the ESimManager Mojo service.
     * @private {?chromeos.cellularSetup.mojom.ESimManagerRemote}
     */
    eSimManagerRemote_: null,

    /** @private {?chromeos.cellularSetup.mojom.EuiccRemote} */
    euicc_: null,

    listeners: {
      'activation-code-updated': 'onActivationCodeUpdated_',
    },

    observers: ['onSelectedProfileChanged_(selectedProfile_)'],

    /** @override */
    created() {
      this.eSimManagerRemote_ = cellular_setup.getESimManagerRemote();
    },

    initSubflow() {
      this.fetchProfiles_();
      this.onNetworkStateListChanged();
    },

    /** @private */
    async fetchProfiles_() {
      const euicc = await cellular_setup.getEuicc();
      if (!euicc) {
        // TODO(crbug.com/1093185) User should have at least 1 EUICC or
        // we shouldn't have gotten to this flow. Add check for this in
        // cellular_setup.
        console.error('No Euiccs found');
        return;
      }
      this.euicc_ = euicc;
      const requestPendingProfilesResponse =
          await euicc.requestPendingProfiles();
      if (requestPendingProfilesResponse.result ===
          chromeos.cellularSetup.mojom.ESimOperationResult.kFailure) {
        console.error(
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
     * @param {{result: chromeos.cellularSetup.mojom.ProfileInstallResult}}
     *     response
     */
    handleProfileInstallResponse_(response) {
      if (response.result ===
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode) {
        this.state_ = ESimUiState.CONFIRMATION_CODE_ENTRY;
        return;
      }
      this.showError_ = response.result !==
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess;
      if (response.result ===
              chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure &&
          this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING) {
        this.state_ = ESimUiState.CONFIRMATION_CODE_ENTRY_READY;
        return;
      }
      if (response.result ===
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorInvalidActivationCode) {
        this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY_READY;
        return;
      }
      if (response.result ===
              chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess ||
          response.result ===
              chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure) {
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
        case ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING:
          this.selectedESimPageName_ = ESimPageName.ACTIVATION_CODE;
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

    /** @private */
    updateButtonBarState_() {
      let buttonState;
      const cancelButtonStateIfEnabled =
          this.delegate.shouldShowCancelButton() ?
          cellularSetup.ButtonState.ENABLED :
          undefined;
      switch (this.state_) {
        case ESimUiState.PROFILE_SEARCH:
        case ESimUiState.ACTIVATION_CODE_ENTRY:
          this.forwardButtonLabel = this.i18n('next');
          buttonState = {
            backward: cellularSetup.ButtonState.ENABLED,
            cancel: cancelButtonStateIfEnabled,
            forward: cellularSetup.ButtonState.DISABLED,
          };
          break;
        case ESimUiState.ACTIVATION_CODE_ENTRY_READY:
          this.forwardButtonLabel = this.i18n('next');
          buttonState = {
            backward: cellularSetup.ButtonState.ENABLED,
            cancel: cancelButtonStateIfEnabled,
            forward: cellularSetup.ButtonState.ENABLED,
          };
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY:
          this.forwardButtonLabel = this.i18n('confirm');
          buttonState = {
            backward: cellularSetup.ButtonState.ENABLED,
            cancel: cancelButtonStateIfEnabled,
            forward: cellularSetup.ButtonState.DISABLED,
          };
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY_READY:
          this.forwardButtonLabel = this.i18n('confirm');
          buttonState = {
            backward: cellularSetup.ButtonState.ENABLED,
            cancel: cancelButtonStateIfEnabled,
            forward: cellularSetup.ButtonState.ENABLED,
          };
          break;
        case ESimUiState.PROFILE_SELECTION:
          this.forwardButtonLabel = this.selectedProfile_ ?
              this.i18n('next') :
              this.i18n('skipDiscovery');
          buttonState = {
            backward: cellularSetup.ButtonState.ENABLED,
            cancel: cancelButtonStateIfEnabled,
            forward: cellularSetup.ButtonState.ENABLED,
          };
          break;
        case ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING:
        case ESimUiState.PROFILE_SELECTION_INSTALLING:
        case ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING:
          buttonState = {
            backward: cellularSetup.ButtonState.DISABLED,
            cancel: cancelButtonStateIfEnabled,
            forward: cellularSetup.ButtonState.DISABLED,
          };
          break;
        case ESimUiState.SETUP_FINISH:
          this.forwardButtonLabel = this.i18n('done');
          buttonState = {
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
                  this.activationCode_, /*confirmationCode=*/ '')
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
                    this.activationCode_, this.confirmationCode_)
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

    /**
     * @returns {boolean} true if backward navigation was handled
     * SubflowBehavior override
     */
    attemptBackwardNavigation() {
      if ((this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY ||
           this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_READY) &&
          this.pendingProfiles_.length > 1) {
        this.state_ = ESimUiState.PROFILE_SELECTION;
        return true;
      } else if (
          this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY ||
          this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_READY) {
        if (this.activationCode_) {
          this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY_READY;
        } else if (this.pendingProfiles_.length > 1) {
          this.state_ = ESimUiState.PROFILE_SELECTION;
        } else {
          return false;
        }
        return true;
      }
      return false;
    },

    /** @private */
    getShowNoProfilesMessage_() {
      return !(this.pendingProfiles_ && this.pendingProfiles_.length > 0);
    },

    /** NetworkListenerBehavior override */
    onNetworkStateListChanged() {
      hasActivePSimNetwork().then((hasActive) => {
        this.hasActivePSimNetwork_ = hasActive;
      });
    },

    /** @private */
    shouldShowSubpageBusy_() {
      return this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING ||
          this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING ||
          this.state_ === ESimUiState.PROFILE_SELECTION_INSTALLING;
    },

    /**
     * @param {boolean} hasActivePSimNetwork
     * @private
     */
    getLoadingPageState_(hasActivePSimNetwork) {
      return hasActivePSimNetwork ?
          LoadingPageState.CELLULAR_DISCONNECT_WARNING :
          LoadingPageState.LOADING;
    },
  });

  // #cr_define_end
  return {ESimPageName: ESimPageName, ESimUiState: ESimUiState};
});