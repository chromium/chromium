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
     * TODO(crbug.com/1093185) Fetch real profiles.
     * @type {Array<!Object>}
     * @private
     */
    profiles_: {
      type: Array,
      value() {
        return [
          {
            name: 'Profile 1',
            provider: 'Google Fi',
          },
          {
            name: 'Profile 2',
            provider: 'Verizon',
          },
          {
            name: 'Profile 3',
            provider: 'Google Fi',
          },
        ];
      },
    },

    /**
     * @type {Array<!Object>}
     * @private
     */
    selectedProfiles: {
      type: Object,
      notify: true,
    },
  },

  /**
   * @param {Object} profile
   * @private
   */
  isProfileSelected_(profile) {
    return this.selectedProfiles.some(p => p === profile);
  }
});
