/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the WebKit specific range wrapper.  Inherits most
 * functionality from W3CRange, but adds exceptions as necessary.
 *
 * DO NOT USE THIS FILE DIRECTLY.  Use goog.dom.Range instead.
 */


goog.provide('goog.dom.browserrange.WebKitRange');

goog.require('goog.dom.browserrange.W3cRange');



/**
 * The constructor for WebKit specific browser ranges.
 * @param {Range} range The range object.
 * @constructor
 * @extends {goog.dom.browserrange.W3cRange}
 * @final
 */
goog.dom.browserrange.WebKitRange = function(range) {
  'use strict';
  goog.dom.browserrange.W3cRange.call(this, range);
};
goog.inherits(
    goog.dom.browserrange.WebKitRange, goog.dom.browserrange.W3cRange);


/**
 * Creates a range object that selects the given node's text.
 * @param {Node} node The node to select.
 * @return {!goog.dom.browserrange.WebKitRange} A WebKit range wrapper object.
 */
goog.dom.browserrange.WebKitRange.createFromNodeContents = function(node) {
  'use strict';
  return new goog.dom.browserrange.WebKitRange(
      goog.dom.browserrange.W3cRange.getBrowserRangeForNode(node));
};


/**
 * Creates a range object that selects between the given nodes.
 * @param {Node} startNode The node to start with.
 * @param {number} startOffset The offset within the start node.
 * @param {Node} endNode The node to end with.
 * @param {number} endOffset The offset within the end node.
 * @return {!goog.dom.browserrange.WebKitRange} A wrapper object.
 */
goog.dom.browserrange.WebKitRange.createFromNodes = function(
    startNode, startOffset, endNode, endOffset) {
  'use strict';
  return new goog.dom.browserrange.WebKitRange(
      goog.dom.browserrange.W3cRange.getBrowserRangeForNodes(
          startNode, startOffset, endNode, endOffset));
};


/** @override */
goog.dom.browserrange.WebKitRange.prototype.compareBrowserRangeEndpoints =
    function(range, thisEndpoint, otherEndpoint) {
  'use strict';
  return (
      goog.dom.browserrange.WebKitRange.superClass_.compareBrowserRangeEndpoints
          .call(this, range, thisEndpoint, otherEndpoint));
};


/** @override */
goog.dom.browserrange.WebKitRange.prototype.selectInternal = function(
    selection, reversed) {
  'use strict';
  if (reversed) {
    selection.setBaseAndExtent(
        this.getEndNode(), this.getEndOffset(), this.getStartNode(),
        this.getStartOffset());
  } else {
    selection.setBaseAndExtent(
        this.getStartNode(), this.getStartOffset(), this.getEndNode(),
        this.getEndOffset());
  }
};
