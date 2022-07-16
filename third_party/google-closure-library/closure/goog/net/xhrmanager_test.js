/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.XhrManagerTest');
goog.setTestOnly();

const EventType = goog.require('goog.net.EventType');
const TestingNetXhrIo = goog.requireType('goog.testing.net.XhrIo');
const XhrIo = goog.require('goog.net.XhrIo');
const XhrIoPool = goog.require('goog.testing.net.XhrIoPool');
const XhrManager = goog.require('goog.net.XhrManager');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.events');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

/** @type {XhrManager} */
let xhrManager;

/** @type {TestingNetXhrIo} */
let xhrIo;

testSuite({
  setUp() {
    xhrManager = new XhrManager();
    /** @suppress {visibility} suppression added to enable type checking */
    xhrManager.xhrPool_ = new XhrIoPool();
    /** @suppress {visibility} suppression added to enable type checking */
    xhrIo = xhrManager.xhrPool_.getXhr();
  },

  tearDown() {
    dispose(xhrManager);
  },

  testGetOutstandingRequestIds() {
    assertArrayEquals(
        'No outstanding requests', [], xhrManager.getOutstandingRequestIds());

    xhrManager.send('test1', '/test1');
    assertArrayEquals(
        'Single outstanding request', ['test1'],
        xhrManager.getOutstandingRequestIds());

    xhrManager.send('test2', '/test2');
    assertArrayEquals(
        'Two outstanding requests', ['test1', 'test2'],
        xhrManager.getOutstandingRequestIds());

    xhrIo.simulateResponse(200, 'data');
    assertArrayEquals(
        'Single outstanding request', ['test2'],
        xhrManager.getOutstandingRequestIds());

    xhrIo.simulateResponse(200, 'data');
    assertArrayEquals(
        'No outstanding requests', [], xhrManager.getOutstandingRequestIds());
  },

  testForceAbortQueuedRequest() {
    xhrManager.send('test', '/test');
    xhrManager.send('queued', '/queued');

    assertNotThrows(
        'Forced abort of queued request should not throw an error',
        goog.bind(xhrManager.abort, xhrManager, 'queued', true));

    assertNotThrows(
        'Forced abort of normal request should not throw an error',
        goog.bind(xhrManager.abort, xhrManager, 'test', true));
  },

  testDefaultResponseType() {
    const callback = recordFunction((e) => {
      assertEquals('test1', e.id);
      assertEquals(XhrIo.ResponseType.DEFAULT, e.xhrIo.getResponseType());
    });
    events.listenOnce(xhrManager, EventType.READY, callback);
    xhrManager.send('test1', '/test2');
    assertEquals(1, callback.getCallCount());

    xhrIo.simulateResponse(200, 'data');  // Do this to make tearDown() happy.
  },

  testNonDefaultResponseType() {
    const callback = recordFunction((e) => {
      assertEquals('test2', e.id);
      assertEquals(XhrIo.ResponseType.ARRAY_BUFFER, e.xhrIo.getResponseType());
    });
    events.listenOnce(xhrManager, EventType.READY, callback);
    xhrManager.send(
        'test2', '/test2', undefined /* opt_method */,
        undefined /* opt_content */, undefined /* opt_headers */,
        undefined /* opt_priority */, undefined /* opt_callback */,
        undefined /* opt_maxRetries */, XhrIo.ResponseType.ARRAY_BUFFER);
    assertEquals(1, callback.getCallCount());

    xhrIo.simulateResponse(200, 'data');  // Do this to make tearDown() happy.
  },
});
