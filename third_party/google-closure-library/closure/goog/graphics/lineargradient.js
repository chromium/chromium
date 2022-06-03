/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Represents a gradient to be used with a Graphics implementor.
 */


goog.provide('goog.graphics.LinearGradient');


goog.require('goog.asserts');
goog.require('goog.graphics.Fill');



/**
 * Creates an immutable linear gradient fill object.
 *
 * @param {number} x1 Start X position of the gradient.
 * @param {number} y1 Start Y position of the gradient.
 * @param {number} x2 End X position of the gradient.
 * @param {number} y2 End Y position of the gradient.
 * @param {string} color1 Start color of the gradient.
 * @param {string} color2 End color of the gradient.
 * @param {?number=} opt_opacity1 Start opacity of the gradient, both or neither
 *     of opt_opacity1 and opt_opacity2 have to be set.
 * @param {?number=} opt_opacity2 End opacity of the gradient.
 * @constructor
 * @extends {goog.graphics.Fill}
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 * @final
 */
goog.graphics.LinearGradient = function(
    x1, y1, x2, y2, color1, color2, opt_opacity1, opt_opacity2) {
  'use strict';
  /**
   * Start X position of the gradient.
   * @type {number}
   * @private
   */
  this.x1_ = x1;

  /**
   * Start Y position of the gradient.
   * @type {number}
   * @private
   */
  this.y1_ = y1;

  /**
   * End X position of the gradient.
   * @type {number}
   * @private
   */
  this.x2_ = x2;

  /**
   * End Y position of the gradient.
   * @type {number}
   * @private
   */
  this.y2_ = y2;

  /**
   * Start color of the gradient.
   * @type {string}
   * @private
   */
  this.color1_ = color1;

  /**
   * End color of the gradient.
   * @type {string}
   * @private
   */
  this.color2_ = color2;

  goog.asserts.assert(
      (typeof opt_opacity1 === 'number') == (typeof opt_opacity2 === 'number'),
      'Both or neither of opt_opacity1 and opt_opacity2 have to be set.');

  /**
   * Start opacity of the gradient.
   * @type {?number}
   * @private
   */
  this.opacity1_ = (opt_opacity1 !== undefined) ? opt_opacity1 : null;

  /**
   * End opacity of the gradient.
   * @type {?number}
   * @private
   */
  this.opacity2_ = (opt_opacity2 !== undefined) ? opt_opacity2 : null;
};
goog.inherits(goog.graphics.LinearGradient, goog.graphics.Fill);


/**
 * @return {number} The start X position of the gradient.
 */
goog.graphics.LinearGradient.prototype.getX1 = function() {
  'use strict';
  return this.x1_;
};


/**
 * @return {number} The start Y position of the gradient.
 */
goog.graphics.LinearGradient.prototype.getY1 = function() {
  'use strict';
  return this.y1_;
};


/**
 * @return {number} The end X position of the gradient.
 */
goog.graphics.LinearGradient.prototype.getX2 = function() {
  'use strict';
  return this.x2_;
};


/**
 * @return {number} The end Y position of the gradient.
 */
goog.graphics.LinearGradient.prototype.getY2 = function() {
  'use strict';
  return this.y2_;
};


/**
 * @override
 */
goog.graphics.LinearGradient.prototype.getColor1 = function() {
  'use strict';
  return this.color1_;
};


/**
 * @override
 */
goog.graphics.LinearGradient.prototype.getColor2 = function() {
  'use strict';
  return this.color2_;
};


/**
 * @return {?number} The start opacity of the gradient.
 */
goog.graphics.LinearGradient.prototype.getOpacity1 = function() {
  'use strict';
  return this.opacity1_;
};


/**
 * @return {?number} The end opacity of the gradient.
 */
goog.graphics.LinearGradient.prototype.getOpacity2 = function() {
  'use strict';
  return this.opacity2_;
};
