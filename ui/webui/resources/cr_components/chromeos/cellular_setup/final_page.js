// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Final page in Cellular Setup flow, which either displays a success or error
 * message depending on the outcome of the flow. This element contains an image
 * asset and description that indicates that the setup flow has completed.
 */
Polymer({
  is: 'final-page',

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
    return showError ? this.i18n('finalPageErrorTitle') :
                       this.i18n('finalPageTitle');
  },

  /**
   * @param {boolean} showError
   * @return {string}
   * @private
   */
  getMessage_: function(showError) {
    return showError ? this.i18n('finalPageErrorMessage') :
                       this.i18n('finalPageMessage');
  },

  /**
   * @param {boolean} showError
   * @return {string}
   * @private
   */
  getPageBodyClass_: function(showError) {
    return showError ? 'error' : '';
  },
});
