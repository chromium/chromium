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
   * @return {string}
   * @private
   */
  getMessage_() {
    const profileName = this.getProfileName_();
    if (!profileName) {
      return '';
    }
    return this.i18n('confirmationCodeMessage', profileName);
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