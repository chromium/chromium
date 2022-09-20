// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that displays a choice of available eSIM Profiles.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './base_page.js';
import './profile_discovery_list_item.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {I18nBehavior} from '../../../cr_elements/i18n_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'profile-discovery-list-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {Array<!ESimProfileRemote>}
     * @private
     */
    pendingProfiles: {
      type: Array,
    },

    /**
     * @type {?ESimProfileRemote}
     * @private
     */
    selectedProfile: {
      type: Object,
      notify: true,
    },

    /**
     * Indicates the UI is busy with an operation and cannot be interacted with.
     */
    showBusy: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @param {ESimProfileRemote} profile
   * @private
   */
  isProfileSelected_(profile) {
    return this.selectedProfile === profile;
  },
});
