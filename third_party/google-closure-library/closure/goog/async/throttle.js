/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the goog.async.Throttle class.
 *
 * @see ../demos/timers.html
 */

goog.module('goog.async.Throttle');
goog.module.declareLegacyNamespace();

const Disposable = goog.require('goog.Disposable');
const Timer = goog.require('goog.Timer');


/**
 * Throttle will perform an action that is passed in no more than once
 * per interval (specified in milliseconds). If it gets multiple signals
 * to perform the action while it is waiting, it will only perform the action
 * once at the end of the interval.
 * @final
 * @template T
 */
class Throttle extends Disposable {
  /**
   * @param {function(this: T, ...?)} listener Function to callback when the
   *     action is triggered.
   * @param {number} interval Interval over which to throttle. The listener can
   *     only be called once per interval.
   * @param {T=} handler Object in whose scope to call the listener.
   */
  constructor(listener, interval, handler) {
    super();
    /**
     * Function to callback
     * @type {function(this: T, ...?)}
     * @private
     */
    this.listener_ = handler != null ? listener.bind(handler) : listener;

    /**
     * Interval for the throttle time
     * @type {number}
     * @private
     */
    this.interval_ = interval;

    /**
     * The last arguments passed into `fire`, or null if there is no pending
     * call.
     * @private {?IArrayLike}
     */
    this.args_ = null;

    /**
     * Indicates that the action is pending and needs to be fired.
     * @type {boolean}
     * @private
     */
    this.shouldFire_ = false;

    /**
     * Indicates the count of nested pauses currently in effect on the throttle.
     * When this count is not zero, fired actions will be postponed until the
     * throttle is resumed enough times to drop the pause count to zero.
     * @type {number}
     * @private
     */
    this.pauseCount_ = 0;

    /**
     * Timer for scheduling the next callback
     * @type {?number}
     * @private
     */
    this.timer_ = null;
  }

  /**
   * Notifies the throttle that the action has happened. It will throttle
   * the call so that the callback is not called too often according to the
   * interval parameter passed to the constructor, passing the arguments
   * from the last call of this function into the throttled function.
   * @param {...?} var_args Arguments to pass on to the throttled function.
   */
  fire(var_args) {
    this.args_ = arguments;
    if (!this.timer_ && !this.pauseCount_) {
      this.doAction_();
    } else {
      this.shouldFire_ = true;
    }
  }

  /**
   * Cancels any pending action callback. The throttle can be restarted by
   * calling {@link #fire}.
   */
  stop() {
    if (this.timer_) {
      Timer.clear(this.timer_);
      this.timer_ = null;
      this.shouldFire_ = false;
      this.args_ = null;
    }
  }

  /**
   * Pauses the throttle.  All pending and future action callbacks will be
   * delayed until the throttle is resumed.  Pauses can be nested.
   */
  pause() {
    this.pauseCount_++;
  }

  /**
   * Resumes the throttle.  If doing so drops the pausing count to zero,
   * pending action callbacks will be executed as soon as possible, but
   * still no sooner than an interval's delay after the previous call.
   * Future action callbacks will be executed as normal.
   */
  resume() {
    this.pauseCount_--;
    if (!this.pauseCount_ && this.shouldFire_ && !this.timer_) {
      this.shouldFire_ = false;
      this.doAction_();
    }
  }

  /** @override */
  disposeInternal() {
    super.disposeInternal();
    this.stop();
  }

  /**
   * Handler for the timer to fire the throttle
   * @private
   */
  onTimer_() {
    this.timer_ = null;

    if (this.shouldFire_ && !this.pauseCount_) {
      this.shouldFire_ = false;
      this.doAction_();
    }
  }

  /**
   * Calls the callback
   * @private
   */
  doAction_() {
    this.timer_ = Timer.callOnce(() => this.onTimer_(), this.interval_);
    const args = this.args_;
    // release memory first so it always happens even if listener throws
    this.args_ = null;
    this.listener_.apply(null, args);
  }
}

exports = Throttle;
