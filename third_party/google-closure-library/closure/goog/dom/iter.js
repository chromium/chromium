/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Iterators over DOM nodes.
 */

goog.provide('goog.dom.iter');
goog.provide('goog.dom.iter.AncestorIterator');
goog.provide('goog.dom.iter.ChildIterator');
goog.provide('goog.dom.iter.SiblingIterator');

goog.require('goog.iter');
goog.require('goog.iter.Iterator');



/**
 * Iterator over a Node's siblings.
 * @param {Node} node The node to start with.
 * @param {boolean=} opt_includeNode Whether to return the given node as the
 *     first return value from next.
 * @param {boolean=} opt_reverse Whether to traverse siblings in reverse
 *     document order.
 * @constructor
 * @extends {goog.iter.Iterator}
 */
goog.dom.iter.SiblingIterator = function(node, opt_includeNode, opt_reverse) {
  'use strict';
  /**
   * The current node, or null if iteration is finished.
   * @type {Node}
   * @private
   */
  this.node_ = node;

  /**
   * Whether to iterate in reverse.
   * @type {boolean}
   * @private
   */
  this.reverse_ = !!opt_reverse;

  if (node && !opt_includeNode) {
    this.next();
  }
};
goog.inherits(goog.dom.iter.SiblingIterator, goog.iter.Iterator);


/**
 * @return {!IIterableResult<!Node>}
 * @override
 */
goog.dom.iter.SiblingIterator.prototype.next = function() {
  'use strict';
  var node = this.node_;
  if (!node) {
    return goog.iter.ES6_ITERATOR_DONE;
  }
  this.node_ = this.reverse_ ? node.previousSibling : node.nextSibling;
  return goog.iter.createEs6IteratorYield(node);
};


/**
 * Iterator over an Element's children.
 * @param {Element} element The element to iterate over.
 * @param {boolean=} opt_reverse Optionally traverse children from last to
 *     first.
 * @param {number=} opt_startIndex Optional starting index.
 * @constructor
 * @extends {goog.dom.iter.SiblingIterator}
 * @final
 */
goog.dom.iter.ChildIterator = function(element, opt_reverse, opt_startIndex) {
  'use strict';
  if (opt_startIndex === undefined) {
    opt_startIndex = opt_reverse && element.childNodes.length ?
        element.childNodes.length - 1 :
        0;
  }
  goog.dom.iter.SiblingIterator.call(
      this, element.childNodes[opt_startIndex], true, opt_reverse);
};
goog.inherits(goog.dom.iter.ChildIterator, goog.dom.iter.SiblingIterator);



/**
 * Iterator over a Node's ancestors, stopping after the document body.
 * @param {Node} node The node to start with.
 * @param {boolean=} opt_includeNode Whether to return the given node as the
 *     first return value from next.
 * @constructor
 * @extends {goog.iter.Iterator}
 * @final
 */
goog.dom.iter.AncestorIterator = function(node, opt_includeNode) {
  'use strict';
  /**
   * The current node, or null if iteration is finished.
   * @type {Node}
   * @private
   */
  this.node_ = node;

  if (node && !opt_includeNode) {
    this.next();
  }
};
goog.inherits(goog.dom.iter.AncestorIterator, goog.iter.Iterator);


/**
 * @return {!IIterableResult<!Node>}
 * @override
 */
goog.dom.iter.AncestorIterator.prototype.next = function() {
  'use strict';
  var node = this.node_;
  if (!node) {
    return goog.iter.ES6_ITERATOR_DONE;
  }
  this.node_ = node.parentNode;
  return goog.iter.createEs6IteratorYield(node);
};