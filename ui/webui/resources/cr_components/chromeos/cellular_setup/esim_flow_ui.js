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
    CONFIRMATION_CODE_ENTRY: 'confirmation-code-entry',
    PROFILE_SELECTION: 'profile-selection',
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
      SubflowBehavior,
    ],

    properties: {
      /** @type {!cellular_setup.CellularSetupDelegate} */
      delegate: Object,

      /**
       * @type {!cellular_setup.ESimUiState}
       * @private
       */
      state_: {
        type: String,
        value: ESimUiState.PROFILE_SEARCH,
      },

      /**
       * Element name of the current selected sub-page.
       * @type {!cellular_setup.ESimPageName}
       * @private
       */
      selectedESimPageName_: {
        type: String,
        value: ESimPageName.PROFILE_LOADING,
      },

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

    observers: [
      'updateSelectedPage_(state_)', 'updateButtonBarState_(state_)',
      'onSelectedProfileChanged_(selectedProfile_)'
    ],

    /** @override */
    created() {
      this.eSimManagerRemote_ = cellular_setup.getESimManagerRemote();
    },

    initSubflow() {
      this.fetchProfiles_();
    },

    /** @private */
    fetchProfiles_() {
      let euicc;
      this.eSimManagerRemote_.getAvailableEuiccs()
          .then(response => {
            // TODO(crbug.com/1093185) User should have at least 1 EUICC or
            // we shouldn't have gotten to this flow. Add check for this in
            // cellular_setup.
            euicc = response.euiccs[0];
            this.euicc_ = euicc;
            return euicc.requestPendingProfiles();
          })
          .then(response => {
            if (response.result ===
                chromeos.cellularSetup.mojom.ESimOperationResult.kFailure) {
              console.error('Error requesting pending profiles: ' + response);
            }
            return cellular_setup.getPendingESimProfiles(euicc);
          })
          .then(profiles => {
            this.pendingProfiles_ = profiles;
            switch (profiles.length) {
              case 0:
                this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
                break;
              case 1:
                this.selectedProfile_ = profiles[0];
                // Assume installing the profile doesn't require a confirmation
                // code, send an empty string.
                this.selectedProfile_.installProfile('').then(
                    this.handleProfileInstallResponse_.bind(this));
                break;
              default:
                this.state_ = ESimUiState.PROFILE_SELECTION;
                break;
            }
          });
    },

    /**
     * @private
     * @param {{result: chromeos.cellularSetup.mojom.ProfileInstallResult}}
     *     response
     */
    handleProfileInstallResponse_(response) {
      // TODO(crbug.com/1093185) Handle error during confirmation code page.
      this.showError_ = response.result !==
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess;
      if (response.result ===
              chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess ||
          response.result ===
              chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure) {
        this.state_ = ESimUiState.SETUP_FINISH;
      } else if (
          response.result ===
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode) {
        this.state_ = ESimUiState.CONFIRMATION_CODE_ENTRY;
      }
    },

    /** @private */
    updateSelectedPage_() {
      switch (this.state_) {
        case ESimUiState.PROFILE_SEARCH:
          this.selectedESimPageName_ = ESimPageName.PROFILE_LOADING;
          break;
        case ESimUiState.ACTIVATION_CODE_ENTRY:
          this.selectedESimPageName_ = ESimPageName.ACTIVATION_CODE;
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY:
          this.selectedESimPageName_ = ESimPageName.CONFIRMATION_CODE;
          break;
        case ESimUiState.PROFILE_SELECTION:
          this.selectedESimPageName_ = ESimPageName.PROFILE_DISCOVERY;
          break;
        case ESimUiState.SETUP_FINISH:
          this.selectedESimPageName_ = ESimPageName.FINAL;
          break;
        default:
          assertNotReached();
          break;
      }
    },

    /** @private */
    updateButtonBarState_() {
      let buttonState;
      switch (this.state_) {
        case ESimUiState.PROFILE_SEARCH:
        case ESimUiState.ACTIVATION_CODE_ENTRY:
          buttonState = {
            backward: cellularSetup.ButtonState.SHOWN_AND_ENABLED,
            cancel: this.delegate.shouldShowCancelButton() ?
                cellularSetup.ButtonState.SHOWN_AND_ENABLED :
                cellularSetup.ButtonState.HIDDEN,
            done: cellularSetup.ButtonState.HIDDEN,
            next: cellularSetup.ButtonState.SHOWN_BUT_DISABLED,
            tryAgain: cellularSetup.ButtonState.HIDDEN,
            skipDiscovery: cellularSetup.ButtonState.HIDDEN,
          };
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY:
          buttonState = {
            backward: cellularSetup.ButtonState.SHOWN_AND_ENABLED,
            cancel: this.delegate.shouldShowCancelButton() ?
                cellularSetup.ButtonState.SHOWN_AND_ENABLED :
                cellularSetup.ButtonState.HIDDEN,
            done: cellularSetup.ButtonState.HIDDEN,
            // TODO(crbug.com/1093185) Add a "Confirm" button state.
            next: cellularSetup.ButtonState.SHOWN_BUT_DISABLED,
            tryAgain: cellularSetup.ButtonState.HIDDEN,
            skipDiscovery: cellularSetup.ButtonState.HIDDEN,
          };
          break;
        case ESimUiState.PROFILE_SELECTION:
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: this.delegate.shouldShowCancelButton() ?
                cellularSetup.ButtonState.SHOWN_AND_ENABLED :
                cellularSetup.ButtonState.HIDDEN,
            done: cellularSetup.ButtonState.HIDDEN,
            next: cellularSetup.ButtonState.HIDDEN,
            tryAgain: cellularSetup.ButtonState.HIDDEN,
            skipDiscovery: cellularSetup.ButtonState.SHOWN_AND_ENABLED,
          };
          break;
        case ESimUiState.SETUP_FINISH:
          buttonState = {
            backward: cellularSetup.ButtonState.HIDDEN,
            cancel: cellularSetup.ButtonState.HIDDEN,
            done: cellularSetup.ButtonState.SHOWN_AND_ENABLED,
            next: cellularSetup.ButtonState.HIDDEN,
            tryAgain: cellularSetup.ButtonState.HIDDEN,
            skipDiscovery: cellularSetup.ButtonState.HIDDEN,
          };
          break;
        default:
          assertNotReached();
          break;
      }
      this.set('buttonState', buttonState);
    },

    /** @private */
    onActivationCodeUpdated_(event) {
      if (event.detail.activationCode) {
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_AND_ENABLED);
      } else {
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_BUT_DISABLED);
      }
    },

    /** @private */
    onSelectedProfileChanged_() {
      if (this.selectedProfile_) {
        this.set('buttonState.skipDiscovery', cellularSetup.ButtonState.HIDDEN);
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_AND_ENABLED);
      } else {
        this.set(
            'buttonState.skipDiscovery',
            cellularSetup.ButtonState.SHOWN_AND_ENABLED);
        this.set('buttonState.next', cellularSetup.ButtonState.HIDDEN);
      }
    },

    /** @private */
    onConfirmationCodeUpdated_() {
      // TODO(crbug.com/1093185) Change this to updating a "Confirm" button's
      // state.
      if (this.confirmationCode_) {
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_AND_ENABLED);
      } else {
        this.set(
            'buttonState.next', cellularSetup.ButtonState.SHOWN_BUT_DISABLED);
      }
    },

    /** SubflowBehavior override */
    navigateForward() {
      switch (this.state_) {
        case ESimUiState.ACTIVATION_CODE_ENTRY:
          // Assume installing the profile doesn't require a confirmation
          // code, send an empty string.
          this.euicc_
              .installProfileFromActivationCode(
                  this.activationCode_, /*confirmationCode=*/ '')
              .then(this.handleProfileInstallResponse_.bind(this));
          break;
        case ESimUiState.PROFILE_SELECTION:
          if (this.selectedProfile_) {
            // Assume installing the profile doesn't require a confirmation
            // code, send an empty string.
            this.selectedProfile_.installProfile('').then(
                this.handleProfileInstallResponse_.bind(this));
          } else {
            this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
          }
          break;
        case ESimUiState.CONFIRMATION_CODE_ENTRY:
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
      // TODO(crbug.com/1093185): Handle state when camera is used
      return false;
    },

    /** @private */
    getShowNoProfilesMessage_() {
      return !(this.pendingProfiles_ && this.pendingProfiles_.length > 0);
    },
  });

  // #cr_define_end
  return {ESimPageName: ESimPageName, ESimUiState: ESimUiState};
});