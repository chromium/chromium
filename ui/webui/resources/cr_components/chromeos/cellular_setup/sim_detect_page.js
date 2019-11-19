// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * SIM Detection subpage in Cellular Setup flow. This element contains image
 * asset and description to indicate that the SIM detection is in progress.
 * It also has an error state that displays a message for errors that may
 * happen during this step.
 */
Polymer({
  is: 'sim-detect-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Whether error state should be shown.
     * @type {boolean}
     */
    showError: Boolean,
  },

  /**
   * @param {boolean} showError
   * @return {string}
   * @private
   */
  getTitle_: function(showError) {
    return this.i18n(
        showError ? 'simDetectPageErrorTitle' : 'simDetectPageTitle');
  },

  /**
   * @param {boolean} showError
   * @return {string}
   * @private
   */
  getMessage_: function(showError) {
    return showError ? this.i18n('simDetectPageErrorMessage') : '';
  },
});
