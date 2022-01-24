/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Defines an interface for serializing objects into strings.
 */

goog.provide('goog.string.Stringifier');



/**
 * An interface for serializing objects into strings.
 * @interface
 */
goog.string.Stringifier = function() {};


/**
 * Serializes an object or a value to a string.
 * Agnostic to the particular format of object and string.
 *
 * @param {*} object The object to stringify.
 * @return {string} A string representation of the input.
 */
goog.string.Stringifier.prototype.stringify;
