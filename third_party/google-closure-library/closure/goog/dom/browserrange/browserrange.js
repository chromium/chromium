/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the browser range namespace and interface, as
 * well as several useful utility functions.
 *
 * DO NOT USE THIS FILE DIRECTLY.  Use goog.dom.Range instead.
 */


goog.provide('goog.dom.browserrange');
goog.provide('goog.dom.browserrange.Error');

goog.require('goog.dom');
goog.require('goog.dom.NodeType');
goog.require('goog.dom.browserrange.GeckoRange');
goog.require('goog.dom.browserrange.W3cRange');
goog.require('goog.dom.browserrange.WebKitRange');
goog.require('goog.userAgent');
goog.requireType('goog.dom.browserrange.AbstractRange');


/**
 * Common error constants.
 * @enum {string}
 */
goog.dom.browserrange.Error = {
  NOT_IMPLEMENTED: 'Not Implemented'
};


// NOTE(robbyw): While it would be nice to eliminate the duplicate switches
//               below, doing so uncovers bugs in the JsCompiler in which
//               necessary code is stripped out.


/**
 * Static method that returns the proper type of browser range.
 * @param {Range|TextRange} range A browser range object.
 * @return {!goog.dom.browserrange.AbstractRange} A wrapper object.
 */
goog.dom.browserrange.createRange = function(range) {
  'use strict';
  if (goog.userAgent.WEBKIT) {
    return new goog.dom.browserrange.WebKitRange(
        /** @type {Range} */ (range));
  } else if (goog.userAgent.GECKO) {
    return new goog.dom.browserrange.GeckoRange(
        /** @type {Range} */ (range));
  } else {
    // Default other browsers, including Opera, to W3c ranges.
    return new goog.dom.browserrange.W3cRange(
        /** @type {Range} */ (range));
  }
};


/**
 * Static method that returns the proper type of browser range.
 * @param {Node} node The node to select.
 * @return {!goog.dom.browserrange.AbstractRange} A wrapper object.
 */
goog.dom.browserrange.createRangeFromNodeContents = function(node) {
  'use strict';
  if (goog.userAgent.WEBKIT) {
    return goog.dom.browserrange.WebKitRange.createFromNodeContents(node);
  } else if (goog.userAgent.GECKO) {
    return goog.dom.browserrange.GeckoRange.createFromNodeContents(node);
  } else {
    // Default other browsers to W3c ranges.
    return goog.dom.browserrange.W3cRange.createFromNodeContents(node);
  }
};


/**
 * Static method that returns the proper type of browser range.
 * @param {Node} startNode The node to start with.
 * @param {number} startOffset The offset within the node to start.  This is
 *     either the index into the childNodes array for element startNodes or
 *     the index into the character array for text startNodes.
 * @param {Node} endNode The node to end with.
 * @param {number} endOffset The offset within the node to end.  This is
 *     either the index into the childNodes array for element endNodes or
 *     the index into the character array for text endNodes.
 * @return {!goog.dom.browserrange.AbstractRange} A wrapper object.
 */
goog.dom.browserrange.createRangeFromNodes = function(
    startNode, startOffset, endNode, endOffset) {
  'use strict';
  if (goog.userAgent.WEBKIT) {
    return goog.dom.browserrange.WebKitRange.createFromNodes(
        startNode, startOffset, endNode, endOffset);
  } else if (goog.userAgent.GECKO) {
    return goog.dom.browserrange.GeckoRange.createFromNodes(
        startNode, startOffset, endNode, endOffset);
  } else {
    // Default other browsers to W3c ranges.
    return goog.dom.browserrange.W3cRange.createFromNodes(
        startNode, startOffset, endNode, endOffset);
  }
};


/**
 * Tests whether the given node can contain a range end point.
 * @param {Node} node The node to check.
 * @return {boolean} Whether the given node can contain a range end point.
 */
goog.dom.browserrange.canContainRangeEndpoint = function(node) {
  'use strict';
  // NOTE(user): This is not complete, as divs with style -
  // 'display:inline-block' or 'position:absolute' can also not contain range
  // endpoints. A more complete check is to see if that element can be partially
  // selected (can be container) or not.
  return goog.dom.canHaveChildren(node) ||
      node.nodeType == goog.dom.NodeType.TEXT;
};
