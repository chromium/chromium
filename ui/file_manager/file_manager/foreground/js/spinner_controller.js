// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller for spinners. Spinner requests can be stacked. Eg. if show()
 * is called 3 times, the hide callback has to be called 3 times to make the
 * spinner invisible.
 */
class SpinnerController {
  /** @param {!Element} element */
  constructor(element) {
    /**
     * The container element of the file list.
     * @type {!Element}
     * @const
     * @private
     */
    this.element_ = element;

    /**
     * @type {number}
     * @private
     */
    this.activeSpinners_ = 0;

    /**
     * @type {!Object<number, boolean>}
     * @private
     */
    this.pendingSpinnerTimerIds_ = {};

    /**
     * @type {number}
     * @private
     */
    this.blinkDuration_ = 1000;  // In milliseconds.
  }

  /**
   * Blinks the spinner for a short period of time. Hides automatically.
   */
  blink() {
    const hideCallback = this.show();
    setTimeout(hideCallback, this.blinkDuration_);
  }

  /**
   * Shows the spinner immediately until the returned callback is called.
   * @return {function()} Hide callback.
   */
  show() {
    return this.showWithDelay(0, () => {});
  }

  /**
   * Shows the spinner until hide is called. The returned callback must be
   * called when the spinner is not necessary anymore.
   * @param {number} delay Delay in milliseconds.
   * @param {function()} callback Show callback.
   * @return {function()} Hide callback.
   */
  showWithDelay(delay, callback) {
    const timerId = setTimeout(() => {
      this.activeSpinners_++;
      if (this.activeSpinners_ === 1) {
        this.element_.hidden = false;
      }
      delete this.pendingSpinnerTimerIds_[timerId];
      callback();
    }, delay);

    this.pendingSpinnerTimerIds_[timerId] = true;
    return this.maybeHide_.bind(this, timerId);
  }

  /**
   * @param {number} duration Duration in milliseconds.
   */
  setBlinkDurationForTesting(duration) {
    this.blinkDuration_ = duration;
  }

  /**
   * @param {number} timerId
   * @private
   */
  maybeHide_(timerId) {
    if (timerId in this.pendingSpinnerTimerIds_) {
      clearTimeout(timerId);
      delete this.pendingSpinnerTimerIds_[timerId];
      return;
    }

    this.activeSpinners_--;
    if (this.activeSpinners_ === 0) {
      this.element_.hidden = true;
    }
  }
}
