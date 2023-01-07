/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Defines used by streams.
 */
goog.module('goog.streams.defines');

/**
 * 'false', 'true', or 'detect'. Detect does runtime feature detection.
 * @define {string}
 */
const USE_NATIVE_IMPLEMENTATION =
    goog.define('goog.streams.USE_NATIVE_IMPLEMENTATION', 'false');

exports = {
  USE_NATIVE_IMPLEMENTATION,
};
