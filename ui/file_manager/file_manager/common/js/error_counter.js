// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GlitchType, reportGlitch} from './glitch.js';

/**
 * This variable is checked in several integration and unit tests, to make sure
 * that new code changes don't cause unhandled exceptions.
 * @type {number}
 */
window.JSErrorCount = 0;

/**
 * Count uncaught exceptions.
 */
window.onerror = (message, url) => {
  window.JSErrorCount++;
  reportGlitch(GlitchType.UNHANDLED_ERROR);
};

/**
 * Count uncaught errors in promises.
 */
window.addEventListener('unhandledrejection', (event) => {
  reportGlitch(GlitchType.UNHANDLED_REJECTION);
  console.error(event.reason);
});

/**
 * Overrides console.error() to count errors.
 *
 * @param {...*} args Message and/or objects to be logged.
 */
console.error = (() => {
  const orig = console.error;
  return (...args) => {
    window.JSErrorCount++;
    const currentStack = new Error('current stack').stack;
    const originalStack = args && args[0] && args[0].stack;
    const prefix = '[unhandled-error]: ';
    if (args.length) {
      args[0] = prefix + args[0];
    } else {
      args.push(prefix);
    }
    args.push([currentStack]);
    if (originalStack) {
      args.push('Original stack:\n' + originalStack);
    }
    return orig.apply(this, [args.join('\n')]);
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
      window.JSErrorCount++;
    }
    return orig.apply(this, [condition].concat(args.join('\n')));
  };
})();
