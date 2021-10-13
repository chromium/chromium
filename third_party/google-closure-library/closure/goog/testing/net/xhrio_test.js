/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.net.XhrIoTest');
goog.setTestOnly();

const ErrorCode = goog.require('goog.net.ErrorCode');
const EventType = goog.require('goog.net.EventType');
const GoogEvent = goog.require('goog.events.Event');
const InstanceOf = goog.require('goog.testing.mockmatchers.InstanceOf');
const MockControl = goog.require('goog.testing.MockControl');
const XhrIo = goog.require('goog.testing.net.XhrIo');
const XmlHttp = goog.require('goog.net.XmlHttp');
const asserts = goog.require('goog.testing.asserts');
const domXml = goog.require('goog.dom.xml');
const events = goog.require('goog.events');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

// In order to emulate the actual behavior of XhrIo, set this value for all
// tests until the default value is false.
XhrIo.allowUnsafeAccessToXhrIoOutsideCallbacks = false;

let mockControl;

testSuite({
  setUp() {
    mockControl = new MockControl();
  },

  testStaticSend() {
    const sendInstances = XhrIo.getSendInstances();
    const returnedXhr = XhrIo.send('url');
    assertEquals('sendInstances_ after send', 1, sendInstances.length);
    const xhr = sendInstances[sendInstances.length - 1];
    assertTrue('isActive after request', xhr.isActive());
    assertEquals(returnedXhr, xhr);
    assertEquals(
        'readyState after request', XmlHttp.ReadyState.LOADING,
        xhr.getReadyState());

    xhr.simulateResponse(200, '');
    assertFalse('isActive after response', xhr.isActive());
    assertEquals(
        'readyState after response', XmlHttp.ReadyState.COMPLETE,
        xhr.getReadyState());

    xhr.simulateReady();
    assertEquals('sendInstances_ after READY', 0, sendInstances.length);
  },

  testStaticSendWithException() {
    XhrIo.send('url', function() {
      if (!this.isSuccess()) {
        throw new Error('The xhr did not complete successfully!');
      }
    });
    const sendInstances = XhrIo.getSendInstances();
    const xhr = sendInstances[sendInstances.length - 1];
    try {
      xhr.simulateResponse(400, '');
    } catch (e) {
      // Do nothing with the exception; we just want to make sure
      // the class cleans itself up properly when an exception is
      // thrown.
    }
    assertEquals(
        'Send instance array not cleaned up properly!', 0,
        sendInstances.length);
  },

  testMultipleSend() {
    const xhr = new XhrIo();
    assertFalse('isActive before first request', xhr.isActive());
    assertEquals(
        'readyState before first request', XmlHttp.ReadyState.UNINITIALIZED,
        xhr.getReadyState());

    xhr.send('url');
    assertTrue('isActive after first request', xhr.isActive());
    assertEquals(
        'readyState after first request', XmlHttp.ReadyState.LOADING,
        xhr.getReadyState());

    xhr.simulateResponse(200, '');
    assertFalse('isActive after first response', xhr.isActive());
    assertEquals(
        'readyState after first response', XmlHttp.ReadyState.COMPLETE,
        xhr.getReadyState());

    xhr.send('url');
    assertTrue('isActive after second request', xhr.isActive());
    assertEquals(
        'readyState after second request', XmlHttp.ReadyState.LOADING,
        xhr.getReadyState());
  },

  testGetLastUri() {
    const xhr = new XhrIo();
    assertEquals('nothing sent yet, empty URI', '', xhr.getLastUri());

    const requestUrl = 'http://www.example.com/';
    xhr.send(requestUrl);
    assertEquals('message sent, URI saved', requestUrl, xhr.getLastUri());
  },

  testGetLastMethod() {
    const xhr = new XhrIo();
    assertEquals('nothing sent yet, empty method', xhr.getLastMethod(), '');

    const method = 'POKE';
    xhr.send('http://www.example.com/', method);
    assertEquals('message sent, method saved', method, xhr.getLastMethod());
    xhr.simulateResponse(200, '');

    xhr.send('http://www.example.com/');
    assertEquals('message sent, method saved', 'GET', xhr.getLastMethod());
  },

  testGetLastContent() {
    const xhr = new XhrIo();
    assertUndefined('nothing sent yet, empty content', xhr.getLastContent());

    const postContent = 'var=value&var2=value2';
    xhr.send('http://www.example.com/', undefined, postContent);
    assertEquals(
        'POST message sent, content saved', postContent, xhr.getLastContent());
    xhr.simulateResponse(200, '');

    xhr.send('http://www.example.com/');
    assertUndefined('GET message sent, content cleaned', xhr.getLastContent());
  },

  testGetLastRequestHeaders() {
    const xhr = new XhrIo();
    assertUndefined(
        'nothing sent yet, empty headers', xhr.getLastRequestHeaders());

    xhr.send(
        'http://www.example.com/', undefined, undefined,
        {'From': 'page@google.com'});
    assertObjectEquals(
        'Request sent with extra headers, headers saved',
        {'From': 'page@google.com'}, xhr.getLastRequestHeaders());
    xhr.simulateResponse(200, '');

    xhr.send('http://www.example.com');
    assertUndefined(
        'New request sent without extra headers', xhr.getLastRequestHeaders());
    xhr.simulateResponse(200, '');

    xhr.headers.set('X', 'A');
    xhr.headers.set('Y', 'B');
    xhr.send(
        'http://www.example.com/', undefined, undefined, {'Y': 'P', 'Z': 'Q'});
    assertObjectEquals(
        'Default headers combined with call headers',
        {'X': 'A', 'Y': 'P', 'Z': 'Q'}, xhr.getLastRequestHeaders());
    xhr.simulateResponse(200, '');
  },

  testGetResponseText() {
    // Text response came.
    let called = false;
    let xhr = new XhrIo();
    events.listen(xhr, EventType.SUCCESS, (e) => {
      called = true;
      assertEquals('text', e.target.getResponseText());
    });
    xhr.simulateResponse(200, 'text');
    assertTrue(called);

    // XML response came.
    called = false;
    xhr = new XhrIo();
    const xml = domXml.createDocument();
    xml.appendChild(xml.createElement('root'));
    events.listen(xhr, EventType.SUCCESS, (e) => {
      called = true;
      const text = e.target.getResponseText();
      assertTrue(/<root ?\/>/.test(text));
    });
    xhr.simulateResponse(200, xml);
    assertTrue(called);

    // Outside the callback, getResponseText returns an empty string.
    assertEquals('', xhr.getResponseText());
  },

  testGetResponseJson() {
    // Valid JSON response came.
    let called = false;
    let xhr = new XhrIo();
    events.listen(xhr, EventType.SUCCESS, (e) => {
      called = true;
      assertArrayEquals([0, 1], e.target.getResponseJson());
    });
    xhr.simulateResponse(200, '[0, 1]');
    assertTrue(called);

    // Valid JSON response with XSSI prefix encoded came.
    called = false;
    xhr = new XhrIo();
    events.listen(xhr, EventType.SUCCESS, (e) => {
      called = true;
      assertArrayEquals([0, 1], e.target.getResponseJson(')]}\', \n'));
    });
    xhr.simulateResponse(200, ')]}\', \n[0, 1]');
    assertTrue(called);

    // Outside the callback, getResponseJson returns undefined.
    assertUndefined(xhr.getResponseJson());

    // Invalid JSON response came.
    called = false;
    xhr = new XhrIo();
    events.listen(xhr, EventType.SUCCESS, (e) => {
      called = true;
      assertThrows(goog.bind(e.target.getResponseJson, e.target));
    });
    xhr.simulateResponse(200, '[0, 1');
    assertTrue(called);

    // XML response came.
    called = false;
    xhr = new XhrIo();
    const xml = domXml.createDocument();
    xml.appendChild(xml.createElement('root'));
    events.listen(xhr, EventType.SUCCESS, (e) => {
      called = true;
      assertThrows(goog.bind(e.target.getResponseJson, e.target));
    });
    xhr.simulateResponse(200, xml);
    assertTrue(called);
  },

  testGetResponseXml() {
    // Text response came.
    let called = false;
    let xhr = new XhrIo();
    events.listen(xhr, EventType.SUCCESS, (e) => {
      called = true;
      assertNull(e.target.getResponseXml());
    });
    xhr.simulateResponse(200, 'text');
    assertTrue(called);

    // XML response came.
    called = false;
    xhr = new XhrIo();
    const xml = domXml.createDocument();
    xml.appendChild(xml.createElement('root'));
    events.listen(xhr, EventType.SUCCESS, (e) => {
      called = true;
      assertEquals(xml, e.target.getResponseXml());
    });
    xhr.simulateResponse(200, xml);
    assertTrue(called);

    // Outside the callback, getResponseXml returns null.
    assertNull(xhr.getResponseXml());
  },

  testGetResponsesAllowUnsafeAccessToXhrIoOutsideCallbacks() {
    // Test that if true is passed for opt_allowAccessToXhrIoOutsideOfCallback,
    // then we can call getResponse*() outside of the SUCCESS event callback.
    XhrIo.allowUnsafeAccessToXhrIoOutsideCallbacks = true;

    const xhr = new XhrIo();
    xhr.simulateResponse(200, 'text');
    assertEquals('text', xhr.getResponseText());

    xhr.simulateResponse(200, '[0, 1]');
    assertArrayEquals([0, 1], xhr.getResponseJson());

    const xml = domXml.createDocument();
    xml.appendChild(xml.createElement('root'));
    xhr.simulateResponse(200, xml);
    assertEquals(xml, xhr.getResponseXml());

    const headers = {'test1': 'foo', 'test2': 'bar'};
    xhr.simulateResponse(200, '', headers);
    assertObjectEquals(headers, xhr.getResponseHeaders());
    assertEquals('test1: foo\r\ntest2: bar', xhr.getAllResponseHeaders());

    // Reset the value for future tests.
    XhrIo.allowUnsafeAccessToXhrIoOutsideCallbacks = false;
  },

  testGetResponseHeaders_noHeadersPresent() {
    const xhr = new XhrIo();
    const mockListener = mockControl.createFunctionMock();
    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      assertTrue(e.type == EventType.SUCCESS);
      assertUndefined(e.target.getResponseHeader('XHR'));
    });
    mockControl.$replayAll();
    events.listen(xhr, EventType.SUCCESS, mockListener);
    xhr.simulateResponse(200, '');

    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  testGetResponseHeaders_headersPresent() {
    const xhr = new XhrIo();
    const mockListener = mockControl.createFunctionMock();
    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      assertTrue(e.type == EventType.SUCCESS);
      assertUndefined(e.target.getResponseHeader('XHR'));
      assertEquals(e.target.getResponseHeader('Pragma'), 'no-cache');
    });
    mockControl.$replayAll();
    events.listen(xhr, EventType.SUCCESS, mockListener);
    xhr.simulateResponse(200, '', {'Pragma': 'no-cache'});

    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  testAbort_WhenNoPendingSentRequests() {
    const xhr = new XhrIo();
    const eventListener = mockControl.createFunctionMock();
    mockControl.$replayAll();

    events.listen(xhr, EventType.COMPLETE, eventListener);
    events.listen(xhr, EventType.SUCCESS, eventListener);
    events.listen(xhr, EventType.ABORT, eventListener);
    events.listen(xhr, EventType.ERROR, eventListener);
    events.listen(xhr, EventType.READY, eventListener);

    xhr.abort();

    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  testAbort_PendingSentRequest() {
    const xhr = new XhrIo();
    const mockListener = mockControl.createFunctionMock();

    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      assertTrue(e.type == EventType.COMPLETE);
      assertObjectEquals(e.target, xhr);
      assertEquals(e.target.getStatus(), -1);
      assertEquals(e.target.getLastErrorCode(), ErrorCode.ABORT);
      assertTrue(e.target.isActive());
    });
    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      assertTrue(e.type == EventType.ABORT);
      assertObjectEquals(e.target, xhr);
      assertTrue(e.target.isActive());
    });
    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      assertTrue(e.type == EventType.READY);
      assertObjectEquals(e.target, xhr);
      assertFalse(e.target.isActive());
    });
    mockControl.$replayAll();

    events.listen(xhr, EventType.COMPLETE, mockListener);
    events.listen(xhr, EventType.SUCCESS, mockListener);
    events.listen(xhr, EventType.ABORT, mockListener);
    events.listen(xhr, EventType.ERROR, mockListener);
    events.listen(xhr, EventType.READY, mockListener);
    xhr.send('dummyurl');
    xhr.abort();

    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  testEvents_Success() {
    const xhr = new XhrIo();
    const mockListener = mockControl.createFunctionMock();

    let readyState = XmlHttp.ReadyState.UNINITIALIZED;
    function readyStateListener(e) {
      assertEquals(e.type, EventType.READY_STATE_CHANGE);
      assertObjectEquals(e.target, xhr);
      readyState++;
      assertEquals(e.target.getReadyState(), readyState);
      assertTrue(e.target.isActive());
    }

    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      assertEquals(e.type, EventType.COMPLETE);
      assertObjectEquals(e.target, xhr);
      assertEquals(e.target.getLastErrorCode(), ErrorCode.NO_ERROR);
      assertTrue(e.target.isActive());
    });
    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      assertEquals(e.type, EventType.SUCCESS);
      assertObjectEquals(e.target, xhr);
      assertTrue(e.target.isActive());
    });
    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      assertEquals(e.type, EventType.READY);
      assertObjectEquals(e.target, xhr);
      assertFalse(e.target.isActive());
    });
    mockControl.$replayAll();

    events.listen(xhr, EventType.READY_STATE_CHANGE, readyStateListener);
    events.listen(xhr, EventType.COMPLETE, mockListener);
    events.listen(xhr, EventType.SUCCESS, mockListener);
    events.listen(xhr, EventType.ABORT, mockListener);
    events.listen(xhr, EventType.ERROR, mockListener);
    events.listen(xhr, EventType.READY, mockListener);
    xhr.send('dummyurl');
    xhr.simulateResponse(200, null);

    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  testGetResponseHeaders() {
    const xhr = new XhrIo();
    const mockListener = mockControl.createFunctionMock();
    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      const headers = e.target.getResponseHeaders();
      assertEquals(2, googObject.getCount(headers));
      assertEquals('foo', headers['test1']);
      assertEquals('bar', headers['test2']);
    });
    mockControl.$replayAll();
    events.listen(xhr, EventType.SUCCESS, mockListener);

    // Simulate an XHR with 2 headers.
    xhr.simulateResponse(200, '', {'test1': 'foo', 'test2': 'bar'});

    // Outside the callback, getResponseHeaders returns an empty object.
    assertObjectEquals({}, xhr.getResponseHeaders());
    assertEquals('', xhr.getAllResponseHeaders());

    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  testGetResponseHeadersWithColonInValue() {
    const xhr = new XhrIo();
    const mockListener = mockControl.createFunctionMock();
    mockListener(new InstanceOf(GoogEvent)).$does((e) => {
      const headers = e.target.getResponseHeaders();
      assertEquals(1, googObject.getCount(headers));
      assertEquals('f:o:o', headers['test1']);
    });
    mockControl.$replayAll();
    events.listen(xhr, EventType.SUCCESS, mockListener);

    // Simulate an XHR with a colon in the http header value.
    xhr.simulateResponse(200, '', {'test1': 'f:o:o'});

    mockControl.$verifyAll();
    mockControl.$resetAll();
  },
});
