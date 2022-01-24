/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Defines an interface for JSON parsing and serialization.
 */

goog.provide('goog.json.Processor');

goog.require('goog.string.Parser');
goog.require('goog.string.Stringifier');



/**
 * An interface for JSON parsing and serialization.
 * @interface
 * @extends {goog.string.Parser}
 * @extends {goog.string.Stringifier}
 */
goog.json.Processor = function() {};
