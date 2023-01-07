// Copyright 2018 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

var allTests = require('./alltests');

// Timeout for individual test package to complete.
var TEST_TIMEOUT = 45 * 1000;
var TEST_SERVER = 'http://localhost:8080';
var IGNORED_TESTS = [
  // Test hangs in IE8.
  'closure/goog/ui/plaintextspellchecker_test.html',
  // TODO(joeltine): Re-enable once fixed for external testing.
  'closure/goog/testing/multitestrunner_test.html',
  // These Promise-based tests all timeout for unknown reasons.
  // Disable for now.
  'closure/goog/testing/fs/integration_test.html',
  'closure/goog/debug/fpsdisplay_test.html',
  'closure/goog/net/jsloader_test.html',
  'closure/goog/net/filedownloader_test.html',
  'closure/goog/promise/promise_test.html',
  'closure/goog/editor/plugins/abstractdialogplugin_test.html',
  'closure/goog/net/crossdomainrpc_test.html',
  // Causes flaky Adobe Acrobat update popups.
  'closure/goog/useragent/flash_test.html',
  'closure/goog/useragent/jscript_test.html',
];

describe('Run all Closure unit tests', function() {
  var removeIgnoredTests = function(tests) {
    for (var i = 0; i < IGNORED_TESTS.length; i++) {
      var index = tests.indexOf(IGNORED_TESTS[i]);
      if (index != -1) {
        tests.splice(index, 1);
      }
    }
    return tests;
  };

  beforeAll(function() {
    allTests = removeIgnoredTests(allTests);
  });

  beforeEach(function() {
    // Ignores synchronization with angular loading. Since we don't use angular,
    // enable it.
    browser.ignoreSynchronization = true;
  });

  // Polls currently loaded test page for test completion. Returns Promise that
  // will resolve when test is finished.
  var waitForTestSuiteCompletion = function(testPath) {
    var testStartTime = +new Date();

    var waitForTest = function(resolve, reject) {
      // executeScript runs the passed method in the "window" context of
      // the current test. JSUnit exposes hooks into the test's status through
      // the "G_testRunner" global object.
      browser
          .executeScript(function() {
            if (window['G_testRunner'] &&
                window['G_testRunner']['isFinished']()) {
              var status = {};
              status['isFinished'] = true;
              status['isSuccess'] = window['G_testRunner']['isSuccess']();
              status['report'] = window['G_testRunner']['getReport']();
              return status;
            } else {
              return {'isFinished': false};
            }
          })
          .then(
              function(status) {
                if (status && status.isFinished) {
                  resolve(status);
                } else {
                  var currTime = +new Date();
                  if (currTime - testStartTime > TEST_TIMEOUT) {
                    status.isSuccess = false;
                    status.report = testPath + ' timed out after ' +
                        (TEST_TIMEOUT / 1000) + 's!';
                    // resolve so tests continue running.
                    resolve(status);
                  } else {
                    // Check every 300ms for completion.
                    setTimeout(
                        waitForTest.bind(undefined, resolve, reject), 300);
                  }
                }
              },
              function(err) {
                reject(err);
              });
    };

    return new Promise(function(resolve, reject) {
      waitForTest(resolve, reject);
    });
  };

  it('should run all tests with 0 failures', function(done) {
    var failureReports = [];

    // Navigates to testPath to invoke tests. Upon completion inspects returned
    // test status and keeps track of the total number failed tests.
    var runNextTest = function(testPath) {
      return browser.navigate()
          .to(TEST_SERVER + '/' + testPath)
          .then(function() {
            return waitForTestSuiteCompletion(testPath);
          })
          .then(function(status) {
            if (!status.isSuccess) {
              failureReports.push(status.report);
            }

            return status;
          });
    };

    // Chains the next test to the completion of the previous through its
    // promise.
    var chainNextTest = function(promise, test) {
      return promise.then(function() {
        runNextTest(test);
      });
    };

    var testPromise = null;
    for (var i = 0; i < allTests.length; i++) {
      if (testPromise != null) {
        testPromise = chainNextTest(testPromise, allTests[i]);
      } else {
        testPromise = runNextTest(allTests[i]);
      }
    }

    testPromise.then(function() {
      var totalFailures = failureReports.length;
      if (totalFailures > 0) {
        console.error('There was ' + totalFailures + ' test failure(s)!');
        for (var i = 0; i < failureReports.length; i++) {
          console.error(failureReports[i]);
        }
      }
      expect(failureReports.length).toBe(0);
      done();
    });
  });
});
