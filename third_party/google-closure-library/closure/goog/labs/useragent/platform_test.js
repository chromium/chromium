/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for userAgentPlatform. */

goog.module('goog.labs.userAgent.platformTest');
goog.setTestOnly();

const testAgents = goog.require('goog.labs.userAgent.testAgents');
const testSuite = goog.require('goog.testing.testSuite');
const userAgentPlatform = goog.require('goog.labs.userAgent.platform');
const util = goog.require('goog.labs.userAgent.util');

function assertVersion(version) {
  assertEquals(version, userAgentPlatform.getVersion());
}

function assertVersionBetween(lowVersion, highVersion) {
  assertTrue(userAgentPlatform.isVersionOrHigher(lowVersion));
  assertFalse(userAgentPlatform.isVersionOrHigher(highVersion));
}
testSuite({
  setUp() {
    util.setUserAgent(null);
  },

  testAndroid() {
    let uaString = testAgents.ANDROID_BROWSER_233;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isAndroid());
    assertVersion('2.3.3');
    assertVersionBetween('2.3.0', '2.3.5');
    assertVersionBetween('2.3', '2.4');
    assertVersionBetween('2', '3');

    uaString = testAgents.ANDROID_BROWSER_221;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isAndroid());
    assertVersion('2.2.1');
    assertVersionBetween('2.2.0', '2.2.5');
    assertVersionBetween('2.2', '2.3');
    assertVersionBetween('2', '3');

    uaString = testAgents.CHROME_ANDROID;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isAndroid());
    assertVersion('4.0.2');
    assertVersionBetween('4.0.0', '4.1.0');
    assertVersionBetween('4.0', '4.1');
    assertVersionBetween('4', '5');
  },

  testKindleFire() {
    const uaString = testAgents.KINDLE_FIRE;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isAndroid());
    assertVersion('4.0.3');
  },

  testIpod() {
    const uaString = testAgents.SAFARI_IPOD;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIpod());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('');
  },

  testIphone() {
    let uaString = testAgents.SAFARI_IPHONE_421;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('4.2.1');
    assertVersionBetween('4', '5');
    assertVersionBetween('4.2', '4.3');

    uaString = testAgents.SAFARI_IPHONE_6;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('6.0');
    assertVersionBetween('5', '7');

    uaString = testAgents.SAFARI_IPHONE_IOS_14;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('14.6');
    assertVersionBetween('14', '15');

    uaString = testAgents.SAFARI_IPHONE_IOS_15;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('15.0');
    assertVersionBetween('15', '16');

    uaString = testAgents.SAFARI_IPHONE_32;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('3.2');
    assertVersionBetween('3', '4');

    uaString = testAgents.WEBVIEW_IPAD;
    util.setUserAgent(uaString);
    assertFalse(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('6.0');
    assertVersionBetween('5', '7');

    uaString = testAgents.FIREFOX_IPHONE;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIphone());
    assertFalse(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('5.1.1');
    assertVersionBetween('4', '6');
  },

  testIpad() {
    let uaString = testAgents.IPAD_4;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('3.2');
    assertVersionBetween('3', '4');
    assertVersionBetween('3.1', '4');

    uaString = testAgents.IPAD_5;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('5.1');
    assertVersionBetween('5', '6');

    uaString = testAgents.IPAD_6;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());
    assertVersion('6.0');
    assertVersionBetween('5', '7');

    uaString = testAgents.SAFARI_DESKTOP_IPAD_IOS_15;
    util.setUserAgent(uaString);
    assertFalse(userAgentPlatform.isIpad());
    assertFalse(userAgentPlatform.isIos());
    assertTrue(userAgentPlatform.isMacintosh());
    // In Safari desktop mode, the OS version reported is Mac OS version.
    assertVersion('10.15.6');
    assertVersionBetween('10.15.6', '10.15.7');

    uaString = testAgents.SAFARI_MOBILE_IPAD_IOS_15;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());
    assertFalse(userAgentPlatform.isMacintosh());
    assertVersion('15.0');
    assertVersionBetween('15.0', '15.1');

    uaString = testAgents.CHROME_IPAD_IOS_15;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());
    assertFalse(userAgentPlatform.isMacintosh());
    assertVersion('15.0');
    assertVersionBetween('15.0', '15.1');
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMac() {
    let uaString = testAgents.CHROME_MAC;
    const platform = 'IntelMac';
    util.setUserAgent(uaString, platform);
    assertTrue(userAgentPlatform.isMacintosh());
    assertVersion('10.8.2');
    assertVersionBetween('10', '11');
    assertVersionBetween('10.8', '10.9');
    assertVersionBetween('10.8.1', '10.8.3');

    uaString = testAgents.OPERA_MAC;
    util.setUserAgent(uaString, platform);
    assertTrue(userAgentPlatform.isMacintosh());
    assertVersion('10.6.8');
    assertVersionBetween('10', '11');
    assertVersionBetween('10.6', '10.7');
    assertVersionBetween('10.6.5', '10.7.0');

    uaString = testAgents.SAFARI_MAC;
    util.setUserAgent(uaString, platform);
    assertTrue(userAgentPlatform.isMacintosh());
    assertVersionBetween('10', '11');
    assertVersionBetween('10.6', '10.7');
    assertVersionBetween('10.6.5', '10.7.0');

    uaString = testAgents.FIREFOX_MAC;
    util.setUserAgent(uaString, platform);
    assertTrue(userAgentPlatform.isMacintosh());
    assertVersion('11.7.9');
    assertVersionBetween('11', '12');
    assertVersionBetween('11.7', '11.8');
    assertVersionBetween('11.7.9', '11.8.0');

    uaString = testAgents.SAFARI_MAC_OS_BIG_SUR;
    util.setUserAgent(uaString);
    assertFalse(userAgentPlatform.isIpad());
    assertFalse(userAgentPlatform.isIos());
    assertTrue(userAgentPlatform.isMacintosh());
    assertVersion('10.15.7');
    assertVersionBetween('10.15.7', '10.15.8');
  },

  testLinux() {
    let uaString = testAgents.FIREFOX_LINUX;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isLinux());
    assertVersion('');

    uaString = testAgents.CHROME_LINUX;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isLinux());
    assertVersion('');

    uaString = testAgents.OPERA_LINUX;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isLinux());
    assertVersion('');
  },

  testWindows() {
    let uaString = testAgents.SAFARI_WINDOWS;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isWindows());
    assertVersion('6.1');
    assertVersionBetween('6', '7');

    uaString = testAgents.IE_10;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isWindows());
    assertVersion('6.2');
    assertVersionBetween('6', '6.5');

    uaString = testAgents.CHROME_25;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isWindows());
    assertVersion('5.1');
    assertVersionBetween('5', '6');

    uaString = testAgents.FIREFOX_WINDOWS;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isWindows());
    assertVersion('6.1');
    assertVersionBetween('6', '7');

    uaString = testAgents.IE_11;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isWindows());
    assertVersion('6.3');
    assertVersionBetween('6', '6.5');

    uaString = testAgents.IE_10_MOBILE;
    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isWindows());
    assertVersion('8.0');
  },

  testChromeOS() {
    let uaString = testAgents.CHROME_OS_910;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isChromeOS());
    assertVersion('9.10.0');
    assertVersionBetween('9', '10');

    uaString = testAgents.CHROME_OS;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isChromeOS());
    assertVersion('3701.62.0');
    assertVersionBetween('3701', '3702');
  },

  testChromecast() {
    const uaString = testAgents.CHROMECAST;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isChromecast());
    assertVersion('');
  },

  testKaiOS() {
    const uaString = testAgents.KAIOS;

    util.setUserAgent(uaString);
    assertTrue(userAgentPlatform.isKaiOS());
    assertVersion('2.5');
  },
});
