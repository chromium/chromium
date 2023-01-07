/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.FetchXmlHttpFactoryTest');
goog.setTestOnly();

const FetchXmlHttp = goog.require('goog.net.FetchXmlHttp');
const FetchXmlHttpFactory = goog.require('goog.net.FetchXmlHttpFactory');
const MockControl = goog.require('goog.testing.MockControl');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const isVersion = goog.require('goog.userAgent.product.isVersion');
const product = goog.require('goog.userAgent.product');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

/** @type {!MockControl} */
let mockControl;

/** @type {?} */
let fetchMock;

/** @type {!FetchXmlHttpFactory} */
let factory;

/** @type {!WorkerGlobalScope} */
let worker;

/** @type {!PropertyReplacer} */
let stubs;

/**
 * Util function to verify send method.
 * @param {string} sendMethod
 * @param {number=} expectedStatusCode
 * @param {boolean=} isStream
 * @param {boolean=} isArrayBuffer
 * @param {boolean=} isStreamBinaryChunks
 * @return {!Promise<void>}
 */
function verifySend(
    sendMethod, expectedStatusCode = 200, isStream = false,
    isArrayBuffer = false, isStreamBinaryChunks = false) {
  return new Promise((resolve, reject) => {
    const xhr = factory.createInstance();
    const expectedBody = 'responseBody';
    if (isArrayBuffer) {
      xhr.responseType = 'arraybuffer';
    }
    xhr.open(sendMethod, 'https://www.google.com', true /* opt_async */);
    let lastState;
    let lastBufferSize = 0;
    let numberOfUpdates = 0;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    xhr.onreadystatechange = () => {
      if (xhr.readyState === FetchXmlHttp.RequestState.HEADER_RECEIVED) {
        lastState = xhr.readyState;
        let expectedHeaders =
            'dummyheader: dummyHeaderValue\r\ndummyheader2: dummyHeaderValue2';
        if (!isStream && !isArrayBuffer) {
          expectedHeaders =
              `content-type: text/plain;charset=UTF-8\r\n${expectedHeaders}`;
        }
        assertEquals(expectedStatusCode, xhr.status);
        assertEquals('', xhr.responseText);
        assertEquals('dummyHeaderValue', xhr.getResponseHeader('dummyHeader'));
        assertEquals(expectedHeaders, xhr.getAllResponseHeaders());
      } else if (xhr.readyState === FetchXmlHttp.RequestState.LOADING) {
        lastState = xhr.readyState;
        assertEquals(expectedStatusCode, xhr.status);
        assertEquals(0, expectedBody.indexOf(xhr.responseText));
        if (isStream && xhr.responseText) {
          assertTrue(xhr.responseText.length > lastBufferSize);
          lastBufferSize = xhr.responseText.length;
          numberOfUpdates++;
        }
      } else if (xhr.readyState === FetchXmlHttp.RequestState.DONE) {
        assertEquals(FetchXmlHttp.RequestState.LOADING, lastState);
        assertEquals(expectedStatusCode, xhr.status);
        if (isArrayBuffer) {
          assertTrue(xhr.response instanceof ArrayBuffer);
          assertEquals(8, xhr.response.byteLength);
        } else if (!isStreamBinaryChunks) {
          assertEquals(expectedBody, xhr.responseText);
        }
        if (isStreamBinaryChunks) {
          const bytes = new TextEncoder().encode('responseBody');
          assertEquals(bytes.length, xhr.response.length);
          assertTrue((xhr.response)[0] instanceof Uint8Array);
          for (let i = 0; i < bytes.length; i++) {
            assertTrue(bytes[i] === xhr.response[i][0]);
          }
        } else if (isStream) {
          assertEquals(expectedBody.length, numberOfUpdates);
        }
        resolve();
      } else {
        reject(new Error('Unexpected request state ' + xhr.readyState));
      }
    };
    xhr.send();
    assertEquals(xhr.readyState, FetchXmlHttp.RequestState.OPENED);

    mockControl.$verifyAll();
  });
}

/**
 * Creates a successful response.
 * @return {!Response}
 */
function createSuccessResponse() {
  const headers = new Headers();
  headers.set('dummyHeader', 'dummyHeaderValue');
  headers.set('dummyHeader2', 'dummyHeaderValue2');
  return new Response(
      'responseBody' /* opt_body */, {status: 200, headers: headers});
}

/**
 * Creates a successful streaming response which returns each letter a separate
 * chunk with a 10ms delay between them.
 * @return {!Response}
 */
function createSuccessStreamingResponse() {
  const headers = new Headers();
  headers.set('dummyHeader', 'dummyHeaderValue');
  headers.set('dummyHeader2', 'dummyHeaderValue2');
  const bytes = new TextEncoder().encode('responseBody');
  const body = new ReadableStream({
    pull(controller) {
      for (let i = 0; i < bytes.length; i++) {
        controller.enqueue(bytes.slice(i, i + 1));
      }
      controller.close();
    },
  });
  return new Response(body, {status: 200, statusText: 'OK', headers: headers});
}

/**
 * Creates a successful response with an ArrayBuffer payload.
 * @return {!Response}
 */
function createArrayBufferResponse() {
  const headers = new Headers();
  headers.set('dummyHeader', 'dummyHeaderValue');
  headers.set('dummyHeader2', 'dummyHeaderValue2');
  return new Response(
      new ArrayBuffer(8), {status: 200, statusText: 'OK', headers: headers});
}

/**
 * Creates a successful response.
 * @return {!Response}
 */
function createFailedResponse() {
  const headers = new Headers();
  headers.set('dummyHeader', 'dummyHeaderValue');
  headers.set('dummyHeader2', 'dummyHeaderValue2');
  return new Response(
      'responseBody' /* opt_body */, {status: 500, headers: headers});
}
testSuite({
  /**
   * Whether the browser supports running this test.
   * @return {boolean}
   */
  shouldRunTests() {
    return product.CHROME && isVersion(43);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  setUp() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    mockControl = new MockControl();
    /** @suppress {checkTypes} suppression added to enable type checking */
    worker = {};
    fetchMock = mockControl.createFunctionMock('fetch');
    worker.fetch = fetchMock;
    stubs = new PropertyReplacer();
    stubs.replace(globalThis, 'fetch', fetchMock);
    factory = new FetchXmlHttpFactory({worker: worker});
  },

  tearDown() {
    mockControl.$tearDown();
    stubs.reset();
  },

  /**
     Verifies the open method. @suppress {checkTypes} suppression added to
     enable type checking
   */
  testOpen() {
    mockControl.$replayAll();

    const xhr = factory.createInstance();
    assertEquals(0, xhr.status);
    assertEquals('', xhr.responseText);
    assertEquals(xhr.readyState, FetchXmlHttp.RequestState.UNSENT);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const onReadyStateChangeHandler = new recordFunction();
    xhr.onreadystatechange = onReadyStateChangeHandler;
    xhr.open('GET', 'https://www.google.com', true /* opt_async */);
    assertEquals(xhr.readyState, FetchXmlHttp.RequestState.OPENED);
    onReadyStateChangeHandler.assertCallCount(1);

    mockControl.$verifyAll();
  },

  /** Verifies the open method when the ready state is not unsent. */
  testOpen_notUnsent() {
    mockControl.$replayAll();

    const xhr = factory.createInstance();
    xhr.open('GET', 'https://www.google.com', true /* opt_async */);
    assertThrows(() => {
      xhr.open('GET', 'https://www.google.com', true /* opt_async */);
    });

    mockControl.$verifyAll();
  },

  /** Verifies that synchronous fetches are not supported. */
  testOpen_notAsync() {
    mockControl.$replayAll();

    const xhr = factory.createInstance();

    assertThrows(() => {
      xhr.open('GET', 'https://www.google.com', false /* opt_async */);
    });

    mockControl.$verifyAll();
  },

  /**
   * Verifies the send method.
   * @return {!Promise<void>}
   */
  testSend() {
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'GET',
    })).$returns(Promise.resolve(createSuccessResponse()));

    mockControl.$replayAll();
    return verifySend('GET');
  },

  /**
   * Verifies the send method without Service Worker.
   * @return {!Promise<void>}
   */
  testSend_nonServiceWorker() {
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'GET',
    })).$returns(Promise.resolve(createSuccessResponse()));

    mockControl.$replayAll();
    factory = new FetchXmlHttpFactory({});
    return verifySend('GET');
  },

  /**
   * Verifies the send method with POST mode.
   * @return {!Promise<void>}
   */
  testSendPost() {
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'POST',
    })).$returns(Promise.resolve(createSuccessResponse()));

    mockControl.$replayAll();
    return verifySend('POST');
  },

  /**
   * Verifies the send method including credentials.
   * @return {!Promise<void>}
   */
  testSend_includeCredentials() {
    factory.setCredentialsMode(/** @type {RequestCredentials} */ ('include'));
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'POST',
      credentials: 'include',
    })).$returns(Promise.resolve(createSuccessResponse()));

    mockControl.$replayAll();
    return verifySend('POST');
  },

  /**
   * Verifies the send method setting cache mode.
   * @return {!Promise<void>}
   */
  testSend_setCacheMode() {
    factory.setCacheMode(/** @type {RequestCache} */ ('no-cache'));
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'POST',
      cache: 'no-cache',
    })).$returns(Promise.resolve(createSuccessResponse()));

    mockControl.$replayAll();
    return verifySend('POST');
  },

  /**
   * Verifies the send method in case of error response.
   * @return {!Promise<void>}
   */
  testSend_error() {
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'GET',
    })).$returns(Promise.resolve(createFailedResponse()));

    mockControl.$replayAll();

    return verifySend('GET', 500 /* expectedStatusCode */);
  },

  /**
   * Tests that streaming responses are properly handled.
   * @return {!Promise<void>}
   */
  testSend_streaming() {
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'POST',
    })).$returns(Promise.resolve(createSuccessStreamingResponse()));

    mockControl.$replayAll();
    return verifySend(
        'POST', 200 /* expectedStatusCode */, true /* isStream */);
  },

  /**
   * Tests that streaming binary responses are properly handled.
   * @return {!Promise<void>}
   */
  testSend_streamBinaryChunks() {
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'POST',
    })).$returns(Promise.resolve(createSuccessStreamingResponse()));

    mockControl.$replayAll();

    factory =
        new FetchXmlHttpFactory({worker: worker, streamBinaryChunks: true});
    return verifySend(
        'POST', 200 /* expectedStatusCode */, true /* isStream */,
        false /* isArrayBuffer */, true /* isStreamBinaryCrunks */);
  },

  /**
   * Verifies the send method in case of getting an ArrayBuffer response.
   * @return {!Promise<void>}
   */
  testSend_arrayBuffer() {
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'POST',
    })).$returns(Promise.resolve(createArrayBufferResponse()));
    mockControl.$replayAll();
    return verifySend(
        'POST', 200 /* expectedStatusCode */, false /* isStream */,
        true /* isArrayBuffer */);
  },

  /**
   * Verifies the send method in case of failure to fetch the url.
   * @return {!Promise<void>}
   */
  testSend_failToFetch() {
    const failedPromise = new Promise(() => {
      throw new Error('failed to fetch');
    });
    fetchMock(new Request('https://www.google.com', {
      headers: new Headers(),
      method: 'GET',
    })).$returns(failedPromise);

    mockControl.$replayAll();
    return new Promise((resolve) => {
      const xhr = factory.createInstance();
      xhr.open('GET', 'https://www.google.com', true /* opt_async */);
      xhr.onreadystatechange = () => {
        assertEquals(xhr.readyState, FetchXmlHttp.RequestState.DONE);
        assertEquals(0, xhr.status);
        assertEquals('', xhr.responseText);
        resolve();
      };
      xhr.send();
      assertEquals(xhr.readyState, FetchXmlHttp.RequestState.OPENED);

      mockControl.$verifyAll();
    });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testWithCredentials_set() {
    const xhr = factory.createInstance();

    assertEquals(xhr.getCredentialsMode(), undefined);

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    xhr.withCredentials = true;
    assertEquals(xhr.getCredentialsMode(), 'include');

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    xhr.withCredentials = false;
    assertEquals(xhr.getCredentialsMode(), 'same-origin');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testWithCredentials_get() {
    const xhr = factory.createInstance();

    assertEquals(xhr.withCredentials, false);

    xhr.setCredentialsMode('include');
    assertEquals(xhr.withCredentials, true);

    xhr.setCredentialsMode('same-origin');
    assertEquals(xhr.withCredentials, false);

    xhr.setCredentialsMode('omit');
    assertEquals(xhr.withCredentials, false);
  }
});
