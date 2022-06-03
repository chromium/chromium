/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Client positioning class.
 */

goog.provide('goog.positioning.ViewportPosition');

goog.require('goog.math.Coordinate');
goog.require('goog.positioning');
goog.require('goog.positioning.AbstractPosition');
goog.require('goog.positioning.Corner');
goog.require('goog.style');
goog.requireType('goog.math.Box');
goog.requireType('goog.math.Size');



/**
 * Encapsulates a popup position where the popup is positioned according to
 * coordinates relative to the  element's viewport (page). This calculates the
 * correct position to use even if the element is relatively positioned to some
 * other element.
 *
 * @param {number|goog.math.Coordinate} arg1 Left position or coordinate.
 * @param {number=} opt_arg2 Top position.
 * @constructor
 * @extends {goog.positioning.AbstractPosition}
 */
goog.positioning.ViewportPosition = function(arg1, opt_arg2) {
  'use strict';
  this.coordinate = arg1 instanceof goog.math.Coordinate ?
      arg1 :
      new goog.math.Coordinate(/** @type {number} */ (arg1), opt_arg2);
};
goog.inherits(
    goog.positioning.ViewportPosition, goog.positioning.AbstractPosition);


/**
 * Repositions the popup according to the current state
 *
 * @param {Element} element The DOM element of the popup.
 * @param {goog.positioning.Corner} popupCorner The corner of the popup
 *     element that that should be positioned adjacent to the anchorElement.
 * @param {goog.math.Box=} opt_margin A margin specified in pixels.
 * @param {goog.math.Size=} opt_preferredSize Preferred size of the element.
 * @override
 */
goog.positioning.ViewportPosition.prototype.reposition = function(
    element, popupCorner, opt_margin, opt_preferredSize) {
  'use strict';
  goog.positioning.positionAtAnchor(
      goog.style.getClientViewportElement(element),
      goog.positioning.Corner.TOP_LEFT, element, popupCorner, this.coordinate,
      opt_margin, null, opt_preferredSize);
};
