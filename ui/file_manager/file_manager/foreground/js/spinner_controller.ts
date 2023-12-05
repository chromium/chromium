// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller for spinners. Spinner requests can be stacked. Eg. if show()
 * is called 3 times, the hide callback has to be called 3 times to make the
 * spinner invisible.
 */
export class SpinnerController {
  private activeSpinners_: number = 0;
  private pendingSpinnerTimerIds_ = new Set<number>();
  private blinkDuration_: number = 1000;  // In milliseconds.

  constructor(private readonly element_: HTMLElement) {}

  /**
   * Blinks the spinner for a short period of time. Hides automatically.
   */
  blink() {
    const hideCallback = this.show();
    setTimeout(hideCallback, this.blinkDuration_);
  }

  /**
   * Shows the spinner immediately until the returned callback is called.
   * @return Hide callback.
   */
  show(): VoidCallback {
    return this.showWithDelay(0, () => {});
  }

  /**
   * Shows the spinner until hide is called. The returned callback must be
   * called when the spinner is not necessary anymore.
   * @param delay Delay in milliseconds.
   * @param callback Show callback.
   * @return Hide callback.
   */
  showWithDelay(delay: number, callback: VoidCallback): VoidCallback {
    const timerId = setTimeout(() => {
      this.activeSpinners_++;
      if (this.activeSpinners_ === 1) {
        this.element_.hidden = false;
      }
      this.pendingSpinnerTimerIds_.delete(timerId);
      callback();
    }, delay);

    this.pendingSpinnerTimerIds_.add(timerId);
    return this.maybeHide_.bind(this, timerId);
  }

  /**
   * Sets blink duration to the given `duration` value that must
   * be specified in milliseconds.
   */
  setBlinkDurationForTesting(duration: number) {
    this.blinkDuration_ = duration;
  }

  private maybeHide_(timerId: number) {
    if (this.pendingSpinnerTimerIds_.has(timerId)) {
      clearTimeout(timerId);
      this.pendingSpinnerTimerIds_.delete(timerId);
      return;
    }

    this.activeSpinners_--;
    if (this.activeSpinners_ === 0) {
      this.element_.hidden = true;
    }
  }
}
