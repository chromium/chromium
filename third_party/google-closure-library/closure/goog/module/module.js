/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 *
 * @fileoverview This class supports the dynamic loading of compiled
 * javascript modules at runtime, as described in the designdoc.
 *
 *   <http://go/js_modules_design>
 */

goog.provide('goog.module');

// TODO(johnlenz): Here we explicitly initialize the namespace to avoid
// problems with the goog.module method in base.js. We should rename this
// entire package to goog.loader and then we can delete this file.
//
// However, note that it is tricky to do that without breaking the world.
/**
 * @suppress {duplicate}
 * @type {function(string):void}
 */
goog.module = goog.module || {};
