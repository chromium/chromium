/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.debug.ErrorTest');
goog.setTestOnly();

const DebugError = goog.require('goog.debug.Error');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let expectedFailures;

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
  },

  tearDown() {
    expectedFailures.handleTearDown();
  },

  testError() {
    function xxxxx() {
      yyyyy();
    }
    function yyyyy() {
      zzzzz();
    }
    function zzzzz() {
      throw new DebugError('testing');
    }

    let message = null;
    let stack = null;

    try {
      xxxxx();
    } catch (e) {
      message = e.message;
      if (e.stack) {
        stack = e.stack.split('\n');
      }
    }

    assertEquals('Message property should be set', 'testing', message);

    expectedFailures.expectFailureFor(
        (userAgent.IE && !userAgent.isVersionOrHigher('10')) ||
            product.SAFARI ||
            (product.CHROME && !userAgent.isVersionOrHigher(532)),
        'error.stack is not widely supported');

    try {
      assertNotNull(stack);

      if (product.FIREFOX) {
        // Firefox 4 and greater does not have the first line that says
        // 'Error'. So we insert a dummy line to simplify the test.
        stack.splice(0, 0, 'Error');
      }

      // If the stack trace came from a synthetic Error object created
      // inside the goog.debug.Error constructor, it will have an extra frame
      // at stack[1]. If it came from captureStackTrace or was attached
      // by IE when the error was caught, it will not.
      if (!Error.captureStackTrace && !userAgent.IE) {
        stack.splice(1, 1);  // Remove stack[1].
      }

      assertContains(
          '1st line of stack should have "Error"', 'Error', stack[0]);
      assertContains(
          '2nd line of stack should have "zzzzz"', 'zzzzz', stack[1]);
      assertContains(
          '3rd line of stack should have "yyyyy"', 'yyyyy', stack[2]);
      assertContains(
          '4th line of stack should have "xxxxx"', 'xxxxx', stack[3]);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testInheriting() {
    function MyError() {
      DebugError.call(this);
    }
    goog.inherits(MyError, DebugError);
    MyError.prototype.message = 'My custom error';

    let message = null;
    try {
      throw new MyError();
    } catch (e) {
      message = e.message;
    }
    assertEquals('My custom error', message);
  },

  testCause() {
    const originalError = new DebugError('original error');
    const error = new DebugError('error', originalError);

    assertEquals(originalError, error.cause);
    assertEquals(undefined, originalError.cause);
  },
});
