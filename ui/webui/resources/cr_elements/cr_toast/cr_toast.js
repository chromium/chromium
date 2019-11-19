// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview A lightweight toast.
 */
Polymer({
  is: 'cr-toast',

  properties: {
    duration: {
      type: Number,
      value: 0,
    },

    open: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  hostAttributes: {
    'role': 'alert',
  },

  observers: ['resetAutoHide_(duration, open)'],

  /** @private {number|null} */
  hideTimeoutId_: null,

  /**
   * Cancels existing auto-hide, and sets up new auto-hide.
   * @private
   */
  resetAutoHide_: function() {
    if (this.hideTimeoutId_ != null) {
      window.clearTimeout(this.hideTimeoutId_);
      this.hideTimeoutId_ = null;
    }

    if (this.open && this.duration != 0) {
      this.hideTimeoutId_ = window.setTimeout(() => {
        this.open = false;
      }, this.duration);
    }
  },

  /**
   * Shows the toast and auto-hides after |this.duration| milliseconds has
   * passed. If the toast is currently being shown, any preexisting auto-hide
   * is cancelled and replaced with a new auto-hide.
   *
   * If |this.duration| is set to 0, the toast will remain open indefinitely.
   * The caller is responsible for hiding the toast.
   *
   * When |duration| is passed in the non-negative number, |this.duration|
   * is updated to that value.
   * @param {number=} duration
   */
  show: function(duration) {
    // |this.resetAutoHide_| is called whenever |this.duration| or |this.open|
    // is changed. If neither is changed, we will still need to reset auto-hide.
    let shouldResetAutoHide = true;

    if (typeof(duration) != 'undefined' && duration >= 0 &&
        this.duration != duration) {
      this.duration = duration;
      shouldResetAutoHide = false;
    }

    if (!this.open) {
      this.open = true;
      shouldResetAutoHide = false;
    }

    if (shouldResetAutoHide) {
      this.resetAutoHide_();
    }
  },

  /**
   * Hides the toast.
   */
  hide: function() {
    this.open = false;
  },
});
