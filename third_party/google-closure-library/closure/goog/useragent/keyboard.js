/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Constants for determining keyboard support.
 */

goog.provide('goog.userAgent.keyboard');

goog.require('goog.labs.userAgent.platform');


/**
 * @define {boolean} Whether the user agent is running with in an environment
 * that should use Mac-based keyboard shortcuts (Meta instead of Ctrl, etc.).
 */
goog.userAgent.keyboard.ASSUME_MAC_KEYBOARD =
    goog.define('goog.userAgent.keyboard.ASSUME_MAC_KEYBOARD', false);


/**
 * Determines whether Mac-based keyboard shortcuts should be used.
 * @return {boolean}
 * @private
 */
goog.userAgent.keyboard.determineMacKeyboard_ = function() {
  'use strict';
  return goog.labs.userAgent.platform.isMacintosh() ||
      goog.labs.userAgent.platform.isIos();
};


/**
 * Whether the user agent is running in an environment that uses Mac-based
 * keyboard shortcuts.
 * @type {boolean}
 */
goog.userAgent.keyboard.MAC_KEYBOARD =
    goog.userAgent.keyboard.ASSUME_MAC_KEYBOARD ||
    goog.userAgent.keyboard.determineMacKeyboard_();
