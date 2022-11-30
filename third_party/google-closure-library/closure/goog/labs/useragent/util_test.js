/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for goog.labs.userAgent.engine. */

goog.module('goog.labs.userAgent.utilTest');
goog.setTestOnly();

const testAgents = goog.require('goog.labs.userAgent.testAgents');
const testSuite = goog.require('goog.testing.testSuite');
const util = goog.require('goog.labs.userAgent.util');

testSuite({
  tearDown() {
    util.resetUserAgentData();
  },

  /** Tests parsing a few example UA strings. */
  testExtractVersionTuples() {
    // Old Android
    let tuples = util.extractVersionTuples(testAgents.ANDROID_BROWSER_235);

    assertEquals(4, tuples.length);
    assertSameElements(
        [
          'Mozilla',
          '5.0',
          'Linux; U; Android 2.3.5; en-us; HTC Vision Build/GRI40',
        ],
        tuples[0]);
    assertSameElements(
        ['AppleWebKit', '533.1', 'KHTML, like Gecko'], tuples[1]);
    assertSameElements(['Version', '4.0', undefined], tuples[2]);
    assertSameElements(['Mobile Safari', '533.1', undefined], tuples[3]);

    // IE 9
    tuples = util.extractVersionTuples(testAgents.IE_9);
    assertEquals(1, tuples.length);
    assertSameElements(
        ['Mozilla', '5.0', 'compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0'],
        tuples[0]);

    // Opera
    tuples = util.extractVersionTuples(testAgents.OPERA_10);
    assertEquals(3, tuples.length);
    assertSameElements(
        ['Opera', '9.80', 'S60; SymbOS; Opera Mobi/447; U; en'], tuples[0]);
    assertSameElements(['Presto', '2.4.18', undefined], tuples[1]);
    assertSameElements(['Version', '10.00', undefined], tuples[2]);
  },

  testSetUserAgent() {
    const ua = 'Five Finger Death Punch';
    util.setUserAgent(ua);
    assertEquals(ua, util.getUserAgent());
    assertTrue(util.matchUserAgent('Punch'));
    assertFalse(util.matchUserAgent('punch'));
    assertFalse(util.matchUserAgent('Mozilla'));
  },

  testSetUserAgentIgnoreCase() {
    const ua = 'Five Finger Death Punch';
    util.setUserAgent(ua);
    assertEquals(ua, util.getUserAgent());
    assertTrue(util.matchUserAgentIgnoreCase('Punch'));
    assertTrue(util.matchUserAgentIgnoreCase('punch'));
    assertFalse(util.matchUserAgentIgnoreCase('Mozilla'));
  },
});
