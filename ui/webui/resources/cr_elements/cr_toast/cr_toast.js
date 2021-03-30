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
      readOnly: true,
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  observers: ['resetAutoHide_(duration, open)'],

  /** @private {number|null} */
  hideTimeoutId_: null,

  /**
   * Cancels existing auto-hide, and sets up new auto-hide.
   * @private
   */
  resetAutoHide_() {
    if (this.hideTimeoutId_ !== null) {
      window.clearTimeout(this.hideTimeoutId_);
      this.hideTimeoutId_ = null;
    }

    if (this.open && this.duration !== 0) {
      this.hideTimeoutId_ = window.setTimeout(() => {
        this.hide();
      }, this.duration);
    }
  },

  /**
   * Shows the toast and auto-hides after |this.duration| milliseconds has
   * passed. If the toast is currently being shown, any preexisting auto-hide
   * is cancelled and replaced with a new auto-hide.
   */
  show() {
    // Force autohide to reset if calling show on an already shown toast.
    const shouldResetAutohide = this.open;

    // The role attribute is removed first so that screen readers to better
    // ensure that screen readers will read out the content inside the toast.
    // If the role is not removed and re-added back in, certain screen readers
    // do not read out the contents, especially if the text remains exactly
    // the same as a previous toast.
    this.removeAttribute('role');

    // Reset the aria-hidden attribute as screen readers need to access the
    // contents of an opened toast.
    this.removeAttribute('aria-hidden');

    this._setOpen(true);
    this.setAttribute('role', 'alert');

    if (shouldResetAutohide) {
      this.resetAutoHide_();
    }
  },

  /**
   * Hides the toast and ensures that screen readers cannot its contents while
   * hidden.
   */
  hide() {
    this.setAttribute('aria-hidden', 'true');
    this._setOpen(false);
  },
});
/* #ignore */ console.warn('crbug/1173575, non-JS module files deprecated.');
