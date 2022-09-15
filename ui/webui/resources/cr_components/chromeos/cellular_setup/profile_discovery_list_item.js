// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in the profile-discovery-list-page list displaying details of an eSIM
 * profile.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './cellular_setup_icons.js';

import {I18nBehavior} from '//resources/cr_elements/i18n_behavior.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  _template: html`{__html_template__}`,
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
