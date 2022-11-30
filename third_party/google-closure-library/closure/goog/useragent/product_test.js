/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.userAgent.productTest');
goog.setTestOnly();

const MockUserAgent = goog.require('goog.testing.MockUserAgent');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const googArray = goog.require('goog.array');
const googUserAgent = goog.require('goog.userAgent');
const isVersion = goog.require('goog.userAgent.product.isVersion');
const product = goog.require('goog.userAgent.product');
const testAgents = goog.require('goog.labs.userAgent.testAgents');
const testSuite = goog.require('goog.testing.testSuite');
const userAgentTestUtil = goog.require('goog.userAgentTestUtil');

let mockAgent;
let replacer;

function updateUserAgentUtils() {
  userAgentTestUtil.reinitializeUserAgent();
}

// The set of products whose corresponding goog.userAgent.product value is set
// in goog.userAgent.product.init_().
const DETECTED_BROWSER_KEYS =
    ['FIREFOX', 'IPHONE', 'IPAD', 'ANDROID', 'CHROME', 'SAFARI'];

// browserKey should be the constant name, as a string
// 'FIREFOX', 'CHROME', 'ANDROID', etc.
function assertIsBrowser(currentBrowser) {
  assertTrue(
      'Current browser key into goog.userAgent.product ' +
          'should be true',
      product[currentBrowser]);

  // Make sure we don't have any false positives for other browsers.
  DETECTED_BROWSER_KEYS.forEach(browserKey => {
    // Ignore the iPad/Safari case, as the new code correctly
    // identifies the test useragent as both iPad and Safari.
    if (currentBrowser == 'IPAD' && browserKey == 'SAFARI') {
      return;
    }
    if (currentBrowser == 'IPHONE' && browserKey == 'SAFARI') {
      return;
    }
    if (currentBrowser == 'CHROME' && browserKey == 'IPHONE') {
      return;
    }
    if (currentBrowser != browserKey) {
      assertFalse(
          `Current browser key is ${currentBrowser}` +
              ' but different key into goog.userAgent.product is true: ' +
              browserKey,
          product[browserKey]);
    }
  });
}

function assertBrowserAndVersion(userAgent, browser, version) {
  mockAgent.setUserAgentString(userAgent);
  updateUserAgentUtils();
  assertIsBrowser(browser);
  assertEquals(
      'User agent should have this version', version, googUserAgent.VERSION);
}

/**
 * @param {Array<{
 *           ua: string,
 *           versions: Array<{
 *             num: (string|number), truth: boolean}>}>} userAgents
 * @param {string} browser
 */
function checkEachUserAgentDetected(userAgents, browser) {
  googArray.forEach(userAgents, (ua) => {
    mockAgent.setUserAgentString(ua.ua);
    updateUserAgentUtils();

    assertIsBrowser(browser);

    // Check versions
    ua.versions.forEach(v => {
      mockAgent.setUserAgentString(ua.ua);
      updateUserAgentUtils();
      assertEquals(
          'Expected version ' + v.num + ' from ' + ua.ua + ' but got ' +
              product.VERSION,
          v.truth, isVersion(v.num));
    });
  });
}

testSuite({
  shouldRunTests() {
    // This test has not yet been updated to run on IE8 and up. See b/2997681.
    return !googUserAgent.IE || !googUserAgent.isVersionOrHigher(8);
  },

  setUp() {
    mockAgent = new MockUserAgent();
    mockAgent.install();
    replacer = new PropertyReplacer();
  },

  tearDown() {
    replacer.reset();
    mockAgent.dispose();
    updateUserAgentUtils();
  },

  testInternetExplorer() {
    const userAgents = [
      {
        ua: 'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; GTB6; ' +
            'chromeframe; .NET CLR 1.1.4322; InfoPath.1; ' +
            '.NET CLR 3.0.04506.30; .NET CLR 3.0.04506.648; ' +
            '.NET CLR 3.0.4506.2152; .NET CLR 3.5.30729; .NET CLR 2.0.50727)',
        versions: [
          {num: 6, truth: true},
          {num: '7.0', truth: true},
          {num: 7.1, truth: false},
          {num: 8, truth: false},
        ],
      },
      {
        ua: 'Mozilla/5.0 (Windows NT 6.3; Trident/7.0; rv:11.0) like Gecko',
        versions: [
          {num: 10, truth: true},
          {num: 11, truth: true},
          {num: '11.0', truth: true},
          {num: '12', truth: false},
        ],
      },
    ];
    // hide any navigator.product value by putting in a navigator with no
    // properties.
    mockAgent.setNavigator({});
    checkEachUserAgentDetected(userAgents, 'IE');
  },

  testEdge() {
    const userAgents = [{
      ua: testAgents.EDGE_12_9600,
      versions: [
        {num: 11, truth: true},
        {num: 12, truth: true},
        {num: '12.96', truth: true},
        {num: '12.9600', truth: true},
        {num: '12.9601', truth: false},
        {num: '12.10240', truth: false},
        {num: 13, truth: false},
      ],
    }];
    checkEachUserAgentDetected(userAgents, 'EDGE');
  },

  testOpera() {
    const opera = {};
    const userAgents = [{
      ua: 'Opera/9.80 (Windows NT 5.1; U; en) Presto/2.2.15 Version/10.01',
      versions: [
        {num: 9, truth: true},
        {num: '10.1', truth: true},
        {num: 11, truth: false},
      ],
    }];
    replacer.set(globalThis, 'opera', opera);
    opera.version = '10.01';
    checkEachUserAgentDetected(userAgents, 'OPERA');
  },

  testFirefox() {
    const userAgents = [
      {
        ua: 'Mozilla/6.0 (Macintosh; U; PPC Mac OS X Mach-O; en-US; ' +
            'rv:2.0.0.0) Gecko/20061028 Firefox/3.0',
        versions: [
          {num: 2, truth: true},
          {num: '3.0', truth: true},
          {num: '3.5.3', truth: false},
        ],
      },
      {
        ua: 'Mozilla/5.0 (Macintosh; U; PPC Mac OS X Mach-O; en-US; ' +
            'rv:1.8.1.4) Gecko/20070515 Firefox/2.0.4',
        versions: [
          {num: 2, truth: true},
          {num: '2.0.4', truth: true},
          {num: 3, truth: false},
          {num: '3.5.3', truth: false},
        ],
      },
      {
        ua: 'Mozilla/5.0 (X11; Linux i686; rv:6.0) Gecko/6.0 Firefox/6.0',
        versions: [
          {num: 6, truth: true},
          {num: '6.0', truth: true},
          {num: 7, truth: false},
          {num: '7.0', truth: false},
        ],
      },
    ];

    checkEachUserAgentDetected(userAgents, 'FIREFOX');

    // Mozilla reported to us that they plan this UA format starting
    // in Firefox 13.
    // See bug at https://bugzilla.mozilla.org/show_bug.cgi?id=588909
    mockAgent.setNavigator({product: 'Gecko'});
    assertBrowserAndVersion(
        'Mozilla/5.0 (X11; Linux i686; rv:6.0) Gecko/6.0 Firefox/6.0',
        'FIREFOX', '6.0');
  },

  testChrome() {
    const userAgents = [
      {
        ua: 'Mozilla/5.0 (Windows; U; Windows NT 6.0; en-US) ' +
            'AppleWebKit/525.19 (KHTML, like Gecko) Chrome/0.2.153.0 ' +
            'Safari/525.19',
        versions: [{num: '0.2.153', truth: true}, {num: 1, truth: false}],
      },
      {
        ua: 'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) ' +
            'AppleWebKit/532.3 (KHTML, like Gecko) Chrome/4.0.223.11 ' +
            'Safari/532.3',
        versions: [
          {num: 4, truth: true},
          {num: '0.2.153', truth: true},
          {num: '4.1.223.13', truth: false},
          {num: '4.0.223.10', truth: true},
        ],
      },
      {
        ua: 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.114 Safari/537.36',
        versions: [
          {num: 91, truth: true},
          {num: '91.0.4472.114', truth: true},
          {num: '0.4472.114', truth: true},
          {num: '91.1.4472.114', truth: false},
        ]
      },
      {
        ua: 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) CriOS/91.0.4472.114 Safari/537.36',
        versions: [
          {num: 91, truth: true},
          {num: '91.0.4472.114', truth: true},
          {num: '0.4472.114', truth: true},
          {num: '91.1.4472.114', truth: false},
        ]
      },
      {
        ua: 'Mozilla/5.0 (Linux; Android 4.0.4; Galaxy Nexus Build/IMM76B)' +
            'AppleWebKit/535.19 (KHTML, like Gecko) ' +
            'Chrome/18.0.1025.133 Mobile' +
            'Safari/535.19',
        versions: [
          {num: 18, truth: true},
          {num: '0.2.153', truth: true},
          {num: 29, truth: false},
          {num: '18.0.1025.133', truth: true},
        ],
      },
      {
        ua: 'Mozilla/5.0 (iPhone; CPU iPhone OS 10_2_1 like Mac OS X) ' +
            'AppleWebKit/602.1.50 (KHTML, like Gecko) CriOS/56.0.2924.79 ' +
            'Mobile/14D27 Safari/602.1',
        versions: [
          {num: '56.1.2924.79', truth: false},
          {num: '56.0.2924.79', truth: true},
        ],
      },
      {
        ua: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 ' +
            '(KHTML, like Gecko) Chrome/74.0.3729.48 Safari/537.36 ' +
            'Edg/74.1.96.24',
        versions: [
          {num: '74.1.3729.48', truth: false},
          {num: '74.0.3729.48', truth: true},
        ],
      },
    ];
    checkEachUserAgentDetected(userAgents, 'CHROME');
  },

  testSafari() {
    const userAgents = [
      {
        ua: 'Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6_7; de-de) ' +
            'AppleWebKit/534.16+ (KHTML, like Gecko) Version/5.0.3 ' +
            'Safari/533.19.4',
        versions: [
          {num: 5, truth: true},
          {num: '5.0.3', truth: true},
          {num: '5.0.4', truth: false},
          {num: '533', truth: false},
        ],
      },
      {
        ua: 'Mozilla/5.0 (Windows; U; Windows NT 6.0; pl-PL) ' +
            'AppleWebKit/525.19 (KHTML, like Gecko) Version/3.1.2 Safari/525.21',
        versions: [
          {num: 3, truth: true},
          {num: '3.0', truth: true},
          {num: '3.1.2', truth: true},
        ],
      },
      {
        ua: 'Mozilla/5.0 (Macintosh; U; PPC Mac OS X 10_5_3; en-us) ' +
            'AppleWebKit/525.18 (KHTML, like Gecko) Version/3.1.1 Safari/525.20',
        versions: [
          {num: 3, truth: true},
          {num: '3.1.1', truth: true},
          {num: '3.1.2', truth: false},
          {num: '525.21', truth: false},
        ],
      },

      // Safari 1 and 2 do not report product version numbers in their
      // user-agent strings. VERSION for these browsers will be set to ''.
      {
        ua: 'Mozilla/5.0 (Macintosh; U; PPC Mac OS X; ja-jp) ' +
            'AppleWebKit/418.9.1 (KHTML, like Gecko) Safari/419.3',
        versions: [
          {num: 3, truth: false},
          {num: 2, truth: false},
          {num: 1, truth: false},
          {num: 0, truth: true},
          {num: '0', truth: true},
          {num: '', truth: true},
        ],
      },
    ];
    checkEachUserAgentDetected(userAgents, 'SAFARI');
  },

  testIphone() {
    const userAgents = [
      {
        ua: 'Mozilla/5.0 (iPhone; U; CPU like Mac OS X; en) AppleWebKit/420+ ' +
            '(KHTML, like Gecko) Version/3.0 Mobile/1A543a Safari/419.3',
        versions: [
          {num: '3.0.1A543a', truth: true},
          {num: '3.0', truth: true},
          {num: '3.0.1B543a', truth: false},
          {num: '3.1.1A543a', truth: false},
          {num: '3.0.1A320c', truth: true},
          {num: '3.0.3A100a', truth: false},
        ],
      },
      {
        ua: 'Mozilla/5.0 (iPod; U; CPU like Mac OS X; en) AppleWebKit/420.1 ' +
            '(KHTML, like Gecko) Version/3.0 Mobile/3A100a Safari/419.3',
        versions: [
          {num: '3.0.1A543a', truth: true}, {num: '3.0.3A100a', truth: true}
        ],
      },
    ];
    checkEachUserAgentDetected(userAgents, 'IPHONE');
  },

  testIpad() {
    const userAgents = [
      {
        ua: 'Mozilla/5.0 (iPad; U; CPU OS 3_2 like Mac OS X; en-us) ' +
            'AppleWebKit/531.21.10 (KHTML, like Gecko) Version/4.0.4 ' +
            'Mobile/7B334b Safari/531.21.10',
        versions: [
          {num: '4.0.4.7B334b', truth: true},
          {num: '4.0', truth: true},
          {num: '4.0.4.7C334b', truth: false},
          {num: '4.1.7B334b', truth: false},
          {num: '4.0.4.7B320c', truth: true},
          {num: '4.0.4.8B334b', truth: false},
        ],
      },
      // Webview in the Facebook iOS app
      {
        ua: 'Mozilla/5.0 (iPad; CPU OS 8_1 like Mac OS X) AppleWebKit/600.1.4' +
            '(KHTML, like Gecko) Mobile/12B410 [FBAN/FBIOS;FBAV/16.0.0.13.22;' +
            'FBBV/4697910;FBDV/iPad3,4;FBMD/iPad;FBSN/iPhone OS;FBSV/8.1;' +
            'FBSS/2; FBCR/;FBID/tablet;FBLC/ja_JP;FBOP/1]',
        versions: [{num: '', truth: true}],
      },
    ];
    checkEachUserAgentDetected(userAgents, 'IPAD');
  },

  testAndroid() {
    const userAgents = [
      {
        ua: 'Mozilla/5.0 (Linux; U; Android 0.5; en-us) AppleWebKit/522+ ' +
            '(KHTML, like Gecko) Safari/419.3',
        versions: [{num: 0.5, truth: true}, {num: '1.0', truth: false}],
      },
      {
        ua: 'Mozilla/5.0 (Linux; U; Android 1.0; en-us; dream) ' +
            'AppleWebKit/525.10+ (KHTML, like Gecko) Version/3.0.4 Mobile ' +
            'Safari/523.12.2',
        versions: [
          {num: 0.5, truth: true},
          {num: 1, truth: true},
          {num: '1.0', truth: true},
          {num: '3.0.12', truth: false},
        ],
      },
    ];
    checkEachUserAgentDetected(userAgents, 'ANDROID');
  },

  testAndroidLegacyBehavior() {
    mockAgent.setUserAgentString(testAgents.FIREFOX_ANDROID_TABLET);
    updateUserAgentUtils();
    // Historically, goog.userAgent.product.ANDROID has referred to the
    // Android browser, not the platform. Firefox on Android should
    // be false.
    assertFalse(product.ANDROID);
  },

  testSafariIosLegacyBehavior() {
    mockAgent.setUserAgentString(testAgents.SAFARI_IPHONE_6);
    updateUserAgentUtils();
    // Historically, goog.userAgent.product.SAFARI has referred to the
    // Safari desktop browser, not the mobile browser.
    assertFalse(product.SAFARI);
  },
});
