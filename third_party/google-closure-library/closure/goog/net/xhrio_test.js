/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.XhrIoTest');
goog.setTestOnly('goog.net.XhrIoTest');

const EntryPointMonitor = goog.require('goog.debug.EntryPointMonitor');
const ErrorHandler = goog.require('goog.debug.ErrorHandler');
const EventType = goog.require('goog.net.EventType');
const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const ReadyState = goog.require('goog.net.XmlHttp.ReadyState');
const TestingNetXhrIo = goog.require('goog.testing.net.XhrIo');
const Uri = goog.require('goog.Uri');
const WrapperXmlHttpFactory = goog.require('goog.net.WrapperXmlHttpFactory');
const XhrIo = goog.require('goog.net.XhrIo');
const XmlHttp = goog.require('goog.net.XmlHttp');
const entryPointRegistry = goog.require('goog.debug.entryPointRegistry');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const object = goog.require('goog.object');
const product = goog.require('goog.userAgent.product');
const recordFunction = goog.require('goog.testing.recordFunction');
const string = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');

function MockXmlHttp() {
  /**
   * The request headers for this XmlHttpRequest.
   * @type {!Object<string>}
   * @suppress {globalThis} suppression added to enable type checking
   */
  this.requestHeaders = {};

  /**
   * The response headers for this XmlHttpRequest.
   * @type {!Object<string>}
   * @suppress {globalThis} suppression added to enable type checking
   */
  this.responseHeaders = {};

  /**
   * @type {string}
   * @suppress {globalThis} suppression added to enable type checking
   */
  this.responseHeadersString = null;

  /**
   * The upload object associated with this XmlHttpRequest.
   * @type {!Object}
   * @suppress {globalThis} suppression added to enable type checking
   */
  this.upload = {};
}

MockXmlHttp.prototype.readyState = XmlHttp.ReadyState.UNINITIALIZED;

MockXmlHttp.prototype.status = 200;

MockXmlHttp.syncSend = false;

MockXmlHttp.prototype.send = function(opt_data) {
  this.readyState = XmlHttp.ReadyState.UNINITIALIZED;

  if (MockXmlHttp.syncSend) {
    this.complete();
  }
};

MockXmlHttp.prototype.complete = function() {
  this.readyState = XmlHttp.ReadyState.LOADING;
  this.onreadystatechange();

  this.readyState = XmlHttp.ReadyState.LOADED;
  this.onreadystatechange();

  this.readyState = XmlHttp.ReadyState.INTERACTIVE;
  this.onreadystatechange();

  this.readyState = XmlHttp.ReadyState.COMPLETE;
  this.onreadystatechange();
};


MockXmlHttp.prototype.open = function(verb, uri, async) {};

MockXmlHttp.prototype.abort = function() {};

MockXmlHttp.prototype.setRequestHeader = function(key, value) {
  this.requestHeaders[key] = value;
};

/**
 * @param {string} key
 * @return {?string}
 */
MockXmlHttp.prototype.getResponseHeader = function(key) {
  return key in this.responseHeaders ? this.responseHeaders[key] : null;
};

/** @return {?string} */
MockXmlHttp.prototype.getAllResponseHeaders = function() {
  return this.responseHeadersString;
};

let lastMockXmlHttp;
XmlHttp.setGlobalFactory(new WrapperXmlHttpFactory(
    function() {
      /** @suppress {checkTypes} suppression added to enable type checking */
      lastMockXmlHttp = new MockXmlHttp();
      return lastMockXmlHttp;
    },
    function() {
      return {};
    }));


const propertyReplacer = new PropertyReplacer();
let clock;
/** @suppress {visibility} suppression added to enable type checking */
const originalEntryPoint = XhrIo.prototype.onReadyStateChangeEntryPoint_;


testSuite({
  setUp() {
    lastMockXmlHttp = null;
    clock = new MockClock(true);
  },

  tearDown() {
    MockXmlHttp.syncSend = false;
    propertyReplacer.reset();
    clock.dispose();
    /** @suppress {visibility} suppression added to enable type checking */
    XhrIo.prototype.onReadyStateChangeEntryPoint_ = originalEntryPoint;
  },


  testSyncSend() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertTrue('Should be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send('url');
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },

  testSyncSendFailure() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertFalse('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send('url');
    lastMockXmlHttp.status = 404;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendRelativeZeroStatus() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertEquals(
          'Should be the same as ', e.target.isSuccess(),
          window.location.href.toLowerCase().indexOf('file:') == 0);
      count++;
    });

    let inSend = true;
    x.send('relative');
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendRelativeUriZeroStatus() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertEquals(
          'Should be the same as ', e.target.isSuccess(),
          window.location.href.toLowerCase().indexOf('file:') == 0);
      count++;
    });

    let inSend = true;
    x.send(Uri.parse('relative'));
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendHttpZeroStatusFailure() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertFalse('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send('http://foo');
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendHttpUpperZeroStatusFailure() {
    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertFalse('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send('HTTP://foo');
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendHttpUpperUriZeroStatusFailure() {
    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertFalse('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send(Uri.parse('HTTP://foo'));
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendHttpUriZeroStatusFailure() {
    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertFalse('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send(Uri.parse('http://foo'));
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendHttpUriZeroStatusFailure_upperCaseHTTP() {
    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertFalse('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send(Uri.parse('HTTP://foo'));
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendHttpsZeroStatusFailure() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertFalse('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send('https://foo');
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendFileUpperZeroStatusSuccess() {
    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertTrue('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send('FILE:///foo');
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendFileUriZeroStatusSuccess() {
    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertTrue('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send(Uri.parse('file:///foo'));
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendDummyUriZeroStatusSuccess() {
    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertTrue('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send(Uri.parse('dummy:///foo'));
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendFileUpperUriZeroStatusSuccess() {
    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertFalse('Should not fire complete from inside send', inSend);
      assertTrue('Should not be successful', e.target.isSuccess());
      count++;
    });

    let inSend = true;
    x.send(Uri.parse('FILE:///foo'));
    lastMockXmlHttp.status = 0;
    inSend = false;

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testSendFromListener() {
    MockXmlHttp.syncSend = true;
    let count = 0;

    const x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      count++;

      e = assertThrows(function() {
        x.send('url2');
      });
      assertEquals(
          '[goog.net.XhrIo] Object is active with another request=url' +
              '; newUri=url2',
          e.message);
    });

    x.send('url');

    clock.tick(1);  // callOnce(f, 0, ...)

    assertEquals('Complete should have been called once', 1, count);
  },


  testStatesDuringEvents() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    MockXmlHttp.syncSend = true;

    const x = new XhrIo;
    let readyState = ReadyState.UNINITIALIZED;
    events.listen(x, EventType.READY_STATE_CHANGE, function(e) {
      readyState++;
      assertObjectEquals(e.target, x);
      assertEquals(x.getReadyState(), readyState);
      assertTrue(x.isActive());
    });
    events.listen(x, EventType.COMPLETE, function(e) {
      assertObjectEquals(e.target, x);
      assertTrue(x.isActive());
    });
    events.listen(x, EventType.SUCCESS, function(e) {
      assertObjectEquals(e.target, x);
      assertTrue(x.isActive());
    });
    events.listen(x, EventType.READY, function(e) {
      assertObjectEquals(e.target, x);
      assertFalse(x.isActive());
    });

    x.send('url');

    clock.tick(1);  // callOnce(f, 0, ...)
  },


  testProtectEntryPointCalledOnAsyncSend() {
    MockXmlHttp.syncSend = false;

    let errorHandlerCallbackCalled = false;
    const errorHandler = new ErrorHandler(function() {
      errorHandlerCallbackCalled = true;
    });

    XhrIo.protectEntryPoints(errorHandler);

    const x = new XhrIo;
    events.listen(x, EventType.READY_STATE_CHANGE, function(e) {
      throw new Error();
    });

    x.send('url');
    assertThrows(function() {
      lastMockXmlHttp.complete();
    });

    assertTrue(
        'Error handler callback should be called on async send.',
        errorHandlerCallbackCalled);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testXHRIsDiposedEvenIfAListenerThrowsAnExceptionOnComplete() {
    MockXmlHttp.syncSend = false;

    const x = new XhrIo;

    events.listen(x, EventType.COMPLETE, function(e) {
      throw new Error();
    }, false, x);

    x.send('url');
    assertThrows(function() {
      lastMockXmlHttp.complete();
    });

    // The XHR should have been disposed, even though the listener threw an
    // exception.
    assertNull(x.xhr_);
  },

  testDisposeInternalDoesNotAbortXhrRequestObjectWhenActiveIsFalse() {
    MockXmlHttp.syncSend = false;

    const originalAbort = XmlHttp.prototype.abort;
    let abortCalled = false;
    const x = new XhrIo;

    XmlHttp.prototype.abort = function() {
      abortCalled = true;
    };

    events.listen(x, EventType.COMPLETE, function(e) {
      /** @suppress {visibility} suppression added to enable type checking */
      this.active_ = false;
      this.dispose();
    }, false, x);

    x.send('url');
    lastMockXmlHttp.complete();

    XmlHttp.prototype.abort = originalAbort;
    assertFalse(abortCalled);
  },

  testCallingAbortFromWithinAbortCallbackDoesntLoop() {
    const x = new XhrIo;
    events.listen(x, EventType.ABORT, function(e) {
      x.abort();  // Shouldn't get a stack overflow
    });
    x.send('url');
    x.abort();
  },

  testPostSetsContentTypeHeader() {
    const x = new XhrIo;

    x.send('url', 'POST', 'content');
    const headers = lastMockXmlHttp.requestHeaders;
    assertEquals(1, object.getCount(headers));
    assertEquals(headers[XhrIo.CONTENT_TYPE_HEADER], XhrIo.FORM_CONTENT_TYPE);
  },

  testNonPostSetsContentTypeHeader() {
    const x = new XhrIo;

    x.send('url', 'PUT', 'content');
    const headers = lastMockXmlHttp.requestHeaders;
    assertEquals(1, object.getCount(headers));
    assertEquals(headers[XhrIo.CONTENT_TYPE_HEADER], XhrIo.FORM_CONTENT_TYPE);
  },

  testContentTypeIsTreatedCaseInsensitively() {
    const x = new XhrIo;

    x.send('url', 'POST', 'content', {'content-type': 'testing'});

    assertObjectEquals(
        'Headers should not be modified since they already contain a ' +
            'content type definition',
        {'content-type': 'testing'}, lastMockXmlHttp.requestHeaders);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPostFormDataDoesNotSetContentTypeHeader() {
    function FakeFormData() {}

    propertyReplacer.set(globalThis, 'FormData', FakeFormData);

    const x = new XhrIo;
    x.send('url', 'POST', new FakeFormData());
    const headers = lastMockXmlHttp.requestHeaders;
    assertTrue(object.isEmpty(headers));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testNonPostFormDataDoesNotSetContentTypeHeader() {
    function FakeFormData() {}

    propertyReplacer.set(globalThis, 'FormData', FakeFormData);

    const x = new XhrIo;
    x.send('url', 'PUT', new FakeFormData());
    const headers = lastMockXmlHttp.requestHeaders;
    assertTrue(object.isEmpty(headers));
  },

  testFactoryInjection() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const xhr = new MockXmlHttp();
    let optionsFactoryCalled = 0;
    let xhrFactoryCalled = 0;
    const wrapperFactory = new WrapperXmlHttpFactory(
        function() {
          xhrFactoryCalled++;
          return xhr;
        },
        function() {
          optionsFactoryCalled++;
          return {};
        });
    const xhrIo = new XhrIo(wrapperFactory);

    xhrIo.send('url');

    assertEquals('XHR factory should have been called', 1, xhrFactoryCalled);
    assertEquals(
        'Options factory should have been called', 1, optionsFactoryCalled);
  },

  testGoogTestingNetXhrIoIsInSync() {
    const xhrIo = new XhrIo();
    const testingXhrIo = new TestingNetXhrIo();

    const propertyComparator = function(value, key, obj) {
      if (string.endsWith(key, '_')) {
        // Ignore private properties/methods
        return true;
      } else if (typeof value == 'function' && typeof this[key] != 'function') {
        // Only type check is sufficient for functions
        fail(
            'Mismatched property:' + key + ': XhrIo has:<' + value +
            '>; while goog.testing.net.XhrIo has:<' + this[key] + '>');
        return true;
      } else {
        // Ignore all other type of properties.
        return true;
      }
    };

    object.every(xhrIo, propertyComparator, testingXhrIo);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testEntryPointRegistry() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const monitor = new EntryPointMonitor();
    const replacement = function() {};
    monitor.wrap = recordFunction(functions.constant(replacement));

    entryPointRegistry.monitorAll(monitor);
    assertTrue(monitor.wrap.getCallCount() >= 1);
    assertEquals(replacement, XhrIo.prototype.onReadyStateChangeEntryPoint_);
  },

  testSetWithCredentials() {
    // Test on XHR objects that don't have the withCredentials property (older
    // browsers).
    let x = new XhrIo;
    x.setWithCredentials(true);
    x.send('url');
    assertFalse(
        'withCredentials should not be set on an XHR object if the property ' +
            'does not exist.',
        object.containsKey(lastMockXmlHttp, 'withCredentials'));

    // Test on XHR objects that have the withCredentials property.
    MockXmlHttp.prototype.withCredentials = false;
    x = new XhrIo;
    x.setWithCredentials(true);
    x.send('url');
    assertTrue(
        'withCredentials should be set on an XHR object if the property exists',
        object.containsKey(lastMockXmlHttp, 'withCredentials'));

    assertTrue(
        'withCredentials value not set on XHR object',
        lastMockXmlHttp.withCredentials);

    // Reset the prototype so it does not effect other tests.
    delete MockXmlHttp.prototype.withCredentials;
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetProgressEventsEnabled() {
    // The default MockXhr object contained by the XhrIo object has no
    // reference to the necessary onprogress field. This is equivalent
    // to a browser which does not support progress events.
    const progressNotSupported = new XhrIo;
    progressNotSupported.setProgressEventsEnabled(true);
    assertTrue(progressNotSupported.getProgressEventsEnabled());
    progressNotSupported.send('url');
    assertUndefined(
        'Progress is not supported for downloads on this request.',
        progressNotSupported.xhr_.onprogress);
    assertUndefined(
        'Progress is not supported for uploads on this request.',
        progressNotSupported.xhr_.upload.onprogress);

    // The following tests will include the necessary onprogress fields
    // indicating progress events are supported.
    MockXmlHttp.prototype.onprogress = null;

    const progressDisabled = new XhrIo;
    progressDisabled.setProgressEventsEnabled(false);
    assertFalse(progressDisabled.getProgressEventsEnabled());
    progressDisabled.send('url');
    assertNull(
        'No progress handler should be set for downloads.',
        progressDisabled.xhr_.onprogress);
    assertUndefined(
        'No progress handler should be set for uploads.',
        progressDisabled.xhr_.upload.onprogress);

    const progressEnabled = new XhrIo;
    progressEnabled.setProgressEventsEnabled(true);
    assertTrue(progressEnabled.getProgressEventsEnabled());
    progressEnabled.send('url');
    assertTrue(
        'Progress handler should be set for downloads.',
        typeof progressEnabled.xhr_.onprogress === 'function');
    assertTrue(
        'Progress handler should be set for uploads.',
        typeof progressEnabled.xhr_.upload.onprogress === 'function');

    // Clean-up.
    delete MockXmlHttp.prototype.onprogress;
  },


  testGetResponse() {
    const x = new XhrIo;

    // No XHR yet
    assertEquals(null, x.getResponse());

    // XHR with no .response and no response type, gets text.
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    x.xhr_ = {};
    /** @suppress {visibility} suppression added to enable type checking */
    x.xhr_.responseText = 'text';
    assertEquals('text', x.getResponse());

    // Response type of text gets text as well.
    x.setResponseType(XhrIo.ResponseType.TEXT);
    /** @suppress {visibility} suppression added to enable type checking */
    x.xhr_.responseText = '';
    assertEquals('', x.getResponse());

    // Response type of array buffer gets the array buffer.
    /** @suppress {visibility} suppression added to enable type checking */
    x.xhr_.mozResponseArrayBuffer = 'ab';
    x.setResponseType(XhrIo.ResponseType.ARRAY_BUFFER);
    assertEquals('ab', x.getResponse());

    // With a response field, it is returned no matter what value it has.
    /** @suppress {visibility} suppression added to enable type checking */
    x.xhr_.response = undefined;
    assertEquals(undefined, x.getResponse());

    /** @suppress {visibility} suppression added to enable type checking */
    x.xhr_.response = null;
    assertEquals(null, x.getResponse());

    /** @suppress {visibility} suppression added to enable type checking */
    x.xhr_.response = '';
    assertEquals('', x.getResponse());

    /** @suppress {visibility} suppression added to enable type checking */
    x.xhr_.response = 'resp';
    assertEquals('resp', x.getResponse());
  },

  testGetResponseHeader() {
    const x = new XhrIo();
    x.send('http://foo');

    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    x.xhr_.responseHeaders['foo'] = null;
    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    x.xhr_.responseHeaders['bar'] = 'xyz';
    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    x.xhr_.responseHeaders['baz'] = '';

    // All headers should be undefined prior to the request completing.
    assertUndefined(x.getResponseHeader('foo'));
    assertUndefined(x.getResponseHeader('bar'));
    assertUndefined(x.getResponseHeader('baz'));

    /** @suppress {visibility} suppression added to enable type checking */
    x.xhr_.readyState = ReadyState.COMPLETE;

    assertUndefined(x.getResponseHeader('foo'));
    assertEquals('xyz', x.getResponseHeader('bar'));
    assertEquals('', x.getResponseHeader('baz'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetResponseHeaders() {
    MockXmlHttp.syncSend = true;
    const x = new XhrIo();

    // No XHR yet
    assertEquals(0, object.getCount(x.getResponseHeaders()));

    x.send();

    // Simulate an XHR with 2 headers.
    lastMockXmlHttp.responseHeadersString = 'test1: foo\r\ntest2: bar';

    const headers = x.getResponseHeaders();
    assertEquals(2, object.getCount(headers));
    assertEquals('foo', headers['test1']);
    assertEquals('bar', headers['test2']);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetResponseHeadersWithColonInValue() {
    MockXmlHttp.syncSend = true;
    const x = new XhrIo();

    x.send();

    // Simulate an XHR with a colon in the http header value.
    lastMockXmlHttp.responseHeadersString = 'test1: f:o : o';

    const headers = x.getResponseHeaders();
    assertEquals(1, object.getCount(headers));
    assertEquals('f:o : o', headers['test1']);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetResponseHeadersMultipleValuesForOneKey() {
    MockXmlHttp.syncSend = true;
    const x = new XhrIo();

    // No XHR yet
    assertEquals(0, object.getCount(x.getResponseHeaders()));

    x.send();

    // Simulate an XHR with 2 headers.
    lastMockXmlHttp.responseHeadersString = 'test1: foo\r\ntest1: bar';

    const headers = x.getResponseHeaders();
    assertEquals(1, object.getCount(headers));
    assertEquals('foo, bar', headers['test1']);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetResponseHeadersWhitespaceValue() {
    MockXmlHttp.syncSend = true;
    const x = new XhrIo();

    // No XHR yet
    assertEquals(0, object.getCount(x.getResponseHeaders()));

    x.send();

    // Simulate an XHR with whitespace as its value..
    lastMockXmlHttp.responseHeadersString = 'test2:   ';

    const headers = x.getResponseHeaders();
    assertEquals(1, object.getCount(headers));
    assertEquals('', headers['test2']);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetResponseHeadersEmptyHeader() {
    MockXmlHttp.syncSend = true;
    const x = new XhrIo();

    // No XHR yet
    assertEquals(0, object.getCount(x.getResponseHeaders()));

    x.send();

    // Simulate an XHR with 2 headers, the last of which is empty.
    lastMockXmlHttp.responseHeadersString = 'test2: bar\r\n';

    const headers = x.getResponseHeaders();
    assertEquals(1, object.getCount(headers));
    assertEquals('bar', headers['test2']);
  },


  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetResponseHeadersNullHeader() {
    MockXmlHttp.syncSend = true;

    const x = new XhrIo();

    // No XHR yet
    assertEquals(0, object.getCount(x.getResponseHeaders()));

    x.send();

    const headers = x.getResponseHeaders();
    assertEquals(0, object.getCount(headers));
  },

  testSetTrustTokenHeaderTrustTokenNotSupported() {
    const trustToken = {
      type: 'send-redemption-record',
      issuers: ['https://www.a.com', 'https://www.b.com'],
      refreshPolicy: 'none',
      signRequestData: 'include',
      includeTimestampHeader: true,
      additionalSignedHeaders: ['sec-time', 'Sec-Redemption-Record'],
      additionalSigningData: 'ENCODED_URL',
    };

    MockXmlHttp.syncSend = true;

    let x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertTrue('Should be successful', e.target.isSuccess());
      count++;
    });

    let count = 0;
    // Test on XHR objects that don't have the setTrustToken function (browser
    // doesn't support or disabled trust token).
    x.setTrustToken(trustToken);
    x.send('url');
    clock.tick(1);  // callOnce(f, 0, ...)
    assertEquals('Complete should have been called once', 1, count);
  },

  testSetTrustTokenHeaderTrustTokenSupported() {
    const trustToken = {
      type: 'send-redemption-record',
      issuers: ['https://www.a.com', 'https://www.b.com'],
      refreshPolicy: 'none',
      signRequestData: 'include',
      includeTimestampHeader: true,
      additionalSignedHeaders: ['sec-time', 'Sec-Redemption-Record'],
      additionalSigningData: 'ENCODED_URL',
    };

    MockXmlHttp.syncSend = true;

    let x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertTrue('Should be successful', e.target.isSuccess());
      count++;
    });

    let count = 0;

    // Test on XHR objects that have the setTrustToken function
    MockXmlHttp.prototype.setTrustToken = () => {};
    x = new XhrIo;
    events.listen(x, EventType.COMPLETE, function(e) {
      assertTrue('Should be successful', e.target.isSuccess());
      count++;
    });
    x.setTrustToken(trustToken);
    x.send('url');
    clock.tick(1);  // callOnce(f, 0, ...)
    assertEquals('Complete should have been called once', 1, count);
    // Reset the prototype so it does not effect other tests.
    delete MockXmlHttp.prototype.setTrustToken;
  },
});
