// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Processes touch events and calls back to the class user when tap events
 * defined by FileTapHandler.TapEvent are detected.
 *
 * The user can choose to 1) handle the tap event, in which case this class
 * will suppress browser mouse event generation, or 2) not handle the event
 * to let it be handled by mouse event handlers.
 */
export class FileTapHandler {
  /**
   * Whether the pointer is currently down and at the same place as the
   * initial position.
   */
  private tapStarted_ = false;
  private isLongTap_ = false;
  private isTwoFingerTap_ = false;
  private hasLongPressProcessed_ = false;
  private longTapDetectorTimerId_ = -1;
  /**
   * If defined, the identifier of the active touch. Note that 0 is a valid
   * touch identifier.
   */
  private activeTouchId_: number|undefined = undefined;

  /**
   * The index of the item which is being touched by the active touch. Valid
   * only when |activeTouchId_| is defined.
   */
  private activeItemIndex_ = -1;

  /**
   * Last touch X position in client co-ords.
   */
  private lastTouchX_ = 0;

  /**
   * Last touch Y position in client co-ords.
   */
  private lastTouchY_ = 0;

  /**
   * The absolute sum of all touch X deltas.
   */
  private totalMoveX_ = 0;

  /**
   * The absolute sum of all touch Y deltas.
   */
  private totalMoveY_ = 0;

  /**
   * Handles touch events. Calls touchend.preventDefault() if the |callback|
   * takes any action on the detected tap events to suppress the browser's
   * automatic conversion of touch events to mouse events:
   *
   *   browser events: touchstart > [touchmove] > touchend
   *    ... if touchend.preventDefault() not called ...
   *      browser events: mouseover > mousedown > [mousemove] > mouseup
   *
   * @param event Touch event.
   * @param index Index of the target item in the file list.
   * @param callback Called when a tap event is detected. Should return true if
   *     it has taken any action, and false if it ignores the event.
   * @return True if a tap event was detected and the |callback| processed the
   *     event. False otherwise.
   */
  handleTouchEvents(
      event: TouchEvent, index: number,
      callback:
          (event: TouchEvent, index: number, eventType: TapEvent) => boolean) {
    // If the event is not cancelable, touch scrolling is active. Reset the
    // touch tracking to disable tap event detection during scrolling.
    if (event.cancelable === false) {
      this.resetTouchTracking_();
      return false;
    }

    switch (event.type) {
      case 'touchcancel':
        this.resetTouchTracking_();
        break;

      case 'touchstart': {
        // Only track the position of the single touch. However, we detect a
        // two-finger tap for opening a context menu of the target.
        if (event.touches.length > 2) {
          this.tapStarted_ = false;
          return false;
        } else if (this.activeTouchId_ !== undefined) {
          this.isTwoFingerTap_ = event.touches.length === 2;
          return false;
        }

        this.resetTouchTracking_();
        const touch = event.targetTouches[0];
        this.activeTouchId_ = touch?.identifier;
        this.tapStarted_ = true;

        this.activeItemIndex_ = index;
        this.isLongTap_ = false;
        this.isTwoFingerTap_ = event.touches.length === 2;

        this.hasLongPressProcessed_ = false;
        this.longTapDetectorTimerId_ = setTimeout(() => {
          this.longTapDetectorTimerId_ = -1;
          if (!this.tapStarted_) {
            return;
          }
          this.isLongTap_ = true;
          if (callback(event, index, TapEvent.LONG_PRESS)) {
            this.hasLongPressProcessed_ = true;
          }
        }, LONG_PRESS_THRESHOLD_MILLISECONDS);

        this.lastTouchX_ = touch?.clientX ?? 0;
        this.lastTouchY_ = touch?.clientY ?? 0;
        this.totalMoveX_ = 0;
        this.totalMoveY_ = 0;
      } break;

      case 'touchmove': {
        const touch = this.findActiveTouch_(event.changedTouches);
        if (touch === undefined) {
          break;
        }

        if (!this.tapStarted_) {
          break;
        }

        this.totalMoveX_ += Math.abs(this.lastTouchX_ - touch.clientX);
        this.totalMoveY_ += Math.abs(this.lastTouchY_ - touch.clientY);

        // Allow some movement for two-finger taps, and none otherwise.
        let moveLimit = 0;
        if (this.isTwoFingerTap_) {
          moveLimit = MAX_TRACKING_FOR_TAP_;
        }

        // If the touch has moved outside limits, it's no longer a tap.
        if (this.totalMoveX_ > moveLimit || this.totalMoveY_ > moveLimit) {
          this.tapStarted_ = false;
        }

        this.lastTouchX_ = touch.clientX;
        this.lastTouchY_ = touch.clientY;
      } break;

      case 'touchend': {
        // Mark as no longer being touched.
        // Two-finger tap event is issued when either of the 2 touch points is
        // released. Stop tracking the tap to avoid issuing duplicate events.
        const tapStarted = this.resetTouchTracking_();

        if (!tapStarted) {
          break;
        }

        if (this.isLongTap_) {
          // The item at the touch start position is treated as the target item,
          // rather than the one at the touch end position. Note that |index| is
          // the latter.
          if (this.hasLongPressProcessed_ ||
              callback(event, this.activeItemIndex_, TapEvent.LONG_TAP)) {
            event.preventDefault();
            return true;
          }
        } else {
          // The item at the touch start position of the active touch is treated
          // as the target item. In case of the two-finger tap, the first touch
          // point points to the target.
          if (callback(
                  event, this.activeItemIndex_,
                  this.isTwoFingerTap_ ? TapEvent.TWO_FINGER_TAP :
                                         TapEvent.TAP)) {
            event.preventDefault();
            return true;
          }
        }
      } break;
    }

    return false;
  }

  /**
   * Resets the touch tracking state variables. Saves the |this.tapStarted_|
   * state first, then resets all tracking state variables.
   *
   * @return The saved |this.tapStarted_| state or false if there is no active
   *     touch Id.
   */
  private resetTouchTracking_(): boolean {
    const tapStarted = this.tapStarted_;
    this.tapStarted_ = false;

    const activeTouchId = this.activeTouchId_;
    this.activeTouchId_ = undefined;

    if (this.longTapDetectorTimerId_ !== -1) {
      clearTimeout(this.longTapDetectorTimerId_);
      this.longTapDetectorTimerId_ = -1;
    }

    if (activeTouchId !== undefined) {
      return tapStarted;
    }

    return false;
  }

  /**
   * Given a list of Touches, find the one matching the active touch Id. Note
   * Chrome currently always uses 0 as the Id, so we end up always choosing
   * the first element in the list.
   *
   * @param touches List of Touch objects to search.
   * @return Touch matching the active touch Id, or undefined if there is no
   *     active touch Id or no match was found.
   */
  private findActiveTouch_(touches: TouchList): Touch|undefined {
    if (this.activeTouchId_ !== undefined) {
      for (const touch of touches) {
        if (touch.identifier === this.activeTouchId_) {
          return touch;
        }
      }
    }
    return;
  }
}

/**
 * The minimum duration of a tap to be recognized as long press and long tap.
 * This should be consistent with the Views of Android.
 * https://android.googlesource.com/platform/frameworks/base/+/HEAD/core/java/android/view/ViewConfiguration.java
 * Also this should also be consistent with Chrome's behavior for issuing
 * drag-and-drop events by touchscreen.
 */
const LONG_PRESS_THRESHOLD_MILLISECONDS = 500;

/**
 * Maximum movement of touch required to be considered a tap.
 */
const MAX_TRACKING_FOR_TAP_ = 8;

export enum TapEvent {
  /**
  The touch started and ended quickly, aka, both events have triggered:
  touchstart and touchend.
  */
  TAP = 'tap',

  /**
  The touch started and took more than the threshold, it hasn't triggered
  the touchend yet, but the LONG_PRESS is processed.
  */
  LONG_PRESS = 'longpress',

  /**
  The touchstart and the touchend have triggered and took more than the
  threshold between the two.
  */
  LONG_TAP = 'longtap',

  /** Similart to TAP but with exactly 2 fingers. */
  TWO_FINGER_TAP = 'twofingertap',
}
