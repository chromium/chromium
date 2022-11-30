/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview A thin wrapper around the DOM element for ellipses.
 */


goog.provide('goog.graphics.EllipseElement');

goog.require('goog.graphics.StrokeAndFillElement');
goog.requireType('goog.graphics.AbstractGraphics');
goog.requireType('goog.graphics.Fill');
goog.requireType('goog.graphics.Stroke');



/**
 * Interface for a graphics ellipse element.
 * You should not construct objects from this constructor. The graphics
 * will return an implementation of this interface for you.
 * @param {Element} element The DOM element to wrap.
 * @param {goog.graphics.AbstractGraphics} graphics The graphics creating
 *     this element.
 * @param {goog.graphics.Stroke?} stroke The stroke to use for this element.
 * @param {goog.graphics.Fill?} fill The fill to use for this element.
 * @constructor
 * @extends {goog.graphics.StrokeAndFillElement}
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.EllipseElement = function(element, graphics, stroke, fill) {
  'use strict';
  goog.graphics.StrokeAndFillElement.call(
      this, element, graphics, stroke, fill);
};
goog.inherits(goog.graphics.EllipseElement, goog.graphics.StrokeAndFillElement);


/**
 * Update the center point of the ellipse.
 * @param {number} cx  Center X coordinate.
 * @param {number} cy  Center Y coordinate.
 */
goog.graphics.EllipseElement.prototype.setCenter = goog.abstractMethod;


/**
 * Update the radius of the ellipse.
 * @param {number} rx  Radius length for the x-axis.
 * @param {number} ry  Radius length for the y-axis.
 */
goog.graphics.EllipseElement.prototype.setRadius = goog.abstractMethod;
