// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that displays a choice of available eSIM Profiles.
 */

Polymer({
  is: 'profile-discovery-list-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>}
     * @private
     */
    pendingProfiles: {
      type: Array,
    },

    /**
     * @type {?chromeos.cellularSetup.mojom.ESimProfileRemote}
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
   * @param {chromeos.cellularSetup.mojom.ESimProfileRemote} profile
   * @private
   */
  isProfileSelected_(profile) {
    return this.selectedProfile === profile;
  }
});
