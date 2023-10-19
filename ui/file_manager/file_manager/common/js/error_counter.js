// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GlitchType, reportGlitch} from './glitch.js';

/**
 * This variable is checked in several integration and unit tests, to make sure
 * that new code changes don't cause unhandled exceptions.
 * @type {number}
 */
// @ts-ignore: error TS2339: Property 'JSErrorCount' does not exist on type
// 'Window & typeof globalThis'.
window.JSErrorCount = 0;

/**
 * Creates a list of arguments extended with stack information.
 * @param {string} prefix The prefix indicating type of error situation.
 * @param {...*} args The remaining, if any, arguments of the call.
 * @return {string} A string representing args and stack traces.
 */
function createLoggableArgs(prefix, ...args) {
  const argsStack = args && args[0] && args[0].stack;
  if (args.length) {
    const args0 = args[0];
    args[0] = `[${prefix}]: ` +
        (args0 instanceof PromiseRejectionEvent ? args0.reason : args0);
  } else {
    args.push(prefix);
  }
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  const currentStack = new Error('current stack').stack.split('\n');
  // Remove stack trace that is specific to this function.
  currentStack.splice(1, 1);
  args.push(currentStack.join('\n'));
  if (argsStack) {
    args.push('Original stack:\n' + argsStack);
  }
  return args.join('\n');
}

/**
 * Count uncaught exceptions.
 */
// @ts-ignore: error TS6133: 'url' is declared but its value is never read.
window.onerror = (message, url) => {
  // @ts-ignore: error TS2339: Property 'JSErrorCount' does not exist on type
  // 'Window & typeof globalThis'.
  window.JSErrorCount++;
  reportGlitch(GlitchType.UNHANDLED_ERROR);
};

/**
 * Count uncaught errors in promises.
 */
window.addEventListener('unhandledrejection', (event) => {
  // @ts-ignore: error TS2339: Property 'JSErrorCount' does not exist on type
  // 'Window & typeof globalThis'.
  window.JSErrorCount++;
  reportGlitch(GlitchType.UNHANDLED_REJECTION);
  console.warn(createLoggableArgs('unhandled-rejection', event));
});

/**
 * Overrides console.error() to count errors.
 *
 * @param {...*} args Message and/or objects to be logged.
 */
console.error = (() => {
  const orig = console.error;
  return (...args) => {
    // @ts-ignore: error TS2339: Property 'JSErrorCount' does not exist on type
    // 'Window & typeof globalThis'.
    window.JSErrorCount++;
    return orig.apply(this, [createLoggableArgs('unhandled-error', ...args)]);
  };
})();

/**
 * Overrides console.assert() to count errors.
 *
 * @param {boolean} condition If false, log a message and stack trace.
 * @param {...*} args Message and/or objects to be logged when condition is
 * false.
 */
console.assert = (() => {
  const orig = console.assert;
  return (condition, ...args) => {
    const stack = new Error('original stack').stack;
    args.push(stack);
    if (!condition) {
      // @ts-ignore: error TS2339: Property 'JSErrorCount' does not exist on
      // type 'Window & typeof globalThis'.
      window.JSErrorCount++;
    }
    // @ts-ignore: error TS2769: No overload matches this call.
    return orig.apply(this, [condition].concat(args.join('\n')));
  };
})();
