/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview A thick wrapper around rectangles.
 */


goog.provide('goog.graphics.ext.Rectangle');

goog.require('goog.graphics.ext.StrokeAndFillElement');
goog.requireType('goog.graphics.ext.Group');



/**
 * Wrapper for a graphics rectangle element.
 * @param {goog.graphics.ext.Group} group Parent for this element.
 * @constructor
 * @extends {goog.graphics.ext.StrokeAndFillElement}
 * @final
 */
goog.graphics.ext.Rectangle = function(group) {
  'use strict';
  // Initialize with some stock values.
  const wrapper = group.getGraphicsImplementation().drawRect(
      0, 0, 1, 1, null, null, group.getWrapper());
  goog.graphics.ext.StrokeAndFillElement.call(this, group, wrapper);
};
goog.inherits(
    goog.graphics.ext.Rectangle, goog.graphics.ext.StrokeAndFillElement);


/**
 * Redraw the rectangle.  Called when the coordinate system is changed.
 * @protected
 * @override
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.graphics.ext.Rectangle.prototype.redraw = function() {
  'use strict';
  goog.graphics.ext.Rectangle.superClass_.redraw.call(this);

  // Our position is already handled by transform_.
  this.getWrapper().setSize(this.getWidth(), this.getHeight());
};
