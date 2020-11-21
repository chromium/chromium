// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellular_setup', function() {
  /** @enum{string} */
  /* #export */ const ESimPageName = {
    PROFILE_LOADING: 'profileLoadingPage',
    PROFILE_DISCOVERY: 'profileDiscoveryPage',
    ACTIVATION_CODE: 'activationCodePage',
    FINAL: 'finalPage',
  };

  /** @enum{string} */
  /* #export */ const ESimUiState = {
    PROFILE_SEARCH: 'profile-search',
    ACTIVATION_CODE_ENTRY: 'activation-code-entry',
    MULTI_PROFILE_SELECTION: 'multi-profile-selection',
    SETUP_SUCCESS: 'setup-success',
    SETUP_FAILURE: 'setup-failure',
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
       * @type {Array<!Object>}
       * @private
       */
      selectedProfiles_: {
        type: Object,
      },
    },

    /**
     * Provides an interface to the ESimManager Mojo service.
     * @private {?chromeos.cellularSetup.mojom.ESimManagerRemote}
     */
    eSimManagerRemote_: null,

    listeners: {
      'activation-code-updated': 'onActivationCodeUpdated_',
    },

    observers: [
      'updateSelectedPage_(state_)', 'updateButtonBarState_(state_)',
      'onSelectedProfilesChanged_(selectedProfiles_.splices)'
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
            return euicc.requestPendingProfiles();
          })
          .then(response => {
            if (response.result ===
                chromeos.cellularSetup.mojom.ESimOperationResult.kFailure) {
              console.error('Error requesting pending profiles: ' + response);
            }
            return euicc.getProfileList();
          })
          .then(response => {
            return this.filterForPendingProfiles_(response.profiles);
          })
          .then(profiles => {
            switch (profiles.length) {
              case 0:
                this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
                break;
              case 1:
                // TODO(crbug.com/1093185) Install the
                // profile. Handle error state. Handle
                // confirmation code if needed.
                this.state_ = ESimUiState.SETUP_SUCCESS;
                break;
              default:
                // TODO(crbug.com/1093185) Populate the profile discovery with
                // profiles.
                this.state_ = ESimUiState.MULTI_PROFILE_SELECTION;
                break;
            }
          });
    },

    /**
     * @private
     * @param {!Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>} profiles
     * @return {!Promise<Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>>}
     */
    filterForPendingProfiles_(profiles) {
      const profilePromises = profiles.map(profile => {
        return profile.getProperties().then(response => {
          if (response.properties.state !==
              chromeos.cellularSetup.mojom.ProfileState.kPending) {
            return null;
          }
          return profile;
        });
      });
      return Promise.all(profilePromises).then(profiles => {
        return profiles.filter(profile => {
          return profile !== null;
        });
      });
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
        case ESimUiState.MULTI_PROFILE_SELECTION:
          this.selectedESimPageName_ = ESimPageName.PROFILE_DISCOVERY;
          break;
        case ESimUiState.SETUP_SUCCESS:
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
        case ESimUiState.MULTI_PROFILE_SELECTION:
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
        case ESimUiState.SETUP_SUCCESS:
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
    onSelectedProfilesChanged_() {
      if (this.selectedProfiles_.length > 0) {
        this.set('buttonState.skipDiscovery', cellularSetup.ButtonState.HIDDEN);
        // TODO(crbug.com/1093185): Install the profiles when 'Done' is pressed.
        this.set(
            'buttonState.done', cellularSetup.ButtonState.SHOWN_AND_ENABLED);
      } else {
        this.set(
            'buttonState.skipDiscovery',
            cellularSetup.ButtonState.SHOWN_AND_ENABLED);
        this.set('buttonState.done', cellularSetup.ButtonState.HIDDEN);
      }
    },

    navigateForward() {
      switch (this.state_) {
        case ESimUiState.ACTIVATION_CODE_ENTRY:
          // TODO(crbug.com/1093185) Install the profile. Handle error state.
          // Handle confirmation code if needed.
          this.state_ = ESimUiState.SETUP_SUCCESS;
          break;
        case ESimUiState.MULTI_PROFILE_SELECTION:
          this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
          break;
        default:
          assertNotReached();
          break;
      }
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
  return {ESimPageName: ESimPageName, ESimUiState: ESimUiState};
});