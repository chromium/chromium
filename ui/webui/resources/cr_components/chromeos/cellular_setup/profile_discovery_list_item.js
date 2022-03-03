// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in the profile-discovery-list-page list displaying details of an eSIM
 * profile.
 */

Polymer({
  is: 'profile-discovery-list-item',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?ash.cellularSetup.mojom.ESimProfileRemote} */
    profile: {
      type: Object,
      value: null,
      observer: 'onProfileChanged_',
    },

    selected: {
      type: Boolean,
      reflectToAttribute: true,
    },

    showLoadingIndicator: {
      type: Boolean,
    },

    /**
     * @type {?ash.cellularSetup.mojom.ESimProfileProperties}
     * @private
     */
    profileProperties_: {
      type: Object,
      value: null,
      notify: true,
    },

    /**
     * @type {boolean}
     * @private
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
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

  /** @private */
  getProfileName_() {
    if (!this.profileProperties_) {
      return '';
    }
    return String.fromCharCode(...this.profileProperties_.name.data);
  },

  /** @private */
  getProfileProvider_() {
    if (!this.profileProperties_) {
      return '';
    }
    return String.fromCharCode(...this.profileProperties_.serviceProvider.data);
  },

  /**
   * @return {string}
   * @private
   */
  getProfileImage_() {
    return this.isDarkModeActive_ ?
        'chrome://resources/cr_components/chromeos/cellular_setup/default_esim_profile_dark.svg' :
        'chrome://resources/cr_components/chromeos/cellular_setup/default_esim_profile.svg';
  },
});
