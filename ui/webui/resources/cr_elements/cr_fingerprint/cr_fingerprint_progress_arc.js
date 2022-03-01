// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {string} */
/* #export */ const FINGERPRINT_TICK_DARK_URL =
    'chrome://theme/IDR_FINGERPRINT_COMPLETE_TICK_DARK';

/** @type {string} */
/* #export */ const FINGERPRINT_TICK_LIGHT_URL =
    'chrome://theme/IDR_FINGERPRINT_COMPLETE_TICK';


/**
 * The dark-mode color of the progress circle background: Google Grey 700.
 * @type {string}
 */
/* #export */ const PROGRESS_CIRCLE_BACKGROUND_COLOR_DARK =
    'rgba(95, 99, 104, 1.0)';

/**
 * The light-mode color of the progress circle background: Google Grey 300.
 * @type {string}
 */
/* #export */ const PROGRESS_CIRCLE_BACKGROUND_COLOR_LIGHT =
    'rgba(218, 220, 224, 1.0)';

/**
 * The dark-mode color of the setup progress arc: Google Blue 400.
 * @type {string}
 */
/* #export */ const PROGRESS_CIRCLE_FILL_COLOR_DARK =
    'rgba(102, 157, 246, 1.0)';

/**
 * The light-mode color of the setup progress arc: Google Blue 500.
 * @type {string}
 */
/* #export */ const PROGRESS_CIRCLE_FILL_COLOR_LIGHT =
    'rgba(66, 133, 244, 1.0)';

(function() {

/**
 * The time in milliseconds of the animation updates.
 * @type {number}
 */
const ANIMATE_TICKS_MS = 20;

/**
 * The duration in milliseconds of the animation of the progress circle when the
 * user is touching the scanner.
 * @type {number}
 */
const ANIMATE_DURATION_MS = 200;

/**
 * The radius of the add fingerprint progress circle.
 * @type {number}
 */
const DEFAULT_PROGRESS_CIRCLE_RADIUS = 114;

/**
 * The default height of the icon located in the center of the fingerprint
 * progress circle.
 * @type {number}
 */
const ICON_HEIGHT = 118;

/**
 * The default width of the icon located in the center of the fingerprint
 * progress circle.
 * @type {number}
 */
const ICON_WIDTH = 106;

/**
 * The default size of the checkmark located in the left bottom corner of the
 * fingerprint progress circle.
 * @type {number}
 */
const CHECK_MARK_SIZE = 53;

/**
 * The time in milliseconds of the fingerprint scan success timeout.
 * @type {number}
 */
const FINGERPRINT_SCAN_SUCCESS_MS = 500;

/**
 * The thickness of the fingerprint progress circle.
 * @type {number}
 */
const PROGRESS_CIRCLE_STROKE_WIDTH = 4;

Polymer({
  is: 'cr-fingerprint-progress-arc',

  properties: {
    /**
     * Radius of the fingerprint progress circle being displayed.
     * @type {number}
     */
    circleRadius: {
      type: Number,
      value: DEFAULT_PROGRESS_CIRCLE_RADIUS,
    },

    /**
     * Whether lottie animation should be autoplayed.
     * @type {boolean}
     */
    autoplay: {
      type: Boolean,
      value: false,
    },

    /**
     * Scale factor based the configured radius (circleRadius) vs the default
     * radius (DEFAULT_PROGRESS_CIRCLE_RADIUS).
     * This will affect the size of icons and checkmark.
     * @type {number}
     * @private
     */
    scale_: {
      type: Number,
      value: 1.0,
    },

    /**
     * Whether fingerprint enrollment is complete.
     * @type {boolean}
     * @private
     */
    isComplete_: Boolean,

    /**
     * Whether the fingerprint progress page is being rendered in dark mode.
     * @type {boolean}
     * @private
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
      observer: 'onDarkModeChanged_',
    },
  },

  /**
   * Animation ID for the fingerprint progress circle.
   * @type {number|undefined}
   * @private
   */
  progressAnimationIntervalId_: undefined,

  /**
   * Percentage of the enrollment process completed as of the last update.
   * @type {number}
   * @private
   */
  progressPercentDrawn_: 0,

  /**
   * Timer ID for fingerprint scan success update.
   * @type {number|undefined}
   * @private
   */
  updateTimerId_: undefined,

  /**
   * Updates the current state to account for whether dark mode is enabled.
   * @private
   */
  onDarkModeChanged_() {
    this.clearCanvas_();
    this.drawProgressCircle_(this.progressPercentDrawn_);
    this.updateAnimationAsset_();
  },

  /** @override */
  attached() {
    this.scale_ = this.circleRadius / DEFAULT_PROGRESS_CIRCLE_RADIUS;
    this.updateImages_();
  },

  /**
   * Reset the element to initial state, when the enrollment just starts.
   * @public
   */
  reset() {
    this.cancelAnimations_();
    this.clearCanvas_();
    this.isComplete_ = false;
    // Draw an empty background for the progress circle.
    this.drawProgressCircle_(/** currentPercent = */ 0);
    this.$.enrollmentDone.hidden = true;

    const scanningAnimation =
        /** @type {CrLottieElement|HTMLElement} */ (this.$.scanningAnimation);
    scanningAnimation.singleLoop = false;
    scanningAnimation.classList.add('translucent');
    this.updateAnimationAsset_();
    this.resizeAndCenterIcon_(scanningAnimation);
    scanningAnimation.hidden = false;
  },

  /**
   * Animates the progress circle. Animates an arc that starts at the top of
   * the circle to prevPercentComplete, to an arc that starts at the top of the
   * circle to currPercentComplete.
   * @param {number} prevPercentComplete The previous progress indicates the
   *                 start angle of the arc we want to draw.
   * @param {number} currPercentComplete The current progress indicates the end
   *                 angle of the arc we want to draw.
   * @param {boolean} isComplete Indicate whether enrollment is complete.
   * @public
   */
  setProgress(prevPercentComplete, currPercentComplete, isComplete) {
    if (this.isComplete_) {
      return;
    }
    this.isComplete_ = isComplete;
    this.cancelAnimations_();

    let nextPercentToDraw = prevPercentComplete;
    const endPercent = isComplete ? 100 : Math.min(100, currPercentComplete);
    // The value by which to update the progress percent each tick.
    const step = (endPercent - prevPercentComplete) /
        (ANIMATE_DURATION_MS / ANIMATE_TICKS_MS);

    // Function that is called every tick of the interval, draws the arc a bit
    // closer to the final destination each tick, until it reaches the final
    // destination.
    const doAnimate = () => {
      if (nextPercentToDraw >= endPercent) {
        if (this.progressAnimationIntervalId_) {
          clearInterval(this.progressAnimationIntervalId_);
          this.progressAnimationIntervalId_ = undefined;
        }
        nextPercentToDraw = endPercent;
      }

      this.clearCanvas_();
      this.drawProgressCircle_(nextPercentToDraw);
      if (!this.progressAnimationIntervalId_) {
        this.fire('cr-fingerprint-progress-arc-drawn');
      }
      nextPercentToDraw += step;
    };

    this.progressAnimationIntervalId_ =
        setInterval(doAnimate, ANIMATE_TICKS_MS);

    if (isComplete) {
      this.animateScanComplete_();
    } else {
      this.animateScanProgress_();
    }
  },

  /**
   * Controls the animation based on the value of |shouldPlay|.
   * @param {boolean} shouldPlay Will play the animation if true else pauses it.
   * @public
   */
  setPlay(shouldPlay) {
    const scanningAnimation =
        /** @type {CrLottieElement|HTMLElement} */ (this.$.scanningAnimation);
    scanningAnimation.setPlay(shouldPlay);
  },

  /** @public */
  isComplete() {
    return this.isComplete_;
  },

  /**
   * Draws an arc on the canvas element around the center with radius
   * |circleRadius|.
   * @param {number} startAngle The start angle of the arc we want to draw.
   * @param {number} endAngle The end angle of the arc we want to draw.
   * @param {string} color The color of the arc we want to draw. The string is
   *     in the format rgba(r',g',b',a'). r', g', b' are values from [0-255]
   *     and a' is a value from [0-1].
   * @private
   */
  drawArc_(startAngle, endAngle, color) {
    const c = this.$.canvas;
    const ctx = c.getContext('2d');

    ctx.beginPath();
    ctx.arc(c.width / 2, c.height / 2, this.circleRadius, startAngle, endAngle);
    ctx.lineWidth = PROGRESS_CIRCLE_STROKE_WIDTH;
    ctx.strokeStyle = color;
    ctx.stroke();
  },

  /**
   * Draws a circle on the canvas element around the center with radius
   * |circleRadius|. The first |currentPercent| of the circle, starting at the
   * top, is drawn with |PROGRESS_CIRCLE_FILL_COLOR|; the remainder of the
   * circle is drawn |PROGRESS_CIRCLE_BACKGROUND_COLOR|.
   * @param {number} currentPercent A value from [0-100] indicating the
   *     percentage of progress to display.
   * @private
   */
  drawProgressCircle_(currentPercent) {
    // Angles on HTML canvases start at 0 radians on the positive x-axis and
    // increase in the clockwise direction. We want to start at the top of the
    // circle, which is 3pi/2.
    const start = 3 * Math.PI / 2;
    const currentAngle = 2 * Math.PI * currentPercent / 100;

    // Drawing two arcs to form a circle gives a nicer look than drawing an arc
    // on top of a circle (i.e., compared to drawing a full background circle
    // first). If |currentAngle| is 0, draw from 3pi/2 to 7pi/2 explicitly;
    // otherwise, the regular draw from |start| + |currentAngle| to |start|
    // will do nothing.
    this.drawArc_(
        start, start + currentAngle,
        this.isDarkModeActive_ ? PROGRESS_CIRCLE_FILL_COLOR_DARK :
                                 PROGRESS_CIRCLE_FILL_COLOR_LIGHT);
    this.drawArc_(
        start + currentAngle, currentAngle <= 0 ? 7 * Math.PI / 2 : start,
        this.isDarkModeActive_ ? PROGRESS_CIRCLE_BACKGROUND_COLOR_DARK :
                                 PROGRESS_CIRCLE_BACKGROUND_COLOR_LIGHT);
    this.progressPercentDrawn_ = currentPercent;
  },

  /**
   * Updates the lottie animation taking into account the current state and
   * whether dark mode is enabled.
   * @private
   */
  updateAnimationAsset_() {
    const scanningAnimation =
        /** @type {CrLottieElement} */ (this.$.scanningAnimation);
    if (this.isComplete_) {
      scanningAnimation.animationUrl = this.isDarkModeActive_ ?
          FINGERPRINT_TICK_DARK_URL :
          FINGERPRINT_TICK_LIGHT_URL;
      return;
    }
    scanningAnimation.animationUrl =
        'chrome://theme/IDR_FINGERPRINT_ICON_ANIMATION';
  },

  /*
   * Cleans up any pending animation update created by setInterval().
   * @private
   */
  cancelAnimations_() {
    this.progressPercentDrawn_ = 0;
    if (this.progressAnimationIntervalId_) {
      clearInterval(this.progressAnimationIntervalId_);
      this.progressAnimationIntervalId_ = undefined;
    }
    if (this.updateTimerId_) {
      window.clearTimeout(this.updateTimerId_);
      this.updateTimerId_ = undefined;
    }
  },

  /**
   * Show animation for enrollment completion.
   * @private
   */
  animateScanComplete_() {
    const scanningAnimation =
        /** @type {CrLottieElement|HTMLElement} */ (this.$.scanningAnimation);
    scanningAnimation.singleLoop = true;
    scanningAnimation.classList.remove('translucent');
    this.updateAnimationAsset_();
    this.resizeCheckMark_(scanningAnimation);
    this.$.enrollmentDone.hidden = false;
  },

  /**
   * Show animation for enrollment in progress.
   * @private
   */
  animateScanProgress_() {
    this.$.enrollmentDone.hidden = false;
    this.updateTimerId_ = window.setTimeout(() => {
      this.$.enrollmentDone.hidden = true;
    }, FINGERPRINT_SCAN_SUCCESS_MS);
  },

  /**
   * Clear the canvas of any renderings.
   * @private
   */
  clearCanvas_() {
    const c = this.$.canvas;
    const ctx = c.getContext('2d');
    ctx.clearRect(0, 0, c.width, c.height);
  },

  /**
   * Update the size and position of the animation images.
   * @private
   */
  updateImages_() {
    this.resizeAndCenterIcon_(
        /** @type {!HTMLElement} */ (this.$.scanningAnimation));
    this.resizeAndCenterIcon_(
        /** @type {!HTMLElement} */ (this.$.enrollmentDone));
  },

  /**
   * Resize the icon based on the scale and place it in the center of the
   * fingerprint progress circle.
   * @param {!HTMLElement} target
   * @private
   */
  resizeAndCenterIcon_(target) {
    // Resize icon based on the default width/height and scale.
    target.style.width = ICON_WIDTH * this.scale_ + 'px';
    target.style.height = ICON_HEIGHT * this.scale_ + 'px';

    // Place in the center of the canvas.
    const left = this.$.canvas.width / 2 - ICON_WIDTH * this.scale_ / 2;
    const top = this.$.canvas.height / 2 - ICON_HEIGHT * this.scale_ / 2;
    target.style.left = left + 'px';
    target.style.top = top + 'px';
  },

  /**
   * Resize the checkmark based on the scale and place it in the left bottom
   * corner of fingerprint progress circle.
   * @param {!HTMLElement} target
   * @private
   */
  resizeCheckMark_(target) {
    // Resize checkmark based on the default size and scale.
    target.style.width = CHECK_MARK_SIZE * this.scale_ + 'px';
    target.style.height = CHECK_MARK_SIZE * this.scale_ + 'px';

    // Place it in the left bottom corner of fingerprint progress circle.
    const top = this.$.canvas.height / 2 + this.circleRadius -
        CHECK_MARK_SIZE * this.scale_;
    const left = this.$.canvas.width / 2 + this.circleRadius -
        CHECK_MARK_SIZE * this.scale_;
    target.style.left = left + 'px';
    target.style.top = top + 'px';
  },
});
/* #ignore */ console.warn('crbug/1173575, non-JS module files deprecated.');
})();
