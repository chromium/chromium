/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.XhrIoPoolTest');
goog.setTestOnly();

const StructsMap = goog.require('goog.structs.Map');
const XhrIoPool = goog.require('goog.net.XhrIoPool');
const dispose = goog.require('goog.dispose');
const testSuite = goog.require('goog.testing.testSuite');

const headers = new StructsMap();
headers.set('X-Foo', 'Bar');

testSuite({
  testSetHeadersForNewPoolObjects() {
    const xhrIoPool = new XhrIoPool(headers, 0);
    const xhrIo = xhrIoPool.getObject();

    assertEquals('Request should contain 1 header', 1, xhrIo.headers.size);
    assertTrue(
        'Request should contain right header key', xhrIo.headers.has('X-Foo'));
    assertEquals(
        'Request should contain right header value', xhrIo.headers.get('X-Foo'),
        'Bar');

    xhrIoPool.releaseObject(xhrIo);
    dispose(xhrIoPool);
  },

  testSetHeadersForInitializedPoolObjects() {
    const xhrIoPool = new XhrIoPool(headers, 1, 1);
    const xhrIo = xhrIoPool.getObject();

    assertEquals('Request should contain 1 header', 1, xhrIo.headers.size);
    assertTrue(
        'Request should contain right header key', xhrIo.headers.has('X-Foo'));
    assertEquals(
        'Request should contain right header value', 'Bar',
        xhrIo.headers.get('X-Foo'));

    xhrIoPool.releaseObject(xhrIo);
    dispose(xhrIoPool);
  },

  testSetCredentials() {
    const xhrIoPool = new XhrIoPool(
        undefined /* opt_headers */, undefined /* opt_minCount */,
        undefined /* opt_maxCount */, true /* opt_withCredentials */);
    const xhrIo = xhrIoPool.getObject();

    assertTrue(
        'withCredentials should be set on a request object',
        xhrIo.getWithCredentials());

    xhrIoPool.releaseObject(xhrIo);
    dispose(xhrIoPool);
  },
});
