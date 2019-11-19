// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Simple class to represent a cursor selection.
 * A cursor selection is just two cursors; one for the start and one for
 * the end of some interval in the document.
 */

goog.provide('cvox.CursorSelection');

goog.require('cvox.Cursor');
goog.require('cvox.SelectionUtil');
goog.require('cvox.TraverseUtil');


/**
 * If the start node and end node are the same, and the indexes are the same,
 * the selection is interpreted to be a node. Otherwise, it is interpreted
 * to be a range.
 * @param {!cvox.Cursor} start The starting cursor.
 * @param {!cvox.Cursor} end The ending cursor.
 * @param {boolean=} opt_reverse Whether to make it a reversed selection or
 * not. Default is selection is not reversed. If start and end are in the
 * wrong order, they will be swapped automatically.
 * NOTE: Can't infer automatically whether the selection is reversed because
 * for a selection on a single node, the start and end are equal.
 * @constructor
 */
cvox.CursorSelection = function(start, end, opt_reverse) {
  this.start = start.clone();
  this.end = end.clone();

  if (opt_reverse == undefined) {
    opt_reverse = false;
  }
  /** @private */
  this.isReversed_ = opt_reverse;

  if ((this.isReversed_ &&
       this.start.node.compareDocumentPosition(this.end.node) ==
       cvox.CursorSelection.BEFORE) ||
      (!this.isReversed_ &&
       this.end.node.compareDocumentPosition(this.start.node) ==
       cvox.CursorSelection.BEFORE)) {
    var oldStart = this.start;
    this.start = this.end;
    this.end = oldStart;
  }
};


/**
 * From http://www.w3schools.com/jsref/met_node_comparedocumentposition.asp
 */
cvox.CursorSelection.BEFORE = 4;


/**
 * If true, ensures that this selection is reversed. Otherwise, ensures that
 * it is not reversed.
 * @param {boolean} reversed True to reverse. False to nonreverse.
 * @return {!cvox.CursorSelection} For chaining.
 */
cvox.CursorSelection.prototype.setReversed = function(reversed) {
  if (reversed == this.isReversed_) {
    return this;
  }
  var oldStart = this.start;
  this.start = this.end;
  this.end = oldStart;
  this.isReversed_ = reversed;
  return this;
};


/**
 * Returns true if this selection is a reverse selection.
 * @return {boolean} true if reversed.
 */
cvox.CursorSelection.prototype.isReversed = function() {
  return this.isReversed_;
};


/**
 * Returns start if not reversed, end if reversed.
 * @return {!cvox.Cursor} start if not reversed, end if reversed.
 */
cvox.CursorSelection.prototype.absStart = function() {
  return this.isReversed_ ? this.end : this.start;
};

/**
 * Returns end if not reversed, start if reversed.
 * @return {!cvox.Cursor} end if not reversed, start if reversed.
 */
cvox.CursorSelection.prototype.absEnd = function() {
  return this.isReversed_ ? this.start : this.end;
};


/**
 * Clones the selection.
 * @return {!cvox.CursorSelection} The cloned selection.
 */
cvox.CursorSelection.prototype.clone = function() {
  return new cvox.CursorSelection(this.start, this.end, this.isReversed_);
};


/**
 * Places a DOM selection around this CursorSelection.
 */
cvox.CursorSelection.prototype.select = function() {
  var sel = window.getSelection();
  sel.removeAllRanges();
  this.normalize();
  sel.addRange(this.getRange());
};


/**
 * Creates a new cursor selection that starts and ends at the node.
 * Returns null if node is null.
 * @param {Node} node The node.
 * @return {cvox.CursorSelection} The selection.
 */
cvox.CursorSelection.fromNode = function(node) {
  if (!node) {
    return null;
  }
  var text = cvox.TraverseUtil.getNodeText(node);

  return new cvox.CursorSelection(
      new cvox.Cursor(node, 0, text),
      new cvox.Cursor(node, 0, text));
};


/**
 * Creates a new cursor selection that starts and ends at document.body.
 * @return {!cvox.CursorSelection} The selection.
 */
cvox.CursorSelection.fromBody = function() {
    return /** @type {!cvox.CursorSelection} */ (
        cvox.CursorSelection.fromNode(document.body));
};

/**
 * Returns the text that the selection spans.
 * @return {string} Text within the selection. '' if it is a node selection.
 */
cvox.CursorSelection.prototype.getText = function() {
  if (this.start.equals(this.end)) {
    return cvox.TraverseUtil.getNodeText(this.start.node);
  }
  return cvox.SelectionUtil.getRangeText(this.getRange());
};

/**
 * Returns a range from the given selection.
 * @return {Range} The range.
 */
cvox.CursorSelection.prototype.getRange = function() {
  var range = document.createRange();
  if (this.isReversed_) {
    range.setStart(this.end.node, this.end.index);
    range.setEnd(this.start.node, this.start.index);
  } else {
    range.setStart(this.start.node, this.start.index);
    range.setEnd(this.end.node, this.end.index);
  }
  return range;
};

/**
 * Check for equality.
 * @param {!cvox.CursorSelection} rhs The CursorSelection to compare against.
 * @return {boolean} True if equal.
 */
cvox.CursorSelection.prototype.equals = function(rhs) {
  return this.start.equals(rhs.start) && this.end.equals(rhs.end);
};

/**
 * Check for equality regardless of direction.
 * @param {!cvox.CursorSelection} rhs The CursorSelection to compare against.
 * @return {boolean} True if equal.
 */
cvox.CursorSelection.prototype.absEquals = function(rhs) {
  return ((this.start.equals(rhs.start) && this.end.equals(rhs.end)) ||
      (this.end.equals(rhs.start) && this.start.equals(rhs.end)));
};

/**
 * Determines if this starts before another CursorSelection in document order.
 * If this is reversed, then a reversed document order is checked.
 * In the case that this and rhs start at the same position, we return true.
 * @param {!cvox.CursorSelection} rhs The selection to compare.
 * @return {boolean} True if this is before rhs.
 */
cvox.CursorSelection.prototype.directedBefore = function(rhs) {
  var leftToRight = this.start.node.compareDocumentPosition(rhs.start.node) ==
      cvox.CursorSelection.BEFORE;
  return this.start.node == rhs.start.node ||
      (this.isReversed() ? !leftToRight : leftToRight);
};
/**
 * Normalizes this selection.
 * Use this routine to adjust CursorSelection's that have been collapsed due to
 * convention such as when a CursorSelection references a node without attention
 * to its endpoints.
 * The result is to surround the node with this cursor.
 * @return {!cvox.CursorSelection} The normalized selection.
 */
cvox.CursorSelection.prototype.normalize = function() {
  if (this.absEnd().index == 0 && this.absEnd().node) {
    var node = this.absEnd().node;

    // DOM ranges use different conventions when surrounding a node. For
    // instance, input nodes endOffset is always 0 while h1's endOffset is 1
    //with both having no children. Use a range to compute the endOffset.
    var testRange = document.createRange();
    testRange.selectNodeContents(node);
    this.absEnd().index = testRange.endOffset;
  }
  return this;
};

/**
 * Collapses to the directed start of the selection.
 * @return {!cvox.CursorSelection} For chaining.
 */
cvox.CursorSelection.prototype.collapse = function() {
  // Not a selection.
  if (this.start.equals(this.end)) {
    return this;
  }
  this.end.copyFrom(this.start);
  if (this.start.text.length == 0) {
    return this;
  }
  if (this.isReversed()) {
    if (this.end.index > 0) {
      this.end.index--;
    }
  } else {
    if (this.end.index < this.end.text.length) {
      this.end.index++;
    }
  }
  return this;
};
