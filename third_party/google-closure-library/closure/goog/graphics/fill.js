/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Represents a fill goog.graphics.
 */


goog.provide('goog.graphics.Fill');



/**
 * Creates a fill object
 * @constructor
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.Fill = function() {};


/**
 * @return {string} The start color of a gradient fill.
 */
goog.graphics.Fill.prototype.getColor1 = goog.abstractMethod;


/**
 * @return {string} The end color of a gradient fill.
 */
goog.graphics.Fill.prototype.getColor2 = goog.abstractMethod;
