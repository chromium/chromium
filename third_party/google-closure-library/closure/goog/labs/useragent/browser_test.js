/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for userAgentBrowser. */

goog.module('goog.labs.userAgent.browserTest');
goog.setTestOnly();

const googObject = goog.require('goog.object');
const testAgents = goog.require('goog.labs.userAgent.testAgents');
const testSuite = goog.require('goog.testing.testSuite');
const userAgentBrowser = goog.require('goog.labs.userAgent.browser');
const util = goog.require('goog.labs.userAgent.util');

/*
 * Map of browser name to checking method.
 * Used by assertBrowser() to verify that only one is true at a time.
 */
const Browser = {
  ANDROID_BROWSER: userAgentBrowser.isAndroidBrowser,
  CHROME: userAgentBrowser.isChrome,
  COAST: userAgentBrowser.isCoast,
  FIREFOX: userAgentBrowser.isFirefox,
  OPERA: userAgentBrowser.isOpera,
  IE: userAgentBrowser.isIE,
  IOS_WEBVIEW: userAgentBrowser.isIosWebview,
  SAFARI: userAgentBrowser.isSafari,
  SILK: userAgentBrowser.isSilk,
  EDGE: userAgentBrowser.isEdge
};

/*
 * Map of browser name to checking method.
 * Used by assertChromiumBrowser() to verify that only one is true at a time.
 */
const NonChromeChromiumBrowser = {
  EDGE_CHROMIUM: userAgentBrowser.isEdgeChromium,
  OPERA_CHROMIUM: userAgentBrowser.isOperaChromium
};

/*
 * Assert that the given browser is true and the others are false.
 */
function assertBrowser(browser) {
  assertTrue(
      'Supplied argument "browser" not in Browser object',
      googObject.containsValue(Browser, browser));

  // Verify that the method is true for the given browser
  // and false for all others.
  googObject.forEach(Browser, (f, name) => {
    if (f == browser) {
      assertTrue(`Value for browser ${name}`, f());
    } else {
      assertFalse('Value for browser ' + name, f());
    }
  });
}


/*
 * Assert that a given browser is a Chromium variant.
 */
function assertNonChromeChromiumBrowser(browser) {
  assertTrue(
      'Supplied argument "browser" not in ChromiumBrowser object',
      googObject.containsValue(NonChromeChromiumBrowser, browser));

  // Verify that the method is true for the given browser
  // and false for all others.
  googObject.forEach(NonChromeChromiumBrowser, (f, name) => {
    if (f == browser) {
      assertTrue(`Value for browser ${name}`, f());
    } else {
      assertFalse(`Value for browser ${name}`, f());
    }
  });
}

function assertVersion(version) {
  assertEquals(version, userAgentBrowser.getVersion());
}

function assertVersionBetween(lowVersion, highVersion) {
  assertTrue(userAgentBrowser.isVersionOrHigher(lowVersion));
  assertFalse(userAgentBrowser.isVersionOrHigher(highVersion));
}

testSuite({
  setUp() {
    util.setUserAgent(null);
    util.setUserAgentData(null);
  },

  testOpera10() {
    util.setUserAgent(testAgents.OPERA_10);
    assertBrowser(Browser.OPERA);
    assertVersion('10.00');
    assertVersionBetween('10.00', '10.10');
  },

  testOperaMac() {
    util.setUserAgent(testAgents.OPERA_MAC);
    assertBrowser(Browser.OPERA);
    assertVersion('11.52');
    assertVersionBetween('11.50', '12.00');
  },

  testOperaLinux() {
    util.setUserAgent(testAgents.OPERA_LINUX);
    assertBrowser(Browser.OPERA);
    assertVersion('11.50');
    assertVersionBetween('11.00', '12.00');
  },

  testOpera15() {
    util.setUserAgent(testAgents.OPERA_15);
    // Opera 15 is Chromium 28.  We treat all Chromium variants as Chrome.
    assertBrowser(Browser.CHROME);
    assertNonChromeChromiumBrowser(NonChromeChromiumBrowser.OPERA_CHROMIUM);
    assertVersion('28.0.1500.52');
    assertVersionBetween('28.00', '29.00');
  },

  testOperaMini() {
    util.setUserAgent(testAgents.OPERA_MINI);
    assertBrowser(Browser.OPERA);
    assertVersion('11.10');
    assertVersionBetween('11.00', '12.00');
  },

  testOperaChromiumUserAgentData() {
    util.setUserAgentData(testAgents.OPERACHROMIUM_USERAGENT_DATA);
    assertBrowser(Browser.CHROME);
    assertNonChromeChromiumBrowser(NonChromeChromiumBrowser.OPERA_CHROMIUM);
    assertFalse(userAgentBrowser.isOpera());
  },

  testIE6() {
    util.setUserAgent(testAgents.IE_6);
    assertBrowser(Browser.IE);
    assertVersion('6.0');
    assertVersionBetween('5.0', '7.0');
  },

  testIE7() {
    util.setUserAgent(testAgents.IE_7);
    assertBrowser(Browser.IE);
    assertVersion('7.0');
  },

  testIE8() {
    util.setUserAgent(testAgents.IE_8);
    assertBrowser(Browser.IE);
    assertVersion('8.0');
    assertVersionBetween('7.0', '9.0');
  },

  testIE8Compatibility() {
    util.setUserAgent(testAgents.IE_8_COMPATIBILITY);
    assertBrowser(Browser.IE);
    assertVersion('8.0');
  },

  testIE9() {
    util.setUserAgent(testAgents.IE_9);
    assertBrowser(Browser.IE);
    assertVersion('9.0');
    assertVersionBetween('8.0', '10.0');
  },

  testIE9Compatibility() {
    util.setUserAgent(testAgents.IE_9_COMPATIBILITY);
    assertBrowser(Browser.IE);
    assertVersion('9.0');
  },

  testIE10() {
    util.setUserAgent(testAgents.IE_10);
    assertBrowser(Browser.IE);
    assertVersion('10.0');
    assertVersionBetween('10.0', '11.0');
  },

  testIE10Compatibility() {
    util.setUserAgent(testAgents.IE_10_COMPATIBILITY);
    assertBrowser(Browser.IE);
    assertVersion('10.0');
  },

  testIE10Mobile() {
    util.setUserAgent(testAgents.IE_10_MOBILE);
    assertBrowser(Browser.IE);
    assertVersion('10.0');
  },

  testIE11() {
    util.setUserAgent(testAgents.IE_11);
    assertBrowser(Browser.IE);
    assertVersion('11.0');
    assertVersionBetween('10.0', '12.0');
  },

  testIE11CompatibilityMSIE7() {
    util.setUserAgent(testAgents.IE_11_COMPATIBILITY_MSIE_7);
    assertBrowser(Browser.IE);
    assertVersion('11.0');
  },

  testIE11CompatibilityMSIE9() {
    util.setUserAgent(testAgents.IE_11_COMPATIBILITY_MSIE_9);
    assertBrowser(Browser.IE);
    assertVersion('11.0');
  },

  testEdge120() {
    util.setUserAgent(testAgents.EDGE_12_0);
    assertBrowser(Browser.EDGE);
    assertVersion('12.0');
    assertVersionBetween('11.0', '13.0');
  },

  testEdge() {
    util.setUserAgent(testAgents.EDGE_12_9600);
    assertBrowser(Browser.EDGE);
    assertVersion('12.9600');
    assertVersionBetween('11.0', '13.0');
  },

  testEdgeChromium() {
    util.setUserAgent(testAgents.EDGE_CHROMIUM);
    assertBrowser(Browser.CHROME);
    assertNonChromeChromiumBrowser(NonChromeChromiumBrowser.EDGE_CHROMIUM);
    assertVersion('74.1.96.24');
    assertVersionBetween('74.1', '74.2');
  },

  testEdgeChromiumUserAgentData() {
    util.setUserAgentData(testAgents.EDGECHROMIUM_USERAGENT_DATA);
    assertBrowser(Browser.CHROME);
    assertNonChromeChromiumBrowser(NonChromeChromiumBrowser.EDGE_CHROMIUM);
    assertFalse(userAgentBrowser.isEdge());
  },

  testFirefox19() {
    util.setUserAgent(testAgents.FIREFOX_19);
    assertBrowser(Browser.FIREFOX);
    assertVersion('19.0');
    assertVersionBetween('18.0', '20.0');
  },

  testFirefoxWindows() {
    util.setUserAgent(testAgents.FIREFOX_WINDOWS);
    assertBrowser(Browser.FIREFOX);
    assertVersion('14.0.1');
    assertVersionBetween('14.0', '15.0');
  },

  testFirefoxLinux() {
    util.setUserAgent(testAgents.FIREFOX_LINUX);
    assertBrowser(Browser.FIREFOX);
    assertTrue(userAgentBrowser.isFirefox());
    assertVersion('15.0.1');
  },

  testFirefoxiOS() {
    util.setUserAgent(testAgents.FIREFOX_IPHONE);
    assertBrowser(Browser.FIREFOX);
    assertTrue(userAgentBrowser.isFirefox());
    assertFalse(userAgentBrowser.isSafari());
    assertVersion('1.0');
  },

  testChromeAndroid() {
    util.setUserAgent(testAgents.CHROME_ANDROID);
    assertBrowser(Browser.CHROME);
    assertTrue(userAgentBrowser.isChrome());
    assertVersion('18.0.1025.133');
    assertVersionBetween('18.0', '19.0');
    assertVersionBetween('17.0', '18.1');
  },

  testChromeHeadless() {
    util.setUserAgent(testAgents.CHROME_HEADLESS);
    assertBrowser(Browser.CHROME);
    assertTrue(userAgentBrowser.isChrome());
    assertVersion('79.0.3945.84');
    assertVersionBetween('78.0', '80.0');
    assertVersionBetween('79.0', '79.1');
  },

  testChromeIphone() {
    util.setUserAgent(testAgents.CHROME_IPHONE);
    assertBrowser(Browser.CHROME);
    assertTrue(userAgentBrowser.isChrome());
    assertVersion('22.0.1194.0');
    assertVersionBetween('22.0', '23.0');
    assertVersionBetween('22.0', '22.10');
  },

  testChromeIpad() {
    util.setUserAgent(testAgents.CHROME_IPAD);
    assertBrowser(Browser.CHROME);
    assertTrue(userAgentBrowser.isChrome());
    assertVersion('32.0.1700.20');
    assertVersionBetween('32.0', '33.0');
    assertVersionBetween('32.0', '32.10');
  },

  testChromeMac() {
    util.setUserAgent(testAgents.CHROME_MAC);
    assertBrowser(Browser.CHROME);
    assertTrue(userAgentBrowser.isChrome());
    assertVersion('24.0.1309.0');
    assertVersionBetween('24.0', '25.0');
    assertVersionBetween('24.0', '24.10');
  },

  testChromeUserAgentData() {
    util.setUserAgentData(testAgents.CHROME_USERAGENT_DATA);
    assertBrowser(Browser.CHROME);
    assertTrue(userAgentBrowser.isChrome());
  },

  testSafariIpad() {
    util.setUserAgent(testAgents.IPAD_6);
    assertBrowser(Browser.SAFARI);
    assertTrue(userAgentBrowser.isSafari());
    assertVersion('6.0');
    assertVersionBetween('5.1', '7.0');
  },

  testSafari6() {
    util.setUserAgent(testAgents.SAFARI_6);
    assertBrowser(Browser.SAFARI);
    assertTrue(userAgentBrowser.isSafari());
    assertVersion('6.0');
    assertVersionBetween('6.0', '7.0');
  },

  testSafariIphone() {
    util.setUserAgent(testAgents.SAFARI_IPHONE_6);
    assertBrowser(Browser.SAFARI);
    assertTrue(userAgentBrowser.isSafari());
    assertVersion('6.0');
    assertVersionBetween('5.0', '7.0');
  },

  testSafariOnIphoneIos14() {
    util.setUserAgent(testAgents.SAFARI_IPHONE_IOS_14);
    assertBrowser(Browser.SAFARI);
    assertTrue(userAgentBrowser.isSafari());
    assertVersion('14.1.1');
    assertVersionBetween('14.0', '15.0');
  },

  testSafariOnIphoneIos15() {
    util.setUserAgent(testAgents.SAFARI_IPHONE_IOS_15);
    assertBrowser(Browser.SAFARI);
    assertTrue(userAgentBrowser.isSafari());
    assertVersion('15.0');
    assertVersionBetween('15.0', '16.0');
  },

  testSafariDesktopOnIpadIos15() {
    util.setUserAgent(testAgents.SAFARI_DESKTOP_IPAD_IOS_15);
    assertBrowser(Browser.SAFARI);
    assertTrue(userAgentBrowser.isSafari());
    assertVersion('15.0');
    assertVersionBetween('15.0', '16.0');
  },

  testSafariMobileOnIpadIos15() {
    util.setUserAgent(testAgents.SAFARI_MOBILE_IPAD_IOS_15);
    assertBrowser(Browser.SAFARI);
    assertTrue(userAgentBrowser.isSafari());
    assertVersion('15.0');
    assertVersionBetween('15.0', '16.0');
  },

  testSafariOnMacOsBigSur() {
    util.setUserAgent(testAgents.SAFARI_MAC_OS_BIG_SUR);
    assertBrowser(Browser.SAFARI);
    assertTrue(userAgentBrowser.isSafari());
    assertVersion('14.1.2');
    assertVersionBetween('14.1', '14.2');
  },

  testCoast() {
    util.setUserAgent(testAgents.COAST);
    assertBrowser(Browser.COAST);
  },

  testWebviewIOS() {
    util.setUserAgent(testAgents.WEBVIEW_IPHONE);
    assertBrowser(Browser.IOS_WEBVIEW);
    util.setUserAgent(testAgents.WEBVIEW_IPAD);
    assertBrowser(Browser.IOS_WEBVIEW);
  },

  testAndroidBrowser235() {
    util.setUserAgent(testAgents.ANDROID_BROWSER_235);
    assertBrowser(Browser.ANDROID_BROWSER);
    assertVersion('4.0');
    assertVersionBetween('3.0', '5.0');
  },

  testAndroidBrowser403() {
    util.setUserAgent(testAgents.ANDROID_BROWSER_403);
    assertBrowser(Browser.ANDROID_BROWSER);
    assertVersion('4.0');
    assertVersionBetween('3.0', '5.0');
  },

  testAndroidBrowser233() {
    util.setUserAgent(testAgents.ANDROID_BROWSER_233);
    assertBrowser(Browser.ANDROID_BROWSER);
    assertVersion('4.0');
    assertVersionBetween('3.0', '5.0');
  },

  testAndroidWebView411() {
    util.setUserAgent(testAgents.ANDROID_WEB_VIEW_4_1_1);
    assertBrowser(Browser.ANDROID_BROWSER);
    assertVersion('4.0');
    assertVersionBetween('3.0', '5.0');
  },

  testAndroidWebView44() {
    util.setUserAgent(testAgents.ANDROID_WEB_VIEW_4_4);
    assertBrowser(Browser.CHROME);
    assertVersion('30.0.0.0');
    assertVersionBetween('29.0', '31.0');
  },

  testSilk() {
    util.setUserAgent(testAgents.KINDLE_FIRE);
    assertBrowser(Browser.SILK);
    assertVersion('2.1');
  },

  testFirefoxOnAndroidTablet() {
    util.setUserAgent(testAgents.FIREFOX_ANDROID_TABLET);
    assertBrowser(Browser.FIREFOX);
    assertVersion('28.0');
    assertVersionBetween('27.0', '29.0');
  },

  testIncompleteUserAgentData() {
    util.setUserAgentData(testAgents.INCOMPLETE_USERAGENT_DATA);
    util.setUserAgent(testAgents.CHROME_HEADLESS);
    assertBrowser(Browser.CHROME);
    assertTrue(userAgentBrowser.isChrome());
  },
});
