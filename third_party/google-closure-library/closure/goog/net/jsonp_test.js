/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.JsonpTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const Jsonp = goog.require('goog.net.Jsonp');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const recordFunction = goog.require('goog.testing.recordFunction');
const safe = goog.require('goog.dom.safe');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

// Global vars to facilitate a shared set up function.

let timeoutWasCalled;
let timeoutHandler;

const fakeUrl = 'https://fake-site.eek/';
const fakeTrustedUrl = TrustedResourceUrl.fromConstant(Const.from(fakeUrl));

let originalTimeout;

// Firefox throws a JS error when a script is not found.  We catch that here and
// ensure the test case doesn't fail because of it.
const originalOnError = window.onerror;
window.onerror = (msg, url, line) => {
  // TODO(user): Safari 3 on the farm returns an object instead of the typical
  // params.  Pass through errors for safari for now.
  if (userAgent.WEBKIT ||
      msg == 'Error loading script' && url.indexOf('fake-site') != -1) {
    return true;
  } else {
    return originalOnError && originalOnError(msg, url, line);
  }
};

// Quick function records the before-state of the DOM, and then return a
// a function to check that XDC isn't leaving stuff behind.
function newCleanupGuard() {
  const bodyChildCount = document.body.childNodes.length;

  return () => {
    // let any timeout queues finish before we check these:
    window.setTimeout(/**
                         @suppress {checkTypes} suppression added to enable type
                         checking
                       */
                      () => {
                        let propCounter = 0;

                        // All callbacks should have been deleted or be the null
                        // function.
                        for (const key in globalThis) {
                          // NOTES: callbacks are stored on globalThis with
                          // property name prefixed with
                          // goog.net.Jsonp.CALLBACKS.
                          if (key.indexOf(Jsonp.CALLBACKS) == 0) {
                            /**
                             * @suppress {visibility} suppression added to
                             * enable type checking
                             */
                            const callbackId = Jsonp.getCallbackId_(key);
                            if (globalThis[callbackId] &&
                                globalThis[callbackId] != goog.nullFunction) {
                              propCounter++;
                            }
                          }
                        }

                        assertEquals(
                            'script cleanup', bodyChildCount,
                            document.body.childNodes.length);
                        assertEquals(
                            'window jsonp array empty', 0, propCounter);
                      },
                      0);
  };
}

/** @suppress {missingProperties} suppression added to enable type checking */
function getScriptElement(result) {
  return result.deferred_.defaultScope_.script_;
}

testSuite({
  setUp() {
    timeoutWasCalled = false;
    timeoutHandler = null;
    originalTimeout = window.setTimeout;
    /** @suppress {missingReturn} suppression added to enable type checking */
    window.setTimeout = (handler, time) => {
      timeoutWasCalled = true;
      timeoutHandler = handler;
    };
  },

  tearDown() {
    window.setTimeout = originalTimeout;
  },

  // Check that send function is sane when things go well.
  /** @suppress {checkTypes} suppression added to enable type checking */
  testSend() {
    let replyReceived;
    const jsonp = new Jsonp(fakeTrustedUrl);

    const checkCleanup = newCleanupGuard();

    const userCallback = (data) => {
      replyReceived = data;
    };

    const payload = {atisket: 'atasket', basket: 'yellow'};
    const result = jsonp.send(payload, userCallback);

    const script = getScriptElement(result);

    assertNotNull('script created', script);
    assertEquals('encoding is utf-8', 'UTF-8', script.charset);

    // Check that the URL matches our payload.
    assertTrue('payload in url', script.src.indexOf('basket=yellow') > -1);
    assertTrue('server url', script.src.indexOf(fakeUrl) == 0);

    // Now, we have to track down the name of the callback function, so we can
    // call that to simulate a returned request + verify that the callback
    // function does not break if it receives a second unexpected parameter.
    const callbackName = /callback=([^&]+)/.exec(script.src)[1];
    const callbackFunc = eval(callbackName);
    callbackFunc(
        {some: 'data', another: ['data', 'right', 'here']}, 'unexpected');
    assertEquals('input was received', 'right', replyReceived.another[1]);

    // Because the callbackFunc calls cleanUp_ and that calls setTimeout which
    // we have overwritten, we have to call the timeoutHandler to actually do
    // the cleaning.
    timeoutHandler();

    checkCleanup();
    timeoutHandler();
  },

  // Check that send function is sane when things go well.
  /** @suppress {checkTypes} suppression added to enable type checking */
  testSendWhenCallbackHasTwoParameters() {
    let replyReceived;
    let replyReceived2;
    const jsonp = new Jsonp(fakeTrustedUrl);

    const checkCleanup = newCleanupGuard();

    const userCallback = (data, opt_data2) => {
      replyReceived = data;
      replyReceived2 = opt_data2;
    };

    const payload = {atisket: 'atasket', basket: 'yellow'};
    const result = jsonp.send(payload, userCallback);
    const script = getScriptElement(result);

    // Test a callback function that receives two parameters.
    const callbackName = /callback=([^&]+)/.exec(script.src)[1];
    const callbackFunc = eval(callbackName);
    callbackFunc('param1', {some: 'data', another: ['data', 'right', 'here']});
    assertEquals('input was received', 'param1', replyReceived);
    assertEquals(
        'second input was received', 'right', replyReceived2.another[1]);

    // Because the callbackFunc calls cleanUp_ and that calls setTimeout which
    // we have overwritten, we have to call the timeoutHandler to actually do
    // the cleaning.
    timeoutHandler();

    checkCleanup();
    timeoutHandler();
  },

  // Check that send function works correctly when callback param value is
  // specified.
  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testSendWithCallbackParamValue() {
    let replyReceived;
    const jsonp = new Jsonp(fakeTrustedUrl);

    const checkCleanup = newCleanupGuard();

    const userCallback = (data) => {
      replyReceived = data;
    };

    const payload = {atisket: 'atasket', basket: 'yellow'};
    const result = jsonp.send(payload, userCallback, undefined, 'dummyId');

    const script = getScriptElement(result);

    assertNotNull('script created', script);
    assertEquals('encoding is utf-8', 'UTF-8', script.charset);

    // Check that the URL matches our payload.
    assertTrue('payload in url', script.src.indexOf('basket=yellow') > -1);
    assertTrue(
        'dummyId in url',
        script.src.indexOf('callback=' + Jsonp.getCallbackId_('dummyId')) > -1);
    assertTrue('server url', script.src.indexOf(fakeUrl) == 0);

    // Now, we simulate a returned request using the known callback function
    // name.
    /** @suppress {visibility} suppression added to enable type checking */
    const callbackFunc =
        eval('window.callback=' + Jsonp.getCallbackId_('dummyId'));
    callbackFunc({some: 'data', another: ['data', 'right', 'here']});
    assertEquals('input was received', 'right', replyReceived.another[1]);

    // Because the callbackFunc calls cleanUp_ and that calls setTimeout which
    // we have overwritten, we have to call the timeoutHandler to actually do
    // the cleaning.
    timeoutHandler();

    checkCleanup();
    timeoutHandler();
  },

  // Check that the send function is sane when the thing goes south.
  /** @suppress {checkTypes} suppression added to enable type checking */
  testSendFailure() {
    let replyReceived = false;
    let errorReplyReceived = false;

    const jsonp = new Jsonp(fakeTrustedUrl);

    const checkCleanup = newCleanupGuard();

    const userCallback = (data) => {
      replyReceived = data;
    };
    const userErrorCallback = (data) => {
      errorReplyReceived = data;
    };

    const payload = {justa: 'test'};

    jsonp.send(payload, userCallback, userErrorCallback);

    assertTrue('timeout called', timeoutWasCalled);

    // Now, simulate the time running out, so we go into error mode.
    // After jsonp.send(), the timeoutHandler now is the Jsonp.cleanUp_
    // function.
    timeoutHandler();
    // But that function also calls a setTimeout(), so it changes the timeout
    // handler once again, so to actually clean up we have to call the
    // timeoutHandler() once again. Fun!
    timeoutHandler();

    assertFalse('standard callback not called', replyReceived);

    // The user's error handler should be called back with the same payload
    // passed back to it.
    assertEquals('error handler called', 'test', errorReplyReceived.justa);

    // Check that the relevant cleanup has occurred.
    checkCleanup();
    // Check cleanup just calls setTimeout so we have to call the handler to
    // actually check that the cleanup worked.
    timeoutHandler();
  },

  // Check that a cancel call works and cleans up after itself.
  /** @suppress {checkTypes} suppression added to enable type checking */
  testCancel() {
    const checkCleanup = newCleanupGuard();

    let successCalled = false;
    const successCallback = () => {
      successCalled = true;
    };

    // Send and cancel a request, then make sure it was cleaned up.
    const jsonp = new Jsonp(fakeTrustedUrl);
    const requestObject = jsonp.send({test: 'foo'}, successCallback);
    jsonp.cancel(requestObject);

    for (const key in globalThis[Jsonp.CALLBACKS]) {
      // NOTES: callbacks are stored on globalThis with property
      // name prefixed with goog.net.Jsonp.CALLBACKS.
      if (key.indexOf('goog.net.Jsonp.CALLBACKS') == 0) {
        /** @suppress {visibility} suppression added to enable type checking */
        const callbackId = Jsonp.getCallbackId_(key);
        assertNotEquals(
            'The success callback should have been removed',
            globalThis[callbackId], successCallback);
      }
    }

    // Make sure cancelling removes the script tag
    checkCleanup();
    timeoutHandler();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPayloadParameters() {
    const checkCleanup = newCleanupGuard();

    const jsonp = new Jsonp(fakeTrustedUrl);
    const result = jsonp.send({'foo': 3, 'bar': 'baz'});

    const script = getScriptElement(result);
    assertEquals(
        'Payload parameters should have been added to url.',
        `${fakeUrl}?foo=3&bar=baz`, script.src);

    checkCleanup();
    timeoutHandler();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testNonce() {
    const checkCleanup = newCleanupGuard();

    const jsonp = new Jsonp(fakeTrustedUrl);
    let nonce = safe.getScriptNonce();
    if (!nonce) {
      nonce = 'foo';
    }
    jsonp.setNonce(nonce);
    const result = jsonp.send();

    const script = getScriptElement(result);
    assertEquals(
        'Nonce attribute should have been added to script element.', nonce,
        (script['nonce'] || script.getAttribute('nonce')));

    checkCleanup();
    timeoutHandler();
  },

  testOptionalPayload() {
    const checkCleanup = newCleanupGuard();

    const errorCallback = recordFunction();

    const stubs = new PropertyReplacer();
    stubs.set(globalThis, 'setTimeout', (errorHandler) => {
      errorHandler();
    });

    const jsonp = new Jsonp(fakeTrustedUrl);
    const result = jsonp.send(null, null, errorCallback);

    const script = getScriptElement(result);
    assertEquals(
        'Parameters should not have been added to url.', fakeUrl, script.src);

    // Clear the script hooks because we triggered the error manually.
    script.onload = goog.nullFunction;
    script.onerror = goog.nullFunction;
    script.onreadystatechange = goog.nullFunction;

    const errorCallbackArguments = errorCallback.getLastCall().getArguments();
    assertEquals(1, errorCallbackArguments.length);
    assertObjectEquals({}, errorCallbackArguments[0]);

    checkCleanup();
    stubs.reset();
  },
});
