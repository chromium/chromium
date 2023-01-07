/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Defines a class for parsing JSON using the browser's built in
 * JSON library.
 */

goog.provide('goog.json.NativeJsonProcessor');

goog.require('goog.asserts');
goog.require('goog.json.Processor');



/**
 * A class that parses and stringifies JSON using the browser's built-in JSON
 * library, if it is available.
 *
 * Note that the native JSON api has subtle differences across browsers, so
 * use this implementation with care.  See json_test#assertSerialize
 * for details on the differences from goog.json.
 *
 * This implementation is signficantly faster than goog.json, at least on
 * Chrome.  See json_perf.html for a perf test showing the difference.
 *
 * @param {?goog.json.Replacer=} opt_replacer An optional replacer to use during
 *     serialization.
 * @param {?goog.json.Reviver=} opt_reviver An optional reviver to use during
 *     parsing.
 * @constructor
 * @implements {goog.json.Processor}
 * @final
 */
goog.json.NativeJsonProcessor = function(opt_replacer, opt_reviver) {
  'use strict';
  goog.asserts.assert(goog.global['JSON'] !== undefined, 'JSON not defined');

  /**
   * @type {goog.json.Replacer|null|undefined}
   * @private
   */
  this.replacer_ = opt_replacer;

  /**
   * @type {goog.json.Reviver|null|undefined}
   * @private
   */
  this.reviver_ = opt_reviver;
};


/** @override */
goog.json.NativeJsonProcessor.prototype.stringify = function(object) {
  'use strict';
  return goog.global['JSON'].stringify(object, this.replacer_);
};


/** @override */
goog.json.NativeJsonProcessor.prototype.parse = function(s) {
  'use strict';
  return goog.global['JSON'].parse(s, this.reviver_);
};
