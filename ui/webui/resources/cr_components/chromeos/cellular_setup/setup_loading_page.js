// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Loading subpage in Cellular Setup flow. This element contains image
 * asset and description to indicate that a SIM detection or eSIM profiles
 * loading is in progress. It also has an error state that displays a message
 * for errors that may happen during this step.
 */
Polymer({
  is: 'setup-loading-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!cellular_setup.CellularSetupDelegate} */
    delegate: Object,

    /** Whether error state should be shown. */
    showError: {
      type: Boolean,
      value: false,
    },

    loadingMessage: {
      type: String,
      value: '',
    }
  },

  /**
   * @param {boolean} showError
   * @return {?string}
   * @private
   */
  getTitle_(showError) {
    if (this.delegate.shouldShowPageTitle() && showError) {
      return this.i18n('simDetectPageErrorTitle');
    }
    return null;
  },

  /**
   * @param {boolean} showError
   * @return {string}
   * @private
   */
  getMessage_(showError) {
    return showError ? this.i18n('simDetectPageErrorMessage') : '';
  },
});
