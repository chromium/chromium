// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
const DEFAULT_CANVAS_CIRCLE_RADIUS = 114;

/**
 * The default height of the icon located in the center of fingerprint progress
 * circle.
 * @type {number}
 */
const ICON_HEIGHT = 118;

/**
 * The default width of the icon located in the center of fingerprint progress
 * circle.
 * @type {number}
 */
const ICON_WIDTH = 106;

/**
 * The default size of the checkmark located in the left bottom corner of
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
 * The thickness of the add fingerprint progress circle.
 * @type {number}
 */
const CANVAS_CIRCLE_STROKE_WIDTH = 4;

/**
 * The color of the canvas circle background.
 * @type {string}
 */
const CANVAS_CIRCLE_BACKGROUND_COLOR = 'rgba(218, 220, 224, 1.0)';

/**
 * The color of the arc/circle which indicates setup progress.
 * @type {string}
 */
const CANVAS_CIRCLE_PROGRESS_COLOR = 'rgba(66, 133, 224, 1.0)';

Polymer({
  is: 'cr-fingerprint-progress-arc',

  properties: {
    /**
     * Radius of the add fingerprint progress circle being displayed.
     * @type {number}
     */
    circleRadius: {
      type: Number,
      value: DEFAULT_CANVAS_CIRCLE_RADIUS,
    },

    /**
     * Scale factor based the configured radius (circleRadius) vs the default
     * radius (DEFAULT_CANVAS_CIRCLE_RADIUS).
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
  },

  // Also put these values as member values so they can be overridden by tests
  // and the tests do not need to be changed every time the UI is.
  /** @private {number} */
  canvasCircleStrokeWidth_: CANVAS_CIRCLE_STROKE_WIDTH,
  /** @private {string} */
  canvasCircleBackgroundColor_: CANVAS_CIRCLE_BACKGROUND_COLOR,
  /** @private {string} */
  canvasCircleProgressColor_: CANVAS_CIRCLE_PROGRESS_COLOR,

  /**
   * Animation ID for fingerprint scan progress bar.
   * @type {number|undefined}
   * @private
   */
  progressAnimationIntervalId_: undefined,

  /**
   * Timer ID for fingerprint scan success update.
   * @type {number|undefined}
   * @private
   */
  updateTimerId_: undefined,

  /** @override */
  attached: function() {
    this.scale_ = this.circleRadius / DEFAULT_CANVAS_CIRCLE_RADIUS;
    this.updateImages_();
  },

  /**
   * Draws an arc on the canvas element around the center with radius
   * |circleRadius|.
   * @param {number} startAngle The start angle of the arc we want to draw.
   * @param {number} endAngle The end angle of the arc we want to draw.
   * @param {string} color The color of the arc we want to draw. The string is
   *     in the format rgba(r',g',b',a'). r', g', b' are values from [0-255]
   *     and a' is a value from [0-1].
   */
  drawArc: function(startAngle, endAngle, color) {
    const c = this.$.canvas;
    const ctx = c.getContext('2d');

    ctx.beginPath();
    ctx.arc(c.width / 2, c.height / 2, this.circleRadius, startAngle, endAngle);
    ctx.lineWidth = this.canvasCircleStrokeWidth_;
    ctx.strokeStyle = color;
    ctx.stroke();
  },

  /**
   * Draws a circle on the canvas element around the center with radius
   * |circleRadius| and color |CANVAS_CIRCLE_BACKGROUND_COLOR|.
   */
  drawBackgroundCircle: function() {
    this.drawArc(0, 2 * Math.PI, this.canvasCircleBackgroundColor_);
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
   */
  setProgress: function(prevPercentComplete, currPercentComplete, isComplete) {
    if (this.isComplete_) {
      return;
    }
    this.isComplete_ = isComplete;
    this.cancelAnimations_();

    const slice = 2 * Math.PI / 100;
    const startAngle = prevPercentComplete * slice;
    const endAngle = isComplete ?
        2 * Math.PI :
        Math.min(2 * Math.PI, currPercentComplete * slice);
    let currentAngle = startAngle;
    // The value to update the angle by each tick.
    const step =
        (endAngle - startAngle) / (ANIMATE_DURATION_MS / ANIMATE_TICKS_MS);
    // Function that is called every tick of the interval, draws the arc a bit
    // closer to the final destination each tick, until it reaches the final
    // destination.
    const doAnimate = () => {
      if (currentAngle >= endAngle) {
        if (this.progressAnimationIntervalId_) {
          clearInterval(this.progressAnimationIntervalId_);
          this.progressAnimationIntervalId_ = undefined;
        }
        currentAngle = endAngle;
      }

      // Clears the canvas and draws the new progress circle.
      this.clearCanvas_();
      // Drawing two arcs to form a circle gives a nicer look than drawing
      // an arc on top of a circle. If |currentAngle| is 0, draw from
      // |start| + |currentAngle| to 7 * Math.PI / 2 (start is 3 * Math.PI /
      // 2) otherwise the regular draw from |start| to |currentAngle| will
      // draw nothing which will cause a flicker for one frame.
      this.drawArc(
          start, start + currentAngle, this.canvasCircleProgressColor_);
      this.drawArc(
          start + currentAngle, currentAngle <= 0 ? 7 * Math.PI / 2 : start,
          this.canvasCircleBackgroundColor_);
      currentAngle += step;
    };

    this.progressAnimationIntervalId_ =
        setInterval(doAnimate, ANIMATE_TICKS_MS);
    // Circles on html canvas have 0 radians on the positive x-axis and go in
    // clockwise direction. We want to start at the top of the circle which is
    // 3pi/2.
    const start = 3 * Math.PI / 2;

    if (isComplete) {
      this.animateScanComplete_();
    } else {
      this.animateScanProgress_();
    }
  },

  /*
   * Cleans up any pending animation update created by setInterval().
   * @private
   */
  cancelAnimations_: function() {
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
  animateScanComplete_: function() {
    this.$.checkmarkDiv.hidden = false;
    this.$.enrollmentDone.hidden = false;
    this.$.scanningAnimation.hidden = true;
    this.$.enrollmentDone.style.opacity = 1;
  },

  /**
   * Show animation for enrollment in progress.
   * @private
   */
  animateScanProgress_: function() {
    this.$.enrollmentDone.hidden = false;
    this.$.enrollmentDone.style.opacity = 0.3;
    this.$.scanningAnimation.hidden = true;
    this.updateTimerId_ = window.setTimeout(() => {
      this.$.enrollmentDone.hidden = true;
      this.$.enrollmentDone.style.opacity = 1;
      this.$.scanningAnimation.hidden = false;
    }, FINGERPRINT_SCAN_SUCCESS_MS);
  },

  /**
   * Clear the canvas of any renderings.
   * @private
   */
  clearCanvas_: function() {
    const c = this.$.canvas;
    const ctx = c.getContext('2d');
    ctx.clearRect(0, 0, c.width, c.height);
  },

  /**
   * Reset the element to initial state, when the enrollment just starts.
   */
  reset: function() {
    this.cancelAnimations_();
    this.clearCanvas_();
    this.isComplete_ = false;
    this.drawBackgroundCircle();
    this.$.enrollmentDone.hidden = true;
    this.$.scanningAnimation.hidden = false;
    this.$.checkmarkDiv.hidden = true;
  },

  /**
   * Update the size and position of the animation images.
   * @private
   */
  updateImages_: function() {
    this.resizeAndCenterIcon_(this.$.scanningAnimation);
    this.resizeAndCenterIcon_(this.$.enrollmentDone);
    this.resizeCheckMark_(this.$.checkmarkAnimation);
  },

  /**
   * Resize the icon based on the scale and place it in the center of the
   * fingerprint progress circle.
   * @param {!HTMLElement} target
   * @private
   */
  resizeAndCenterIcon_: function(target) {
    // Resize icon based on the default width/height and scale.
    target.style.width = ICON_WIDTH * this.scale_ + 'px';
    target.style.height = ICON_HEIGHT * this.scale_ + 'px';

    // Place in the center of the canvas.
    const left = this.$.canvas.width / 2 - ICON_WIDTH * this.scale_ / 2;
    const top = 0 - ICON_HEIGHT * this.scale_ / 2 - this.$.canvas.height / 2;
    target.style.left = left + 'px';
    target.style.top = top + 'px';
  },

  /**
   * Resize the checkmark based on the scale and place it in the left bottom
   * corner of fingerprint progress circle.
   * @param {!HTMLElement} target
   * @private
   */
  resizeCheckMark_: function(target) {
    // Resize checkmark based on the default size and scale.
    target.style.width = CHECK_MARK_SIZE * this.scale_ + 'px';
    target.style.height = CHECK_MARK_SIZE * this.scale_ + 'px';

    // Place it in the left bottom corner of fingerprint progress circle.
    const top = 0 -
        (CHECK_MARK_SIZE * this.scale_ + this.$.canvas.height / 2 -
         this.circleRadius);
    const left = this.$.canvas.width / 2 + this.circleRadius -
        CHECK_MARK_SIZE * this.scale_;
    target.style.left = left + 'px';
    target.style.top = top + 'px';
  },

  /** @public */
  isComplete: function() {
    return this.isComplete_;
  },
});
})();
