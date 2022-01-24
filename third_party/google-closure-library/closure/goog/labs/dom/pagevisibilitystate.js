/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Page visibility states for PageVisiblityMonitor.
 */

goog.module('goog.labs.dom.PageVisibilityState');
goog.module.declareLegacyNamespace();

/**
 * The different visibility states.
 * @enum {string}
 */
exports = {
  HIDDEN: 'hidden',
  VISIBLE: 'visible',
  PRERENDER: 'prerender',
  UNLOADED: 'unloaded'
};
