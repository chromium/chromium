/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Typedef for JavaScript types that can be JSON serialized.
 */

goog.module('goog.json.Jsonable');

/**
 * @typedef {boolean|number|string}
 */
let Primitive;

/**
 * @typedef {!Primitive|!Array|!Object}
 */
let NestedType;

/**
 * Types that can be JSON serialized. We only check one level deep for Objects
 * and Arrays so it's not checked at compile time whether nested types are
 * correct, and it would be possible for a user to pass in an invalid JSON
 * object. Which would be a bummer.
 * NOTE: If the compiler were to support recursive typedefs, this would be
 * {boolean|number|string|!Object<string, !Jsonable>|!Array<!Jsonable>}.
 * Recursive type checking is supported by @record but not @typedef.
 * @typedef {?Primitive|!Object<string, ?NestedType>|!Array<?NestedType>}
 */
let Jsonable;

exports = Jsonable;
