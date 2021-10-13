/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Generic immutable node object to be used in collections.
 */


goog.provide('goog.structs.Node');



/**
 * A generic immutable node. This can be used in various collections that
 * require a node object for its item (such as a heap).
 * @param {K} key Key.
 * @param {V} value Value.
 * @constructor
 * @template K, V
 */
goog.structs.Node = function(key, value) {
  'use strict';
  /**
   * The key.
   * @private {K}
   */
  this.key_ = key;

  /**
   * The value.
   * @private {V}
   */
  this.value_ = value;
};


/**
 * Gets the key.
 * @return {K} The key.
 */
goog.structs.Node.prototype.getKey = function() {
  'use strict';
  return this.key_;
};


/**
 * Gets the value.
 * @return {V} The value.
 */
goog.structs.Node.prototype.getValue = function() {
  'use strict';
  return this.value_;
};


/**
 * Clones a node and returns a new node.
 * @return {!goog.structs.Node<K, V>} A new goog.structs.Node with the same
 *     key value pair.
 */
goog.structs.Node.prototype.clone = function() {
  'use strict';
  return new goog.structs.Node(this.key_, this.value_);
};
