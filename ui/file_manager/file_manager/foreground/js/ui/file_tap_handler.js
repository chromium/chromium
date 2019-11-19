// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Processes touch events and calls back upon tap, longpress and longtap.
 * This class is similar to cr.ui.TouchHandler. The major difference is that,
 * the user of this class can choose to either handle the event as a tap
 * distincted from mouse clicks, or leave it handled by the mouse event
 * handlers by default.
 */
class FileTapHandler {
  constructor() {
    /**
     * Whether the pointer is currently down and at the same place as the
     * initial position.
     * @private {boolean}
     */
    this.tapStarted_ = false;

    /** @private {boolean} */
    this.isLongTap_ = false;

    /** @private {boolean} */
    this.isTwoFingerTap_ = false;

    /** @private {boolean} */
    this.hasLongPressProcessed_ = false;

    /** @private {number} */
    this.longTapDetectorTimerId_ = -1;

    /**
     * The absolute sum of all touch y deltas.
     * @private {number}
     */
    this.totalMoveY_ = 0;

    /**
     * The absolute sum of all touch x deltas.
     * @private {number}
     */
    this.totalMoveX_ = 0;

    /**
     * If defined, the identifier of the single touch that is active.  Note that
     * 0 is a valid touch identifier - it should not be treated equivalently to
     * undefined.
     * @private {number|undefined}
     */
    this.activeTouchId_ = undefined;

    /**
     * The index of the item which is being touched by the active touch. This is
     * valid only when |activeTouchId_| is defined.
     * @private {number}
     */
    this.activeItemIndex_ = -1;

    /** @private {?number} */
    this.lastMoveX_ = null;

    /** @private {?number} */
    this.lastMoveY_ = null;

    /** @private {?number} */
    this.lastTouchX_ = null;

    /** @private {?number} */
    this.lastTouchY_ = null;

    /** @private {?number} */
    this.startTouchX_ = null;

    /** @private {?number} */
    this.startTouchY_ = null;
  }

  /**
   * Handles touch events.
   * The propagation of the |event| will be cancelled if the |callback| takes
   * any action, so as to avoid receiving mouse click events for the tapping and
   * processing them duplicatedly.
   * @param {!Event} event a touch event.
   * @param {number} index of the target item in the file list.
   * @param {function(!Event, number, !FileTapHandler.TapEvent)} callback called
   *     when a tap event is detected. Should return true if it has taken any
   *     action, and false if it ignroes the event.
   * @return {boolean} true if a tap or longtap event was detected and the
   *     callback processed it. False otherwise.
   */
  handleTouchEvents(event, index, callback) {
    switch (event.type) {
      case 'touchstart':
        // Only track the position of the single touch. However, we detect a
        // two-finger tap for opening a context menu of the target.
        if (event.touches.length == 2) {
          this.isTwoFingerTap_ = true;
          return false;
        } else if (event.touches.length > 2) {
          this.tapStarted_ = false;
          return false;
        }

        // It's still possible there could be an active "touch" if the user is
        // simultaneously using a mouse and a touch input.
        // TODO(yamaguchi): add this after adding handler for touchcancel that
        // can reset this.activeTouchId_ to undefined.
        // if (this.activeTouchId_ !== undefined)
        //   return;
        var touch = event.targetTouches[0];
        this.activeTouchId_ = touch.identifier;
        this.startTouchX_ = this.lastTouchX_ = touch.clientX;
        this.startTouchY_ = this.lastTouchY_ = touch.clientY;
        this.totalMoveX_ = 0;
        this.totalMoveY_ = 0;

        this.tapStarted_ = true;
        this.activeItemIndex_ = index;
        this.isLongTap_ = false;
        this.isTwoFingerTap_ = false;
        this.hasLongPressProcessed_ = false;
        this.longTapDetectorTimerId_ = setTimeout(() => {
          this.longTapDetectorTimerId_ = -1;
          if (!this.tapStarted_) {
            return;
          }
          this.isLongTap_ = true;
          if (callback(event, index, FileTapHandler.TapEvent.LONG_PRESS)) {
            this.hasLongPressProcessed_ = true;
          }
        }, FileTapHandler.LONG_PRESS_THRESHOLD_MILLISECONDS);
        break;

      case 'touchmove':
        if (this.activeTouchId_ === undefined) {
          break;
        }
        var touch = this.findActiveTouch_(event.changedTouches);
        if (!touch) {
          break;
        }

        const clientX = touch.clientX;
        const clientY = touch.clientY;

        const moveX = this.lastTouchX_ - clientX;
        const moveY = this.lastTouchY_ - clientY;
        this.totalMoveX_ += Math.abs(moveX);
        this.totalMoveY_ += Math.abs(moveY);
        this.lastTouchX_ = clientX;
        this.lastTouchY_ = clientY;

        const couldBeTap =
            this.totalMoveY_ <= FileTapHandler.MAX_TRACKING_FOR_TAP_ ||
            this.totalMoveX_ <= FileTapHandler.MAX_TRACKING_FOR_TAP_;

        if (!couldBeTap) {
          // If the pointer is slided, it is a drag. It is no longer a tap.
          this.tapStarted_ = false;
        }
        this.lastMoveX_ = moveX;
        this.lastMoveY_ = moveY;
        break;

      case 'touchend':
        if (!this.tapStarted_) {
          break;
        }
        // Mark as no longer being touched.
        // Two-finger tap event is issued when either of the 2 touch points is
        // released. Stop tracking the tap to avoid issuing duplicate events.
        this.tapStarted_ = false;
        this.activeTouchId_ = undefined;
        if (this.longTapDetectorTimerId_ != -1) {
          clearTimeout(this.longTapDetectorTimerId_);
          this.longTapDetectorTimerId_ = -1;
        }
        if (this.isLongTap_) {
          // The item at the touch start position is treated as the target item,
          // rather than the one at the touch end position. Note that |index| is
          // the latter.
          if (this.hasLongPressProcessed_ ||
              callback(
                  event, this.activeItemIndex_,
                  FileTapHandler.TapEvent.LONG_TAP)) {
            event.preventDefault();
            return true;
          }
        } else {
          // The item at the touch start position of the active touch is treated
          // as the target item. In case of the two-finger tap, the first touch
          // point points to the target.
          if (callback(
                  event, this.activeItemIndex_,
                  this.isTwoFingerTap_ ?
                      FileTapHandler.TapEvent.TWO_FINGER_TAP :
                      FileTapHandler.TapEvent.TAP)) {
            event.preventDefault();
            return true;
          }
        }
        break;
    }
    return false;
  }

  /**
   * Given a list of Touches, find the one matching our activeTouch
   * identifier. Note that Chrome currently always uses 0 as the identifier.
   * In that case we'll end up always choosing the first element in the list.
   * @param {TouchList} touches The list of Touch objects to search.
   * @return {!Touch|undefined} The touch matching our active ID if any.
   * @private
   */
  findActiveTouch_(touches) {
    assert(this.activeTouchId_ !== undefined, 'Expecting an active touch');
    // A TouchList isn't actually an array, so we shouldn't use
    // Array.prototype.filter/some, etc.
    for (let i = 0; i < touches.length; i++) {
      if (touches[i].identifier == this.activeTouchId_) {
        return touches[i];
      }
    }
    return undefined;
  }
}

/**
 * The minimum duration of a tap to be recognized as long press and long tap.
 * This should be consistent with the Views of Android.
 * https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/view/ViewConfiguration.java
 * Also this should also be consistent with Chrome's behavior for issuing
 * drag-and-drop events by touchscreen.
 * @type {number}
 * @const
 */
FileTapHandler.LONG_PRESS_THRESHOLD_MILLISECONDS = 500;

/**
 * Maximum movement of touch required to be considered a tap.
 * @type {number}
 * @private
 */
FileTapHandler.MAX_TRACKING_FOR_TAP_ = 8;

/**
 * @enum {string}
 * @const
 */
FileTapHandler.TapEvent = {
  TAP: 'tap',
  LONG_PRESS: 'longpress',
  LONG_TAP: 'longtap',
  TWO_FINGER_TAP: 'twofingertap'
};
