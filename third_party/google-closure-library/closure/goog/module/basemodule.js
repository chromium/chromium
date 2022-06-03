/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Defines the base class for a module. This is used to allow the
 * code to be modularized, giving the benefits of lazy loading and loading on
 * demand.
 */

goog.provide('goog.module.BaseModule');

goog.require('goog.Disposable');
/** @suppress {extraRequire} */
goog.require('goog.module');



/**
 * A basic module object that represents a module of JavaScript code that can
 * be dynamically loaded.
 *
 * @constructor
 * @extends {goog.Disposable}
 */
goog.module.BaseModule = function() {
  'use strict';
  goog.Disposable.call(this);
};
goog.inherits(goog.module.BaseModule, goog.Disposable);


/**
 * Performs any load-time initialization that the module requires.
 * @param {Object} context The module context.
 */
goog.module.BaseModule.prototype.initialize = function(context) {};
