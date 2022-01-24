/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides a deprecated alias for goog.async.Throttle
 * @deprecated Use goog.async.Throttle instead.
 */
goog.module('goog.Throttle');
goog.module.declareLegacyNamespace();

const Throttle = goog.require('goog.async.Throttle');

exports = Throttle;
