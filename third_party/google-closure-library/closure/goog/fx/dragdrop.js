/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Single Element Drag and Drop.
 *
 * Drag and drop implementation for sources/targets consisting of a single
 * element.
 *
 * @see ../demos/dragdrop.html
 */

goog.provide('goog.fx.DragDrop');

goog.require('goog.fx.AbstractDragDrop');
goog.require('goog.fx.DragDropItem');



/**
 * Drag/drop implementation for creating drag sources/drop targets consisting of
 * a single HTML Element.
 *
 * @param {Element|string} element Dom Node, or string representation of node
 *     id, to be used as drag source/drop target.
 * @param {Object=} opt_data Data associated with the source/target.
 * @throws Error If no element argument is provided or if the type is invalid
 * @extends {goog.fx.AbstractDragDrop}
 * @constructor
 * @struct
 */
goog.fx.DragDrop = function(element, opt_data) {
  'use strict';
  goog.fx.AbstractDragDrop.call(this);

  var item = new goog.fx.DragDropItem(element, opt_data);
  item.setParent(this);
  this.items_.push(item);
};
goog.inherits(goog.fx.DragDrop, goog.fx.AbstractDragDrop);
