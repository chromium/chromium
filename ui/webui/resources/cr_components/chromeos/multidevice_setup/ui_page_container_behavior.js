// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @polymerBehavior */
const UiPageContainerBehaviorImpl = {
  properties: {
    /**
     * ID of loadTimeData string for forward button label, which must be
     * translated for display. Undefined if the visible page has no
     * forward-navigation button.
     * @type {string|undefined}
     */
    forwardButtonTextId: String,

    /**
     * ID of loadTimeData string for cancel button label, which must be
     * translated for display. Undefined if the visible page has no
     * cancel button.
     * @type {string|undefined}
     */
    cancelButtonTextId: String,

    /**
     * ID of loadTimeData string for backward button label, which must be
     * translated for display. Undefined if the visible page has no
     * backward-navigation button.
     * @type {string|undefined}
     */
    backwardButtonTextId: String,
  },

  /**
   * Returns a promise which always resolves and returns a boolean representing
   * whether it should be possible to navigate forward. This function is called
   * before forward navigation is requested; if false is returned, the active
   * page does not change.
   * @return {!Promise}
   */
  getCanNavigateToNextPage: function() {
    return new Promise((resolve) => {
      resolve(true /* canNavigate */);
    });
  },
};

/** @polymerBehavior */
const UiPageContainerBehavior = [
  I18nBehavior,
  UiPageContainerBehaviorImpl,
];
