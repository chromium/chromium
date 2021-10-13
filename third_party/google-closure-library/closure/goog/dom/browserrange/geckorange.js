/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the Gecko specific range wrapper.  Inherits most
 * functionality from W3CRange, but adds exceptions as necessary.
 *
 * DO NOT USE THIS FILE DIRECTLY.  Use goog.dom.Range instead.
 */


goog.provide('goog.dom.browserrange.GeckoRange');

goog.require('goog.dom.browserrange.W3cRange');



/**
 * The constructor for Gecko specific browser ranges.
 * @param {Range} range The range object.
 * @constructor
 * @extends {goog.dom.browserrange.W3cRange}
 * @final
 */
goog.dom.browserrange.GeckoRange = function(range) {
  'use strict';
  goog.dom.browserrange.W3cRange.call(this, range);
};
goog.inherits(goog.dom.browserrange.GeckoRange, goog.dom.browserrange.W3cRange);


/**
 * Creates a range object that selects the given node's text.
 * @param {Node} node The node to select.
 * @return {!goog.dom.browserrange.GeckoRange} A Gecko range wrapper object.
 */
goog.dom.browserrange.GeckoRange.createFromNodeContents = function(node) {
  'use strict';
  return new goog.dom.browserrange.GeckoRange(
      goog.dom.browserrange.W3cRange.getBrowserRangeForNode(node));
};


/**
 * Creates a range object that selects between the given nodes.
 * @param {Node} startNode The node to start with.
 * @param {number} startOffset The offset within the node to start.
 * @param {Node} endNode The node to end with.
 * @param {number} endOffset The offset within the node to end.
 * @return {!goog.dom.browserrange.GeckoRange} A wrapper object.
 */
goog.dom.browserrange.GeckoRange.createFromNodes = function(
    startNode, startOffset, endNode, endOffset) {
  'use strict';
  return new goog.dom.browserrange.GeckoRange(
      goog.dom.browserrange.W3cRange.getBrowserRangeForNodes(
          startNode, startOffset, endNode, endOffset));
};


/** @override */
goog.dom.browserrange.GeckoRange.prototype.selectInternal = function(
    selection, reversed) {
  'use strict';
  if (!reversed || this.isCollapsed()) {
    // The base implementation for select() is more robust, and works fine for
    // collapsed and forward ranges.  This works around
    // https://bugzilla.mozilla.org/show_bug.cgi?id=773137, and is tested by
    // range_test.html's testFocusedElementDisappears.
    goog.dom.browserrange.GeckoRange.base(
        this, 'selectInternal', selection, reversed);
  } else {
    // Reversed selection -- start with a caret on the end node, and extend it
    // back to the start.  Unfortunately, collapse() fails when focus is
    // invalid.
    selection.collapse(this.getEndNode(), this.getEndOffset());
    selection.extend(this.getStartNode(), this.getStartOffset());
  }
};
