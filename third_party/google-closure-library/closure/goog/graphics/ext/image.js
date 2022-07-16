/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview A thick wrapper around images.
 */


goog.provide('goog.graphics.ext.Image');

goog.require('goog.graphics.ext.Element');
goog.requireType('goog.graphics.ext.Group');



/**
 * Wrapper for a graphics image element.
 * @param {goog.graphics.ext.Group} group Parent for this element.
 * @param {string} src The path to the image to display.
 * @constructor
 * @extends {goog.graphics.ext.Element}
 * @final
 */
goog.graphics.ext.Image = function(group, src) {
  'use strict';
  // Initialize with some stock values.
  const wrapper = group.getGraphicsImplementation().drawImage(
      0, 0, 1, 1, src, group.getWrapper());
  goog.graphics.ext.Element.call(this, group, wrapper);
};
goog.inherits(goog.graphics.ext.Image, goog.graphics.ext.Element);


/**
 * Redraw the image.  Called when the coordinate system is changed.
 * @protected
 * @override
 */
goog.graphics.ext.Image.prototype.redraw = function() {
  'use strict';
  goog.graphics.ext.Image.superClass_.redraw.call(this);

  // Our position is already handled bu transform_.
  this.getWrapper().setSize(this.getWidth(), this.getHeight());
};


/**
 * Update the source of the image.
 * @param {string} src  Source of the image.
 */
goog.graphics.ext.Image.prototype.setSource = function(src) {
  'use strict';
  this.getWrapper().setSource(src);
};
