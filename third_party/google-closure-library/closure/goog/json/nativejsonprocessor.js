/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Defines a class for parsing JSON using the browser's built in
 * JSON library.
 */

goog.module('goog.json.NativeJsonProcessor');
goog.module.declareLegacyNamespace();

const Parser = goog.require('goog.string.Parser');
const Stringifier = goog.require('goog.string.Stringifier');
const asserts = goog.require('goog.asserts');
const {Replacer, Reviver} = goog.require('goog.json.types');



/**
 * A class that parses and stringifies JSON using the browser's built-in JSON
 * library.
 *

 * @implements {Parser}
 * @implements {Stringifier}
 * @final
 */
exports = class {
  /**
   * @param {?Replacer=} opt_replacer An optional replacer to use during
   *     serialization.
   * @param {?=} opt_reviver An optional reviver to use during
   *     parsing.
   */
  constructor(opt_replacer, opt_reviver) {
    asserts.assert(goog.global['JSON'] !== undefined, 'JSON not defined');

    /**
     * @type {!Replacer|null|undefined}
     * @private
     */
    this.replacer_ = opt_replacer;

    /**
     * @type {!Reviver|null|undefined}
     * @private
     */
    this.reviver_ = opt_reviver;
  };

  /**
   * Serializes an object or a value to a string.
   * Agnostic to the particular format of object and string.
   *
   * @param {*} object The object to stringify.
   * @return {string} A string representation of the input.
   * @override
   */
  stringify(object) {
    return goog.global['JSON'].stringify(object, this.replacer_);
  }

  /**
   * Parses a string into an object and returns the result.
   * Agnostic to the format of string and object.
   *
   * @param {string} s The string to parse.
   * @return {*} The object generated from the string.
   * @override
   */
  parse(s) {
    return goog.global['JSON'].parse(s, this.reviver_);
  }
};
