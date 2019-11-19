// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Element containing navigation buttons for the Cellular Setup flow. */
Polymer({
  is: 'button-bar',

  behaviors: [I18nBehavior],

  properties: {
    /** When set, displays the Try Again action button. */
    showTryAgainButton: {
      type: Boolean,
      value: false,
    },

    /** When set, displays the Finish action button. */
    showFinishButton: {
      type: Boolean,
      value: false,
    },

    /** When set, displays a cancel button instead of back. */
    showCancelButton: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onBackwardButtonClicked_: function() {
    this.fire('backward-nav-requested');
  },

  /** @private */
  onTryAgainButtonClicked_: function() {
    this.fire('retry-requested');
  },

  /** @private */
  onFinishButtonClicked_: function() {
    this.fire('complete-flow-requested');
  },
});
