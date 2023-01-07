/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for engine. */

goog.module('goog.labs.userAgent.engineTest');
goog.setTestOnly();

const engine = goog.require('goog.labs.userAgent.engine');
const testAgents = goog.require('goog.labs.userAgent.testAgents');
const testSuite = goog.require('goog.testing.testSuite');
const util = goog.require('goog.labs.userAgent.util');

function assertVersion(version) {
  assertEquals(version, engine.getVersion());
}

function assertLowAndHighVersions(lowVersion, highVersion) {
  assertTrue(engine.isVersionOrHigher(lowVersion));
  assertFalse(engine.isVersionOrHigher(highVersion));
}

testSuite({
  setUp() {
    util.setUserAgent(null);
  },

  testPresto() {
    util.setUserAgent(testAgents.OPERA_LINUX);
    assertTrue(engine.isPresto());
    assertFalse(engine.isGecko());
    assertVersion('2.9.168');
    assertLowAndHighVersions('2.9', '2.10');

    util.setUserAgent(testAgents.OPERA_MAC);
    assertTrue(engine.isPresto());
    assertFalse(engine.isGecko());
    assertVersion('2.9.168');
    assertLowAndHighVersions('2.9', '2.10');

    util.setUserAgent(testAgents.OPERA_MINI);
    assertTrue(engine.isPresto());
    assertFalse(engine.isGecko());
    assertVersion('2.8.119');
    assertLowAndHighVersions('2.8', '2.9');
  },

  testTrident() {
    util.setUserAgent(testAgents.IE_6);
    assertTrue(engine.isTrident());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('');

    util.setUserAgent(testAgents.IE_10);
    assertTrue(engine.isTrident());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('6.0');
    assertLowAndHighVersions('6.0', '7.0');

    util.setUserAgent(testAgents.IE_8);
    assertTrue(engine.isTrident());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('4.0');
    assertLowAndHighVersions('4.0', '5.0');

    util.setUserAgent(testAgents.IE_9_COMPATIBILITY);
    assertTrue(engine.isTrident());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('5.0');
    assertLowAndHighVersions('5.0', '6.0');

    util.setUserAgent(testAgents.IE_11);
    assertTrue(engine.isTrident());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('7.0');
    assertLowAndHighVersions('6.0', '8.0');

    util.setUserAgent(testAgents.IE_10_MOBILE);
    assertTrue(engine.isTrident());
    assertFalse(engine.isEdge());
    assertVersion('6.0');
  },

  testEdge() {
    util.setUserAgent(testAgents.EDGE_12_0);
    assertTrue(engine.isEdge());
    assertFalse(engine.isTrident());
    assertFalse(engine.isGecko());
    assertVersion('12.0');
    assertLowAndHighVersions('11.0', '13.0');
  },

  testWebKit() {
    util.setUserAgent(testAgents.ANDROID_BROWSER_235);
    assertTrue(engine.isWebKit());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('533.1');
    assertLowAndHighVersions('533.0', '534.0');

    util.setUserAgent(testAgents.ANDROID_BROWSER_403_ALT);
    assertTrue(engine.isWebKit());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('534.30');
    assertLowAndHighVersions('533.0', '535.0');

    util.setUserAgent(testAgents.CHROME_25);
    assertTrue(engine.isWebKit());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('535.8');
    assertLowAndHighVersions('535.0', '536.0');

    util.setUserAgent(testAgents.SAFARI_6);
    assertTrue(engine.isWebKit());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('536.25');
    assertLowAndHighVersions('536.0', '537.0');

    util.setUserAgent(testAgents.SAFARI_IPHONE_6);
    assertTrue(engine.isWebKit());
    assertFalse(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('536.26');
    assertLowAndHighVersions('536.0', '537.0');
  },

  testOpera15() {
    util.setUserAgent(testAgents.OPERA_15);
    assertTrue(engine.isWebKit());
    assertFalse(engine.isPresto());
    assertVersion('537.36');
  },

  testGecko() {
    util.setUserAgent(testAgents.FIREFOX_LINUX);
    assertTrue(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('15.0.1');
    assertLowAndHighVersions('14.0', '16.0');

    util.setUserAgent(testAgents.FIREFOX_19);
    assertTrue(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('19.0');
    assertLowAndHighVersions('18.0', '20.0');

    util.setUserAgent(testAgents.FIREFOX_WINDOWS);
    assertTrue(engine.isGecko());
    assertFalse(engine.isEdge());
    assertVersion('14.0.1');
    assertLowAndHighVersions('14.0', '15.0');
  },
});
