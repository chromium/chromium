/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Execution hooks for BrowserChannel. Allows applications to
 * receive a callback when BrowserChannel is processing requests.
 */
goog.module('goog.net.browserchannelinternal.hooks');
goog.module.declareLegacyNamespace();

/**
 * Allows the application to set an execution hooks for when BrowserChannel
 * starts processing requests. This is useful to track timing or logging
 * special information. The function takes no parameters and return void.
 * @param {Function} startHook  The function for the start hook.
 */
const setStartThreadExecutionHook = function(startHook) {
  startExecutionHook = startHook;
};
exports.setStartThreadExecutionHook = setStartThreadExecutionHook;


/**
 * Allows the application to set an execution hooks for when BrowserChannel
 * stops processing requests. This is useful to track timing or logging
 * special information. The function takes no parameters and return void.
 * @param {Function} endHook  The function for the end hook.
 */
function setEndThreadExecutionHook(endHook) {
  endExecutionHook = endHook;
}
exports.setEndThreadExecutionHook = setEndThreadExecutionHook;


/**
 * Application provided execution hook for the start hook.
 *
 * @type {Function}
 * @private
 */
let startExecutionHook = function() {};


/**
 * Application provided execution hook for the end hook.
 *
 * @type {Function}
 * @private
 */
let endExecutionHook = function() {};


/**
 * Helper function to call the start hook
 */
function onStartExecution() {
  startExecutionHook();
}
exports.onStartExecution = onStartExecution;


/**
 * Helper function to call the end hook
 */
function onEndExecution() {
  endExecutionHook();
}
exports.onEndExecution = onEndExecution;


/**
 * Wrapper around SafeTimeout which calls the start and end execution hooks
 * with a try...finally block.
 * @param {Function} fn The callback function.
 * @param {number} ms The time in MS for the timer.
 * @return {number} The ID of the timer.
 */
function setTimeout(fn, ms) {
  if (typeof fn !== 'function') {
    throw new Error('Fn must not be null and must be a function');
  }
  return goog.global.setTimeout(function() {
    onStartExecution();
    try {
      fn();
    } finally {
      onEndExecution();
    }
  }, ms);
}
exports.setTimeout = setTimeout;
