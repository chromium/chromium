/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utilities for working with ranges comprised of multiple
 * sub-ranges.
 */


goog.provide('goog.dom.AbstractMultiRange');

goog.require('goog.array');
goog.require('goog.dom');
goog.require('goog.dom.AbstractRange');
goog.require('goog.dom.TextRange');



/**
 * Creates a new multi range with no properties.  Do not use this
 * constructor: use one of the goog.dom.Range.createFrom* methods instead.
 * @constructor
 * @extends {goog.dom.AbstractRange}
 * @abstract
 */
goog.dom.AbstractMultiRange = function() {};
goog.inherits(goog.dom.AbstractMultiRange, goog.dom.AbstractRange);


/** @override */
goog.dom.AbstractMultiRange.prototype.containsRange = function(
    otherRange, opt_allowPartial) {
  'use strict';
  // TODO(user): This will incorrectly return false if two (or more) adjacent
  // elements are both in the control range, and are also in the text range
  // being compared to.
  var /** !Array<?goog.dom.TextRange> */ ranges = this.getTextRanges();
  var otherRanges = otherRange.getTextRanges();

  var fn = opt_allowPartial ? goog.array.some : goog.array.every;
  return fn(otherRanges, function(otherRange) {
    'use strict';
    return goog.array.some(ranges, function(range) {
      'use strict';
      return range.containsRange(otherRange, opt_allowPartial);
    });
  });
};


/** @override */
goog.dom.AbstractMultiRange.prototype.containsNode = function(
    node, opt_allowPartial) {
  'use strict';
  return this.containsRange(
      goog.dom.TextRange.createFromNodeContents(node), opt_allowPartial);
};



/** @override */
goog.dom.AbstractMultiRange.prototype.insertNode = function(node, before) {
  'use strict';
  if (before) {
    goog.dom.insertSiblingBefore(node, this.getStartNode());
  } else {
    goog.dom.insertSiblingAfter(node, this.getEndNode());
  }
  return node;
};


/** @override */
goog.dom.AbstractMultiRange.prototype.surroundWithNodes = function(
    startNode, endNode) {
  'use strict';
  this.insertNode(startNode, true);
  this.insertNode(endNode, false);
};
