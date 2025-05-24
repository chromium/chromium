/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview This shim namespace defines the shared
 * mechanism variables used in mechanismSeparationTester
 * and mechanismSelectionTester. This exists to allow test compilation
 * to work correctly for these legacy tests.
 */

goog.module('goog.storage.mechanism.testhelpers');
goog.setTestOnly('goog.storage.mechanism.testhelpers');

const {getFunctionName} = goog.require('goog.debug');

/**
 * @param {!Array<!Function>} tests
 * @param {!Function} bindFnCallback
 * @return {!Object}
 */
exports.bindTests = function(tests, bindFnCallback) {
  const boundTests = {};
  for (const test of tests) {
    const name = getFunctionName(test);
    if (!name.startsWith('test')) continue;
    boundTests[name] = () => void bindFnCallback(test);
  }
  return boundTests;
};
