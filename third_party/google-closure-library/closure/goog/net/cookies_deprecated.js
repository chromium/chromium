/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A static instance of goog.net.Cookies that uses the default
 * window document.
 * @deprecated use `goog.net.Cookies.getInstance()` instead.
 */

goog.provide('goog.net.cookies');

goog.require('goog.net.Cookies');

// TODO(closure-team): This should be a singleton getter instead of a static
// instance.
/**
 * A static default instance.
 * @const {!goog.net.Cookies}
 */
goog.net.cookies = goog.net.Cookies.getInstance();
