// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Page in eSIM Cellular Setup flow shown if an eSIM profile requires a
 * confirmation code to install. This element contains an input for the user to
 * enter the confirmation code.
 */
Polymer({
  is: 'confirmation-code-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {?chromeos.cellularSetup.mojom.ESimProfileRemote}
     */
    profile: {
      type: Object,
      observer: 'onProfileChanged_',
    },

    confirmationCode: {
      type: String,
      notify: true,
    },

    showError: {
      type: Boolean,
    },

    showLoadingIndicator: {
      type: Boolean,
    },

    /**
     * @type {?chromeos.cellularSetup.mojom.ESimProfileProperties}
     * @private
     */
    profileProperties_: {
      type: Object,
      value: null,
    },
  },

  /** @private */
  onProfileChanged_() {
    if (!this.profile) {
      this.profileProperties_ = null;
      return;
    }
    this.profile.getProperties().then(response => {
      this.profileProperties_ = response.properties;
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowProfileDetails_() {
    return !!this.profile;
  },

  /**
   * @return {string}
   * @private
   */
  getProfileName_() {
    if (!this.profileProperties_) {
      return '';
    }
    return String.fromCharCode(...this.profileProperties_.name.data);
  },
});