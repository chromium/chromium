/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Description of this file.
 */
goog.module('goog.json.types');

/**
 * JSON replacer, as defined in Section 15.12.3 of the ES5 spec.
 * @see http://ecma-international.org/ecma-262/5.1/#sec-15.12.3
 *
 * TODO(nicksantos): Array should also be a valid replacer.
 *
 * @typedef {function(this:Object, string, *): *}
 */
exports.Replacer;


/**
 * JSON reviver, as defined in Section 15.12.2 of the ES5 spec.
 * @see http://ecma-international.org/ecma-262/5.1/#sec-15.12.3
 *
 * @typedef {function(this:Object, string, *): *}
 */
exports.Reviver;
