/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.CrossDomainRpcTest');
goog.setTestOnly();

const CrossDomainRpc = goog.require('goog.net.CrossDomainRpc');
const GoogPromise = goog.require('goog.Promise');
const TestCase = goog.require('goog.testing.TestCase');
const log = goog.require('goog.log');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

function print(o) {
  if (Object.prototype.toSource) {
    return o.toSource();
  } else {
    const fragments = [];
    fragments.push('{');
    let first = true;
    for (let p in o) {
      if (!first) fragments.push(',');
      fragments.push(p);
      fragments.push(':"');
      fragments.push(o[p]);
      fragments.push('"');
      first = false;
    }
    return fragments.join('');
  }
}

testSuite({
  setUpPage() {
    TestCase.getActiveTestCase().promiseTimeout = 20000;  // 20s
  },

  testNormalRequest() {
    const start = Date.now();
    return new GoogPromise((resolve, reject) => {
             CrossDomainRpc.send(
                 'crossdomainrpc_test_response.html', resolve, 'POST',
                 {xyz: '01234567891123456789'});
           })
        .then(/**
                 @suppress {visibility,strictMissingProperties,checkTypes}
                 suppression added to enable type checking
               */
              (e) => {
                if (e.target.status < 300) {
                  const elapsed = Date.now() - start;
                  const responseData = eval(e.target.responseText);
                  log.fine(
                      CrossDomainRpc.logger_,
                      `${elapsed}ms: [` + responseData.result.length + '] ' +
                          print(responseData));
                  assertEquals(16 * 1024, responseData.result.length);
                  assertEquals(123, e.target.status);
                  assertEquals(1, e.target.responseHeaders.a);
                  assertEquals('2', e.target.responseHeaders.b);
                } else {
                  log.fine(CrossDomainRpc.logger_, print(e));
                  fail();
                }
              });
  },

  testErrorRequest() {
    // Firefox and Safari do not give a valid error event.
    if (userAgent.GECKO || product.SAFARI) {
      return;
    }

    return new GoogPromise((resolve, reject) => {
             CrossDomainRpc.send(
                 'http://hoodjimcwaadji.google.com/index.html', resolve, 'POST',
                 {xyz: '01234567891123456789'});
             setTimeout(() => {
               reject('CrossDomainRpc.send did not complete within 4000ms');
             }, 4000);
           })
        .then(/**
                 @suppress {visibility} suppression added to enable type
                 checking
               */
              (e) => {
                if (e.target.status < 300) {
                  fail('should have failed requesting a non-existent URI');
                } else {
                  log.fine(
                      CrossDomainRpc.logger_,
                      'expected error seen; event=' + print(e));
                }
              });
  },

  testGetDummyResourceUri() {
    /** @suppress {visibility} suppression added to enable type checking */
    const url = CrossDomainRpc.getDummyResourceUri_();
    assertTrue(
        'dummy resource URL should not contain "?"', url.indexOf('?') < 0);
    assertTrue(
        'dummy resource URL should not contain "#"', url.indexOf('#') < 0);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveHash() {
    assertEquals('abc', CrossDomainRpc.removeHash_('abc#123'));
    assertEquals('abc', CrossDomainRpc.removeHash_('abc#12#3'));
  },
});
