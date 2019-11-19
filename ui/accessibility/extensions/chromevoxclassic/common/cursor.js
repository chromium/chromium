// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Simple class to represent a cursor location in the document.
 */

goog.provide('cvox.Cursor');

/**
 * A class to represent a cursor location in the document,
 * like the start position or end position of a selection range.
 *
 * Later this may be extended to support "virtual text" for an object,
 * like the ALT text for an image.
 *
 * Note: we cache the text of a particular node at the time we
 * traverse into it. Later we should add support for dynamically
 * reloading it.
 * NOTE: Undefined behavior if node is null
 * @param {Node} node The DOM node.
 * @param {number} index The index of the character within the node.
 * @param {string} text The cached text contents of the node.
 * @constructor
 */
cvox.Cursor = function(node, index, text) {
  this.node = node;
  this.index = index;
  this.text = text;
};

/**
 * @return {!cvox.Cursor} A new cursor pointing to the same location.
 */
cvox.Cursor.prototype.clone = function() {
  return new cvox.Cursor(this.node, this.index, this.text);
};

/**
 * Modify this cursor to point to the location that another cursor points to.
 * @param {!cvox.Cursor} otherCursor The cursor to copy from.
 */
cvox.Cursor.prototype.copyFrom = function(otherCursor) {
  this.node = otherCursor.node;
  this.index = otherCursor.index;
  this.text = otherCursor.text;
};

/**
 * Check for equality.
 * @param {!cvox.Cursor} rhs The cursor to compare against.
 * @return {boolean} True if equal.
 */
cvox.Cursor.prototype.equals = function(rhs) {
  return this.node == rhs.node &&
      this.index == rhs.index &&
      this.text == rhs.text;
};
