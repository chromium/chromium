/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.userAgent.keyboardTest');
goog.setTestOnly();

const MockUserAgent = goog.require('goog.testing.MockUserAgent');
const keyboard = goog.require('goog.userAgent.keyboard');
const testAgents = goog.require('goog.labs.userAgent.testAgents');
const testSuite = goog.require('goog.testing.testSuite');
const userAgentTestUtil = goog.require('goog.userAgentTestUtil');
const util = goog.require('goog.labs.userAgent.util');

let mockAgent;

function setUserAgent(ua) {
  mockAgent.setUserAgentString(ua);
  util.setUserAgent(ua);
  userAgentTestUtil.reinitializeUserAgent();
}
testSuite({
  setUp() {
    mockAgent = new MockUserAgent();
    mockAgent.install();
  },

  tearDown() {
    mockAgent.dispose();
    util.setUserAgent(null);
    userAgentTestUtil.reinitializeUserAgent();
  },

  testAndroid() {
    mockAgent.setNavigator({platform: 'Linux'});

    setUserAgent(testAgents.ANDROID_BROWSER_235);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.ANDROID_BROWSER_221);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.ANDROID_BROWSER_233);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.ANDROID_BROWSER_403);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.ANDROID_BROWSER_403_ALT);
    assertFalse(keyboard.MAC_KEYBOARD);
  },

  testIe() {
    mockAgent.setNavigator({platform: 'Windows'});

    setUserAgent(testAgents.IE_6);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IE_7);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IE_8);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IE_8_COMPATIBILITY);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IE_9);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IE_10);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IE_10_COMPATIBILITY);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IE_11);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IE_11_COMPATIBILITY_MSIE_7);
    assertFalse(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IE_11_COMPATIBILITY_MSIE_9);
    assertFalse(keyboard.MAC_KEYBOARD);
  },

  testFirefoxMac() {
    mockAgent.setNavigator({platform: 'Macintosh'});
    setUserAgent(testAgents.FIREFOX_MAC);
    assertTrue(keyboard.MAC_KEYBOARD);
  },

  testFirefoxNotMac() {
    mockAgent.setNavigator({platform: 'X11'});
    setUserAgent(testAgents.FIREFOX_LINUX);
    assertFalse(keyboard.MAC_KEYBOARD);

    mockAgent.setNavigator({platform: 'Windows'});
    setUserAgent(testAgents.FIREFOX_WINDOWS);
    assertFalse(keyboard.MAC_KEYBOARD);
  },

  testSafari() {
    mockAgent.setNavigator({platform: 'Macintosh'});
    setUserAgent(testAgents.SAFARI_6);
    assertTrue(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.SAFARI_MAC);
    assertTrue(keyboard.MAC_KEYBOARD);

    mockAgent.setNavigator({platform: 'iPhone'});
    setUserAgent(testAgents.SAFARI_IPHONE_32);
    assertTrue(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.SAFARI_IPHONE_421);
    assertTrue(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.SAFARI_IPHONE_431);
    assertTrue(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.SAFARI_IPHONE_6);
    assertTrue(keyboard.MAC_KEYBOARD);

    mockAgent.setNavigator({platform: 'iPod'});
    setUserAgent(testAgents.SAFARI_IPOD);
    assertTrue(keyboard.MAC_KEYBOARD);
  },

  testSafariWndows() {
    mockAgent.setNavigator({platform: 'Macintosh'});
    setUserAgent(testAgents.SAFARI_WINDOWS);
    assertFalse(keyboard.MAC_KEYBOARD);
  },

  testOperaMac() {
    mockAgent.setNavigator({platform: 'Macintosh'});
    setUserAgent(testAgents.OPERA_MAC);
    assertTrue(keyboard.MAC_KEYBOARD);
  },

  testOperaNonMac() {
    mockAgent.setNavigator({platform: 'X11'});
    setUserAgent(testAgents.OPERA_LINUX);
    assertFalse(keyboard.MAC_KEYBOARD);

    mockAgent.setNavigator({platform: 'Windows'});
    setUserAgent(testAgents.OPERA_15);
    assertFalse(keyboard.MAC_KEYBOARD);
  },

  testIPad() {
    mockAgent.setNavigator({platform: 'iPad'});
    setUserAgent(testAgents.IPAD_4);
    assertTrue(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IPAD_5);
    assertTrue(keyboard.MAC_KEYBOARD);

    setUserAgent(testAgents.IPAD_6);
    assertTrue(keyboard.MAC_KEYBOARD);
  },

  testChromeMac() {
    mockAgent.setNavigator({platform: 'Macintosh'});
    setUserAgent(testAgents.CHROME_MAC);
    assertTrue(keyboard.MAC_KEYBOARD);

    mockAgent.setNavigator({platform: 'iPhone'});
    setUserAgent(testAgents.CHROME_IPHONE);
    assertTrue(keyboard.MAC_KEYBOARD);
  },

  testChromeNonMac() {
    mockAgent.setNavigator({platform: 'Linux'});
    setUserAgent(testAgents.CHROME_ANDROID);
    assertFalse(keyboard.MAC_KEYBOARD);

    mockAgent.setNavigator({platform: 'X11'});
    setUserAgent(testAgents.CHROME_OS);
    assertFalse(keyboard.MAC_KEYBOARD);

    mockAgent.setNavigator({platform: 'X11'});
    setUserAgent(testAgents.CHROME_LINUX);
    assertFalse(keyboard.MAC_KEYBOARD);

    mockAgent.setNavigator({platform: 'Windows'});
    setUserAgent(testAgents.CHROME_25);

    assertFalse(keyboard.MAC_KEYBOARD);
  },
});
