/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview A thin wrapper around the DOM element for elements with a
 * stroke and fill.
 */


goog.provide('goog.graphics.StrokeAndFillElement');

goog.require('goog.graphics.Element');
goog.requireType('goog.graphics.AbstractGraphics');
goog.requireType('goog.graphics.Fill');
goog.requireType('goog.graphics.Stroke');



/**
 * Interface for a graphics element with a stroke and fill.
 * This is the base interface for ellipse, rectangle and other
 * shape interfaces.
 * You should not construct objects from this constructor. The graphics
 * will return an implementation of this interface for you.
 *
 * @param {Element} element The DOM element to wrap.
 * @param {goog.graphics.AbstractGraphics} graphics The graphics creating
 *     this element.
 * @param {goog.graphics.Stroke?} stroke The stroke to use for this element.
 * @param {goog.graphics.Fill?} fill The fill to use for this element.
 * @constructor
 * @extends {goog.graphics.Element}
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.StrokeAndFillElement = function(element, graphics, stroke, fill) {
  'use strict';
  goog.graphics.Element.call(this, element, graphics);
  this.setStroke(stroke);
  this.setFill(fill);
};
goog.inherits(goog.graphics.StrokeAndFillElement, goog.graphics.Element);


/**
 * The latest fill applied to this element.
 * @type {goog.graphics.Fill?}
 * @protected
 */
goog.graphics.StrokeAndFillElement.prototype.fill = null;


/**
 * The latest stroke applied to this element.
 * @type {goog.graphics.Stroke?}
 * @private
 */
goog.graphics.StrokeAndFillElement.prototype.stroke_ = null;


/**
 * Sets the fill for this element.
 * @param {goog.graphics.Fill?} fill The fill object.
 */
goog.graphics.StrokeAndFillElement.prototype.setFill = function(fill) {
  'use strict';
  this.fill = fill;
  this.getGraphics().setElementFill(this, fill);
};


/**
 * @return {goog.graphics.Fill?} fill The fill object.
 */
goog.graphics.StrokeAndFillElement.prototype.getFill = function() {
  'use strict';
  return this.fill;
};


/**
 * Sets the stroke for this element.
 * @param {goog.graphics.Stroke?} stroke The stroke object.
 */
goog.graphics.StrokeAndFillElement.prototype.setStroke = function(stroke) {
  'use strict';
  this.stroke_ = stroke;
  this.getGraphics().setElementStroke(this, stroke);
};


/**
 * @return {goog.graphics.Stroke?} stroke The stroke object.
 */
goog.graphics.StrokeAndFillElement.prototype.getStroke = function() {
  'use strict';
  return this.stroke_;
};


/**
 * Re-strokes the element to react to coordinate size changes.
 */
goog.graphics.StrokeAndFillElement.prototype.reapplyStroke = function() {
  'use strict';
  if (this.stroke_) {
    this.setStroke(this.stroke_);
  }
};
