/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview A thin wrapper around the DOM element for text elements.
 */


goog.provide('goog.graphics.TextElement');

goog.require('goog.graphics.StrokeAndFillElement');
goog.requireType('goog.graphics.AbstractGraphics');
goog.requireType('goog.graphics.Fill');
goog.requireType('goog.graphics.Stroke');



/**
 * Interface for a graphics text element.
 * You should not construct objects from this constructor. The graphics
 * will return an implementation of this interface for you.
 *
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
goog.graphics.TextElement = function(element, graphics, stroke, fill) {
  'use strict';
  goog.graphics.StrokeAndFillElement.call(
      this, element, graphics, stroke, fill);
};
goog.inherits(goog.graphics.TextElement, goog.graphics.StrokeAndFillElement);


/**
 * Update the displayed text of the element.
 * @param {string} text The text to draw.
 */
goog.graphics.TextElement.prototype.setText = goog.abstractMethod;
