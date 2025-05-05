/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for userAgentPlatform. */

goog.module('goog.labs.userAgent.platformTest');
goog.setTestOnly();

const testAgentData = goog.require('goog.labs.userAgent.testAgentData');
const testAgents = goog.require('goog.labs.userAgent.testAgents');
const testSuite = goog.require('goog.testing.testSuite');
const userAgentPlatform = goog.require('goog.labs.userAgent.platform');
const util = goog.require('goog.labs.userAgent.util');
const {setUseClientHintsForTesting} = goog.require('goog.labs.userAgent');

/**
 * Asserts that getVersion correctly returns the given version.
 * @param {string} version
 */
function assertPreUachVersion(version) {
  assertEquals(version, userAgentPlatform.getVersion());
}

/**
 * Asserts that isVersionOrHigher correctly identifies that the platform version
 * is within the given range.
 * @param {string} lowVersion
 * @param {string} highVersion
 */
function assertPreUachVersionBetween(lowVersion, highVersion) {
  assertTrue(userAgentPlatform.isVersionOrHigher(lowVersion));
  assertFalse(userAgentPlatform.isVersionOrHigher(highVersion));
}

/**
 * Asserts that userAgentPlatform.version has not cached a value yet.
 */
function assertHighEntroyVersionIsntCached() {
  const platformVersion = userAgentPlatform.version;
  assertEquals(
      undefined, platformVersion.getIfLoaded()?.toVersionStringForLogging());
}

/**
 * Asserts that userAgentPlatform.version correctly matches the given version.
 * @param {string=} version
 */
async function assertHighEntropyVersion(version) {
  const platformVersion = userAgentPlatform.version;
  assertEquals(
      version, (await platformVersion.load()).toVersionStringForLogging());
  assertEquals(
      version, platformVersion.getIfLoaded()?.toVersionStringForLogging());
}

/**
 * Asserts that userAgentPlatform.version can be used to correctly identify that
 * the platform version is within the given range.
 * @param {string} lowVersion
 * @param {string} highVersion
 */
async function assertHighEntropyVersionBetween(lowVersion, highVersion) {
  const loadedPlatformVersion = await userAgentPlatform.version.load();
  assertNotNullNorUndefined(loadedPlatformVersion);
  assertTrue(loadedPlatformVersion.isAtLeast(lowVersion));
  assertFalse(loadedPlatformVersion.isAtLeast(highVersion));
}

testSuite({
  setUp() {
    // setUserAgent uses the browser's original user agent string if null is
    // passed to it, so pass an empty string instead.
    util.setUserAgent('');
    util.setUserAgentData(null);
    setUseClientHintsForTesting(false);
    userAgentPlatform.version.resetForTesting();
  },

  async testAndroid233() {
    util.setUserAgent(testAgents.ANDROID_BROWSER_233);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isAndroid());

    assertPreUachVersion('2.3.3');
    assertPreUachVersionBetween('2.3.0', '2.3.5');
    // Using the High-entropy APIs in UACH-fallback mode, 'load' must be called
    // at least once for it to return a version, even if the fallback could be
    // synchronously accessed.
    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('2.3.3');
    await assertHighEntropyVersionBetween('2.3.0', '2.3.5');
    await assertHighEntropyVersionBetween('2.3', '2.4');
    await assertHighEntropyVersionBetween('2', '3');
  },

  async testAndroid221() {
    util.setUserAgent(testAgents.ANDROID_BROWSER_221);
    assertNull(util.getUserAgentData());
    assertTrue(userAgentPlatform.isAndroid());

    assertPreUachVersion('2.2.1');
    assertPreUachVersionBetween('2.2.0', '2.2.5');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('2.2.1');
    await assertHighEntropyVersionBetween('2.2.0', '2.2.5');
    await assertHighEntropyVersionBetween('2.2', '2.3');
    await assertHighEntropyVersionBetween('2', '3');
  },

  async testChromeAndroid() {
    util.setUserAgent(testAgents.CHROME_ANDROID);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isAndroid());

    assertPreUachVersion('4.0.2');
    assertPreUachVersionBetween('4.0.0', '4.1.0');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('4.0.2');
    await assertHighEntropyVersionBetween('4.0.0', '4.1.0');
    await assertHighEntropyVersionBetween('4.0', '4.1');
    await assertHighEntropyVersionBetween('4', '5');
  },

  async testKindleFire() {
    util.setUserAgent(testAgents.KINDLE_FIRE);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isAndroid());

    assertPreUachVersion('4.0.3');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('4.0.3');
  },

  async testIpod() {
    util.setUserAgent(testAgents.SAFARI_IPOD);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIpod());
    assertTrue(userAgentPlatform.isIos());

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('');
  },

  async testIphone421() {
    util.setUserAgent(testAgents.SAFARI_IPHONE_421);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('4.2.1');
    assertPreUachVersionBetween('4', '5');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('4.2.1');
    await assertHighEntropyVersionBetween('4', '5');
    await assertHighEntropyVersionBetween('4.2', '4.3');
  },

  async testIphone6() {
    util.setUserAgent(testAgents.SAFARI_IPHONE_6);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('6.0');
    assertPreUachVersionBetween('5', '7');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('6.0');
    await assertHighEntropyVersionBetween('5', '7');
  },

  async testIphoneIos14() {
    util.setUserAgent(testAgents.SAFARI_IPHONE_IOS_14);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('14.6');
    assertPreUachVersionBetween('14', '15');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('14.6');
    await assertHighEntropyVersionBetween('14', '15');
  },

  async testIphoneIos15() {
    util.setUserAgent(testAgents.SAFARI_IPHONE_IOS_15);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('15.0');
    assertPreUachVersionBetween('15', '16');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('15.0');
    await assertHighEntropyVersionBetween('15', '16');
  },

  async testIphone32() {
    util.setUserAgent(testAgents.SAFARI_IPHONE_32);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('3.2');
    assertPreUachVersionBetween('3', '4');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('3.2');
    await assertHighEntropyVersionBetween('3', '4');
  },

  async testWebviewIpad() {
    util.setUserAgent(testAgents.WEBVIEW_IPAD);
    assertNull(util.getUserAgentData());

    assertFalse(userAgentPlatform.isIphone());
    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('6.0');
    assertPreUachVersionBetween('5', '7');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('6.0');
    await assertHighEntropyVersionBetween('5', '7');
  },

  async testFirefoxIphone() {
    util.setUserAgent(testAgents.FIREFOX_IPHONE);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIphone());
    assertFalse(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('5.1.1');
    assertPreUachVersionBetween('4', '6');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('5.1.1');
    await assertHighEntropyVersionBetween('4', '6');
  },

  async testIpad() {
    util.setUserAgent(testAgents.IPAD_4);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('3.2');
    assertPreUachVersionBetween('3', '4');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('3.2');
    await assertHighEntropyVersionBetween('3', '4');
    await assertHighEntropyVersionBetween('3.1', '4');
  },

  async testIpad5() {
    util.setUserAgent(testAgents.IPAD_5);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('5.1');
    assertPreUachVersionBetween('5', '6');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('5.1');
    await assertHighEntropyVersionBetween('5', '6');
  },

  async testIpad6() {
    util.setUserAgent(testAgents.IPAD_6);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());

    assertPreUachVersion('6.0');
    assertPreUachVersionBetween('5', '7');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('6.0');
    await assertHighEntropyVersionBetween('5', '7');
  },

  async testSafariDesktopIpadIos15() {
    util.setUserAgent(testAgents.SAFARI_DESKTOP_IPAD_IOS_15);
    assertNull(util.getUserAgentData());

    assertFalse(userAgentPlatform.isIpad());
    assertFalse(userAgentPlatform.isIos());
    assertTrue(userAgentPlatform.isMacintosh());
    // In Safari desktop mode, the OS version reported is Mac OS version.

    assertPreUachVersion('10.15.6');
    assertPreUachVersionBetween('10.15.6', '10.15.7');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('10.15.6');
    await assertHighEntropyVersionBetween('10.15.6', '10.15.7');
  },

  async testSafariMobileIpadIos15() {
    util.setUserAgent(testAgents.SAFARI_MOBILE_IPAD_IOS_15);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());
    assertFalse(userAgentPlatform.isMacintosh());

    assertPreUachVersion('15.0');
    assertPreUachVersionBetween('15.0', '15.1');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('15.0');
    await assertHighEntropyVersionBetween('15.0', '15.1');
  },

  async testChromeIpadIos15() {
    util.setUserAgent(testAgents.CHROME_IPAD_IOS_15);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isIpad());
    assertTrue(userAgentPlatform.isIos());
    assertFalse(userAgentPlatform.isMacintosh());

    assertPreUachVersion('15.0');
    assertPreUachVersionBetween('15.0', '15.1');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('15.0');
    await assertHighEntropyVersionBetween('15.0', '15.1');
  },

  async testChromeMac() {
    util.setUserAgent(testAgents.CHROME_MAC);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isMacintosh());

    assertPreUachVersion('10.8.2');
    assertPreUachVersionBetween('10', '11');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('10.8.2');
    await assertHighEntropyVersionBetween('10', '11');
    await assertHighEntropyVersionBetween('10.8', '10.9');
    await assertHighEntropyVersionBetween('10.8.1', '10.8.3');
  },

  async testOperaMac() {
    util.setUserAgent(testAgents.OPERA_MAC);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isMacintosh());

    assertPreUachVersion('10.6.8');
    assertPreUachVersionBetween('10', '11');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('10.6.8');
    await assertHighEntropyVersionBetween('10', '11');
    await assertHighEntropyVersionBetween('10.6', '10.7');
    await assertHighEntropyVersionBetween('10.6.5', '10.7.0');
  },

  async testSafariMac() {
    util.setUserAgent(testAgents.SAFARI_MAC);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isMacintosh());
    assertPreUachVersionBetween('10', '11');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('10.6.8');
    await assertHighEntropyVersionBetween('10', '11');
    await assertHighEntropyVersionBetween('10.6', '10.7');
    await assertHighEntropyVersionBetween('10.6.5', '10.7.0');
  },

  async testFirefoxMac() {
    util.setUserAgent(testAgents.FIREFOX_MAC);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isMacintosh());

    assertPreUachVersion('11.7.9');
    assertPreUachVersionBetween('11', '12');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('11.7.9');
    await assertHighEntropyVersionBetween('11', '12');
    await assertHighEntropyVersionBetween('11.7', '11.8');
    await assertHighEntropyVersionBetween('11.7.9', '11.8.0');
  },

  async testSafariMacOsBigSur() {
    util.setUserAgent(testAgents.SAFARI_MAC_OS_BIG_SUR);
    assertNull(util.getUserAgentData());

    assertFalse(userAgentPlatform.isIpad());
    assertFalse(userAgentPlatform.isIos());
    assertTrue(userAgentPlatform.isMacintosh());

    assertPreUachVersion('10.15.7');
    assertPreUachVersionBetween('10.15.7', '10.15.8');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('10.15.7');
    await assertHighEntropyVersionBetween('10.15.7', '10.15.8');
  },

  async testFirefoxLinux() {
    util.setUserAgent(testAgents.FIREFOX_LINUX);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isLinux());

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('');
  },

  async testChromeLinux() {
    util.setUserAgent(testAgents.CHROME_LINUX);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isLinux());

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('');
  },

  async testOperaLinux() {
    util.setUserAgent(testAgents.OPERA_LINUX);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isLinux());

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('');
  },

  async testSafariWindows() {
    util.setUserAgent(testAgents.SAFARI_WINDOWS);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isWindows());

    assertPreUachVersion('6.1');
    assertPreUachVersionBetween('6', '7');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('6.1');
    await assertHighEntropyVersionBetween('6', '7');
  },

  async testIE10Windows() {
    util.setUserAgent(testAgents.IE_10);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isWindows());

    assertPreUachVersion('6.2');
    assertPreUachVersionBetween('6', '6.5');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('6.2');
    await assertHighEntropyVersionBetween('6', '6.5');
  },

  async testChrome25Windows() {
    util.setUserAgent(testAgents.CHROME_25);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isWindows());

    assertPreUachVersion('5.1');
    assertPreUachVersionBetween('5', '6');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('5.1');
    await assertHighEntropyVersionBetween('5', '6');
  },

  async testFirefoxWindows() {
    util.setUserAgent(testAgents.FIREFOX_WINDOWS);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isWindows());

    assertPreUachVersion('6.1');
    assertPreUachVersionBetween('6', '7');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('6.1');
    await assertHighEntropyVersionBetween('6', '7');
  },

  async testIE11Windows() {
    util.setUserAgent(testAgents.IE_11);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isWindows());

    assertPreUachVersion('6.3');
    assertPreUachVersionBetween('6', '6.5');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('6.3');
    await assertHighEntropyVersionBetween('6', '6.5');
  },

  async testIE10WindowsMobile() {
    util.setUserAgent(testAgents.IE_10_MOBILE);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isWindows());

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('8.0');
  },

  async testChromeOS910() {
    util.setUserAgent(testAgents.CHROME_OS_910);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isChromeOS());

    assertPreUachVersion('9.10.0');
    assertPreUachVersionBetween('9', '10');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('9.10.0');
    await assertHighEntropyVersionBetween('9', '10');
  },

  async testChromeOS() {
    util.setUserAgent(testAgents.CHROME_OS);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isChromeOS());

    assertPreUachVersion('3701.62.0');
    assertPreUachVersionBetween('3701', '3702');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('3701.62.0');
    await assertHighEntropyVersionBetween('3701', '3702');
  },

  async testChromecast() {
    util.setUserAgent(testAgents.CHROMECAST);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isChromecast());

    assertPreUachVersion('');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('');
  },

  async testKaiOS() {
    util.setUserAgent(testAgents.KAIOS);
    assertNull(util.getUserAgentData());

    assertTrue(userAgentPlatform.isKaiOS());

    assertPreUachVersion('2.5');

    assertHighEntroyVersionIsntCached();
    await assertHighEntropyVersion('2.5');
  },

  async testAndroidUserAgentData() {
    const uaData = testAgentData.withHighEntropyData(
        testAgentData.CHROME_USERAGENT_DATA_MOBILE, {
          platformVersion: '11.0.0',
        });
    util.setUserAgentData(uaData);
    await assertHighEntropyVersion('11.0.0');
  },

  async testAndroidUserAgentDataWithRejectedHighEntropyValues() {
    let uaData = testAgentData.CHROME_USERAGENT_DATA_MOBILE;
    util.setUserAgentData(uaData);
    assertEquals(undefined, userAgentPlatform.version.getIfLoaded());

    await assertRejects(userAgentPlatform.version.load());
    assertEquals(undefined, userAgentPlatform.version.getIfLoaded());
  },

  async testChromeOSUserAgentData() {
    const uaData = testAgentData.withHighEntropyData(
        testAgentData.CHROME_USERAGENT_DATA_CROS, {
          platformVersion: '14150.74.0',
        });
    util.setUserAgentData(uaData);

    setUseClientHintsForTesting(false);
    // No data, and the mocked UA string is wrong.
    assertFalse(userAgentPlatform.isChromeOS());

    setUseClientHintsForTesting(true);
    assertTrue(userAgentPlatform.isChromeOS());

    setUseClientHintsForTesting(false);
    await assertHighEntropyVersion('14150.74.0');
  },

  async testLinuxUserAgentData() {
    // Linux version should be an empty string.
    // See https://wicg.github.io/ua-client-hints/#sec-ch-ua-platform-version
    const uaData = testAgentData.withHighEntropyData(
        testAgentData.CHROME_USERAGENT_DATA_LINUX, {
          platformVersion: '',
        });
    util.setUserAgentData(uaData);

    setUseClientHintsForTesting(false);
    // No data, and the mocked UA string is wrong.
    assertFalse(userAgentPlatform.isLinux());

    setUseClientHintsForTesting(true);
    assertTrue(userAgentPlatform.isLinux());

    setUseClientHintsForTesting(false);
    await assertHighEntropyVersion('');
  },

  async testMacOSUserAgentData() {
    const uaData = testAgentData.withHighEntropyData(
        testAgentData.CHROME_USERAGENT_DATA_MACOS, {
          platformVersion: '11.6.0',
        });
    util.setUserAgentData(uaData);

    setUseClientHintsForTesting(false);
    // No data, and the mocked UA string is wrong.
    assertFalse(userAgentPlatform.isMacintosh());

    setUseClientHintsForTesting(true);
    assertTrue(userAgentPlatform.isMacintosh());

    setUseClientHintsForTesting(false);
    await assertHighEntropyVersion('11.6.0');
  },

  async testWindowsUserAgentData() {
    const uaData = testAgentData.withHighEntropyData(
        testAgentData.CHROME_USERAGENT_DATA_WINDOWS, {
          platformVersion: '10.0.0',
        });
    util.setUserAgentData(uaData);

    setUseClientHintsForTesting(false);
    assertFalse(userAgentPlatform.isWindows());

    setUseClientHintsForTesting(true);
    assertTrue(userAgentPlatform.isWindows());

    setUseClientHintsForTesting(false);
    await assertHighEntropyVersion('10.0.0');
  },
});
