// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This page is displayed when the activation code is being verified, and
 * an ESim profile is being installed.
 */
Polymer({
  is: 'activation-verification-page',

  behaviors: [I18nBehavior],

  properties: {
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
