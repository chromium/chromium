// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Integration module for QUnit tests running in browser tests.
 * Specifically it:
 * - Sets QUnit.autostart to false, so that the browser test can hook the test
 *   results callback before the test starts.
 * - Implements a text-based test reporter to report test results back to the
 *   browser test.
 */

(function(QUnit, exports) {

'use strict';

var TEST_TIMEOUT_IN_MS = 5000;

var TestReporter = function() {
  this.errorMessage_ = '';
  this.failedTestsCount_ = 0;
  this.failedAssertions_ = [];
};

TestReporter.prototype.init = function(qunit) {
  qunit.testStart(this.onTestStart_.bind(this));
  qunit.testDone(this.onTestDone_.bind(this));
  qunit.log(this.onAssertion_.bind(this));
};

/**
 * @param {{ module:string, name: string }} details
 */
TestReporter.prototype.onTestStart_ = function(details) {};

/**
 * @param {{ module:string, name: string }} details
 */
TestReporter.prototype.onTestDone_ = function(details) {
  if (this.failedAssertions_.length > 0) {
    this.errorMessage_ += '  ' + details.module + '.' + details.name + '\n';
    this.errorMessage_ += this.failedAssertions_.map(
        function(assertion, index){
          return '    ' + (index + 1) + '. ' + assertion.message + '\n' +
                 '    ' + assertion.source;
        }).join('\n') + '\n';
    this.failedAssertions_ = [];
    this.failedTestsCount_++;
  }
};

TestReporter.prototype.onAssertion_ = function(details) {
  if (!details.result) {
    this.failedAssertions_.push(details);
  }
};

TestReporter.prototype.getErrorMessage = function(){
  var errorMessage = '';
  if (this.failedTestsCount_ > 0) {
    var test = (this.failedTestsCount_ > 1) ? 'tests' : 'test';
    errorMessage = this.failedTestsCount_  + ' ' + test + ' failed:\n';
    errorMessage += this.errorMessage_;
  }
  return errorMessage;
};

var BrowserTestHarness = function(qunit, reporter) {
  this.qunit_ = qunit;
  this.reporter_ = reporter;
};

BrowserTestHarness.prototype.init = function() {
  this.qunit_.config.autostart = false;
};

BrowserTestHarness.prototype.run = function() {
  return new Promise((resolve, reject) => {
    this.reporter_.init(this.qunit_);
    this.qunit_.start();
    this.qunit_.done(function(details) {
      resolve(JSON.stringify({
        passed: details.passed == details.total,
        errorMessage: this.reporter_.getErrorMessage()
      }));
    }.bind(this));
  });
};


if (!QUnit) {
  console.error('browser_test_harness.js must be included after QUnit.js.');
  return;
}

var testHarness = new BrowserTestHarness(QUnit, new TestReporter());
testHarness.init();
exports.browserTestHarness = testHarness;

var qunitTest = QUnit.test;

/**
 * Returns a promise that resolves after |delay| along with a timerId
 * for cancellation.
 *
 * @return {promise: !Promise, timerId: number}
 */
BrowserTestHarness.timeout = function(delay) {
  var timerId = 0;
  var promise = new Promise(function(resolve) {
      timerId  = window.setTimeout(function() {
        resolve();
      }, delay);
  });
  return {
    timerId: timerId,
    promise: promise
  };
};

QUnit.config.urlConfig.push({
    id: "disableTestTimeout",
    label: "disable test timeout",
    tooltip: "Check this when debugging locally to disable test timeout.",
});

/**
 * Forces the test to fail after |TEST_TIMEOUT_IN_MS|.
 *
 * @param {function(QUnit.Assert)} testCallback
 */
BrowserTestHarness.test = function(testCallback) {
  return function() {
    var args = Array.prototype.slice.call(arguments);
    var timeout = BrowserTestHarness.timeout(TEST_TIMEOUT_IN_MS);

    var testPromise = Promise.resolve(testCallback.apply(this, args))
                          .then(function(response) {
                            window.clearTimeout(timeout.timerId);
                            return response;
                          });

    var asserts = args[0];
    var timeoutPromise = timeout.promise.then(function(){
      asserts.ok(false, 'Test timed out after ' + TEST_TIMEOUT_IN_MS + ' ms')
    })

    return Promise.race([testPromise, timeoutPromise]);
  };
};

if (!QUnit.urlParams.disableTestTimeout) {
  QUnit.test = function(name, expected, testCallback, async) {
    qunitTest(name, expected, BrowserTestHarness.test(testCallback), async);
  };
}
})(window.QUnit, window);
