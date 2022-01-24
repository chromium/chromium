/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.debug.ErrorReporterTest');
goog.setTestOnly();

const DebugError = goog.require('goog.debug.Error');
const ErrorReporter = goog.require('goog.debug.ErrorReporter');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const dispose = goog.require('goog.dispose');
const errorcontext = goog.require('goog.debug.errorcontext');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

class MockXhrIo {
  onReadyStateChangeEntryPoint_() {}

  static protectEntryPoints() {}

  static send(
      url, callback = undefined, method = undefined, content = undefined,
      headers = undefined, timeInterval = undefined) {
    MockXhrIo.lastUrl = url;
    MockXhrIo.lastContent = content;
    MockXhrIo.lastHeaders = headers;
  }
}

MockXhrIo.lastUrl = null;

let errorReporter;
const originalSetTimeout = window.setTimeout;
const stubs = new PropertyReplacer();
const url = 'http://www.your.tst/more/bogus.js';
const encodedUrl = 'http%3A%2F%2Fwww.your.tst%2Fmore%2Fbogus.js';

/**
 * @param {*} filename
 * @param {*} line
 * @param {*} message
 * @param {*=} stack
 * @param {*=} cause
 * @return {*}
 */
function createError(
    filename, line, message, stack = undefined, cause = undefined) {
  const error = {
    message: message,
    fileName: filename,
    lineNumber: line,
    toString: function() {
      return 'Error: ' + message;
    }
  };
  if (stack) {
    error['stack'] = stack;
  }
  if (cause) {
    error['cause'] = cause;
  }

  return error;
}

/**
 * @param {*} script
 * @param {*} line
 * @param {*} message
 * @param {*=} stack
 * @param {*=} cause
 */
function throwAnErrorWith(
    script, line, message, stack = undefined, cause = undefined) {
  throw createError(script, line, message, stack, cause);
}

testSuite({
  setUp() {
    stubs.set(goog.net, 'XhrIo', MockXhrIo);
    // NOTE: bypass compiler check for the define
    ErrorReporter['ALLOW_AUTO_PROTECT'] = true;
  },

  tearDown() {
    dispose(errorReporter);
    stubs.reset();
    MockXhrIo.lastUrl = null;
  },

  testsendErrorReport() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.sendErrorReport('message', 'filename.js', 123, 'trace');

    assertEquals(
        '/log?script=filename.js&error=message&line=123', MockXhrIo.lastUrl);
    assertEquals('trace=trace', MockXhrIo.lastContent);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testsendErrorReportWithCustomSender() {
    let uri = null;
    let method = null;
    let content = null;
    let headers = null;
    function mockXhrSender(uriIn, methodIn, contentIn, headersIn) {
      uri = uriIn;
      method = methodIn;
      content = contentIn;
      headers = headersIn;
    }

    errorReporter = new ErrorReporter('/log');
    errorReporter.setXhrSender(mockXhrSender);
    errorReporter.sendErrorReport('message', 'filename.js', 123, 'trace');

    assertEquals('/log?script=filename.js&error=message&line=123', uri);
    assertEquals('POST', method);
    assertEquals('trace=trace', content);
    assertUndefined(headers);
  },

  testsendErrorReport_noTrace() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.sendErrorReport('message', 'filename.js', 123);

    assertEquals(
        '/log?script=filename.js&error=message&line=123', MockXhrIo.lastUrl);
    assertEquals('', MockXhrIo.lastContent);
  },

  test_nonInternetExplorerSendErrorReport() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    stubs.set(userAgent, 'IE', false);
    stubs.set(globalThis, 'setTimeout', (fcn, time) => {
      fcn.call();
    });

    errorReporter = ErrorReporter.install('/errorreporter');

    const errorFunction = goog.partial(throwAnErrorWith, url, 5, 'Hello :)');

    try {
      globalThis.setTimeout(errorFunction, 0);
    } catch (e) {
      // Expected. The error is rethrown after sending.
    }

    assertEquals(
        `/errorreporter?script=${encodedUrl}&error=Hello%20%3A)&line=5`,
        MockXhrIo.lastUrl);
    assertEquals('trace=Not%20available', MockXhrIo.lastContent);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  test_internetExplorerSendErrorReport() {
    stubs.set(userAgent, 'IE', true);
    stubs.set(userAgent, 'isVersionOrHigher', functions.FALSE);

    // Remove test runner's onerror handler so the test doesn't fail.
    stubs.set(globalThis, 'onerror', null);

    errorReporter = ErrorReporter.install('/errorreporter');
    globalThis.onerror('Goodbye :(', url, 22);
    assertEquals(
        `/errorreporter?script=${encodedUrl}` +
            '&error=Goodbye%20%3A(&line=22',
        MockXhrIo.lastUrl);
    assertEquals('trace=Not%20available', MockXhrIo.lastContent);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  test_setLoggingHeaders() {
    stubs.set(userAgent, 'IE', true);
    stubs.set(userAgent, 'isVersionOrHigher', functions.FALSE);
    // Remove test runner's onerror handler so the test doesn't fail.
    stubs.set(globalThis, 'onerror', null);

    errorReporter = ErrorReporter.install('/errorreporter');
    errorReporter.setLoggingHeaders('header!');
    globalThis.onerror('Goodbye :(', 'http://www.your.tst/more/bogus.js', 22);
    assertEquals('header!', MockXhrIo.lastHeaders);
  },

  test_nonInternetExplorerSendErrorReportWithTrace() {
    stubs.set(userAgent, 'IE', false);
    stubs.set(globalThis, 'setTimeout', (fcn, time) => {
      fcn.call();
    });

    errorReporter = ErrorReporter.install('/errorreporter');

    const trace = 'Error(\"Something Wrong\")@:0\n' +
        '$MF$E$Nx$([object Object])@http://a.b.c:83/a/f.js:901\n' +
        '([object Object])@http://a.b.c:813/a/f.js:37';

    const errorFunction =
        goog.partial(throwAnErrorWith, url, 5, 'Hello :)', trace);

    try {
      globalThis.setTimeout(errorFunction, 0);
    } catch (e) {
      // Expected. The error is rethrown after sending.
    }

    assertEquals(
        `/errorreporter?script=${encodedUrl}&error=Hello%20%3A)&line=5`,
        MockXhrIo.lastUrl);
    assertEquals(
        'trace=' +
            'Error(%22Something%20Wrong%22)%40%3A0%0A' +
            '%24MF%24E%24Nx%24(%5Bobject%20Object%5D)%40' +
            'http%3A%2F%2Fa.b.c%3A83%2Fa%2Ff.js%3A901%0A' +
            '(%5Bobject%20Object%5D)%40http%3A%2F%2Fa.b.c%3A813%2Fa%2Ff.js%3A37',
        MockXhrIo.lastContent);
  },

  test_nonInternetExplorerSendErrorReportWithTraceAndCauses() {
    stubs.set(userAgent, 'IE', false);
    stubs.set(globalThis, 'setTimeout', (fcn, time) => {
      fcn.call();
    });

    const maintrace = 'Error(\"Something Wrong\")@:0\n' +
        '$MF$E$Nx$([object Object])@http://a.b.c:83/a/f.js:901\n' +
        '([object Object])@http://a.b.c:813/a/f.js:37';

    // For this cause, the error message is part of the stacktrace.
    const causetrace1 =
        'Error: Cause1 Error\n([object Object])@http://a.b.c:813/b/d.js:35';
    const causetrace2 =
        '$AB$B$Wx$([object Object])@http://a.b.c:83/c/e.js:101\n' +
        '([object Object])@http://a.b.c:813/c/d.js:3';

    const cause2 =
        createError(url, 1, 'Cause2 Error', causetrace2, 'String cause');
    const cause1 = createError(url, 12, 'Cause1 Error', causetrace1, cause2);

    const expectedTrace = 'Error("Something Wrong")@:0\n' +
        '$MF$E$Nx$([object Object])@http://a.b.c:83/a/f.js:901\n' +
        '([object Object])@http://a.b.c:813/a/f.js:37\n' +
        'Caused by: Error: Cause1 Error\n' +
        '([object Object])@http://a.b.c:813/b/d.js:35\n' +
        'Caused by: Cause2 Error\n' +
        '$AB$B$Wx$([object Object])@http://a.b.c:83/c/e.js:101\n' +
        '([object Object])@http://a.b.c:813/c/d.js:3\n' +
        'Caused by: String cause';

    errorReporter = ErrorReporter.install('/errorreporter');
    events.listen(errorReporter, ErrorReporter.ExceptionEvent.TYPE, (event) => {
      assertEquals(expectedTrace, event.error.stack);
    });

    const errorFunction =
        goog.partial(throwAnErrorWith, url, 5, 'MainError', maintrace, cause1);

    try {
      globalThis.setTimeout(errorFunction, 0);
    } catch (e) {
      // Expected. The error is rethrown after sending.
    }

    assertEquals(
        `/errorreporter?script=${encodedUrl}&error=MainError&line=5`,
        MockXhrIo.lastUrl);
    assertEquals(
        'trace=' + encodeURIComponent(expectedTrace), MockXhrIo.lastContent);
  },

  test_nonInternetExplorerSendErrorReportWithCyclicCauses() {
    stubs.set(userAgent, 'IE', false);
    stubs.set(globalThis, 'setTimeout', (fcn, time) => {
      fcn.call();
    });

    const maintrace = 'Error(\"Something Wrong\")@:0\n' +
        '$MF$E$Nx$([object Object])@http://a.b.c:83/a/f.js:901\n' +
        '([object Object])@http://a.b.c:813/a/f.js:37';

    // For this cause, the error message is part of the stacktrace.
    const causetrace1 =
        'Error: Cause1 Error\n([object Object])@http://a.b.c:813/b/d.js:35';
    const causetrace2 =
        '$AB$B$Wx$([object Object])@http://a.b.c:83/c/e.js:101\n' +
        '([object Object])@http://a.b.c:813/c/d.js:3';

    const cause2 = createError(url, 1, 'Cause2 Error', causetrace2);
    const cause1 = createError(url, 12, 'Cause1 Error', causetrace1, cause2);
    // introduce a cycle
    cause2['cause'] = cause1;

    const expectedTrace = 'Error("Something Wrong")@:0\n' +
        '$MF$E$Nx$([object Object])@http://a.b.c:83/a/f.js:901\n' +
        '([object Object])@http://a.b.c:813/a/f.js:37\n' +
        'Caused by: Error: Cause1 Error\n' +
        '([object Object])@http://a.b.c:813/b/d.js:35\n' +
        'Caused by: Cause2 Error\n' +
        '$AB$B$Wx$([object Object])@http://a.b.c:83/c/e.js:101\n' +
        '([object Object])@http://a.b.c:813/c/d.js:3';

    errorReporter = ErrorReporter.install('/errorreporter');
    events.listen(errorReporter, ErrorReporter.ExceptionEvent.TYPE, (event) => {
      assertEquals(expectedTrace, event.error.stack);
    });

    const errorFunction =
        goog.partial(throwAnErrorWith, url, 5, 'MainError', maintrace, cause1);

    try {
      globalThis.setTimeout(errorFunction, 0);
    } catch (e) {
      // Expected. The error is rethrown after sending.
    }

    assertEquals(
        `/errorreporter?script=${encodedUrl}&error=MainError&line=5`,
        MockXhrIo.lastUrl);
    assertEquals(
        'trace=' + encodeURIComponent(expectedTrace), MockXhrIo.lastContent);
  },

  testProtectAdditionalEntryPoint_nonIE() {
    stubs.set(userAgent, 'IE', false);

    errorReporter = ErrorReporter.install('/errorreporter');
    const fn = () => {};
    const protectedFn = errorReporter.protectAdditionalEntryPoint(fn);
    assertNotNull(protectedFn);
    assertNotEquals(fn, protectedFn);
  },

  testProtectAdditionalEntryPoint_IE() {
    stubs.set(userAgent, 'IE', true);
    stubs.set(userAgent, 'isVersionOrHigher', functions.FALSE);

    errorReporter = ErrorReporter.install('/errorreporter');
    const fn = () => {};
    const protectedFn = errorReporter.protectAdditionalEntryPoint(fn);
    assertNull(protectedFn);
  },

  testHandleException_dispatchesEvent() {
    errorReporter = ErrorReporter.install('/errorreporter');
    let loggedErrors = 0;
    events.listen(errorReporter, ErrorReporter.ExceptionEvent.TYPE, (event) => {
      assertNotNullNorUndefined(event.error);
      loggedErrors++;
    });
    errorReporter.handleException(new Error());
    errorReporter.handleException(new Error());
    assertEquals(
        'Expected 2 errors. ' +
            '(Ensure an exception was not swallowed.)',
        2, loggedErrors);
  },

  testHandleException_includesContext() {
    errorReporter = ErrorReporter.install('/errorreporter');
    let loggedErrors = 0;
    const testError = new Error('test error');
    const testContext = {'contextParam': 'contextValue'};
    events.listen(errorReporter, ErrorReporter.ExceptionEvent.TYPE, (event) => {
      assertNotNullNorUndefined(event.error);
      assertObjectEquals({contextParam: 'contextValue'}, event.context);
      loggedErrors++;
    });
    errorReporter.handleException(testError, testContext);
    assertEquals(
        'Expected 1 error. ' +
            '(Ensure an exception was not swallowed.)',
        1, loggedErrors);
  },

  testContextProvider() {
    errorReporter =
        ErrorReporter.install('/errorreporter', (error, context) => {
          /**
           * @suppress {strictMissingProperties} suppression added to enable
           * type checking
           */
          context.providedContext = 'value';
        });
    let loggedErrors = 0;
    const testError = new Error('test error');
    events.listen(errorReporter, ErrorReporter.ExceptionEvent.TYPE, (event) => {
      assertNotNullNorUndefined(event.error);
      assertObjectEquals({providedContext: 'value'}, event.context);
      loggedErrors++;
    });
    errorReporter.handleException(testError);
    assertEquals(
        'Expected 1 error. ' +
            '(Ensure an exception was not swallowed.)',
        1, loggedErrors);
  },

  testContextProvider_withOtherContext() {
    errorReporter =
        ErrorReporter.install('/errorreporter', (error, context) => {
          /**
           * @suppress {strictMissingProperties} suppression added to enable
           * type checking
           */
          context.providedContext = 'value';
        });
    let loggedErrors = 0;
    const testError = new Error('test error');
    events.listen(errorReporter, ErrorReporter.ExceptionEvent.TYPE, (event) => {
      assertNotNullNorUndefined(event.error);
      assertObjectEquals(
          {providedContext: 'value', otherContext: 'value'}, event.context);
      loggedErrors++;
    });
    errorReporter.handleException(testError, {'otherContext': 'value'});
    assertEquals(
        'Expected 1 error. ' +
            '(Ensure an exception was not swallowed.)',
        1, loggedErrors);
  },

  testErrorWithContext() {
    errorReporter = ErrorReporter.install('/errorreporter');
    let loggedErrors = 0;
    const testError = new Error('test error');
    errorcontext.addErrorContext(testError, 'key1', 'value1');
    errorcontext.addErrorContext(testError, 'animalType', 'dog');
    events.listen(errorReporter, ErrorReporter.ExceptionEvent.TYPE, (event) => {
      assertNotNullNorUndefined(event.error);
      assertObjectEquals({key1: 'value1', animalType: 'dog'}, event.context);
      loggedErrors++;
    });
    errorReporter.handleException(testError);
    assertEquals(
        'Expected 1 error. ' +
            '(Ensure an exception was not swallowed.)',
        1, loggedErrors);
  },

  testErrorWithDifferentContextSources() {
    errorReporter =
        ErrorReporter.install('/errorreporter', (error, context) => {
          /**
           * @suppress {strictMissingProperties} suppression added to enable
           * type checking
           */
          context.providedContext = 'provided ctx';
        });
    let loggedErrors = 0;
    const testError = new Error('test error');
    errorcontext.addErrorContext(testError, 'addErrorContext', 'some value');
    events.listen(errorReporter, ErrorReporter.ExceptionEvent.TYPE, (event) => {
      assertNotNullNorUndefined(event.error);
      assertObjectEquals(
          {
            addErrorContext: 'some value',
            providedContext: 'provided ctx',
            handleExceptionContext: 'another value',
          },
          event.context);
      loggedErrors++;
    });
    errorReporter.handleException(
        testError, {handleExceptionContext: 'another value'});
    assertEquals(
        'Expected 1 error. ' +
            '(Ensure an exception was not swallowed.)',
        1, loggedErrors);
  },

  testHandleException_ignoresExceptionsDuringEventDispatch() {
    errorReporter = ErrorReporter.install('/errorreporter');
    events.listen(errorReporter, ErrorReporter.ExceptionEvent.TYPE, (event) => {
      throw new Error('This exception should be swallowed.');
    });
    errorReporter.handleException(new Error());
  },

  testHandleException_doNotReportErrorToServer() {
    const error = new DebugError();
    error.reportErrorToServer = false;
    errorReporter.handleException(error);
    assertNull(MockXhrIo.lastUrl);
  },

  testDisposal() {
    errorReporter = ErrorReporter.install('/errorreporter');
    if (!userAgent.IE) {
      assertNotEquals(originalSetTimeout, window.setTimeout);
    }
    dispose(errorReporter);
    assertEquals(originalSetTimeout, window.setTimeout);
  },

  testSetContextPrefix() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.setContextPrefix('baz.');
    errorReporter.sendErrorReport(
        'message', 'filename.js', 123, 'trace', {'foo': 'bar'});
    assertEquals('trace=trace&baz.foo=bar', MockXhrIo.lastContent);
  },

  testTruncationLimit() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.setTruncationLimit(6);
    errorReporter.sendErrorReport(
        'message', 'filename.js', 123, 'trace', {'foo': 'bar'});
    assertEquals('trace=', MockXhrIo.lastContent);
  },

  testZeroTruncationLimit() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.setTruncationLimit(0);
    errorReporter.sendErrorReport(
        'message', 'filename.js', 123, 'trace', {'foo': 'bar'});
    assertEquals('', MockXhrIo.lastContent);
  },

  testTruncationLimitLargerThanBody() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.setTruncationLimit(9999);
    errorReporter.sendErrorReport(
        'message', 'filename.js', 123, 'trace', {'foo': 'bar'});
    assertEquals('trace=trace&context.foo=bar', MockXhrIo.lastContent);
  },

  testSetNegativeTruncationLimit() {
    errorReporter = new ErrorReporter('/log');
    assertThrows(() => {
      errorReporter.setTruncationLimit(-10);
    });
  },

  testSetTruncationLimitNull() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.setTruncationLimit(null);
    errorReporter.sendErrorReport(
        'message', 'filename.js', 123, 'trace', {'foo': 'bar'});
    assertEquals('trace=trace&context.foo=bar', MockXhrIo.lastContent);
  },

  testAttemptAutoProtectWithAllowAutoProtectOff() {
    // Use computed property to bypass compiler check for the define value
    ErrorReporter['ALLOW_AUTO_PROTECT'] = false;
    assertThrows(() => {
      errorReporter = new ErrorReporter('/log', (e, context) => {}, false);
    });
  },

  testSetAdditionalArgumentsArgsEmptyObject() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.setAdditionalArguments({});
    errorReporter.sendErrorReport(
        'message', 'filename.js', 123, 'trace', {'foo': 'bar'});
    assertEquals(
        '/log?script=filename.js&error=message&line=123', MockXhrIo.lastUrl);
  },

  testSetAdditionalArgumentsSingleArgument() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.setAdditionalArguments({'extra': 'arg'});
    errorReporter.sendErrorReport(
        'message', 'filename.js', 123, 'trace', {'foo': 'bar'});
    assertEquals(
        '/log?script=filename.js&error=message&line=123&extra=arg',
        MockXhrIo.lastUrl);
  },

  testSetAdditionalArgumentsMultipleArguments() {
    errorReporter = new ErrorReporter('/log');
    errorReporter.setAdditionalArguments({'extra': 'arg', 'cat': 'dog'});
    errorReporter.sendErrorReport(
        'message', 'filename.js', 123, 'trace', {'foo': 'bar'});
    assertEquals(
        '/log?script=filename.js&error=message&line=123&extra=arg&cat=dog',
        MockXhrIo.lastUrl);
  },
});
