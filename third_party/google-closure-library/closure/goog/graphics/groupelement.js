/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview A thin wrapper around the DOM element for graphics groups.
 */


goog.provide('goog.graphics.GroupElement');

goog.require('goog.graphics.Element');
goog.requireType('goog.graphics.AbstractGraphics');



/**
 * Interface for a graphics group element.
 * You should not construct objects from this constructor. The graphics
 * will return the object for you.
 * @param {Element} element The DOM element to wrap.
 * @param {goog.graphics.AbstractGraphics} graphics The graphics creating
 *     this element.
 * @constructor
 * @extends {goog.graphics.Element}
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.GroupElement = function(element, graphics) {
  'use strict';
  goog.graphics.Element.call(this, element, graphics);
};
goog.inherits(goog.graphics.GroupElement, goog.graphics.Element);


/**
 * Remove all drawing elements from the group.
 */
goog.graphics.GroupElement.prototype.clear = goog.abstractMethod;


/**
 * Set the size of the group element.
 * @param {number|string} width The width of the group element.
 * @param {number|string} height The height of the group element.
 */
goog.graphics.GroupElement.prototype.setSize = goog.abstractMethod;
