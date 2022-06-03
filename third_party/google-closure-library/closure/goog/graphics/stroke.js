/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Represents a stroke object for goog.graphics.
 */


goog.provide('goog.graphics.Stroke');



/**
 * Creates an immutable stroke object.
 *
 * @param {number|string} width The width of the stroke.
 * @param {string} color The color of the stroke.
 * @param {number=} opt_opacity The opacity of the background fill. The value
 *    must be greater than or equal to zero (transparent) and less than or
 *    equal to 1 (opaque).
 * @constructor
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.Stroke = function(width, color, opt_opacity) {
  'use strict';
  /**
   * The width of the stroke.
   * @type {number|string}
   * @private
   */
  this.width_ = width;


  /**
   * The color with which to fill.
   * @type {string}
   * @private
   */
  this.color_ = color;


  /**
   * The opacity of the fill.
   * @type {number}
   * @private
   */
  this.opacity_ = opt_opacity == null ? 1.0 : opt_opacity;
};


/**
 * @return {number|string} The width of this stroke.
 */
goog.graphics.Stroke.prototype.getWidth = function() {
  'use strict';
  return this.width_;
};


/**
 * @return {string} The color of this stroke.
 */
goog.graphics.Stroke.prototype.getColor = function() {
  'use strict';
  return this.color_;
};


/**
 * @return {number} The opacity of this fill.
 */
goog.graphics.Stroke.prototype.getOpacity = function() {
  'use strict';
  return this.opacity_;
};
