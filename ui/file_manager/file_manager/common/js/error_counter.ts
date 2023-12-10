// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GlitchType, reportGlitch} from './glitch.js';

/**
 * This variable is checked in several integration and unit tests, to make sure
 * that new code changes don't cause unhandled exceptions.
 */
window.JSErrorCount = 0;

/**
 * Creates a list of arguments extended with stack information.
 * @param prefix The prefix indicating type of error situation.
 * @param args The remaining, if any, arguments of the call.
 * @return A string representing args and stack traces.
 */
function createLoggableArgs(prefix: string, ...args: any[]): string {
  const argsStack = args && args[0] && args[0].stack;
  if (args.length) {
    const args0 = args[0];
    args[0] = `[${prefix}]: ` +
        (args0 instanceof PromiseRejectionEvent ? args0.reason : args0);
  } else {
    args.push(prefix);
  }
  const currentStack = new Error('current stack').stack!.split('\n');
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
window.onerror = () => {
  window.JSErrorCount++;
  reportGlitch(GlitchType.UNHANDLED_ERROR);
};

/**
 * Count uncaught errors in promises.
 */
window.addEventListener('unhandledrejection', (event) => {
  window.JSErrorCount++;
  reportGlitch(GlitchType.UNHANDLED_REJECTION);
  console.warn(createLoggableArgs('unhandled-rejection', event));
});

/**
 * Overrides console.error() to count errors.
 *
 * @param args Message and/or objects to be logged.
 */
console.error = (() => {
  const orig = console.error;
  return (...args: any[]) => {
    window.JSErrorCount++;
    return orig.apply(this, [createLoggableArgs('unhandled-error', ...args)]);
  };
})();

/**
 * Overrides console.assert() to count errors.
 *
 * @param condition If false, log a message and stack trace.
 * @param args Message and/or objects to be logged when condition is
 * false.
 */
console.assert = (() => {
  const orig = console.assert;
  return (condition: boolean, ...args: any[]) => {
    const stack = new Error('original stack').stack;
    args.push(stack);
    if (!condition) {
      window.JSErrorCount++;
    }
    return orig.apply(this, [condition].concat(args));
  };
})();
