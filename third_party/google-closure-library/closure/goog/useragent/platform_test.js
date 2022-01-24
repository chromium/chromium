/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.userAgent.platformTest');
goog.setTestOnly();

const MockUserAgent = goog.require('goog.testing.MockUserAgent');
const platform = goog.require('goog.userAgent.platform');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');
const userAgentTestUtil = goog.require('goog.userAgentTestUtil');

let mockAgent;

function updateUserAgentUtils() {
  userAgentTestUtil.reinitializeUserAgent();
}

testSuite({
  setUp() {
    mockAgent = new MockUserAgent();
    mockAgent.install();
  },

  tearDown() {
    mockAgent.dispose();
    updateUserAgentUtils();
  },

  testWindows() {
    mockAgent.setNavigator({platform: 'Win32'});

    const win98 =
        'Mozilla/4.0 (compatible; MSIE 6.0b; Windows 98; Win 9x 4.90)';
    const win2k = 'Mozilla/5.0 (Windows; U; MSIE 7.0; Windows NT 5.0; en-US)';
    const xp = 'Mozilla/5.0 (Windows; U; MSIE 7.0; Windows NT 5.1; en-US)';
    const vista = 'Mozilla/5.0 (Windows; U; MSIE 7.0; Windows NT 6.0; en-US)';
    const win7 = 'Mozilla/5.0 (Windows; U; MSIE 7.0; Windows NT 6.1; en-US)';
    const win81 =
        'Mozilla/5.0 (Windows NT 6.3; Trident/7.0; rv:11.0) like Gecko';

    mockAgent.setUserAgentString(win98);
    updateUserAgentUtils();
    assertEquals('0', platform.VERSION);

    mockAgent.setUserAgentString(win2k);
    updateUserAgentUtils();
    assertEquals('5.0', platform.VERSION);

    mockAgent.setUserAgentString(xp);
    updateUserAgentUtils();
    assertEquals('5.1', platform.VERSION);

    mockAgent.setUserAgentString(vista);
    updateUserAgentUtils();
    assertEquals('6.0', platform.VERSION);

    mockAgent.setUserAgentString(win7);
    updateUserAgentUtils();
    assertEquals('6.1', platform.VERSION);

    mockAgent.setUserAgentString(win81);
    updateUserAgentUtils();
    assertEquals('6.3', platform.VERSION);
  },

  testMac() {
    // For some reason Chrome substitutes _ for . in the OS version.
    const chrome = 'Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_5_8; en-US)' +
        'AppleWebKit/532.5 (KHTML, like Gecko) Chrome/4.0.249.49 Safari/532.5';

    const ff = 'Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.5; en-US;' +
        'rv:1.9.1.7) Gecko/20091221 Firefox/3.5.7 GTB6';

    const chrome_osx_11 = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 11_1_0)' +
        'AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.66' +
        'Safari/537.36';

    mockAgent.setNavigator({platform: 'IntelMac'});

    mockAgent.setUserAgentString(chrome);
    updateUserAgentUtils();
    assertEquals('10.5.8', platform.VERSION);

    mockAgent.setUserAgentString(ff);
    updateUserAgentUtils();
    assertEquals('10.5', platform.VERSION);

    mockAgent.setUserAgentString(chrome_osx_11);
    updateUserAgentUtils();
    assertEquals('11.1.0', platform.VERSION);
  },

  testChromeOnAndroid() {
    // Borrowing search's test user agent string for android.
    const uaString =
        'Mozilla/5.0 (Linux; U; Android 4.0.2; en-us; Galaxy Nexus' +
        ' Build/ICL53F) AppleWebKit/535.7 (KHTML, like Gecko) ' +
        'Chrome/18.0.1025.133 Mobile Safari/535.7';

    // Need to set this lest the testing platform be used for detection.
    mockAgent.setNavigator({platform: 'Android'});

    mockAgent.setUserAgentString(uaString);
    updateUserAgentUtils();
    assertTrue(userAgent.ANDROID);
    assertEquals('4.0.2', platform.VERSION);
  },

  testAndroidBrowser() {
    const uaString = 'Mozilla/5.0 (Linux; U; Android 2.3.4; fr-fr;' +
        'HTC Desire Build/GRJ22) AppleWebKit/533.1 (KHTML, like Gecko)' +
        'Version/4.0 Mobile Safari/533.1';

    // Need to set this lest the testing platform be used for detection.
    mockAgent.setNavigator({platform: 'Android'});

    mockAgent.setUserAgentString(uaString);
    updateUserAgentUtils();
    assertTrue(userAgent.ANDROID);
    assertEquals('2.3.4', platform.VERSION);
  },

  testIPhone() {
    // Borrowing search's test user agent string for the iPhone.
    const uaString =
        'Mozilla/5.0 (iPhone; U; CPU iPhone OS 4_0 like Mac OS X; ' +
        'en-us) AppleWebKit/532.9 (KHTML, like Gecko) Version/4.0.5 ' +
        'Mobile/8A293 Safari/6531.22.7';

    // Need to set this lest the testing platform be used for detection.
    mockAgent.setNavigator({platform: 'iPhone'});

    mockAgent.setUserAgentString(uaString);
    updateUserAgentUtils();
    assertTrue(userAgent.IPHONE);
    assertEquals('4.0', platform.VERSION);
  },

  testIPad() {
    // Borrowing search's test user agent string for the iPad.
    const uaString =
        'Mozilla/5.0 (iPad; U; CPU OS 4_2_1 like Mac OS X; ja-jp) ' +
        'AppleWebKit/533.17.9 (KHTML, like Gecko) Version/5.0.2 Mobile/8C148 ' +
        'Safari/6533.18.5';

    // Need to set this lest the testing platform be used for detection.
    mockAgent.setNavigator({platform: 'iPad'});

    mockAgent.setUserAgentString(uaString);
    updateUserAgentUtils();
    assertTrue(userAgent.IPAD);
    assertEquals('4.2.1', platform.VERSION);
  },

  testIPod22() {
    // Borrowing from webserver/browser_rules for user agents
    const uaString = 'Mozilla/5.0 (iPod; U; CPU iPhone OS 2_2 like ' +
        'Mac OS X; en-us) AppleWebKit/525.18.1 (KHTML, like Gecko) Mobile/5G77a';

    // Need to set this lest the testing platform be used for detection.
    mockAgent.setNavigator({platform: 'iPod'});

    mockAgent.setUserAgentString(uaString);
    updateUserAgentUtils();
    assertTrue(userAgent.IPOD);
    assertEquals('2.2', platform.VERSION);
  },

  testIPod91() {
    // Borrowing from webserver/browser_rules for user agents
    const uaString = 'Mozilla/5.0 (iPod; CPU iPhone OS 9_1 like ' +
        'Mac OS X) AppleWebKit/601.1 (KHTML, like Gecko) ' +
        'CriOS/47.0.2526.70 Mobile/13B143 Safari/601.1.46,gzip(gfe)';

    // Need to set this lest the testing platform be used for detection.
    mockAgent.setNavigator({platform: 'iPod'});

    mockAgent.setUserAgentString(uaString);
    updateUserAgentUtils();
    assertTrue(userAgent.IPOD);
    assertEquals('9.1', platform.VERSION);
  },
});
