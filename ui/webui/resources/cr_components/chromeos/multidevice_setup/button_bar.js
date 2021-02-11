// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * DOM Element containing (page-dependent) navigation buttons for the
 * MultiDevice Setup WebUI.
 */
Polymer({
  is: 'button-bar',

  properties: {
    /** Whether the forward button should be hidden. */
    forwardButtonHidden: {
      type: Boolean,
      value: true,
    },

    /** Whether the cancel button should be hidden. */
    cancelButtonHidden: {
      type: Boolean,
      value: true,
    },

    /** Whether the backward button should be hidden. */
    backwardButtonHidden: {
      type: Boolean,
      value: true,
    },

    /** Whether a shadow should appear over the button bar. */
    shouldShowShadow: {
      type: Boolean,
      value: false,
      observer: 'onShouldShowShadowChange_',
    }
  },

  /** @private */
  onForwardButtonClicked_() {
    this.fire('forward-navigation-requested');
  },

  /** @private */
  onCancelButtonClicked_() {
    this.fire('cancel-requested');
  },

  /** @private */
  onBackwardButtonClicked_() {
    this.fire('backward-navigation-requested');
  },

  /** @private */
  onShouldShowShadowChange_() {
    this.$.shadow.hidden = !!this.shouldShowShadow;
  },
});
