/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview A thick wrapper around shapes with custom paths.
 */



// TODO(user): We're trying to migrate all ES5 subclasses of Closure
// Library to ES6. In ES6 this cannot be referenced before super is called. This
// file has at least one this before a super call (in ES5) and cannot be
// automatically upgraded to ES6 as a result. Please fix this if you have a
// chance. Note: This can sometimes be caused by not calling the super
// constructor at all. You can run the conversion tool yourself to see what it
// does on this file: blaze run //javascript/refactoring/es6_classes:convert.

goog.provide('goog.graphics.ext.Shape');

goog.require('goog.graphics.ext.StrokeAndFillElement');
goog.requireType('goog.graphics.Path');
goog.requireType('goog.graphics.ext.Group');
goog.requireType('goog.graphics.ext.Path');
goog.requireType('goog.math.Rect');



/**
 * Wrapper for a graphics shape element.
 * @param {goog.graphics.ext.Group} group Parent for this element.
 * @param {!goog.graphics.ext.Path} path  The path to draw.
 * @param {boolean=} opt_autoSize Optional flag to specify the path should
 *     automatically resize to fit the element.  Defaults to false.
 * @constructor
 * @extends {goog.graphics.ext.StrokeAndFillElement}
 * @final
 */
goog.graphics.ext.Shape = function(group, path, opt_autoSize) {
  'use strict';
  this.autoSize_ = !!opt_autoSize;

  const graphics = group.getGraphicsImplementation();
  const wrapper = graphics.drawPath(path, null, null, group.getWrapper());
  goog.graphics.ext.StrokeAndFillElement.call(this, group, wrapper);
  this.setPath(path);
};
goog.inherits(goog.graphics.ext.Shape, goog.graphics.ext.StrokeAndFillElement);


/**
 * Whether or not to automatically resize the shape's path when the element
 * itself is resized.
 * @type {boolean}
 * @private
 */
goog.graphics.ext.Shape.prototype.autoSize_ = false;


/**
 * The original path, specified by the caller.
 * @type {goog.graphics.Path}
 * @private
 */
goog.graphics.ext.Shape.prototype.path_;


/**
 * The bounding box of the original path.
 * @type {goog.math.Rect?}
 * @private
 */
goog.graphics.ext.Shape.prototype.boundingBox_ = null;


/**
 * The scaled path.
 * @type {goog.graphics.Path}
 * @private
 */
goog.graphics.ext.Shape.prototype.scaledPath_;


/**
 * Get the path drawn by this shape.
 * @return {goog.graphics.Path?} The path drawn by this shape.
 */
goog.graphics.ext.Shape.prototype.getPath = function() {
  'use strict';
  return this.path_;
};


/**
 * Set the path to draw.
 * @param {goog.graphics.ext.Path} path The path to draw.
 */
goog.graphics.ext.Shape.prototype.setPath = function(path) {
  'use strict';
  this.path_ = path;

  if (this.autoSize_) {
    this.boundingBox_ = path.getBoundingBox();
  }

  this.scaleAndSetPath_();
};


/**
 * Scale the internal path to fit.
 * @private
 */
goog.graphics.ext.Shape.prototype.scaleAndSetPath_ = function() {
  'use strict';
  this.scaledPath_ = this.boundingBox_ ?
      this.path_.clone().modifyBounds(
          -this.boundingBox_.left, -this.boundingBox_.top,
          this.getWidth() / (this.boundingBox_.width || 1),
          this.getHeight() / (this.boundingBox_.height || 1)) :
      this.path_;

  const wrapper = this.getWrapper();
  if (wrapper) {
    wrapper.setPath(this.scaledPath_);
  }
};


/**
 * Redraw the ellipse.  Called when the coordinate system is changed.
 * @protected
 * @override
 */
goog.graphics.ext.Shape.prototype.redraw = function() {
  'use strict';
  goog.graphics.ext.Shape.superClass_.redraw.call(this);
  if (this.autoSize_) {
    this.scaleAndSetPath_();
  }
};


/**
 * @return {boolean} Whether the shape is parent dependent.
 * @protected
 * @override
 */
goog.graphics.ext.Shape.prototype.checkParentDependent = function() {
  'use strict';
  return this.autoSize_ ||
      goog.graphics.ext.Shape.superClass_.checkParentDependent.call(this);
};
