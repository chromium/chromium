/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview A thick wrapper around ellipses.
 */


goog.provide('goog.graphics.ext.Ellipse');

goog.require('goog.graphics.ext.StrokeAndFillElement');
goog.requireType('goog.graphics.ext.Group');



/**
 * Wrapper for a graphics ellipse element.
 * @param {goog.graphics.ext.Group} group Parent for this element.
 * @constructor
 * @extends {goog.graphics.ext.StrokeAndFillElement}
 * @final
 */
goog.graphics.ext.Ellipse = function(group) {
  'use strict';
  // Initialize with some stock values.
  const wrapper = group.getGraphicsImplementation().drawEllipse(
      1, 1, 2, 2, null, null, group.getWrapper());
  goog.graphics.ext.StrokeAndFillElement.call(this, group, wrapper);
};
goog.inherits(
    goog.graphics.ext.Ellipse, goog.graphics.ext.StrokeAndFillElement);


/**
 * Redraw the ellipse.  Called when the coordinate system is changed.
 * @protected
 * @override
 */
goog.graphics.ext.Ellipse.prototype.redraw = function() {
  'use strict';
  goog.graphics.ext.Ellipse.superClass_.redraw.call(this);

  // Our position is already transformed in transform_, but because this is an
  // ellipse we need to position the center.
  const xRadius = this.getWidth() / 2;
  const yRadius = this.getHeight() / 2;
  const wrapper = this.getWrapper();
  wrapper.setCenter(xRadius, yRadius);
  wrapper.setRadius(xRadius, yRadius);
};
