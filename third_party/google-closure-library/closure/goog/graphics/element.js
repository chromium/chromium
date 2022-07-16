/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview A thin wrapper around the DOM element returned from
 * the different draw methods of the graphics implementation, and
 * all interfaces that the various element types support.
 */


goog.provide('goog.graphics.Element');

goog.require('goog.asserts');
goog.require('goog.events');
goog.require('goog.events.EventTarget');
goog.require('goog.events.Listenable');
goog.require('goog.graphics.AffineTransform');
goog.require('goog.math');
goog.requireType('goog.graphics.AbstractGraphics');



/**
 * Base class for a thin wrapper around the DOM element returned from
 * the different draw methods of the graphics.
 * You should not construct objects from this constructor. The graphics
 * will return the object for you.
 * @param {Element} element  The DOM element to wrap.
 * @param {goog.graphics.AbstractGraphics} graphics  The graphics creating
 *     this element.
 * @constructor
 * @extends {goog.events.EventTarget}
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.Element = function(element, graphics) {
  'use strict';
  goog.events.EventTarget.call(this);
  this.element_ = element;
  this.graphics_ = graphics;
  // Overloading EventTarget field to state that this is not a custom event.
  // TODO(user) Should be handled in EventTarget.js (see bug 846824).
  this[goog.events.Listenable.IMPLEMENTED_BY_PROP] = false;
};
goog.inherits(goog.graphics.Element, goog.events.EventTarget);


/**
 * The graphics object that contains this element.
 * @type {goog.graphics.AbstractGraphics?}
 * @private
 */
goog.graphics.Element.prototype.graphics_ = null;


/**
 * The native browser element this class wraps.
 * @type {?Element}
 * @private
 */
goog.graphics.Element.prototype.element_ = null;


/**
 * The transformation applied to this element.
 * @type {goog.graphics.AffineTransform?}
 * @private
 */
goog.graphics.Element.prototype.transform_ = null;


/**
 * Returns the underlying object.
 * @return {Element} The underlying element.
 */
goog.graphics.Element.prototype.getElement = function() {
  'use strict';
  return this.element_;
};


/**
 * Returns the graphics.
 * @return {goog.graphics.AbstractGraphics} The graphics that created the
 *     element.
 */
goog.graphics.Element.prototype.getGraphics = function() {
  'use strict';
  return this.graphics_;
};


/**
 * Set the translation and rotation of the element.
 *
 * If a more general affine transform is needed than this provides
 * (e.g. skew and scale) then use setTransform.
 * @param {number} x The x coordinate of the translation transform.
 * @param {number} y The y coordinate of the translation transform.
 * @param {number} rotate The angle of the rotation transform.
 * @param {number} centerX The horizontal center of the rotation transform.
 * @param {number} centerY The vertical center of the rotation transform.
 */
goog.graphics.Element.prototype.setTransformation = function(
    x, y, rotate, centerX, centerY) {
  'use strict';
  this.transform_ =
      goog.graphics.AffineTransform
          .getRotateInstance(goog.math.toRadians(rotate), centerX, centerY)
          .translate(x, y);
  this.getGraphics().setElementTransform(this, x, y, rotate, centerX, centerY);
};


/**
 * @return {!goog.graphics.AffineTransform} The transformation applied to
 *     this element.
 */
goog.graphics.Element.prototype.getTransform = function() {
  'use strict';
  return this.transform_ ? this.transform_.clone() :
                           new goog.graphics.AffineTransform();
};


/**
 * Set the affine transform of the element.
 * @param {!goog.graphics.AffineTransform} affineTransform The
 *     transformation applied to this element.
 */
goog.graphics.Element.prototype.setTransform = function(affineTransform) {
  'use strict';
  this.transform_ = affineTransform.clone();
  this.getGraphics().setElementAffineTransform(this, affineTransform);
};


/** @override */
goog.graphics.Element.prototype.addEventListener = function(
    type, handler, opt_capture, opt_handlerScope) {
  'use strict';
  goog.events.listen(
      this.element_, type, handler, opt_capture, opt_handlerScope);
};


/** @override */
goog.graphics.Element.prototype.removeEventListener = function(
    type, handler, opt_capture, opt_handlerScope) {
  'use strict';
  goog.events.unlisten(
      this.element_, type, handler, opt_capture, opt_handlerScope);
};


/** @override */
goog.graphics.Element.prototype.disposeInternal = function() {
  'use strict';
  goog.graphics.Element.superClass_.disposeInternal.call(this);
  goog.asserts.assert(this.element_);
  goog.events.removeAll(this.element_);
};
