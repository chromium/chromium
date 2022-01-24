/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for device. */

goog.module('goog.labs.userAgent.deviceTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const device = goog.require('goog.labs.userAgent.device');
const functions = goog.require('goog.functions');
const testAgents = goog.require('goog.labs.userAgent.testAgents');
const testSuite = goog.require('goog.testing.testSuite');
const util = goog.require('goog.labs.userAgent.util');

const stubs = new PropertyReplacer();

/**
 * @param {?string} uaString
 * @param {?NavigatorUAData=} uaData
 */
function assertIsMobile(uaString, uaData) {
  util.setUserAgent(uaString);
  stubs.set(util, 'getUserAgentData', functions.constant(uaData || null));
  assertTrue(device.isMobile());
  assertFalse(device.isTablet());
  assertFalse(device.isDesktop());
}

/**
 * @param {?string} uaString
 * @param {?NavigatorUAData=} uaData
 */
function assertIsTablet(uaString, uaData) {
  util.setUserAgent(uaString);
  stubs.set(util, 'getUserAgentData', functions.constant(uaData || null));
  assertTrue(device.isTablet());
  assertFalse(device.isMobile());
  assertFalse(device.isDesktop());
}

/**
 * @param {?string} uaString
 * @param {?NavigatorUAData=} uaData
 */
function assertIsDesktop(uaString, uaData) {
  util.setUserAgent(uaString);
  stubs.set(util, 'getUserAgentData', functions.constant(uaData || null));
  assertTrue(device.isDesktop());
  assertFalse(device.isMobile());
  assertFalse(device.isTablet());
}
testSuite({
  setUp() {
    util.setUserAgent(null);
  },

  testMobile() {
    assertIsMobile(testAgents.ANDROID_BROWSER_235);
    assertIsMobile(testAgents.CHROME_ANDROID);
    assertIsMobile(testAgents.SAFARI_IPHONE_6);
    assertIsMobile(testAgents.IE_10_MOBILE);
    assertIsMobile(null, testAgents.CHROME_USERAGENT_DATA_MOBILE);
  },

  testTablet() {
    assertIsTablet(testAgents.CHROME_ANDROID_TABLET);
    assertIsTablet(testAgents.KINDLE_FIRE);
    assertIsTablet(testAgents.IPAD_6);
    assertIsTablet(
        testAgents.CHROME_ANDROID_TABLET, testAgents.CHROME_USERAGENT_DATA);
    assertIsTablet(testAgents.KINDLE_FIRE, testAgents.CHROME_USERAGENT_DATA);
  },

  testDesktop() {
    assertIsDesktop(testAgents.CHROME_25);
    assertIsDesktop(testAgents.OPERA_10);
    assertIsDesktop(testAgents.FIREFOX_19);
    assertIsDesktop(testAgents.IE_9);
    assertIsDesktop(testAgents.IE_10);
    assertIsDesktop(testAgents.IE_11);
    assertIsDesktop(testAgents.CHROME_25, testAgents.CHROME_USERAGENT_DATA);
  },
});
