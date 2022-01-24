/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Test adapter for testing Closure Promises against the
 * Promises/A+ Compliance Test Suite, which is implemented as a Node.js module.
 *
 * This test suite adapter may not be run in Node.js directly, but must first be
 * compiled with the Closure Compiler to pull in the required dependencies.
 *
 * @see https://npmjs.org/package/promises-aplus-tests
 * @suppress {undefinedVars} Node.js's process and require
 */

goog.provide('goog.promise.testSuiteAdapter');

goog.require('goog.Promise');

goog.setTestOnly('goog.promise.testSuiteAdapter');


var promisesAplusTests = /** @type {function(!Object, function(*))} */ (
    require('promises-aplus-tests'));


/**
 * Adapter for specifying Promise-creating functions to the Promises test suite.
 * @const
 */
goog.promise.testSuiteAdapter = {
  /** @type {function(*): !goog.Promise} */
  'resolved': goog.Promise.resolve,

  /** @type {function(*): !goog.Promise} */
  'rejected': goog.Promise.reject,

  /** @return {!Object} */
  'deferred': function() {
    'use strict';
    var promiseObj = {};
    promiseObj['promise'] = new goog.Promise(function(resolve, reject) {
      'use strict';
      promiseObj['resolve'] = resolve;
      promiseObj['reject'] = reject;
    });
    return promiseObj;
  }
};


// Node.js defines setTimeout globally, but Closure relies on finding it
// defined on goog.global.
goog.exportSymbol('setTimeout', setTimeout);


// Rethrowing an error to the global scope kills Node immediately. Suppress
// error rethrowing for running this test suite.
goog.Promise.setUnhandledRejectionHandler(goog.nullFunction);


// Run the tests, exiting with a failure code if any of the tests fail.
promisesAplusTests(goog.promise.testSuiteAdapter, function(err) {
  'use strict';
  if (err) {
    process.exit(1);
  }
});
