/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Client viewport positioning class.
 */

goog.provide('goog.positioning.AbsolutePosition');

goog.require('goog.math.Coordinate');
goog.require('goog.positioning');
goog.require('goog.positioning.AbstractPosition');
goog.requireType('goog.math.Box');
goog.requireType('goog.math.Size');



/**
 * Encapsulates a popup position where the popup absolutely positioned by
 * setting the left/top style elements directly to the specified values.
 * The position is generally relative to the element's offsetParent. Normally,
 * this is the document body, but can be another element if the popup element
 * is scoped by an element with relative position.
 *
 * @param {number|!goog.math.Coordinate} arg1 Left position or coordinate.
 * @param {number=} opt_arg2 Top position.
 * @constructor
 * @extends {goog.positioning.AbstractPosition}
 */
goog.positioning.AbsolutePosition = function(arg1, opt_arg2) {
  'use strict';
  /**
   * Coordinate to position popup at.
   * @type {goog.math.Coordinate}
   */
  this.coordinate = arg1 instanceof goog.math.Coordinate ?
      arg1 :
      new goog.math.Coordinate(/** @type {number} */ (arg1), opt_arg2);
};
goog.inherits(
    goog.positioning.AbsolutePosition, goog.positioning.AbstractPosition);


/**
 * Repositions the popup according to the current state.
 *
 * @param {Element} movableElement The DOM element to position.
 * @param {goog.positioning.Corner} movableCorner The corner of the movable
 *     element that should be positioned at the specified position.
 * @param {goog.math.Box=} opt_margin A margin specified in pixels.
 * @param {goog.math.Size=} opt_preferredSize Preferred size of the
 *     movableElement.
 * @override
 */
goog.positioning.AbsolutePosition.prototype.reposition = function(
    movableElement, movableCorner, opt_margin, opt_preferredSize) {
  'use strict';
  goog.positioning.positionAtCoordinate(
      this.coordinate, movableElement, movableCorner, opt_margin, null, null,
      opt_preferredSize);
};
