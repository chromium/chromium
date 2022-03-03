// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Loading subpage in Cellular Setup flow that shows an in progress operation or
 * an error. This element contains error image asset and loading animation.
 */
Polymer({
  is: 'setup-loading-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Message displayed with spinner when in LOADING state.
     */
    loadingMessage: {
      type: String,
      value: '',
    },

    /**
     * Title for page if needed.
     * @type {?string}
     */
    loadingTitle: {
      type: Object,
      value: '',
    },

    /**
     * Displays a sim detect error graphic if true.
     */
    isSimDetectError: {
      type: Boolean,
      value: false,
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

  /**
   * @return {string}
   * @private
   */
  getAnimationUrl_() {
    return this.isDarkModeActive_ ? 'spinner_dark.json' : 'spinner.json';
  },
});
