/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.windowTest');
goog.setTestOnly();

const GoogPromise = goog.require('goog.Promise');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SafeUrl = goog.require('goog.html.SafeUrl');
const TagName = goog.require('goog.dom.TagName');
const TestCase = goog.require('goog.testing.TestCase');
const browser = goog.require('goog.labs.userAgent.browser');
const dom = goog.require('goog.dom');
const engine = goog.require('goog.labs.userAgent.engine');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const googString = goog.require('goog.string');
const googWindow = goog.require('goog.window');
const platform = goog.require('goog.labs.userAgent.platform');
const testSuite = goog.require('goog.testing.testSuite');

const REDIRECT_URL_PREFIX = 'window_test.html?runTests=';
const WIN_LOAD_TRY_TIMEOUT = 100;
const MAX_WIN_LOAD_TRIES = 50;  // 50x100ms = 5s waiting for window to load.

const stubs = new PropertyReplacer();

// To test goog.window.open we open a new window with this file again. Once
// the new window parses this file it sets this variable to true, indicating
// that the parent test may check window properties like referrer and location.
/**
 * @suppress {strictMissingProperties} suppression added to enable type
 * checking
 */
window.newWinLoaded = true;

let /** ?Window */ newWin = null;


/**
 * Returns a promise for `win` once JS has been evaluated in it.
 * @param {Window} win
 * @return {!GoogPromise<!Window>} Promise for a window that resolves once the
 *     window has loaded.
 */
function waitForTestWindow(win) {
  return new GoogPromise((resolve, reject) => {
    if (!win) {
      fail('Could not open new window. Check if popup blocker is enabled.');
    }

    let attemptCount = 0;
    const intervalToken =
        window
            .setInterval(/**
                            @suppress {strictMissingProperties} suppression
                            added to enable type checking
                          */
                         () => {
                           if (++attemptCount > MAX_WIN_LOAD_TRIES) {
                             try {
                               fail(
                                   'Window did not load after maximum number of checks.');
                             } catch (e) {
                               window.clearInterval(intervalToken);
                               reject(e);
                             }
                           } else if (win.newWinLoaded) {
                             window.clearInterval(intervalToken);
                             resolve(win);
                           }
                         },
                         WIN_LOAD_TRY_TIMEOUT);
  });
}

/**
 * Opens a window and then verifies that the new window has the expected
 * properties.
 * @param {boolean} noreferrer Whether to test the noreferrer option.
 * @param {string} urlParam Url param to append to the url being opened.
 * @param {boolean=} encodeUrlParam_opt Whether to percent-encode urlParam. This
 *     is needed because IE will not encode it automatically like other browsers
 *     browser and the Closure test server will 400 on certain characters in the
 *     URL (like '<' and '"').
 * @return {!GoogPromise} Promise that resolves once the test is complete.
 */
function doTestOpenWindow(noreferrer, urlParam, encodeUrlParam_opt) {
  if (encodeUrlParam_opt) {
    urlParam = encodeURIComponent(urlParam);
  }
  // TODO(mlourenco): target is set because goog.window.open() will currently
  // allow it to be undefined, which in IE seems to result in the same window
  // being reused, instead of a new one being created. If goog.window.open()
  // is fixed to use "_blank" by default then target can be removed here.
  newWin = googWindow.open(
      REDIRECT_URL_PREFIX + urlParam,
      {'noreferrer': noreferrer, 'target': '_blank'});

  return waitForTestWindow(newWin).then((win) => {
    verifyWindow(win, noreferrer, urlParam);
  });
}

/**
 * Asserts that a newly created window has the correct parameters.
 * @param {Window} win
 * @param {boolean} noreferrer Whether the noreferrer option is being tested.
 * @param {string} urlParam Url param appended to the url being opened.
 */
function verifyWindow(win, noreferrer, urlParam) {
  if (noreferrer) {
    assertEquals(
        'Referrer should have been stripped', '', win.document.referrer);
  }

  const winUrl = decodeURI(String(win.location));
  const expectedUrlSuffix = decodeURI(urlParam);
  assertTrue(
      `New window href should have ended with <${expectedUrlSuffix}` +
          '> but was <' + winUrl + '>',
      googString.endsWith(winUrl, expectedUrlSuffix));
}

testSuite({
  shouldRunTests() {
    // TODO(user): Edge has a flaky test failures around window.open.
    return !browser.isEdge();
  },

  setUpPage() {
    const anchors =
        dom.getElementsByTagNameAndClass(TagName.DIV, 'goog-like-link');
    for (let i = 0; i < anchors.length; i++) {
      events.listen(anchors[i], 'click', (e) => {
        googWindow.open(dom.getTextContent(e.target), {'noreferrer': true});
      });
    }
    TestCase.getActiveTestCase().promiseTimeout = 60000;  // 60s
  },

  setUp() {
    newWin = null;
  },

  tearDown() {
    if (newWin) {
      newWin.close();
    }
    stubs.reset();
  },

  testOpenNotEncoded() {
    return doTestOpenWindow(false, 'bogus~');
  },

  testOpenEncoded() {
    return doTestOpenWindow(false, 'bogus%7E');
  },

  testOpenEncodedPercent() {
    // Intent of url is to pass %7E to the server, so it was encoded to %257E .
    return doTestOpenWindow(false, 'bogus%257E');
  },

  testOpenNotEncodedHidingReferrer() {
    return doTestOpenWindow(true, 'bogus~');
  },

  testOpenEncodedHidingReferrer() {
    return doTestOpenWindow(true, 'bogus%7E');
  },

  testOpenEncodedPercentHidingReferrer() {
    // Intent of url is to pass %7E to the server, so it was encoded to %257E .
    return doTestOpenWindow(true, 'bogus%257E');
  },

  testOpenSemicolon() {
    return doTestOpenWindow(true, 'beforesemi;aftersemi');
  },

  testTwoSemicolons() {
    return doTestOpenWindow(true, 'a;b;c');
  },

  testOpenAmpersand() {
    return doTestOpenWindow(true, 'this&that');
  },

  testOpenSingleQuote() {
    return doTestOpenWindow(true, '\'');
  },

  testOpenDoubleQuote() {
    return doTestOpenWindow(true, '"', browser.isIE());
  },

  testOpenTag() {
    return doTestOpenWindow(true, '<', browser.isIE());
  },

  testOpenWindowSanitization() {
    let navigatedUrl;
    const /** ? */ mockWin = {
      open: function(url) {
        navigatedUrl = url;
      },
    };

    googWindow.open('javascript:evil();', {}, mockWin);
    assertEquals(SafeUrl.INNOCUOUS_STRING, navigatedUrl);

    // Try the other code path
    googWindow.open({href: 'javascript:evil();'}, {}, mockWin);
    assertEquals(SafeUrl.INNOCUOUS_STRING, navigatedUrl);

    googWindow.open('javascript:\'\'', {}, mockWin);
    assertEquals(SafeUrl.INNOCUOUS_STRING, navigatedUrl);

    googWindow.open('about:blank', {}, mockWin);
    assertEquals(SafeUrl.INNOCUOUS_STRING, navigatedUrl);
  },

  testOpenWindowNoSanitization() {
    let navigatedUrl;
    const /** ? */ mockWin = {
      open: function(url) {
        navigatedUrl = url;
      },
    };

    googWindow.open('', {}, mockWin);
    assertEquals('', navigatedUrl);

    googWindow.open(SafeUrl.ABOUT_BLANK, {}, mockWin);
    assertEquals('about:blank', navigatedUrl);
  },

  testOpenBlank() {
    newWin = googWindow.openBlank('Loading...');
    const urlParam = 'bogus~';
    newWin.location.href = REDIRECT_URL_PREFIX + urlParam;
    return waitForTestWindow(newWin).then(() => {
      verifyWindow(newWin, false, urlParam);
    });
  },

  testOpenBlankReturnsNullPopupBlocker() {
    const /** ? */ mockWin = {
      // emulate popup-blocker by returning a null window on open().
      open: function() {
        return null;
      },
    };
    const win = googWindow.openBlank('', {noreferrer: true}, mockWin);
    assertNull(win);
  },

  testOpenBlankEscapesSafely() {
    // Opening a window with javascript: and then reading from its document.body
    // is problematic because in some browsers the document.body won't have been
    // updated yet, and in some IE versions the parent window does not have
    // access to document.body in new blank window.
    let navigatedUrl;
    const /** ? */ mockWin = {
      open: function(url) {
        navigatedUrl = url;
      },
    };

    // Test string determines that all necessary escaping transformations
    // happen, and that they happen in the right order (HTML->JS->URI).
    // - " which would be escaped by HTML escaping and JS string escaping. It
    //     should be HTML escaped.
    // - \ which would be escaped by JS string escaping and percent-encoded
    //     by encodeURI(). It gets JS string escaped first (to two '\') and then
    //     percent-encoded.
    const win = googWindow.openBlank('"\\', {}, mockWin);
    assertEquals('javascript:"&quot;%5C%5C"', navigatedUrl);
  },

  testOpenIosBlank() {
    if (!engine.isWebKit() || !window.navigator) {
      // Don't even try this on IE8!
      return;
    }
    const attrs = {};
    let dispatchedEvent = null;
    const element = {
      setAttribute: function(name, value) {
        attrs[name] = value;
      },
      dispatchEvent: function(event) {
        dispatchedEvent = event;
      },
      href: undefined,
    };
    stubs.replace(window.document, 'createElement', (name) => {
      if (name == TagName.A) {
        return element;
      }
      return null;
    });
    stubs.set(window.navigator, 'standalone', true);
    stubs.replace(platform, 'isIos', functions.TRUE);

    const newWin = googWindow.open('http://google.com', {target: '_blank'});

    // This mode cannot return a new window.
    assertNotNull(newWin);
    assertUndefined(newWin.document);

    // Attributes.
    // element.href is directly set through goog.dom.safe.setAnchorHref, not
    // with element.setAttribute.
    assertEquals('http://google.com', element.href);
    assertEquals('_blank', attrs['target']);
    assertEquals('', attrs['rel'] || '');

    // Click event.
    assertNotNull(dispatchedEvent);
    assertEquals('click', dispatchedEvent.type);
  },

  testOpenIosBlankNoreferrer() {
    if (!engine.isWebKit() || !window.navigator) {
      // Don't even try this on IE8!
      return;
    }
    const attrs = {};
    let dispatchedEvent = null;
    const element = {
      setAttribute: function(name, value) {
        attrs[name] = value;
      },
      dispatchEvent: function(event) {
        dispatchedEvent = event;
      },
      href: undefined,
    };
    stubs.replace(window.document, 'createElement', (name) => {
      if (name == TagName.A) {
        return element;
      }
      return null;
    });
    stubs.set(window.navigator, 'standalone', true);
    stubs.replace(platform, 'isIos', functions.TRUE);

    const newWin = googWindow.open(
        'http://google.com', {target: '_blank', noreferrer: true});

    // This mode cannot return a new window.
    assertNotNull(newWin);
    assertUndefined(newWin.document);

    // Attributes.
    // element.href is directly set through goog.dom.safe.setAnchorHref, not
    // with element.setAttribute.
    assertEquals('http://google.com', element.href);
    assertEquals('_blank', attrs['target']);
    assertEquals('noreferrer', attrs['rel']);

    // Click event.
    assertNotNull(dispatchedEvent);
    assertEquals('click', dispatchedEvent.type);
  },

  testOpenNoReferrerEscapesUrl() {
    let documentWriteHtml;
    const mockNewWin = {};
    mockNewWin.document = {
      write: function(html) {
        documentWriteHtml = html;
      },
      close: function() {},
    };
    const /** ? */ mockWin = {
      open: function() {
        return mockNewWin;
      },
    };
    googWindow.open('https://hello&world', {noreferrer: true}, mockWin);
    assertRegExp(
        `Does not contain expected HTML-escaped string: ${documentWriteHtml}`,
        /hello&amp;world/, documentWriteHtml);
  },

  testOpenNewWindowNoopener() {
    newWin = googWindow.open(
        `${REDIRECT_URL_PREFIX}theBest`,
        {'target': '_blank', 'noopener': true});

    // This mode cannot return a new window.
    assertNotNull(newWin);
    assertNotEquals(undefined, newWin.document);
    assertNull(newWin.opener);

    return waitForTestWindow(newWin).then((win) => {
      verifyWindow(win, false, 'theBest');
    });
  },
});
