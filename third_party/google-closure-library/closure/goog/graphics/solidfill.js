/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Represents a solid color fill goog.graphics.
 */


goog.provide('goog.graphics.SolidFill');


goog.require('goog.graphics.Fill');



/**
 * Creates an immutable solid color fill object.
 *
 * @param {string} color The color of the background.
 * @param {number=} opt_opacity The opacity of the background fill. The value
 *    must be greater than or equal to zero (transparent) and less than or
 *    equal to 1 (opaque).
 * @constructor
 * @extends {goog.graphics.Fill}
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.SolidFill = function(color, opt_opacity) {
  'use strict';
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
goog.inherits(goog.graphics.SolidFill, goog.graphics.Fill);


/**
 * @return {string} The color of this fill.
 */
goog.graphics.SolidFill.prototype.getColor = function() {
  'use strict';
  return this.color_;
};


/**
 * @return {number} The opacity of this fill.
 */
goog.graphics.SolidFill.prototype.getOpacity = function() {
  'use strict';
  return this.opacity_;
};
