/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Defines an interface for parsing strings into objects.
 */

goog.provide('goog.string.Parser');



/**
 * An interface for parsing strings into objects.
 * @interface
 */
goog.string.Parser = function() {};


/**
 * Parses a string into an object and returns the result.
 * Agnostic to the format of string and object.
 *
 * @param {string} s The string to parse.
 * @return {*} The object generated from the string.
 */
goog.string.Parser.prototype.parse;
