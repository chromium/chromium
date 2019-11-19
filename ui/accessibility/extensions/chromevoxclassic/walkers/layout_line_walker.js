// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A JavaScript class for walking lines consisting of one or more
 * clickable nodes.
 */


goog.provide('cvox.LayoutLineWalker');

goog.require('cvox.AbstractWalker');
goog.require('cvox.StructuralLineWalker');


/**
 * @constructor
 * @extends {cvox.AbstractWalker}
 */
cvox.LayoutLineWalker = function() {
  this.subWalker_ = new cvox.StructuralLineWalker();
};
goog.inherits(cvox.LayoutLineWalker, cvox.AbstractWalker);


/**
 * @override
 */
cvox.LayoutLineWalker.prototype.next = function(sel) {
  // Collapse selection to the directed end.
  var endSel = new cvox.CursorSelection(sel.end, sel.end, sel.isReversed());

  // Sync to the line.
  var sync = this.subWalker_.sync(endSel);
  if (!sync) {
    return null;
  }

  // Compute the next selection.
  var start = this.subWalker_.next(endSel);
  if (!start) {
    return null;
  }
  start.setReversed(sel.isReversed());
  return this.extend_(start).setReversed(false);
};


/**
 * @override
 */
cvox.LayoutLineWalker.prototype.sync = function(sel) {
  var line = this.subWalker_.sync(sel);
  if (!line) {
    return null;
  }

  // Extend to both line breaks (in each direction).
  var end = this.extend_(line);
  var start = this.extend_(line.setReversed(!line.isReversed()));

  return new cvox.CursorSelection(start.end, end.end, sel.isReversed());
};


/**
 * @override
 */
cvox.LayoutLineWalker.prototype.getDescription = function(prevSel, sel) {
  var descriptions = [];
  var prev = prevSel;
  var absSel = sel.clone().setReversed(false);
  var cur = new cvox.CursorSelection(absSel.start, absSel.start);
  cur = this.subWalker_.sync(cur);
  if (!cur) {
    return [];
  }

  // No need to accumulate descriptions.
  if (absSel.start.node == absSel.end.node) {
    return this.subWalker_.getDescription(prevSel, sel);
  }

  // Walk through and collect descriptions for each line.
  while (cur && !cur.end.equals(absSel.end)) {
    descriptions.push.apply(
        descriptions, this.subWalker_.getDescription(prev, cur));
    prev = cur;
    cur = this.subWalker_.next(cur);
  }
  if (cur) {
    descriptions.push.apply(
        descriptions, this.subWalker_.getDescription(prev, cur));
  }
  return descriptions;
};


/**
 * @override
 */
cvox.LayoutLineWalker.prototype.getBraille = function(prevSel, sel) {
  var braille = new cvox.NavBraille({});
  var absSel = this.subWalker_.sync(sel.clone().setReversed(false));
  var layoutSel = this.sync(sel).setReversed(false);
  if (!layoutSel || !absSel) {
    return braille;
  }
  var cur = new cvox.CursorSelection(layoutSel.start, layoutSel.start);
  cur = this.subWalker_.sync(cur);
  if (!cur) {
    return braille;
  }

  // Walk through and collect braille for each line.
  while (cur && !cur.end.equals(layoutSel.end)) {
    this.appendBraille_(prevSel, absSel, cur, braille);
    prevSel = cur;
    cur = this.subWalker_.next(cur);
  }
  if (cur) {
    this.appendBraille_(prevSel, absSel, cur, braille);
  }
  return braille;
};


/**
 * @override
 */
cvox.LayoutLineWalker.prototype.getGranularityMsg = function() {
  return Msgs.getMsg('layout_line');
};


/**
 * Compares two selections and determines if the lie on the same horizontal
 * line as determined by their bounding rectangles.
 * @param {!cvox.CursorSelection} lSel Left selection.
 * @param {!cvox.CursorSelection} rSel Right selection.
 * @return {boolean} Whether lSel and rSel are on different visual lines.
 * @private
 */
cvox.LayoutLineWalker.prototype.isVisualLineBreak_ = function(lSel, rSel) {
  if (this.wantsOwnLine_(lSel.end.node) ||
      this.wantsOwnLine_(rSel.start.node)) {
    return true;
  }
  var lRect = lSel.getRange().getBoundingClientRect();
  var rRect = rSel.getRange().getBoundingClientRect();

  // Some ranges from the browser give us 0-sized rects (such as in the case of
  // select's). Detect these cases and use a more reliable method (take the
  // bounding rect of the actual element rather than the range).
  if (lRect.width == 0 &&
      lRect.height == 0 &&
      lSel.end.node.nodeType == Node.ELEMENT_NODE) {
    lRect = lSel.end.node.getBoundingClientRect();
  }

  if (rRect.width == 0 &&
      rRect.height == 0 &&
      rSel.start.node.nodeType == Node.ELEMENT_NODE) {
    rRect = rSel.start.node.getBoundingClientRect();
  }
  return lRect.bottom != rRect.bottom;
};


/**
 * Determines if node should force a line break.
 * This is used for elements with unusual semantics, such as multi-line
 * text fields, where the behaviour would otherwise be confusing.
 * @param {Node} node Node.
 * @return {boolean} True if the node should appear next to a line break.
 * @private
 */
cvox.LayoutLineWalker.prototype.wantsOwnLine_ = function(node) {
  if (!node) {
    return false;
  }
  return node instanceof HTMLTextAreaElement ||
      node.parentNode instanceof HTMLTextAreaElement;
};


/**
 * Extends a given cursor selection up to the next visual line break.
 * @param {!cvox.CursorSelection} start The selection.
 * @return {!cvox.CursorSelection} The resulting selection.
 * @private
 */
cvox.LayoutLineWalker.prototype.extend_ = function(start) {
  // Extend the selection up to just before a new visual line break.
  var end = start;
  var next = start;

  do {
    end = next;
    next = this.subWalker_.next(end);
  } while (next && !this.isVisualLineBreak_(end, next));
  return new cvox.CursorSelection(start.start, end.end, start.isReversed());
};


/**
 * Private routine to append braille given three selections.
 * @param {!cvox.CursorSelection} prevSel A previous selection in walker
 * ordering.
 * @param {!cvox.CursorSelection} sel A selection that represents the location
 * of the braille cursor.
 * @param {!cvox.CursorSelection} cur The specific selection to append.
 * @param {!cvox.NavBraille} braille Braille on which to append.
 * @private
 */
cvox.LayoutLineWalker.prototype.appendBraille_ = function(
    prevSel, sel, cur, braille) {
  var item = this.subWalker_.getBraille(prevSel, cur).text;
  var valueSelectionSpan = item.getSpanInstanceOf(cvox.ValueSelectionSpan);

  if (braille.text.length > 0) {
    braille.text.append(cvox.BrailleUtil.ITEM_SEPARATOR);
  }

  // Find the surrounding logical "leaf node".
  // This prevents us from labelling the braille output with the wrong node,
  // such as a text node child of a <textarea>.
  var node = cur.start.node;
  while (node.parentNode && cvox.DomUtil.isLeafNode(node.parentNode)) {
    node = node.parentNode;
  }

  var nodeStart = braille.text.length;
  var nodeEnd = nodeStart + item.length;
  braille.text.append(item);
  braille.text.setSpan(node, nodeStart, nodeEnd);

  if (sel && cur.absEquals(sel)) {
    if (valueSelectionSpan) {
      braille.startIndex = nodeStart + item.getSpanStart(valueSelectionSpan);
      braille.endIndex = nodeStart + item.getSpanEnd(valueSelectionSpan);
    } else {
      braille.startIndex = nodeStart;
      braille.endIndex = nodeStart + 1;
    }
  }
};
