// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{text:string, callback:(function()|undefined)}}
 */
var FilesToastAction;

/**
 * @typedef {{text:string, action:(FilesToastAction|undefined)}}
 */
var FilesToastData;

/**
 * Files Toast.
 *
 * This toast is shown at the bottom-right in ltr (bottom-left in rtl).
 *
 * Usage:
 * toast.show('Toast without action.');
 * toast.show('Toast with action', {text: 'ACTION', callback:function() {}});
 * toast.hide();
 */
var FilesToast = Polymer({
  is: 'files-toast',

  properties: {
    visible: {
      type: Boolean,
      readOnly: true,
      value: false,
    },
    duration: {
      type: Number,
      value: 5000, /* ms */
    }
  },

  /**
   * Initialize member variables.
   */
  created: function() {
    /**
     * @private {?FilesToastAction}
     */
    this.action_ = null;

    /**
     * @private {!Array<!FilesToastData>}
     */
    this.queue_ = [];

    /**
     * @private {Animation}
     */
    this.enterAnimationPlayer_ = null;

    /**
     * @private {Animation}
     */
    this.hideAnimationPlayer_ = null;
  },

  /**
   * Shows toast. If a toast is already shown, this toast will be added to the
   * queue and shown when others have completed.
   *
   * @param {string} text Text of toast.
   * @param {FilesToastAction=} opt_action Action. Callback
   *     is invoked when user taps an action.
   */
  show: function(text, opt_action) {
    if (this.visible) {
      this.queue_.push({text: text, action: opt_action});
      return;
    }
    this._setVisible(true);

    // Update UI.
    this.$.container.hidden = false;
    this.$.text.innerText = text;
    this.action_ = opt_action || null;

    if (this.action_) {
      this.$.action.hidden = false;
      this.$.action.innerText = this.action_.text;
    } else {
      this.$.action.hidden = true;
    }

    // Perform animation.
    this.enterAnimationPlayer_ = this.$.container.animate([
      {bottom: '-100px', opacity: 0, offset: 0},
      {bottom: '16px', opacity: 1, offset: 1}
    ], 100 /* ms */);

    this.enterAnimationPlayer_.addEventListener('finish', () => {
      this.enterAnimationPlayer_ = null;
    });

    // Set timeout.
    setTimeout(this.hide.bind(this), this.duration);
  },

  /**
   * Handles tap event of action button.
   */
  onActionTapped_: function() {
    if (!this.action_ || !this.action_.callback) {
      return;
    }

    this.action_.callback();
    this.hide();
  },

  /**
   * Clears toast if it's shown.
   * @return {!Promise} A promise which is resolved when toast is hidden.
   */
  hide: function() {
    if (!this.visible) {
      return Promise.resolve();
    }

    // If it's performing enter animation, wait until it's done and come back
    // later.
    if (this.enterAnimationPlayer_ && !this.enterAnimationPlayer_.finished) {
      return new Promise(resolve => {
        // Check that the animation is still playing. Animation can be finished
        // between the above condition check and this function call.
        if (!this.enterAnimationPlayer_ ||
            this.enterAnimationPlayer_.finished) {
          resolve();
        }

        this.enterAnimationPlayer_.addEventListener('finish', resolve);
      }).then(this.hide.bind(this));
    }

    // Start hide animation if it's not performing now.
    if (!this.hideAnimationPlayer_) {
      this.hideAnimationPlayer_ = this.$.container.animate([
        {bottom: '16px', opacity: 1, offset: 0},
        {bottom: '-100px', opacity: 0, offset: 1}
      ], 100 /* ms */);
    }

    return new Promise(resolve => {
      this.hideAnimationPlayer_.addEventListener('finish', resolve);
    }).then(() => {
      this.$.container.hidden = true;
      this.hideAnimationPlayer_ = null;
      this._setVisible(false);
      // Show next in the queue, if any.
      if (this.queue_.length > 0) {
        const next = this.queue_.shift();
        this.show(next.text, next.action);
      }
    });
  }
});
