// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This variable is checked in SelectFileDialogExtensionBrowserTest.
 * @type {number}
 */
window.JSErrorCount = 0;

/**
 * Counts uncaught exceptions.
 */
window.onerror = function() { window.JSErrorCount++; };

/**
 * Wraps the function to use it as a callback.
 * This does:
 *  - Capture the stack trace in case of error.
 *  - Bind this object
 *
 * @param {Object=} opt_thisObject Object to be used as this.
 * @param {...} var_args Arguments to be bound with the wrapped function.
 * @return {function(...)} Wrapped function.
 */
Function.prototype.wrap = function(opt_thisObject, var_args) {
  var func = this;
  var liveStack = (new Error('Stack trace before async call')).stack;
  var thisObject = opt_thisObject || null;
  var boundArguments = Array.prototype.slice.call(arguments, 1);

  return function wrappedCallback(var_args) {
    try {
      var args = boundArguments.concat(Array.prototype.slice.call(arguments));
      return func.apply(thisObject, args);
    } catch (e) {
      // Some async function doesn't handle exception correctly. So outputting
      // the exception message and stack trace just in case.
      // The message will show twice if the caller handles exception correctly.
      console.error(e.stack);
      console.info('Exception above happened in callback.', liveStack);

      window.JSErrorCount++;
      throw e;
    }
  };
};
