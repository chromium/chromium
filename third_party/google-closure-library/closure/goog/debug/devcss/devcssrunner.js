/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Development CSS Compiler runtime execution.
 */

goog.provide('goog.debug.devCssRunner');

goog.require('goog.debug.DevCss');

(function() {
'use strict';
const devCssInstance = new goog.debug.DevCss();
devCssInstance.activateBrowserSpecificCssRules();
})();
