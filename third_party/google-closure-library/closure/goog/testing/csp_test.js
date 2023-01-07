/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.setTestOnly();

goog.require('goog.testing.TestCase');
goog.require('goog.testing.asserts');
goog.require('goog.testing.jsunit');

const meta = /**@type {!HTMLMetaElement} */ (document.createElement('meta'));
meta.httpEquiv = 'Content-Security-Policy';
meta.content = 'object-src \'none\'; ' +
    'script-src \'nonce-CSP+Nonce+For+Tests+Only\' \'unsafe-inline\' ' +
    '\'unsafe-eval\' \'strict-dynamic\' https: http:; base-uri \'none\'';
document.head.appendChild(meta);

function shouldRunTests() {
  return !!window.ReportingObserver;
}

function testCspViolationCausesTestCaseToFail() {
  goog.testing.TestCase.getActiveTestCase().observeCspViolations(false);

  const testCase = new goog.testing.TestCase();
  testCase.addNewTest('test', function() {
    doCspViolation1();
  });
  testCase.runTests();
  assertFalse(testCase.isSuccess());
  assertRegExp(/\bContent Security Policy\b/, testCase.getReport());
}

function testCspViolationReportedForFailingTest() {
  goog.testing.TestCase.getActiveTestCase().observeCspViolations(false);

  const testCase = new goog.testing.TestCase();
  testCase.addNewTest('test', function() {
    doCspViolation2();
    assertTrue(false);
  });
  testCase.runTests();
  assertFalse(testCase.isSuccess());
  assertRegExp(/\bContent Security Policy\b/, testCase.getReport());
}

function testCspViolationCausesTestCaseToFail_lifeCycle() {
  goog.testing.TestCase.getActiveTestCase().observeCspViolations(false);

  const tests = [
    ['setUp', doCspViolation3], ['tearDown', doCspViolation4],
    ['setUpPage', doCspViolation5], ['runTests', doCspViolation6],
    ['shouldRunTests', doCspViolation7]
  ];
  for (const [name, doCspViolation] of tests) {
    const testCase = new goog.testing.TestCase();
    testCase.ignoreStartupCspViolations();
    let didRun = false;
    testCase.setLifecycleObj({
      [name]: function() {
        didRun = true;
        doCspViolation();
        if (name == 'shouldRunTests') {
          return true;
        }
      },
    });
    testCase.addNewTest('test1', function() {
      // do nothing
    });
    testCase.addNewTest('test2', function() {
      // do nothing
    });
    testCase.runTests();
    assertTrue(`${name}() did not run`, didRun);
    assertFalse(`${name}() should have failed`, testCase.isSuccess());
  }
}


async function testCspViolationCausesTestCaseToFail_lifeCycleAsync() {
  goog.testing.TestCase.getActiveTestCase().observeCspViolations(false);

  const tests = [
    ['setUp', doCspViolation8],
    ['tearDown', doCspViolation9],
    ['setUpPage', doCspViolation10],

  ];
  for (const [name, doCspViolation] of tests) {
    const testCase = new goog.testing.TestCase();
    testCase.ignoreStartupCspViolations();
    let didRun = false;
    testCase.setLifecycleObj({
      [name]: function() {
        return new Promise((resolve, reject) => {
          didRun = true;
          doCspViolation();
          resolve();
        });
      },
    });
    testCase.addNewTest('test1', function() {
      // do nothing
    });
    testCase.addNewTest('test2', function() {
      // do nothing
    });
    await testCase.runTestsReturningPromise();
    assertTrue(`${name}() did not run`, didRun);
    assertFalse(`${name}() should have failed`, testCase.isSuccess());
  }
}

// The ReportingObserver API in Chrome only issues a report when a violation
// is trigggered for the first time. The repetition below is used to get around
// this behavior.

/** Generate a CSP violation **/
function doCspViolation1() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}

/** Generate a CSP violation **/
function doCspViolation2() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}

/** Generate a CSP violation **/
function doCspViolation3() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}

/** Generate a CSP violation **/
function doCspViolation4() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}

/** Generate a CSP violation **/
function doCspViolation5() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}

/** Generate a CSP violation **/
function doCspViolation6() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}

/** Generate a CSP violation **/
function doCspViolation7() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}

/** Generate a CSP violation **/
function doCspViolation8() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}

/** Generate a CSP violation **/
function doCspViolation9() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}

/** Generate a CSP violation **/
function doCspViolation10() {
  const elem = document.createElement('A');
  elem.setAttribute('onclick', 'window["VIOLATION"] = true');
  elem.click();
}
