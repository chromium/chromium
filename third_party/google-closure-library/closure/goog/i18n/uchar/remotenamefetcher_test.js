/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} Swapping XhrIo
 */
goog.module('goog.i18n.uChar.RemoteNameFetcherTest');
goog.setTestOnly();

const NetXhrIo = goog.require('goog.testing.net.XhrIo');
const RemoteNameFetcher = goog.require('goog.i18n.uChar.RemoteNameFetcher');
const XhrIo = goog.require('goog.net.XhrIo');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let nameFetcher = null;

testSuite({
  setUp() {
    // This will fail when XhrIo is converted to goog.module.
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.net.XhrIo = NetXhrIo;
    nameFetcher = new RemoteNameFetcher('http://www.example.com');
  },

  tearDown() {
    nameFetcher.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetName_remote() {
    const callback =
        recordFunction(/**
                          @suppress {visibility} suppression added to enable
                          type checking
                        */
                       (name) => {
                         assertEquals('Latin Capital Letter P', name);
                         assertTrue(nameFetcher.charNames_.has('50'));
                       });
    nameFetcher.getName('P', callback);
    const responseJsonText = '{"50":{"name":"Latin Capital Letter P"}}';
    nameFetcher.getNameXhrIo_.simulateResponse(200, responseJsonText);
    assertEquals(1, callback.getCallCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetName_existing() {
    nameFetcher.charNames_.set('1049d', 'OSYMANYA LETTER OO');
    const callback = recordFunction((name) => {
      assertEquals('OSYMANYA LETTER OO', name);
    });
    nameFetcher.getName('\uD801\uDC9D', callback);
    assertEquals(1, callback.getCallCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetName_fail() {
    const callback = recordFunction((name) => {
      assertNull(name);
    });
    nameFetcher.getName('\uD801\uDC9D', callback);
    assertEquals(
        'http://www.example.com?c=1049d&p=name',
        nameFetcher.getNameXhrIo_.getLastUri().toString());
    nameFetcher.getNameXhrIo_.simulateResponse(400);
    assertEquals(1, callback.getCallCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetName_abort() {
    const callback1 = recordFunction((name) => {
      assertNull(name);
    });
    nameFetcher.getName('I', callback1);
    const callback2 = recordFunction((name) => {
      assertEquals(name, 'LATIN SMALL LETTER Y');
    });
    nameFetcher.getName('ÿ', callback2);
    assertEquals(
        'http://www.example.com?c=ff&p=name',
        nameFetcher.getNameXhrIo_.getLastUri().toString());
    const responseJsonText = '{"ff":{"name":"LATIN SMALL LETTER Y"}}';
    nameFetcher.getNameXhrIo_.simulateResponse(200, responseJsonText);
    assertEquals(1, callback1.getCallCount());
    assertEquals(1, callback2.getCallCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPrefetch() {
    nameFetcher.prefetch('ÿI\uD801\uDC9D');
    assertEquals(
        'http://www.example.com?b88=%C3%BFI%F0%90%92%9D&p=name',
        nameFetcher.prefetchXhrIo_.getLastUri().toString());

    const responseJsonText = '{"ff":{"name":"LATIN SMALL LETTER Y"},"49":{' +
        '"name":"LATIN CAPITAL LETTER I"}, "1049d":{"name":"OSMYANA OO"}}';
    nameFetcher.prefetchXhrIo_.simulateResponse(200, responseJsonText);

    assertEquals(3, nameFetcher.charNames_.size);
    assertEquals('LATIN SMALL LETTER Y', nameFetcher.charNames_.get('ff'));
    assertEquals('LATIN CAPITAL LETTER I', nameFetcher.charNames_.get('49'));
    assertEquals('OSMYANA OO', nameFetcher.charNames_.get('1049d'));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPrefetch_abort() {
    nameFetcher.prefetch('I\uD801\uDC9D');
    nameFetcher.prefetch('ÿ');
    assertEquals(
        'http://www.example.com?b88=%C3%BF&p=name',
        nameFetcher.prefetchXhrIo_.getLastUri().toString());
  },

  testIsNameAvailable() {
    assertTrue(nameFetcher.isNameAvailable('a'));
  },
});
