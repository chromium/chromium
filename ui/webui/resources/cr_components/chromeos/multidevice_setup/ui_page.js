// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An element that encapsulates the structure common to all pages in the WebUI.
 */
Polymer({
  is: 'ui-page',

  properties: {
    /**
     * Main heading for the page.
     *
     * @type {string}
     */
    headerText: String,

    /**
     * Name of icon within icon set.
     *
     * @type {string}
     */
    iconName: String,
  },

  /**
   * @return {string}
   * @private
   */
  computeIconIdentifier_: function() {
    return 'multidevice-setup-icons-32:' + this.iconName;
  },
});
