/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A thicker wrapper around the DOM element returned from
 * the different draw methods of the graphics implementation, and
 * all interfaces that the various element types support.
 */

goog.provide('goog.graphics.ext.Element');

goog.require('goog.events.EventTarget');
goog.require('goog.functions');
goog.require('goog.graphics.ext.coordinates');
goog.requireType('goog.graphics.AbstractGraphics');
goog.requireType('goog.graphics.Element');
goog.requireType('goog.graphics.ext.Graphics');
goog.requireType('goog.graphics.ext.Group');



/**
 * Base class for a wrapper around the goog.graphics wrapper that enables
 * more advanced functionality.
 * @param {goog.graphics.ext.Group?} group Parent for this element.
 * @param {goog.graphics.Element} wrapper The thin wrapper to wrap.
 * @constructor
 * @extends {goog.events.EventTarget}
 */
goog.graphics.ext.Element = function(group, wrapper) {
  'use strict';
  goog.events.EventTarget.call(this);
  this.wrapper_ = wrapper;
  this.graphics_ = group ? group.getGraphics() : this;

  this.xPosition_ = new goog.graphics.ext.Element.Position_(this, true);
  this.yPosition_ = new goog.graphics.ext.Element.Position_(this, false);

  // Handle parent / child relationships.
  if (group) {
    this.parent_ = group;
    this.parent_.addChild(this);
  }
};
goog.inherits(goog.graphics.ext.Element, goog.events.EventTarget);


/**
 * The graphics object that contains this element.
 * @type {goog.graphics.ext.Graphics|goog.graphics.ext.Element}
 * @private
 */
goog.graphics.ext.Element.prototype.graphics_;


/**
 * The goog.graphics wrapper this class wraps.
 * @type {goog.graphics.Element}
 * @private
 */
goog.graphics.ext.Element.prototype.wrapper_;


/**
 * The group or surface containing this element.
 * @type {goog.graphics.ext.Group|undefined}
 * @private
 */
goog.graphics.ext.Element.prototype.parent_;


/**
 * Whether or not computation of this element's position or size depends on its
 * parent's size.
 * @type {boolean}
 * @private
 */
goog.graphics.ext.Element.prototype.parentDependent_ = false;


/**
 * Whether the element has pending transformations.
 * @type {boolean}
 * @private
 */
goog.graphics.ext.Element.prototype.needsTransform_ = false;


/**
 * The current angle of rotation, expressed in degrees.
 * @type {number}
 * @private
 */
goog.graphics.ext.Element.prototype.rotation_ = 0;


/**
 * Object representing the x position and size of the element.
 * @type {goog.graphics.ext.Element.Position_}
 * @private
 */
goog.graphics.ext.Element.prototype.xPosition_;


/**
 * Object representing the y position and size of the element.
 * @type {goog.graphics.ext.Element.Position_}
 * @private
 */
goog.graphics.ext.Element.prototype.yPosition_;


/** @return {goog.graphics.Element} The underlying thin wrapper. */
goog.graphics.ext.Element.prototype.getWrapper = function() {
  'use strict';
  return this.wrapper_;
};


/**
 * @return {goog.graphics.ext.Element|goog.graphics.ext.Graphics} The graphics
 *     surface the element is a part of.
 */
goog.graphics.ext.Element.prototype.getGraphics = function() {
  'use strict';
  return this.graphics_;
};


/**
 * Returns the graphics implementation.
 * @return {goog.graphics.AbstractGraphics} The underlying graphics
 *     implementation drawing this element's wrapper.
 * @protected
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.graphics.ext.Element.prototype.getGraphicsImplementation = function() {
  'use strict';
  return this.graphics_.getImplementation();
};


/**
 * @return {goog.graphics.ext.Group|undefined} The parent of this element.
 */
goog.graphics.ext.Element.prototype.getParent = function() {
  'use strict';
  return this.parent_;
};


// GENERAL POSITIONING


/**
 * Internal convenience method for setting position - either as a left/top,
 * center/middle, or right/bottom value.  Only one should be specified.
 * @param {goog.graphics.ext.Element.Position_} position The position object to
 *     set the value on.
 * @param {number|string} value The value of the coordinate.
 * @param {goog.graphics.ext.Element.PositionType_} type The type of the
 *     coordinate.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 * @private
 */
goog.graphics.ext.Element.prototype.setPosition_ = function(
    position, value, type, opt_chain) {
  'use strict';
  position.setPosition(value, type);
  this.computeIsParentDependent_(position);

  this.needsTransform_ = true;
  if (!opt_chain) {
    this.transform();
  }
};


/**
 * Sets the width/height of the element.
 * @param {goog.graphics.ext.Element.Position_} position The position object to
 *     set the value on.
 * @param {string|number} size The new width/height value.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 * @private
 */
goog.graphics.ext.Element.prototype.setSize_ = function(
    position, size, opt_chain) {
  'use strict';
  if (position.setSize(size)) {
    this.needsTransform_ = true;

    this.computeIsParentDependent_(position);

    if (!opt_chain) {
      this.reset();
    }
  } else if (!opt_chain && this.isPendingTransform()) {
    this.reset();
  }
};


/**
 * Sets the minimum width/height of the element.
 * @param {goog.graphics.ext.Element.Position_} position The position object to
 *     set the value on.
 * @param {string|number} minSize The minimum width/height of the element.
 * @private
 */
goog.graphics.ext.Element.prototype.setMinSize_ = function(position, minSize) {
  'use strict';
  position.setMinSize(minSize);
  this.needsTransform_ = true;
  this.computeIsParentDependent_(position);
};


// HORIZONTAL POSITIONING


/**
 * @return {number} The distance from the left edge of this element to the left
 *     edge of its parent, specified in units of the parent's coordinate system.
 */
goog.graphics.ext.Element.prototype.getLeft = function() {
  'use strict';
  return this.xPosition_.getStart();
};


/**
 * Sets the left coordinate of the element.  Overwrites any previous value of
 * left, center, or right for this element.
 * @param {string|number} left The left coordinate.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setLeft = function(left, opt_chain) {
  'use strict';
  this.setPosition_(
      this.xPosition_, left, goog.graphics.ext.Element.PositionType_.START,
      opt_chain);
};


/**
 * @return {number} The right coordinate of the element, in units of the
 *     parent's coordinate system.
 */
goog.graphics.ext.Element.prototype.getRight = function() {
  'use strict';
  return this.xPosition_.getEnd();
};


/**
 * Sets the right coordinate of the element.  Overwrites any previous value of
 * left, center, or right for this element.
 * @param {string|number} right The right coordinate.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setRight = function(right, opt_chain) {
  'use strict';
  this.setPosition_(
      this.xPosition_, right, goog.graphics.ext.Element.PositionType_.END,
      opt_chain);
};


/**
 * @return {number} The center coordinate of the element, in units of the
 * parent's coordinate system.
 */
goog.graphics.ext.Element.prototype.getCenter = function() {
  'use strict';
  return this.xPosition_.getMiddle();
};


/**
 * Sets the center coordinate of the element.  Overwrites any previous value of
 * left, center, or right for this element.
 * @param {string|number} center The center coordinate.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setCenter = function(center, opt_chain) {
  'use strict';
  this.setPosition_(
      this.xPosition_, center, goog.graphics.ext.Element.PositionType_.MIDDLE,
      opt_chain);
};


// VERTICAL POSITIONING


/**
 * @return {number} The distance from the top edge of this element to the top
 *     edge of its parent, specified in units of the parent's coordinate system.
 */
goog.graphics.ext.Element.prototype.getTop = function() {
  'use strict';
  return this.yPosition_.getStart();
};


/**
 * Sets the top coordinate of the element.  Overwrites any previous value of
 * top, middle, or bottom for this element.
 * @param {string|number} top The top coordinate.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setTop = function(top, opt_chain) {
  'use strict';
  this.setPosition_(
      this.yPosition_, top, goog.graphics.ext.Element.PositionType_.START,
      opt_chain);
};


/**
 * @return {number} The bottom coordinate of the element, in units of the
 *     parent's coordinate system.
 */
goog.graphics.ext.Element.prototype.getBottom = function() {
  'use strict';
  return this.yPosition_.getEnd();
};


/**
 * Sets the bottom coordinate of the element.  Overwrites any previous value of
 * top, middle, or bottom for this element.
 * @param {string|number} bottom The bottom coordinate.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setBottom = function(bottom, opt_chain) {
  'use strict';
  this.setPosition_(
      this.yPosition_, bottom, goog.graphics.ext.Element.PositionType_.END,
      opt_chain);
};


/**
 * @return {number} The middle coordinate of the element, in units of the
 *     parent's coordinate system.
 */
goog.graphics.ext.Element.prototype.getMiddle = function() {
  'use strict';
  return this.yPosition_.getMiddle();
};


/**
 * Sets the middle coordinate of the element.  Overwrites any previous value of
 * top, middle, or bottom for this element
 * @param {string|number} middle The middle coordinate.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setMiddle = function(middle, opt_chain) {
  'use strict';
  this.setPosition_(
      this.yPosition_, middle, goog.graphics.ext.Element.PositionType_.MIDDLE,
      opt_chain);
};


// DIMENSIONS


/**
 * @return {number} The width of the element, in units of the parent's
 *     coordinate system.
 */
goog.graphics.ext.Element.prototype.getWidth = function() {
  'use strict';
  return this.xPosition_.getSize();
};


/**
 * Sets the width of the element.
 * @param {string|number} width The new width value.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setWidth = function(width, opt_chain) {
  'use strict';
  this.setSize_(this.xPosition_, width, opt_chain);
};


/**
 * @return {number} The minimum width of the element, in units of the parent's
 *     coordinate system.
 */
goog.graphics.ext.Element.prototype.getMinWidth = function() {
  'use strict';
  return this.xPosition_.getMinSize();
};


/**
 * Sets the minimum width of the element.
 * @param {string|number} minWidth The minimum width of the element.
 */
goog.graphics.ext.Element.prototype.setMinWidth = function(minWidth) {
  'use strict';
  this.setMinSize_(this.xPosition_, minWidth);
};


/**
 * @return {number} The height of the element, in units of the parent's
 *     coordinate system.
 */
goog.graphics.ext.Element.prototype.getHeight = function() {
  'use strict';
  return this.yPosition_.getSize();
};


/**
 * Sets the height of the element.
 * @param {string|number} height The new height value.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setHeight = function(height, opt_chain) {
  'use strict';
  this.setSize_(this.yPosition_, height, opt_chain);
};


/**
 * @return {number} The minimum height of the element, in units of the parent's
 *     coordinate system.
 */
goog.graphics.ext.Element.prototype.getMinHeight = function() {
  'use strict';
  return this.yPosition_.getMinSize();
};


/**
 * Sets the minimum height of the element.
 * @param {string|number} minHeight The minimum height of the element.
 */
goog.graphics.ext.Element.prototype.setMinHeight = function(minHeight) {
  'use strict';
  this.setMinSize_(this.yPosition_, minHeight);
};


// BOUNDS SHORTCUTS


/**
 * Shortcut for setting the left and top position.
 * @param {string|number} left The left coordinate.
 * @param {string|number} top The top coordinate.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setPosition = function(
    left, top, opt_chain) {
  'use strict';
  this.setLeft(left, true);
  this.setTop(top, opt_chain);
};


/**
 * Shortcut for setting the width and height.
 * @param {string|number} width The new width value.
 * @param {string|number} height The new height value.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setSize = function(
    width, height, opt_chain) {
  'use strict';
  this.setWidth(width, true);
  this.setHeight(height, opt_chain);
};


/**
 * Shortcut for setting the left, top, width, and height.
 * @param {string|number} left The left coordinate.
 * @param {string|number} top The top coordinate.
 * @param {string|number} width The new width value.
 * @param {string|number} height The new height value.
 * @param {boolean=} opt_chain Optional flag to specify this function is part
 *     of a chain of calls and therefore transformations should be set as
 *     pending but not yet performed.
 */
goog.graphics.ext.Element.prototype.setBounds = function(
    left, top, width, height, opt_chain) {
  'use strict';
  this.setLeft(left, true);
  this.setTop(top, true);
  this.setWidth(width, true);
  this.setHeight(height, opt_chain);
};


// MAXIMUM BOUNDS


/**
 * @return {number} An estimate of the maximum x extent this element would have
 *     in a parent of no width.
 */
goog.graphics.ext.Element.prototype.getMaxX = function() {
  'use strict';
  return this.xPosition_.getMaxPosition();
};


/**
 * @return {number} An estimate of the maximum y extent this element would have
 *     in a parent of no height.
 */
goog.graphics.ext.Element.prototype.getMaxY = function() {
  'use strict';
  return this.yPosition_.getMaxPosition();
};


// RESET


/**
 * Reset the element.  This is called when the element changes size, or when
 * the coordinate system changes in a way that would affect pixel based
 * rendering
 */
goog.graphics.ext.Element.prototype.reset = function() {
  'use strict';
  this.xPosition_.resetCache();
  this.yPosition_.resetCache();

  this.redraw();

  this.needsTransform_ = true;
  this.transform();
};


/**
 * Overridable function for subclass specific reset.
 * @protected
 */
goog.graphics.ext.Element.prototype.redraw = function() {};


// PARENT DEPENDENCY


/**
 * Computes whether the element is still parent dependent.
 * @param {goog.graphics.ext.Element.Position_} position The recently changed
 *     position object.
 * @private
 */
goog.graphics.ext.Element.prototype.computeIsParentDependent_ = function(
    position) {
  'use strict';
  this.parentDependent_ = position.isParentDependent() ||
      this.xPosition_.isParentDependent() ||
      this.yPosition_.isParentDependent() || this.checkParentDependent();
};


/**
 * Returns whether this element's bounds depend on its parents.
 *
 * This function should be treated as if it has package scope.
 * @return {boolean} Whether this element's bounds depend on its parents.
 */
goog.graphics.ext.Element.prototype.isParentDependent = function() {
  'use strict';
  return this.parentDependent_;
};


/**
 * Overridable function for subclass specific parent dependency.
 * @return {boolean} Whether this shape's bounds depends on its parent's.
 * @protected
 */
goog.graphics.ext.Element.prototype.checkParentDependent = goog.functions.FALSE;


// ROTATION


/**
 * Set the rotation of this element.
 * @param {number} angle The angle of rotation, in degrees.
 */
goog.graphics.ext.Element.prototype.setRotation = function(angle) {
  'use strict';
  if (this.rotation_ != angle) {
    this.rotation_ = angle;

    this.needsTransform_ = true;
    this.transform();
  }
};


/**
 * @return {number} The angle of rotation of this element, in degrees.
 */
goog.graphics.ext.Element.prototype.getRotation = function() {
  'use strict';
  return this.rotation_;
};


// TRANSFORMS


/**
 * Called by the parent when the parent has transformed.
 *
 * Should be treated as package scope.
 */
goog.graphics.ext.Element.prototype.parentTransform = function() {
  'use strict';
  this.needsTransform_ = this.needsTransform_ || this.parentDependent_;
};


/**
 * @return {boolean} Whether this element has pending transforms.
 */
goog.graphics.ext.Element.prototype.isPendingTransform = function() {
  'use strict';
  return this.needsTransform_;
};


/**
 * Performs a pending transform.
 * @protected
 */
goog.graphics.ext.Element.prototype.transform = function() {
  'use strict';
  if (this.isPendingTransform()) {
    this.needsTransform_ = false;

    this.wrapper_.setTransformation(
        this.getLeft(), this.getTop(), this.rotation_,
        (this.getWidth() || 1) / 2, (this.getHeight() || 1) / 2);

    // TODO(robbyw): this._fireEvent('transform', [ this ]);
  }
};


// PIXEL SCALE


/**
 * @return {number} Returns the number of pixels per unit in the x direction.
 */
goog.graphics.ext.Element.prototype.getPixelScaleX = function() {
  'use strict';
  return this.getGraphics().getPixelScaleX();
};


/**
 * @return {number} Returns the number of pixels per unit in the y direction.
 */
goog.graphics.ext.Element.prototype.getPixelScaleY = function() {
  'use strict';
  return this.getGraphics().getPixelScaleY();
};


// EVENT HANDLING


/** @override */
goog.graphics.ext.Element.prototype.disposeInternal = function() {
  'use strict';
  goog.graphics.ext.Element.superClass_.disposeInternal.call(this);
  this.wrapper_.dispose();
};


// INTERNAL POSITION OBJECT


/**
 * Position specification types.  Start corresponds to left/top, middle to
 * center/middle, and end to right/bottom.
 * @enum {number}
 * @private
 */
goog.graphics.ext.Element.PositionType_ = {
  START: 0,
  MIDDLE: 1,
  END: 2
};



/**
 * Manages a position and size, either horizontal or vertical.
 * @param {goog.graphics.ext.Element} element The element the position applies
 *     to.
 * @param {boolean} horizontal Whether the position is horizontal or vertical.
 * @constructor
 * @private
 */
goog.graphics.ext.Element.Position_ = function(element, horizontal) {
  'use strict';
  this.element_ = element;
  this.horizontal_ = horizontal;
};


/**
 * @return {!Object} The coordinate value computation cache.
 * @private
 */
goog.graphics.ext.Element.Position_.prototype.getCoordinateCache_ = function() {
  'use strict';
  return this.coordinateCache_ || (this.coordinateCache_ = {});
};


/**
 * @return {number} The size of the parent's coordinate space.
 * @private
 */
goog.graphics.ext.Element.Position_.prototype.getParentSize_ = function() {
  'use strict';
  const parent = this.element_.getParent();
  return this.horizontal_ ? parent.getCoordinateWidth() :
                            parent.getCoordinateHeight();
};


/**
 * @return {number} The minimum width/height of the element.
 */
goog.graphics.ext.Element.Position_.prototype.getMinSize = function() {
  'use strict';
  return this.getValue_(this.minSize_);
};


/**
 * Sets the minimum width/height of the element.
 * @param {string|number} minSize The minimum width/height of the element.
 */
goog.graphics.ext.Element.Position_.prototype.setMinSize = function(minSize) {
  'use strict';
  this.minSize_ = minSize;
  this.resetCache();
};


/**
 * @return {number} The width/height of the element.
 */
goog.graphics.ext.Element.Position_.prototype.getSize = function() {
  'use strict';
  return Math.max(this.getValue_(this.size_), this.getMinSize());
};


/**
 * Sets the width/height of the element.
 * @param {string|number} size The width/height of the element.
 * @return {boolean} Whether the value was changed.
 */
goog.graphics.ext.Element.Position_.prototype.setSize = function(size) {
  'use strict';
  if (size != this.size_) {
    this.size_ = size;
    this.resetCache();
    return true;
  }
  return false;
};


/**
 * Converts the given x coordinate to a number value in units.
 * @param {string|number} v The coordinate to retrieve the value for.
 * @param {boolean=} opt_forMaximum Whether we are computing the largest value
 *     this coordinate would be in a parent of no size.
 * @return {number} The correct number of coordinate space units.
 * @private
 */
goog.graphics.ext.Element.Position_.prototype.getValue_ = function(
    v, opt_forMaximum) {
  'use strict';
  if (!goog.graphics.ext.coordinates.isSpecial(v)) {
    return parseFloat(String(v));
  }

  const cache = this.getCoordinateCache_();
  const scale = this.horizontal_ ? this.element_.getPixelScaleX() :
                                   this.element_.getPixelScaleY();

  let containerSize;
  if (opt_forMaximum) {
    containerSize =
        goog.graphics.ext.coordinates.computeValue(this.size_ || 0, 0, scale);
  } else {
    const parent = this.element_.getParent();
    containerSize = this.horizontal_ ? parent.getWidth() : parent.getHeight();
  }

  return goog.graphics.ext.coordinates.getValue(
      v, opt_forMaximum, containerSize, scale, cache);
};


/**
 * @return {number} The distance from the left/top edge of this element to the
 *     left/top edge of its parent, specified in units of the parent's
 *     coordinate system.
 */
goog.graphics.ext.Element.Position_.prototype.getStart = function() {
  'use strict';
  if (this.cachedValue_ == null) {
    const value = this.getValue_(this.distance_);
    if (this.distanceType_ == goog.graphics.ext.Element.PositionType_.START) {
      this.cachedValue_ = value;
    } else if (
        this.distanceType_ == goog.graphics.ext.Element.PositionType_.MIDDLE) {
      this.cachedValue_ = value + (this.getParentSize_() - this.getSize()) / 2;
    } else {
      this.cachedValue_ = this.getParentSize_() - value - this.getSize();
    }
  }

  return this.cachedValue_;
};


/**
 * @return {number} The middle coordinate of the element, in units of the
 *     parent's coordinate system.
 */
goog.graphics.ext.Element.Position_.prototype.getMiddle = function() {
  'use strict';
  return this.distanceType_ == goog.graphics.ext.Element.PositionType_.MIDDLE ?
      this.getValue_(this.distance_) :
      (this.getParentSize_() - this.getSize()) / 2 - this.getStart();
};


/**
 * @return {number} The end coordinate of the element, in units of the
 *     parent's coordinate system.
 */
goog.graphics.ext.Element.Position_.prototype.getEnd = function() {
  'use strict';
  return this.distanceType_ == goog.graphics.ext.Element.PositionType_.END ?
      this.getValue_(this.distance_) :
      this.getParentSize_() - this.getStart() - this.getSize();
};


/**
 * Sets the position, either as a left/top, center/middle, or right/bottom
 * value.
 * @param {number|string} value The value of the coordinate.
 * @param {goog.graphics.ext.Element.PositionType_} type The type of the
 *     coordinate.
 */
goog.graphics.ext.Element.Position_.prototype.setPosition = function(
    value, type) {
  'use strict';
  this.distance_ = value;
  this.distanceType_ = type;

  // Clear cached value.
  this.cachedValue_ = null;
};


/**
 * @return {number} An estimate of the maximum x/y extent this element would
 *     have in a parent of no width/height.
 */
goog.graphics.ext.Element.Position_.prototype.getMaxPosition = function() {
  'use strict';
  // TODO(robbyw): Handle transformed or rotated coordinates
  // TODO(robbyw): Handle pixel based sizes?

  return this.getValue_(this.distance_ || 0) +
      (goog.graphics.ext.coordinates.isSpecial(this.size_) ? 0 :
                                                             this.getSize());
};


/**
 * Resets the caches of position values and coordinate values.
 */
goog.graphics.ext.Element.Position_.prototype.resetCache = function() {
  'use strict';
  this.coordinateCache_ = null;
  this.cachedValue_ = null;
};


/**
 * @return {boolean} Whether the size or position of this element depends on
 *     the size of the parent element.
 */
goog.graphics.ext.Element.Position_.prototype.isParentDependent = function() {
  'use strict';
  return this.distanceType_ != goog.graphics.ext.Element.PositionType_.START ||
      goog.graphics.ext.coordinates.isSpecial(this.size_) ||
      goog.graphics.ext.coordinates.isSpecial(this.minSize_) ||
      goog.graphics.ext.coordinates.isSpecial(this.distance_);
};


/**
 * The lazy loaded distance from the parent's top/left edge to this element's
 * top/left edge expressed in the parent's coordinate system.  We cache this
 * because it is most freqeuently requested by the element and it is easy to
 * compute middle and end values from it.
 * @type {?number}
 * @private
 */
goog.graphics.ext.Element.Position_.prototype.cachedValue_ = null;


/**
 * A cache of computed x coordinates.
 * @type {?Object}
 * @private
 */
goog.graphics.ext.Element.Position_.prototype.coordinateCache_ = null;


/**
 * The minimum width/height of this element, as specified by the caller.
 * @type {string|number}
 * @private
 */
goog.graphics.ext.Element.Position_.prototype.minSize_ = 0;


/**
 * The width/height of this object, as specified by the caller.
 * @type {string|number}
 * @private
 */
goog.graphics.ext.Element.Position_.prototype.size_ = 0;


/**
 * The coordinate of this object, as specified by the caller.  The type of
 * coordinate is specified by distanceType_.
 * @type {string|number}
 * @private
 */
goog.graphics.ext.Element.Position_.prototype.distance_ = 0;


/**
 * The coordinate type specified by distance_.
 * @type {goog.graphics.ext.Element.PositionType_}
 * @private
 */
goog.graphics.ext.Element.Position_.prototype.distanceType_ =
    goog.graphics.ext.Element.PositionType_.START;
