/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Represents a font to be used with a Renderer.
 * @see ../demos/graphics/basicelements.html
 */


goog.provide('goog.graphics.Font');



/**
 * This class represents a font to be used with a renderer.
 * @param {number} size  The font size.
 * @param {string} family  The font family.
 * @constructor
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 * @final
 */
goog.graphics.Font = function(size, family) {
  'use strict';
  /**
   * Font size.
   * @type {number}
   */
  this.size = size;
  // TODO(arv): Is this in pixels or drawing units based on the coord size?

  /**
   * The name of the font family to use, can be a comma separated string.
   * @type {string}
   */
  this.family = family;
};


/**
 * Indication if text should be bolded
 * @type {boolean}
 */
goog.graphics.Font.prototype.bold = false;


/**
 * Indication if text should be in italics
 * @type {boolean}
 */
goog.graphics.Font.prototype.italic = false;
