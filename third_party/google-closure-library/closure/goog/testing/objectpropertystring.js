/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Helper for passing property names as string literals in
 * compiled test code.
 */

goog.setTestOnly('goog.testing.ObjectPropertyString');
goog.provide('goog.testing.ObjectPropertyString');



/**
 * Object to pass a property name as a string literal and its containing object
 * when the JSCompiler is rewriting these names. This should only be used in
 * test code.
 *
 * @param {Object} object The containing object.
 * @param {Object|string} propertyString Property name as a string literal.
 * @constructor
 * @final
 * @deprecated Use goog.reflect.objectProperty instead.
 */
goog.testing.ObjectPropertyString = function(object, propertyString) {
  'use strict';
  this.object_ = object;
  this.propertyString_ = /** @type {string} */ (propertyString);
};


/**
 * @type {Object}
 * @private
 */
goog.testing.ObjectPropertyString.prototype.object_;


/**
 * @type {string}
 * @private
 */
goog.testing.ObjectPropertyString.prototype.propertyString_;


/**
 * @return {Object} The object.
 */
goog.testing.ObjectPropertyString.prototype.getObject = function() {
  'use strict';
  return this.object_;
};


/**
 * @return {string} The property string.
 */
goog.testing.ObjectPropertyString.prototype.getPropertyString = function() {
  'use strict';
  return this.propertyString_;
};
