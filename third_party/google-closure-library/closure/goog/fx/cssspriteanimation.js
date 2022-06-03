/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview An animation class that animates CSS sprites by changing the
 * CSS background-position.
 *
 * @see ../demos/cssspriteanimation.html
 */

goog.provide('goog.fx.CssSpriteAnimation');

goog.require('goog.fx.Animation');
goog.requireType('goog.math.Box');
goog.requireType('goog.math.Size');



/**
 * This animation class is used to animate a CSS sprite (moving a background
 * image).  This moves through a series of images in a single image sprite. By
 * default, the animation loops when done.  Looping can be disabled by setting
 * `opt_disableLoop` and results in the animation stopping on the last
 * image in the image sprite.  You should set up the {@code background-image}
 * and size in a CSS rule for the relevant element.
 *
 * @param {Element} element The HTML element to animate the background for.
 * @param {goog.math.Size} size The size of one image in the image sprite.
 * @param {goog.math.Box} box The box describing the layout of the sprites to
 *     use in the large image.  The sprites can be position horizontally or
 *     vertically and using a box here allows the implementation to know which
 *     way to go.
 * @param {number} time The duration in milliseconds for one iteration of the
 *     animation.  For example, if the sprite contains 4 images and the duration
 *     is set to 400ms then each sprite will be displayed for 100ms.
 * @param {function(number) : number=} opt_acc Acceleration function,
 *    returns 0-1 for inputs 0-1.  This can be used to make certain frames be
 *    shown for a longer period of time.
 * @param {boolean=} opt_disableLoop Whether the animation should be halted
 *    after a single loop of the images in the sprite.
 *
 * @constructor
 * @struct
 * @extends {goog.fx.Animation}
 * @final
 */
goog.fx.CssSpriteAnimation = function(
    element, size, box, time, opt_acc, opt_disableLoop) {
  'use strict';
  var start = [box.left, box.top];
  // We never draw for the end so we do not need to subtract for the size
  var end = [box.right, box.bottom];
  goog.fx.CssSpriteAnimation.base(
      this, 'constructor', start, end, time, opt_acc);

  /**
   * HTML element that will be used in the animation.
   * @type {Element}
   * @private
   */
  this.element_ = element;

  /**
   * The size of an individual sprite in the image sprite.
   * @type {goog.math.Size}
   * @private
   */
  this.size_ = size;

  /**
   * Whether the animation should be halted after a single loop of the images
   * in the sprite.
   * @type {boolean}
   * @private
   */
  this.disableLoop_ = !!opt_disableLoop;
};
goog.inherits(goog.fx.CssSpriteAnimation, goog.fx.Animation);


/** @override */
goog.fx.CssSpriteAnimation.prototype.onAnimate = function() {
  'use strict';
  // Round to nearest sprite.
  var x = -Math.floor(this.coords[0] / this.size_.width) * this.size_.width;
  var y = -Math.floor(this.coords[1] / this.size_.height) * this.size_.height;
  this.element_.style.backgroundPosition = x + 'px ' + y + 'px';

  goog.fx.CssSpriteAnimation.base(this, 'onAnimate');
};


/** @override */
goog.fx.CssSpriteAnimation.prototype.onFinish = function() {
  'use strict';
  if (!this.disableLoop_) {
    this.play(true);
  }
  goog.fx.CssSpriteAnimation.base(this, 'onFinish');
};


/**
 * Clears the background position style set directly on the element
 * by the animation. Allows to apply CSS styling for background position on the
 * same element when the sprite animation is not runniing.
 */
goog.fx.CssSpriteAnimation.prototype.clearSpritePosition = function() {
  'use strict';
  var style = this.element_.style;
  style.backgroundPosition = '';

  if (typeof style.backgroundPositionX != 'undefined') {
    // IE needs to clear x and y to actually clear the position
    style.backgroundPositionX = '';
    style.backgroundPositionY = '';
  }
};


/** @override */
goog.fx.CssSpriteAnimation.prototype.disposeInternal = function() {
  'use strict';
  goog.fx.CssSpriteAnimation.superClass_.disposeInternal.call(this);
  this.element_ = null;
};
