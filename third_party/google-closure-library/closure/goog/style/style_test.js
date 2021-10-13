/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Shared unit tests for styles. */

goog.module('goog.style_test');
goog.setTestOnly();

const Box = goog.require('goog.math.Box');
const BrowserEvent = goog.require('goog.events.BrowserEvent');
const Coordinate = goog.require('goog.math.Coordinate');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const GoogRect = goog.require('goog.math.Rect');
const MockUserAgent = goog.require('goog.testing.MockUserAgent');
const Size = goog.require('goog.math.Size');
const TagName = goog.require('goog.dom.TagName');
const UserAgents = goog.require('goog.userAgentTestUtil.UserAgents');
const asserts = goog.require('goog.testing.asserts');
const color = goog.require('goog.color');
const dispose = goog.require('goog.dispose');
const googArray = goog.require('goog.array');
const googDom = goog.require('goog.dom');
const googObject = goog.require('goog.object');
const googStyle = goog.require('goog.style');
const jsunit = goog.require('goog.testing.jsunit');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const userAgent = goog.require('goog.userAgent');
const userAgentTestUtil = goog.require('goog.userAgentTestUtil');

// Delay running the tests after page load. This test has some asynchronous
// behavior that interacts with page load detection.
/** @suppress {constantProperty} suppression added to enable type checking */
jsunit.AUTO_RUN_DELAY_IN_MS = 500;

const isBorderBox = !googDom.isCss1CompatMode();
const EPSILON = 2;
let expectedFailures;
const $ = googDom.getElement;
let mockUserAgent;

function hasWebkitTransform() {
  return 'webkitTransform' in document.body.style;
}

function assertColorRgbEquals(expected, actual) {
  assertEquals(expected, color.hexToRgbStyle(color.parse(actual).hex));
}

/**
 * Asserts that the coordinate is approximately equal to the given
 * x and y coordinates, give or take delta.
 */
function assertCoordinateApprox(x, y, delta, coord) {
  assertTrue(
      `Expected x: ${x}, actual x: ` + coord.x,
      coord.x >= x - delta && coord.x <= x + delta);
  assertTrue(
      `Expected y: ${y}, actual y: ` + coord.y,
      coord.y >= y - delta && coord.y <= y + delta);
}

/**
 * Test browser detection for a user agent configuration.
 * @param {Array<number>} expectedAgents Array of expected userAgents.
 * @param {string} uaString User agent string.
 * @param {string=} product Navigator product string.
 * @param {string=} vendor Navigator vendor string.
 */
function assertUserAgent(
    expectedAgents, uaString, product = undefined, vendor = undefined) {
  const mockNavigator = {
    'userAgent': uaString,
    'product': product,
    'vendor': vendor,
  };

  mockUserAgent.setNavigator(mockNavigator);
  mockUserAgent.setUserAgentString(uaString);

  userAgentTestUtil.reinitializeUserAgent();
  for (const ua in UserAgents) {
    const isExpected = googArray.contains(expectedAgents, UserAgents[ua]);
    assertEquals(
        isExpected, userAgentTestUtil.getUserAgentDetected(UserAgents[ua]));
  }
}

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();

    const viewportSize = googDom.getViewportSize();
    // When the window is too short or not wide enough, some tests, especially
    // those for off-screen elements, fail.  Oddly, the most reliable
    // indicator is a width of zero (which is of course erroneous), since
    // height sometimes includes a scroll bar.  We can make no assumptions on
    // window size on the Selenium farm.
    if (userAgent.IE && viewportSize.width < 300) {
      // Move to origin, since IE won't resize outside the screen.
      window.moveTo(0, 0);
      window.resizeTo(640, 480);
    }
  },

  setUp() {
    window.scrollTo(0, 0);
    userAgentTestUtil.reinitializeUserAgent();
    mockUserAgent = new MockUserAgent();
    mockUserAgent.install();
  },

  tearDown() {
    expectedFailures.handleTearDown();
    const testVisibleDiv2 = googDom.getElement('test-visible2');
    testVisibleDiv2.setAttribute('style', '');
    googDom.removeChildren(testVisibleDiv2);
    const testViewport = googDom.getElement('test-viewport');
    testViewport.setAttribute('style', '');
    googDom.removeChildren(testViewport);
    dispose(mockUserAgent);

    // Prevent multiple vendor prefixed mock elements from poisoning the cache.
    /** @suppress {visibility} suppression added to enable type checking */
    googStyle.styleNameCache_ = {};
  },

  testSetStyle() {
    const el = $('span1');
    googStyle.setStyle(el, 'textDecoration', 'underline');
    assertEquals('Should be underline', 'underline', el.style.textDecoration);
  },

  testSetStyleMap() {
    const el = $('span6');

    const styles = {
      'background-color': 'blue',
      'font-size': '100px',
      textAlign: 'center',
    };

    googStyle.setStyle(el, styles);

    const answers = {
      backgroundColor: 'blue',
      fontSize: '100px',
      textAlign: 'center',
    };

    googObject.forEach(answers, (value, style) => {
      assertEquals(`Should be ${value}`, value, el.style[style]);
    });
  },

  testSetStyleWithNonCamelizedString() {
    const el = $('span5');
    googStyle.setStyle(el, 'text-decoration', 'underline');
    assertEquals('Should be underline', 'underline', el.style.textDecoration);
  },

  testGetStyle() {
    const el = googDom.getElement('styleTest3');
    googStyle.setStyle(el, 'width', '80px');
    googStyle.setStyle(el, 'textDecoration', 'underline');

    assertEquals('80px', googStyle.getStyle(el, 'width'));
    assertEquals('underline', googStyle.getStyle(el, 'textDecoration'));
    assertEquals('underline', googStyle.getStyle(el, 'text-decoration'));
    // Non set properties are always empty strings.
    assertEquals('', googStyle.getStyle(el, 'border'));
  },

  testGetStyleMsFilter() {
    // Element with -ms-filter style set.
    const e = googDom.getElement('msFilter');

    if (userAgent.IE && userAgent.isDocumentModeOrHigher(8) &&
        !userAgent.isDocumentModeOrHigher(10)) {
      // Only IE8/9 supports -ms-filter and returns it as value for the "filter"
      // property. When in compatibility mode, -ms-filter is not supported
      // and IE8 behaves as IE7 so the other case will apply.
      assertEquals('alpha(opacity=0)', googStyle.getStyle(e, 'filter'));
    } else {
      // Any other browser does not support ms-filter so it returns empty
      // string.
      assertEquals('', googStyle.getStyle(e, 'filter'));
    }
  },

  testGetStyleFilter() {
    // Element with filter style set.
    const e = googDom.getElement('filter');

    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(10)) {
      // Filter supported.
      assertEquals('alpha(opacity=0)', googStyle.getStyle(e, 'filter'));
    } else {
      assertEquals('', googStyle.getStyle(e, 'filter'));
    }
  },

  testGetComputedStyleMsFilter() {
    // Element with -ms-filter style set.
    const e = googDom.getElement('msFilter');

    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(10)) {
      if (userAgent.isDocumentModeOrHigher(9)) {
        // IE 9 returns the value.
        assertEquals(
            'alpha(opacity=0)', googStyle.getComputedStyle(e, 'filter'));
      } else {
        // Older IE always returns empty string for computed styles.
        assertEquals('', googStyle.getComputedStyle(e, 'filter'));
      }
    } else {
      // Non IE returns 'none' for filter as it is an SVG property
      assertEquals('none', googStyle.getComputedStyle(e, 'filter'));
    }
  },

  testGetComputedStyleFilter() {
    // Element with filter style set.
    const e = googDom.getElement('filter');

    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(10)) {
      if (userAgent.isDocumentModeOrHigher(9)) {
        // IE 9 returns the value.
        assertEquals(
            'alpha(opacity=0)', googStyle.getComputedStyle(e, 'filter'));
      } else {
        // Older IE always returns empty string for computed styles.
        assertEquals('', googStyle.getComputedStyle(e, 'filter'));
      }
    } else {
      // Non IE returns 'none' for filter as it is an SVG property
      assertEquals('none', googStyle.getComputedStyle(e, 'filter'));
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetComputedBoxSizing() {
    if (!userAgent.IE || userAgent.isVersionOrHigher(8)) {
      const defaultBoxSizing =
          googDom.isCss1CompatMode() ? 'content-box' : 'border-box';
      let el = googDom.getElement('box-sizing-unset');
      assertEquals(defaultBoxSizing, googStyle.getComputedBoxSizing(el));

      el = googDom.getElement('box-sizing-border-box');
      assertEquals('border-box', googStyle.getComputedBoxSizing(el));
    } else {
      // IE7 and below don't support box-sizing.
      assertNull(googStyle.getComputedBoxSizing(
          googDom.getElement('box-sizing-border-box')));
    }
  },

  testGetComputedPosition() {
    assertEquals(
        'position not set', 'static',
        googStyle.getComputedPosition($('position-unset')));
    assertEquals(
        'position:relative in style attribute', 'relative',
        googStyle.getComputedPosition($('style-position-relative')));
    if (userAgent.IE && !googDom.isCss1CompatMode() &&
        !userAgent.isVersionOrHigher(10)) {
      assertEquals(
          'position:fixed in style attribute', 'static',
          googStyle.getComputedPosition($('style-position-fixed')));
    } else {
      assertEquals(
          'position:fixed in style attribute', 'fixed',
          googStyle.getComputedPosition($('style-position-fixed')));
    }
    assertEquals(
        'position:absolute in css', 'absolute',
        googStyle.getComputedPosition($('css-position-absolute')));
  },

  testGetComputedOverflowXAndY() {
    assertEquals(
        'overflow-x:scroll in style attribute', 'scroll',
        googStyle.getComputedOverflowX($('style-overflow-scroll')));
    assertEquals(
        'overflow-y:scroll in style attribute', 'scroll',
        googStyle.getComputedOverflowY($('style-overflow-scroll')));
    assertEquals(
        'overflow-x:hidden in css', 'hidden',
        googStyle.getComputedOverflowX($('css-overflow-hidden')));
    assertEquals(
        'overflow-y:hidden in css', 'hidden',
        googStyle.getComputedOverflowY($('css-overflow-hidden')));
  },

  testGetComputedZIndex() {
    assertEquals(
        'z-index:200 in style attribute', '200',
        '' + googStyle.getComputedZIndex($('style-z-index-200')));
    assertEquals(
        'z-index:200 in css', '200',
        '' + googStyle.getComputedZIndex($('css-z-index-200')));
  },

  testGetComputedTextAlign() {
    assertEquals(
        'text-align:right in style attribute', 'right',
        googStyle.getComputedTextAlign($('style-text-align-right')));
    assertEquals(
        'text-align:right inherited from parent', 'right',
        googStyle.getComputedTextAlign($('style-text-align-right-inner')));
    assertEquals(
        'text-align:center in css', 'center',
        googStyle.getComputedTextAlign($('css-text-align-center')));
  },

  testGetComputedCursor() {
    assertEquals(
        'cursor:move in style attribute', 'move',
        googStyle.getComputedCursor($('style-cursor-move')));
    assertEquals(
        'cursor:move inherited from parent', 'move',
        googStyle.getComputedCursor($('style-cursor-move-inner')));
    assertEquals(
        'cursor:poiner in css', 'pointer',
        googStyle.getComputedCursor($('css-cursor-pointer')));
  },

  testGetBackgroundColor() {
    const dest = $('bgcolorDest');

    for (let i = 0; $(`bgcolorTest${i}`); i++) {
      const src = $(`bgcolorTest${i}`);
      const bgColor = googStyle.getBackgroundColor(src);

      dest.style.backgroundColor = bgColor;
      assertEquals(
          'Background colors should be equal',
          googStyle.getBackgroundColor(src),
          googStyle.getBackgroundColor(dest));

      let c = null;
      try {
        // goog.color.parse throws a generic exception if handed input it
        // doesn't understand.
        c = color.parse(bgColor);
      } catch (e) {
        // Internet Explorer is unable to parse colors correctly after test 4.
        // Other browsers may vary, but all should be able to handle straight
        // hex input.
        assertFalse(`Should be able to parse color "${bgColor}"`, i < 5);
        break;
      }
      if (i <= 5) {
        assertEquals('parse test', 'rgb(255,0,0)', color.hexToRgbStyle(c.hex));
      }
    }
  },

  testSetPosition() {
    const el = $('testEl');

    googStyle.setPosition(el, 100, 100);
    assertEquals('100px', el.style.left);
    assertEquals('100px', el.style.top);

    googStyle.setPosition(el, '50px', '25px');
    assertEquals('50px', el.style.left);
    assertEquals('25px', el.style.top);

    googStyle.setPosition(el, '10ex', '25px');
    assertEquals('10ex', el.style.left);
    assertEquals('25px', el.style.top);

    googStyle.setPosition(el, '10%', '25%');
    assertEquals('10%', el.style.left);
    assertEquals('25%', el.style.top);

    // ignores bad units
    googStyle.setPosition(el, 0, 0);
    googStyle.setPosition(el, '10rainbows', '25rainbows');
    assertEquals('0px', el.style.left);
    assertEquals('0px', el.style.top);

    googStyle.setPosition(el, new Coordinate(20, 40));
    assertEquals('20px', el.style.left);
    assertEquals('40px', el.style.top);
  },

  testGetClientPositionAbsPositionElement() {
    const div = googDom.createDom(TagName.DIV);
    div.style.position = 'absolute';
    div.style.left = '100px';
    div.style.top = '200px';
    document.body.appendChild(div);
    const pos = googStyle.getClientPosition(div);
    assertEquals(100, pos.x);
    assertEquals(200, pos.y);
  },

  testGetClientPositionNestedElements() {
    const innerDiv = googDom.createDom(TagName.DIV);
    innerDiv.style.position = 'relative';
    innerDiv.style.left = '-10px';
    innerDiv.style.top = '-10px';
    const div = googDom.createDom(TagName.DIV);
    div.style.position = 'absolute';
    div.style.left = '150px';
    div.style.top = '250px';
    div.appendChild(innerDiv);
    document.body.appendChild(div);
    const pos = googStyle.getClientPosition(innerDiv);
    assertEquals(140, pos.x);
    assertEquals(240, pos.y);
  },

  testGetClientPositionOfOffscreenElement() {
    const div = googDom.createDom(TagName.DIV);
    div.style.position = 'absolute';
    div.style.left = '2000px';
    div.style.top = '2000px';
    div.style.width = '10px';
    div.style.height = '10px';
    document.body.appendChild(div);

    try {
      window.scroll(0, 0);
      let pos = googStyle.getClientPosition(div);
      assertEquals(2000, pos.x);
      assertEquals(2000, pos.y);

      // The following tests do not work in IE, due to an
      // obscure off-by-one bug in goog.style.getPageOffset.
      if (!userAgent.IE) {
        window.scroll(1, 1);
        let pos = googStyle.getClientPosition(div);
        assertEquals(1999, pos.x);
        assertRoughlyEquals(1999, pos.y, 0.5);

        window.scroll(2, 2);
        pos = googStyle.getClientPosition(div);
        assertEquals(1998, pos.x);
        assertRoughlyEquals(1998, pos.y, 0.5);

        window.scroll(100, 100);
        pos = googStyle.getClientPosition(div);
        assertEquals(1900, pos.x);
        assertRoughlyEquals(1900, pos.y, 0.5);
      }
    } finally {
      window.scroll(0, 0);
      document.body.removeChild(div);
    }
  },

  testGetClientPositionOfOrphanElement() {
    const orphanElem = googDom.createElement(TagName.DIV);
    const pos = googStyle.getClientPosition(orphanElem);
    assertEquals(0, pos.x);
    assertEquals(0, pos.y);
  },

  testGetClientPositionEvent() {
    const mockEvent = {};
    mockEvent.clientX = 100;
    mockEvent.clientY = 200;
    /** @suppress {checkTypes} suppression added to enable type checking */
    const pos = googStyle.getClientPosition(mockEvent);
    assertEquals(100, pos.x);
    assertEquals(200, pos.y);
  },

  testGetClientPositionTouchEvent() {
    const mockTouchEvent = {};
    mockTouchEvent.changedTouches = [{}];
    mockTouchEvent.changedTouches[0].clientX = 100;
    mockTouchEvent.changedTouches[0].clientY = 200;

    /** @suppress {checkTypes} suppression added to enable type checking */
    const pos = googStyle.getClientPosition(mockTouchEvent);
    assertEquals(100, pos.x);
    assertEquals(200, pos.y);
  },

  testGetClientPositionEmptyTouchList() {
    const mockTouchEvent = {};

    mockTouchEvent.touches = [];

    mockTouchEvent.changedTouches = [{}];
    mockTouchEvent.changedTouches[0].clientX = 100;
    mockTouchEvent.changedTouches[0].clientY = 200;

    /** @suppress {checkTypes} suppression added to enable type checking */
    const pos = googStyle.getClientPosition(mockTouchEvent);
    assertEquals(100, pos.x);
    assertEquals(200, pos.y);
  },

  testGetClientPositionAbstractedTouchEvent() {
    const mockTouchEvent = {};
    mockTouchEvent.changedTouches = [{}];
    mockTouchEvent.changedTouches[0].clientX = 100;
    mockTouchEvent.changedTouches[0].clientY = 200;

    /** @suppress {checkTypes} suppression added to enable type checking */
    const e = new BrowserEvent(mockTouchEvent);

    const pos = googStyle.getClientPosition(e);
    assertEquals(100, pos.x);
    assertEquals(200, pos.y);
  },

  testGetPageOffsetAbsPositionedElement() {
    const div = googDom.createDom(TagName.DIV);
    div.style.position = 'absolute';
    div.style.left = '100px';
    div.style.top = '200px';
    document.body.appendChild(div);
    const pos = googStyle.getPageOffset(div);
    assertEquals(100, pos.x);
    assertEquals(200, pos.y);
  },

  testGetPageOffsetNestedElements() {
    const innerDiv = googDom.createDom(TagName.DIV);
    innerDiv.style.position = 'relative';
    innerDiv.style.left = '-10px';
    innerDiv.style.top = '-10px';
    const div = googDom.createDom(TagName.DIV);
    div.style.position = 'absolute';
    div.style.left = '150px';
    div.style.top = '250px';
    div.appendChild(innerDiv);
    document.body.appendChild(div);
    const pos = googStyle.getPageOffset(innerDiv);
    assertRoughlyEquals(140, pos.x, 0.1);
    assertRoughlyEquals(240, pos.y, 0.1);
  },

  testGetPageOffsetWithBodyPadding() {
    document.body.style.margin = '40px';
    document.body.style.padding = '60px';
    document.body.style.borderWidth = '70px';
    let div;
    try {
      div = googDom.createDom(TagName.DIV);
      div.style.position = 'absolute';
      div.style.left = '100px';
      div.style.top = '200px';
      // Margin will affect position, but padding and borders should not.
      div.style.margin = '1px';
      div.style.padding = '2px';
      div.style.borderWidth = '3px';
      document.body.appendChild(div);
      const pos = googStyle.getPageOffset(div);
      assertRoughlyEquals(101, pos.x, 0.1);
      assertRoughlyEquals(201, pos.y, 0.1);
    } finally {
      document.body.removeChild(div);
      document.body.style.margin = '';
      document.body.style.padding = '';
      document.body.style.borderWidth = '';
    }
  },

  testGetPageOffsetWithDocumentElementPadding() {
    document.documentElement.style.margin = '40px';
    document.documentElement.style.padding = '60px';
    document.documentElement.style.borderWidth = '70px';
    let div;
    try {
      div = googDom.createDom(TagName.DIV);
      div.style.position = 'absolute';
      div.style.left = '100px';
      div.style.top = '200px';
      // Margin will affect position, but padding and borders should not.
      div.style.margin = '1px';
      div.style.padding = '2px';
      div.style.borderWidth = '3px';
      document.body.appendChild(div);
      const pos = googStyle.getPageOffset(div);
      assertRoughlyEquals(101, pos.x, 0.1);
      assertRoughlyEquals(201, pos.y, 0.1);
    } finally {
      document.body.removeChild(div);
      document.documentElement.style.margin = '';
      document.documentElement.style.padding = '';
      document.documentElement.style.borderWidth = '';
    }
  },

  testGetPageOffsetElementOffscreen() {
    const div = googDom.createDom(TagName.DIV);
    div.style.position = 'absolute';
    div.style.left = '10000px';
    div.style.top = '20000px';
    document.body.appendChild(div);
    window.scroll(0, 0);
    try {
      let pos = googStyle.getPageOffset(div);
      assertEquals(10000, pos.x);
      assertEquals(20000, pos.y);

      // The following tests do not work in IE, due to an
      // obscure off-by-one bug in goog.style.getPageOffset.
      if (!userAgent.IE) {
        window.scroll(1, 1);
        pos = googStyle.getPageOffset(div);
        assertEquals(10000, pos.x);
        assertRoughlyEquals(20000, pos.y, 0.5);

        window.scroll(1000, 2000);
        pos = googStyle.getPageOffset(div);
        assertEquals(10000, pos.x);
        assertRoughlyEquals(20000, pos.y, 1);

        window.scroll(10000, 20000);
        pos = googStyle.getPageOffset(div);
        assertEquals(10000, pos.x);
        assertRoughlyEquals(20000, pos.y, 1);
      }
    }
    // Undo changes.
    finally {
      document.body.removeChild(div);
      window.scroll(0, 0);
    }
  },

  testGetPageOffsetFixedPositionElements() {
    // Skip these tests in certain browsers.
    // position:fixed is not supported in IE before version 7
    if (!userAgent.IE) {
      // Test with a position fixed element
      let div = googDom.createDom(TagName.DIV);
      div.style.position = 'fixed';
      div.style.top = '10px';
      div.style.left = '10px';
      document.body.appendChild(div);
      let pos = googStyle.getPageOffset(div);
      assertEquals(10, pos.x);
      assertEquals(10, pos.y);

      // Test with a position fixed element as parent
      const innerDiv = googDom.createDom(TagName.DIV);
      div = googDom.createDom(TagName.DIV);
      div.style.position = 'fixed';
      div.style.top = '10px';
      div.style.left = '10px';
      div.style.padding = '5px';
      div.appendChild(innerDiv);
      document.body.appendChild(div);
      pos = googStyle.getPageOffset(innerDiv);
      assertEquals(15, pos.x);
      assertEquals(15, pos.y);
    }
  },

  testGetPositionTolerantToNoDocumentElementBorder() {
    // In IE, removing the border on the document element undoes the normal
    // 2-pixel offset.  Ensure that we're correctly compensating for both cases.
    try {
      document.documentElement.style.borderWidth = '0';
      const div = googDom.createDom(TagName.DIV);
      div.style.position = 'absolute';
      div.style.left = '100px';
      div.style.top = '200px';
      document.body.appendChild(div);

      // Test all major positioning methods.
      // Disabled for IE9 and below - IE8 returns dimensions multiplied by 100.
      // IE9 is flaky. See b/22873770.
      expectedFailures.expectFailureFor(
          userAgent.IE && !userAgent.isVersionOrHigher(10));
      try {
        // Test all major positioning methods.
        const pos = googStyle.getClientPosition(div);
        assertEquals(100, pos.x);
        assertRoughlyEquals(200, pos.y, .1);
        const offset = googStyle.getPageOffset(div);
        assertEquals(100, offset.x);
        assertRoughlyEquals(200, offset.y, .1);
      } catch (e) {
        expectedFailures.handleException(e);
      }
    } finally {
      document.documentElement.style.borderWidth = '';
    }
  },

  testSetSize() {
    const el = $('testEl');

    googStyle.setSize(el, 100, 100);
    assertEquals('100px', el.style.width);
    assertEquals('100px', el.style.height);

    googStyle.setSize(el, '50px', '25px');
    assertEquals('should be "50px"', '50px', el.style.width);
    assertEquals('should be "25px"', '25px', el.style.height);

    googStyle.setSize(el, '10ex', '25px');
    assertEquals('10ex', el.style.width);
    assertEquals('25px', el.style.height);

    googStyle.setSize(el, '10%', '25%');
    assertEquals('10%', el.style.width);
    assertEquals('25%', el.style.height);

    // ignores bad units
    googStyle.setSize(el, 0, 0);
    googStyle.setSize(el, '10rainbows', '25rainbows');
    assertEquals('0px', el.style.width);
    assertEquals('0px', el.style.height);

    googStyle.setSize(el, new Size(20, 40));
    assertEquals('20px', el.style.width);
    assertEquals('40px', el.style.height);
  },

  testSetWidthAndHeight() {
    const el = $('testEl');

    // Replicate all of the setSize tests above.

    googStyle.setWidth(el, 100);
    googStyle.setHeight(el, 100);
    assertEquals('100px', el.style.width);
    assertEquals('100px', el.style.height);

    googStyle.setWidth(el, '50px');
    googStyle.setHeight(el, '25px');
    assertEquals('should be "50px"', '50px', el.style.width);
    assertEquals('should be "25px"', '25px', el.style.height);

    googStyle.setWidth(el, '10ex');
    googStyle.setHeight(el, '25px');
    assertEquals('10ex', el.style.width);
    assertEquals('25px', el.style.height);

    googStyle.setWidth(el, '10%');
    googStyle.setHeight(el, '25%');
    assertEquals('10%', el.style.width);
    assertEquals('25%', el.style.height);

    googStyle.setWidth(el, 0);
    googStyle.setHeight(el, 0);
    assertEquals('0px', el.style.width);
    assertEquals('0px', el.style.height);

    googStyle.setWidth(el, 20);
    googStyle.setHeight(el, 40);
    assertEquals('20px', el.style.width);
    assertEquals('40px', el.style.height);

    // Additional tests testing each separately.
    googStyle.setWidth(el, '');
    googStyle.setHeight(el, '');
    assertEquals('', el.style.width);
    assertEquals('', el.style.height);

    googStyle.setHeight(el, 20);
    assertEquals('', el.style.width);
    assertEquals('20px', el.style.height);

    googStyle.setWidth(el, 40);
    assertEquals('40px', el.style.width);
    assertEquals('20px', el.style.height);
  },

  testGetSize() {
    let el = $('testEl');
    googStyle.setSize(el, 100, 100);

    let dims = googStyle.getSize(el);
    assertEquals(100, dims.width);
    assertEquals(100, dims.height);

    googStyle.setStyle(el, 'display', 'none');
    dims = googStyle.getSize(el);
    assertEquals(100, dims.width);
    assertEquals(100, dims.height);

    el = $('testEl5');
    googStyle.setSize(el, 100, 100);
    dims = googStyle.getSize(el);
    assertEquals(100, dims.width);
    assertEquals(100, dims.height);

    el = $('span0');
    dims = googStyle.getSize(el);
    assertNotEquals(0, dims.width);
    assertNotEquals(0, dims.height);

    el = $('table1');
    dims = googStyle.getSize(el);
    assertNotEquals(0, dims.width);
    assertNotEquals(0, dims.height);

    el = $('td1');
    dims = googStyle.getSize(el);
    assertNotEquals(0, dims.width);
    assertNotEquals(0, dims.height);

    el = $('li1');
    dims = googStyle.getSize(el);
    assertNotEquals(0, dims.width);
    assertNotEquals(0, dims.height);

    el = googDom.getElementsByTagNameAndClass(TagName.HTML)[0];
    dims = googStyle.getSize(el);
    assertNotEquals(0, dims.width);
    assertNotEquals(0, dims.height);

    el = googDom.getElementsByTagNameAndClass(TagName.BODY)[0];
    dims = googStyle.getSize(el);
    assertNotEquals(0, dims.width);
    assertNotEquals(0, dims.height);
  },

  testGetSizeSvgElements() {
    const svgEl = document.createElementNS &&
        document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    if (!svgEl || svgEl.getAttribute('transform') == '' ||
        (userAgent.WEBKIT && !userAgent.isVersionOrHigher(534.8))) {
      // SVG not supported, or getBoundingClientRect not supported on SVG
      // elements.
      return;
    }

    document.body.appendChild(svgEl);
    const el = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
    el.setAttribute('x', 10);
    el.setAttribute('y', 10);
    el.setAttribute('width', 32);
    el.setAttribute('height', 21);
    el.setAttribute('fill', '#000');

    svgEl.appendChild(el);

    // The bounding size in 1 larger than the SVG element in IE.
    const expectedWidth = (userAgent.EDGE_OR_IE) ? 33 : 32;
    const expectedHeight = (userAgent.EDGE_OR_IE) ? 22 : 21;

    let dims = googStyle.getSize(el);
    assertEquals(expectedWidth, dims.width);
    assertRoughlyEquals(expectedHeight, dims.height, 0.01);

    dims = googStyle.getSize(svgEl);
    // The size of the <svg> will be the viewport size on all browsers. This
    // used to not be true for Firefox, but they fixed the glitch in Firefox 33.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=530985
    assertTrue(dims.width >= expectedWidth);
    assertTrue(dims.height >= expectedHeight);

    el.style.visibility = 'none';

    dims = googStyle.getSize(el);
    assertEquals(expectedWidth, dims.width);
    assertRoughlyEquals(expectedHeight, dims.height, 0.01);

    dims = googStyle.getSize(svgEl);
    assertTrue(dims.width >= expectedWidth);
    assertTrue(dims.height >= expectedHeight);
  },

  testGetSizeSvgDocument() {
    const svgEl = document.createElementNS &&
        document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    if (!svgEl || svgEl.getAttribute('transform') == '' ||
        (userAgent.WEBKIT && !userAgent.isVersionOrHigher(534.8))) {
      // SVG not supported, or getBoundingClientRect not supported on SVG
      // elements.
      return;
    }

    const frame = googDom.getElement('svg-frame');
    const doc = googDom.getFrameContentDocument(frame);
    const rect = doc.getElementById('rect');
    const dims = googStyle.getSize(rect);
    if (userAgent.GECKO && userAgent.isVersionOrHigher(53) &&
        !userAgent.isVersionOrHigher(68)) {
      // Firefox >= 53 < 68 auto-scales iframe SVG content to fit the frame
      // b/38432885 | https://bugzilla.mozilla.org/show_bug.cgi?id=1366126
      assertEquals(75, dims.width);
      assertEquals(75, dims.height);
    } else if (!userAgent.EDGE_OR_IE) {
      assertEquals(50, dims.width);
      assertEquals(50, dims.height);
    } else {
      assertEquals(51, dims.width);
      assertEquals(51, dims.height);
    }
  },

  testGetSizeInlineBlock() {
    const el = $('height-test-inner');
    const dims = googStyle.getSize(el);
    assertNotEquals(0, dims.height);
  },

  testGetSizeTransformedRotated() {
    if (!hasWebkitTransform()) return;

    const el = $('rotated');
    googStyle.setSize(el, 300, 200);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const noRotateDims = googStyle.getTransformedSize(el);
    assertEquals(300, noRotateDims.width);
    assertEquals(200, noRotateDims.height);

    el.style.webkitTransform = 'rotate(180deg)';
    const rotate180Dims = googStyle.getTransformedSize(el);
    assertEquals(300, rotate180Dims.width);
    assertEquals(200, rotate180Dims.height);

    el.style.webkitTransform = 'rotate(90deg)';
    const rotate90ClockwiseDims = googStyle.getTransformedSize(el);
    assertEquals(200, rotate90ClockwiseDims.width);
    assertEquals(300, rotate90ClockwiseDims.height);

    el.style.webkitTransform = 'rotate(-90deg)';
    const rotate90CounterClockwiseDims = googStyle.getTransformedSize(el);
    assertEquals(200, rotate90CounterClockwiseDims.width);
    assertEquals(300, rotate90CounterClockwiseDims.height);
  },

  testGetSizeTransformedScaled() {
    if (!hasWebkitTransform()) return;

    const el = $('scaled');
    googStyle.setSize(el, 300, 200);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const noScaleDims = googStyle.getTransformedSize(el);
    assertEquals(300, noScaleDims.width);
    assertEquals(200, noScaleDims.height);

    el.style.webkitTransform = 'scale(2, 0.5)';
    const scaledDims = googStyle.getTransformedSize(el);
    assertEquals(600, scaledDims.width);
    assertEquals(100, scaledDims.height);
  },

  testGetSizeOfOrphanElement() {
    const orphanElem = googDom.createElement(TagName.DIV);
    const size = googStyle.getSize(orphanElem);
    assertEquals(0, size.width);
    assertEquals(0, size.height);
  },

  testGetBounds() {
    const el = $('testEl');

    const dims = googStyle.getSize(el);
    const pos = googStyle.getPageOffset(el);

    const rect = googStyle.getBounds(el);

    // Relies on getSize and getPageOffset being correct.
    assertEquals(dims.width, rect.width);
    assertEquals(dims.height, rect.height);
    assertEquals(pos.x, rect.left);
    assertEquals(pos.y, rect.top);
  },

  testInstallSafeStyleSheet() {
    const el = $('installTest0');
    const originalBackground = googStyle.getBackgroundColor(el);

    // Uses background-color because it's easy to get the computed value
    const result =
        googStyle.installSafeStyleSheet(testing.newSafeStyleSheetForTest(
            '#installTest0 { background-color: rgb(255, 192, 203); }'));

    assertColorRgbEquals('rgb(255,192,203)', googStyle.getBackgroundColor(el));

    googStyle.uninstallStyles(result);
    assertEquals(originalBackground, googStyle.getBackgroundColor(el));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testInstallSafeStyleSheetWithNonce() {
    // IE < 11 doesn't support nonce-based CSP
    if (userAgent.IE && !userAgent.isVersionOrHigher(11)) {
      return;
    }
    const result =
        googStyle.installSafeStyleSheet(testing.newSafeStyleSheetForTest(''));

    const styles = document.head.querySelectorAll('style[nonce]');
    assert(styles.length > 1);
    assertEquals('NONCE', styles[styles.length - 1].getAttribute('nonce'));

    googStyle.uninstallStyles(result);
  },

  testSetSafeStyleSheet() {
    const el = $('installTest1');

    // Change to pink
    const ss = googStyle.installSafeStyleSheet(testing.newSafeStyleSheetForTest(
        '#installTest1 { background-color: rgb(255, 192, 203); }'));

    assertColorRgbEquals('rgb(255,192,203)', googStyle.getBackgroundColor(el));

    // Now change to orange
    googStyle.setSafeStyleSheet(
        ss,
        testing.newSafeStyleSheetForTest(
            '#installTest1 { background-color: rgb(255, 255, 0); }'));
    assertColorRgbEquals('rgb(255,255,0)', googStyle.getBackgroundColor(el));
  },

  testIsRightToLeft() {
    assertFalse(googStyle.isRightToLeft($('rtl1')));
    assertTrue(googStyle.isRightToLeft($('rtl2')));
    assertFalse(googStyle.isRightToLeft($('rtl3')));
    assertFalse(googStyle.isRightToLeft($('rtl4')));
    assertTrue(googStyle.isRightToLeft($('rtl5')));
    assertFalse(googStyle.isRightToLeft($('rtl6')));
    assertTrue(googStyle.isRightToLeft($('rtl7')));
    assertFalse(googStyle.isRightToLeft($('rtl8')));
    assertTrue(googStyle.isRightToLeft($('rtl9')));
    assertFalse(googStyle.isRightToLeft($('rtl10')));
  },

  testIsUnselectable() {
    assertEquals(
        userAgent.GECKO, googStyle.isUnselectable($('unselectable-gecko')));
    assertEquals(userAgent.IE, googStyle.isUnselectable($('unselectable-ie')));
    // Note: Firefox can go either way here - newer versions see -webkit-*
    // properties and automatically add Moz* to the style object.
    if (!userAgent.GECKO) {
      assertEquals(
          userAgent.WEBKIT || userAgent.EDGE,
          googStyle.isUnselectable($('unselectable-webkit')));
    }
  },

  testSetUnselectable() {
    const el = $('make-unselectable');
    assertFalse(googStyle.isUnselectable(el));

    function assertDescendantsUnselectable(unselectable) {
      Array.prototype.forEach.call(el.getElementsByTagName('*'), descendant => {
        // Skip MathML or any other elements that do not have a style property.
        if (descendant.style) {
          assertEquals(unselectable, googStyle.isUnselectable(descendant));
        }
      });
    }

    googStyle.setUnselectable(el, true);
    assertTrue(googStyle.isUnselectable(el));
    assertDescendantsUnselectable(true);

    googStyle.setUnselectable(el, false);
    assertFalse(googStyle.isUnselectable(el));
    assertDescendantsUnselectable(false);

    googStyle.setUnselectable(el, true, true);
    assertTrue(googStyle.isUnselectable(el));
    assertDescendantsUnselectable(false);

    googStyle.setUnselectable(el, false, true);
    assertFalse(googStyle.isUnselectable(el));
    assertDescendantsUnselectable(false);
  },

  testPosWithAbsoluteAndScroll() {
    const el = $('pos-scroll-abs');
    const el1 = $('pos-scroll-abs-1');
    const el2 = $('pos-scroll-abs-2');

    el1.scrollTop = 200;
    const pos = googStyle.getPageOffset(el2);

    assertEquals(200, pos.x);
    // Don't bother with IE in quirks mode
    if (!userAgent.IE || document.compatMode == 'CSS1Compat') {
      assertRoughlyEquals(300, pos.y, .1);
    }
  },

  testPosWithAbsoluteAndWindowScroll() {
    window.scrollBy(0, 200);
    const el = $('abs-upper-left');
    const pos = googStyle.getPageOffset(el);
    assertRoughlyEquals('Top should be about 0', 0, pos.y, 0.1);
  },

  testGetBorderBoxSize() {
    // Strict mode
    const getBorderBoxSize = googStyle.getBorderBoxSize;

    let el = $('size-a');
    let rect = getBorderBoxSize(el);
    assertEquals('width:100px', 100, rect.width);
    assertEquals('height:100px', 100, rect.height);

    // with border: 10px
    el = $('size-b');
    rect = getBorderBoxSize(el);
    assertEquals(
        'width:100px;border:10px', isBorderBox ? 100 : 120, rect.width);
    assertEquals(
        'height:100px;border:10px', isBorderBox ? 100 : 120, rect.height);

    // with border: 10px; padding: 10px
    el = $('size-c');
    rect = getBorderBoxSize(el);
    assertEquals(
        'width:100px;border:10px;padding:10px', isBorderBox ? 100 : 140,
        rect.width);
    assertEquals(
        'height:100px;border:10px;padding:10px', isBorderBox ? 100 : 140,
        rect.height);

    // size, padding and borders are all in non pixel units
    // all we test here is that we get a number out
    el = $('size-d');
    rect = getBorderBoxSize(el);
    assertEquals('number', typeof rect.width);
    assertEquals('number', typeof rect.height);
    assertFalse(isNaN(rect.width));
    assertFalse(isNaN(rect.height));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testGetContentBoxSize() {
    // Strict mode
    const getContentBoxSize = googStyle.getContentBoxSize;

    let el = $('size-a');
    let rect = getContentBoxSize(el);
    assertEquals('width:100px', 100, rect.width);
    assertEquals('height:100px', 100, rect.height);

    // with border: 10px
    el = $('size-b');
    rect = getContentBoxSize(el);
    assertEquals('width:100px;border:10px', isBorderBox ? 80 : 100, rect.width);
    assertEquals(
        'height:100px;border:10px', isBorderBox ? 80 : 100, rect.height);

    // with border: 10px; padding: 10px
    el = $('size-c');
    rect = getContentBoxSize(el);
    assertEquals(
        'width:100px;border:10px;padding:10px', isBorderBox ? 60 : 100,
        rect.width);
    assertEquals(
        'height:100px;border:10px;padding:10px', isBorderBox ? 60 : 100,
        rect.height);

    // size, padding and borders are all in non pixel units
    // all we test here is that we get a number out
    el = $('size-d');
    rect = getContentBoxSize(el);
    assertEquals('number', typeof rect.width);
    assertEquals('number', typeof rect.height);
    assertFalse(isNaN(rect.width));
    assertFalse(isNaN(rect.height));

    // test whether getContentBoxSize works when width and height
    // aren't explicitly set, but the default of 'auto'.
    // 'size-f' has no margin, border, or padding, so offsetWidth/Height
    // should match the content box size
    el = $('size-f');
    rect = getContentBoxSize(el);
    assertEquals(el.offsetWidth, rect.width);
    assertEquals(el.offsetHeight, rect.height);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSetBorderBoxSize() {
    // Strict mode
    const el = $('size-e');
    const setBorderBoxSize = googStyle.setBorderBoxSize;

    // Clean up
    // style element has 100x100, no border and no padding
    el.style.padding = '';
    el.style.margin = '';
    el.style.borderWidth = '';
    el.style.width = '';
    el.style.height = '';

    setBorderBoxSize(el, new Size(100, 100));

    assertEquals(100, el.offsetWidth);
    assertEquals(100, el.offsetHeight);

    el.style.borderWidth = '10px';
    setBorderBoxSize(el, new Size(100, 100));

    assertEquals('width:100px;border:10px', 100, el.offsetWidth);
    assertEquals('height:100px;border:10px', 100, el.offsetHeight);

    el.style.padding = '10px';
    setBorderBoxSize(el, new Size(100, 100));
    assertEquals(100, el.offsetWidth);
    assertEquals(100, el.offsetHeight);

    el.style.borderWidth = '0';
    setBorderBoxSize(el, new Size(100, 100));
    assertEquals(100, el.offsetWidth);
    assertEquals(100, el.offsetHeight);

    if (userAgent.GECKO) {
      assertEquals('border-box', el.style.MozBoxSizing);
    } else if (userAgent.WEBKIT) {
      assertEquals('border-box', el.style.WebkitBoxSizing);
    } else if (
        userAgent.IE && userAgent.isDocumentModeOrHigher(8)) {
      assertEquals('border-box', el.style.boxSizing);
    }

    // Try a negative width/height.
    setBorderBoxSize(el, new Size(-10, -10));

    // Setting the border box smaller than the borders will just give you
    // a content box of size 0.
    // NOTE(nicksantos): I'm not really sure why IE7 is special here.
    const isIeLt8Quirks = userAgent.IE &&
        !userAgent.isDocumentModeOrHigher(8) && !googDom.isCss1CompatMode();
    assertEquals(20, el.offsetWidth);
    assertEquals(isIeLt8Quirks ? 39 : 20, el.offsetHeight);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSetContentBoxSize() {
    // Strict mode
    const el = $('size-e');
    const setContentBoxSize = googStyle.setContentBoxSize;

    // Clean up
    // style element has 100x100, no border and no padding
    el.style.padding = '';
    el.style.margin = '';
    el.style.borderWidth = '';
    el.style.width = '';
    el.style.height = '';

    setContentBoxSize(el, new Size(100, 100));

    assertEquals(100, el.offsetWidth);
    assertEquals(100, el.offsetHeight);

    el.style.borderWidth = '10px';
    setContentBoxSize(el, new Size(100, 100));
    assertEquals('width:100px;border-width:10px', 120, el.offsetWidth);
    assertEquals('height:100px;border-width:10px', 120, el.offsetHeight);

    el.style.padding = '10px';
    setContentBoxSize(el, new Size(100, 100));
    assertEquals(
        'width:100px;border-width:10px;padding:10px', 140, el.offsetWidth);
    assertEquals(
        'height:100px;border-width:10px;padding:10px', 140, el.offsetHeight);

    el.style.borderWidth = '0';
    setContentBoxSize(el, new Size(100, 100));
    assertEquals('width:100px;padding:10px', 120, el.offsetWidth);
    assertEquals('height:100px;padding:10px', 120, el.offsetHeight);

    if (userAgent.GECKO) {
      assertEquals('content-box', el.style.MozBoxSizing);
    } else if (userAgent.WEBKIT) {
      assertEquals('content-box', el.style.WebkitBoxSizing);
    } else if (
        userAgent.IE && userAgent.isDocumentModeOrHigher(8)) {
      assertEquals('content-box', el.style.boxSizing);
    }

    // Try a negative width/height.
    setContentBoxSize(el, new Size(-10, -10));

    // NOTE(nicksantos): I'm not really sure why IE7 is special here.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const isIeLt8Quirks = userAgent.IE &&
        !userAgent.isDocumentModeOrHigher('8') && !googDom.isCss1CompatMode();
    assertEquals(20, el.offsetWidth);
    assertEquals(isIeLt8Quirks ? 39 : 20, el.offsetHeight);
  },

  testGetPaddingBox() {
    // Strict mode
    const el = $('size-e');
    const getPaddingBox = googStyle.getPaddingBox;

    // Clean up
    // style element has 100x100, no border and no padding
    el.style.padding = '';
    el.style.margin = '';
    el.style.borderWidth = '';
    el.style.width = '';
    el.style.height = '';

    el.style.padding = '10px';
    let rect = getPaddingBox(el);
    assertEquals(10, rect.left);
    assertEquals(10, rect.right);
    assertEquals(10, rect.top);
    assertEquals(10, rect.bottom);

    el.style.padding = '0';
    rect = getPaddingBox(el);
    assertEquals(0, rect.left);
    assertEquals(0, rect.right);
    assertEquals(0, rect.top);
    assertEquals(0, rect.bottom);

    el.style.padding = '1px 2px 3px 4px';
    rect = getPaddingBox(el);
    assertEquals(1, rect.top);
    assertEquals(2, rect.right);
    assertEquals(3, rect.bottom);
    assertEquals(4, rect.left);

    el.style.padding = '1mm 2em 3ex 4%';
    rect = getPaddingBox(el);
    assertFalse(isNaN(rect.top));
    assertFalse(isNaN(rect.right));
    assertFalse(isNaN(rect.bottom));
    assertFalse(isNaN(rect.left));
    assertTrue(rect.top >= 0);
    assertTrue(rect.right >= 0);
    assertTrue(rect.bottom >= 0);
    assertTrue(rect.left >= 0);
  },

  testGetPaddingBoxUnattached() {
    const el = googDom.createElement(TagName.DIV);
    const box = googStyle.getPaddingBox(el);
    if (userAgent.WEBKIT ||
        (userAgent.GECKO && userAgent.isVersionOrHigher(64))) {
      assertTrue(isNaN(box.top));
      assertTrue(isNaN(box.right));
      assertTrue(isNaN(box.bottom));
      assertTrue(isNaN(box.left));
    } else {
      assertObjectEquals(new Box(0, 0, 0, 0), box);
    }
  },

  testGetMarginBox() {
    // Strict mode
    const el = $('size-e');
    const getMarginBox = googStyle.getMarginBox;

    // Clean up
    // style element has 100x100, no border and no padding
    el.style.padding = '';
    el.style.margin = '';
    el.style.borderWidth = '';
    el.style.width = '';
    el.style.height = '';

    el.style.margin = '10px';
    let rect = getMarginBox(el);
    assertEquals(10, rect.left);
    // In webkit the right margin is the calculated distance from right edge and
    // not the computed right margin so it is not reliable.
    // See https://bugs.webkit.org/show_bug.cgi?id=19828
    if (!userAgent.WEBKIT) {
      assertEquals(10, rect.right);
    }
    assertEquals(10, rect.top);
    assertEquals(10, rect.bottom);

    el.style.margin = '0';
    rect = getMarginBox(el);
    assertEquals(0, rect.left);
    // In webkit the right margin is the calculated distance from right edge and
    // not the computed right margin so it is not reliable.
    // See https://bugs.webkit.org/show_bug.cgi?id=19828
    if (!userAgent.WEBKIT) {
      assertEquals(0, rect.right);
    }
    assertEquals(0, rect.top);
    assertEquals(0, rect.bottom);

    el.style.margin = '1px 2px 3px 4px';
    rect = getMarginBox(el);
    assertEquals(1, rect.top);
    // In webkit the right margin is the calculated distance from right edge and
    // not the computed right margin so it is not reliable.
    // See https://bugs.webkit.org/show_bug.cgi?id=19828
    if (!userAgent.WEBKIT) {
      assertEquals(2, rect.right);
    }
    assertEquals(3, rect.bottom);
    assertEquals(4, rect.left);

    el.style.margin = '1mm 2em 3ex 4%';
    rect = getMarginBox(el);
    assertFalse(isNaN(rect.top));
    assertFalse(isNaN(rect.right));
    assertFalse(isNaN(rect.bottom));
    assertFalse(isNaN(rect.left));
    assertTrue(rect.top >= 0);
    // In webkit the right margin is the calculated distance from right edge and
    // not the computed right margin so it is not reliable.
    // See https://bugs.webkit.org/show_bug.cgi?id=19828
    if (!userAgent.WEBKIT) {
      assertTrue(rect.right >= 0);
    }
    assertTrue(rect.bottom >= 0);
    assertTrue(rect.left >= 0);
  },

  testGetBorderBox() {
    // Strict mode
    const el = $('size-e');
    const getBorderBox = googStyle.getBorderBox;

    // Clean up
    // style element has 100x100, no border and no padding
    el.style.padding = '';
    el.style.margin = '';
    el.style.borderWidth = '';
    el.style.width = '';
    el.style.height = '';

    el.style.borderWidth = '10px';
    let rect = getBorderBox(el);
    assertEquals(10, rect.left);
    assertEquals(10, rect.right);
    assertEquals(10, rect.top);
    assertEquals(10, rect.bottom);

    el.style.borderWidth = '0';
    rect = getBorderBox(el);
    assertEquals(0, rect.left);
    assertEquals(0, rect.right);
    assertEquals(0, rect.top);
    assertEquals(0, rect.bottom);

    el.style.borderWidth = '1px 2px 3px 4px';
    rect = getBorderBox(el);
    assertEquals(1, rect.top);
    assertEquals(2, rect.right);
    assertEquals(3, rect.bottom);
    assertEquals(4, rect.left);

    // % does not work for border widths in IE
    el.style.borderWidth = '1mm 2em 3ex 4pt';
    rect = getBorderBox(el);
    assertFalse(isNaN(rect.top));
    assertFalse(isNaN(rect.right));
    assertFalse(isNaN(rect.bottom));
    assertFalse(isNaN(rect.left));
    assertTrue(rect.top >= 0);
    assertTrue(rect.right >= 0);
    assertTrue(rect.bottom >= 0);
    assertTrue(rect.left >= 0);

    el.style.borderWidth = 'thin medium thick 1px';
    rect = getBorderBox(el);
    assertFalse(isNaN(rect.top));
    assertFalse(isNaN(rect.right));
    assertFalse(isNaN(rect.bottom));
    assertFalse(isNaN(rect.left));
    assertTrue(rect.top >= 0);
    assertTrue(rect.right >= 0);
    assertTrue(rect.bottom >= 0);
    assertTrue(rect.left >= 0);
  },

  testGetFontFamily() {
    // I tried to use common fonts for these tests. It's possible the test fails
    // because the testing platform doesn't have one of these fonts installed:
    //   Comic Sans MS or Century Schoolbook L
    //   Times
    //   Helvetica

    let tmpFont = googStyle.getFontFamily($('font-tag'));
    assertTrue(
        'FontFamily should be detectable when set via <font face>',
        'Times' == tmpFont || 'Times New Roman' == tmpFont);
    tmpFont = googStyle.getFontFamily($('small-text'));
    assertTrue(
        'Multiword fonts should be reported with quotes stripped.',
        'Comic Sans MS' == tmpFont || 'Century Schoolbook L' == tmpFont);
    // Firefox fails this test & retuns a generic 'monospace' instead of the
    // actually displayed font (e.g., "Times New").
    // tmpFont = goog.style.getFontFamily($('pre-font'));
    // assertEquals('<pre> tags should use a fixed-width font',
    //             'Times New',
    //             tmpFont);
    tmpFont = googStyle.getFontFamily($('inherit-font'));
    assertEquals(
        'Explicitly inherited fonts should be detectable', 'Helvetica',
        tmpFont);
    tmpFont = googStyle.getFontFamily($('times-font-family'));
    assertEquals(
        'Font-family set via style attribute should be detected', 'Times',
        tmpFont);
    tmpFont = googStyle.getFontFamily($('bold-font'));
    assertEquals(
        'Implicitly inherited font should be detected', 'Helvetica', tmpFont);
    tmpFont = googStyle.getFontFamily($('css-html-tag-redefinition'));
    assertEquals('HTML tag CSS rewrites should be detected', 'Times', tmpFont);
    tmpFont = googStyle.getFontFamily($('no-text-font-styles'));
    assertEquals(
        'Font family should exist even with no text', 'Helvetica', tmpFont);
    tmpFont = googStyle.getFontFamily($('icon-font'));
    assertNotEquals(
        'icon is a special font-family value', 'icon', tmpFont.toLowerCase());
    tmpFont = googStyle.getFontFamily($('font-style-badfont'));
    // Firefox fails this test and reports the specified "badFont", which is
    // obviously not displayed.
    // assertEquals('Invalid fonts should not be returned',
    //             'Helvetica',
    //             tmpFont);
    tmpFont = googStyle.getFontFamily($('img-font-test'));
    assertTrue(
        'Even img tags should inherit the document body\'s font',
        tmpFont != '');
    tmpFont = googStyle.getFontFamily($('nested-font'));
    assertEquals(
        'An element with nested content should be unaffected.', 'Arial',
        tmpFont);
    // IE raises an 'Invalid Argument' error when using the moveToElementText
    // method from the TextRange object with an element that is not attached to
    // a document.
    const element = googDom.createDom(
        TagName.SPAN, {style: 'font-family:Times,sans-serif;'}, 'some text');
    tmpFont = googStyle.getFontFamily(element);
    assertEquals(
        'Font should be correctly retrieved for element not attached' +
            ' to a document',
        'Times', tmpFont);
  },

  testGetFontSize() {
    assertEquals(
        'Font size should be determined even without any text', 30,
        googStyle.getFontSize($('no-text-font-styles')));
    assertEquals(
        'A 5em font should be 5x larger than its parent.', 150,
        googStyle.getFontSize($('css-html-tag-redefinition')));
    assertTrue(
        'Setting font size=-1 should result in a positive font size.',
        googStyle.getFontSize($('font-tag')) > 0);
    assertEquals(
        'Inheriting a 50% font-size should have no additional effect',
        googStyle.getFontSize($('font-style-badfont')),
        googStyle.getFontSize($('inherit-50pct-font')));
    assertTrue(
        'In pretty much any display, 3in should be > 8px',
        googStyle.getFontSize($('times-font-family')) >
            googStyle.getFontSize($('no-text-font-styles')));
    assertTrue(
        'With no applied styles, font-size should still be defined.',
        googStyle.getFontSize($('no-font-style')) > 0);
    assertEquals(
        '50% of 30px is 15', 15,
        googStyle.getFontSize($('font-style-badfont')));
    assertTrue(
        'x-small text should be smaller than small text',
        googStyle.getFontSize($('x-small-text')) <
            googStyle.getFontSize($('small-text')));
    // IE fails this test, the decimal portion of px lengths isn't reported
    // by getCascadedStyle. Firefox passes, but only because it ignores the
    // decimals altogether.
    // assertEquals('12.5px should be the same as 0.5em nested in a 25px node.',
    //             goog.style.getFontSize($('font-size-12-point-5-px')),
    //             goog.style.getFontSize($('font-size-50-pct-of-25-px')));

    assertEquals(
        'Font size should not doubly count em values', 2,
        googStyle.getFontSize($('em-font-size')));
  },

  testGetLengthUnits() {
    assertEquals('px', googStyle.getLengthUnits('15px'));
    assertEquals('%', googStyle.getLengthUnits('99%'));
    assertNull(googStyle.getLengthUnits(''));
  },

  testParseStyleAttribute() {
    const css = 'left: 0px; text-align: center';
    const expected = {'left': '0px', 'textAlign': 'center'};

    assertObjectEquals(expected, googStyle.parseStyleAttribute(css));
  },

  testToStyleAttribute() {
    const object = {'left': '0px', 'textAlign': 'center'};
    const expected = 'left:0px;text-align:center;';

    assertEquals(expected, googStyle.toStyleAttribute(object));
  },

  testStyleAttributePassthrough() {
    const object = {'left': '0px', 'textAlign': 'center'};

    assertObjectEquals(
        object,
        googStyle.parseStyleAttribute(googStyle.toStyleAttribute(object)));
  },

  testGetFloat() {
    assertEquals('', googStyle.getFloat($('no-float')));
    assertEquals('none', googStyle.getFloat($('float-none')));
    assertEquals('left', googStyle.getFloat($('float-left')));
  },

  testSetFloat() {
    const el = $('float-test');

    googStyle.setFloat(el, 'left');
    assertEquals('left', googStyle.getFloat(el));

    googStyle.setFloat(el, 'right');
    assertEquals('right', googStyle.getFloat(el));

    googStyle.setFloat(el, 'none');
    assertEquals('none', googStyle.getFloat(el));

    googStyle.setFloat(el, '');
    assertEquals('', googStyle.getFloat(el));
  },

  testIsElementShown() {
    const el = $('testEl');

    googStyle.setElementShown(el, false);
    assertFalse(googStyle.isElementShown(el));

    googStyle.setElementShown(el, true);
    assertTrue(googStyle.isElementShown(el));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetOpacity() {
    const el1 = {style: {opacity: '0.3'}};

    const el2 = {style: {MozOpacity: '0.1'}};

    const el3 = {
      style: {filter: 'some:other,filter;alpha(opacity=25.5);alpha(more=100);'},
    };

    assertEquals(0.3, googStyle.getOpacity(el1));
    assertEquals(0.1, googStyle.getOpacity(el2));
    assertEquals(0.255, googStyle.getOpacity(el3));

    el1.style.opacity = '0';
    el2.style.MozOpacity = '0';
    el3.style.filter = 'some:other,filter;alpha(opacity=0);alpha(more=100);';

    assertEquals(0, googStyle.getOpacity(el1));
    assertEquals(0, googStyle.getOpacity(el2));
    assertEquals(0, googStyle.getOpacity(el3));

    el1.style.opacity = '';
    el2.style.MozOpacity = '';
    el3.style.filter = '';

    assertEquals('', googStyle.getOpacity(el1));
    assertEquals('', googStyle.getOpacity(el2));
    assertEquals('', googStyle.getOpacity(el3));

    const el4 = {style: {}};

    assertEquals('', googStyle.getOpacity(el4));
    assertEquals('', googStyle.getOpacity($('test-opacity')));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetOpacity() {
    const el1 = {style: {opacity: '0.3'}};
    googStyle.setOpacity(el1, 0.8);

    const el2 = {style: {MozOpacity: '0.1'}};
    googStyle.setOpacity(el2, 0.5);

    const el3 = {style: {filter: 'alpha(opacity=25)'}};
    googStyle.setOpacity(el3, 0.1);

    assertEquals(0.8, Number(el1.style.opacity));
    assertEquals(0.5, Number(el2.style.MozOpacity));
    assertEquals('alpha(opacity=10)', el3.style.filter);

    googStyle.setOpacity(el1, 0);
    googStyle.setOpacity(el2, 0);
    googStyle.setOpacity(el3, 0);

    assertEquals(0, Number(el1.style.opacity));
    assertEquals(0, Number(el2.style.MozOpacity));
    assertEquals('alpha(opacity=0)', el3.style.filter);

    googStyle.setOpacity(el1, '');
    googStyle.setOpacity(el2, '');
    googStyle.setOpacity(el3, '');

    assertEquals('', el1.style.opacity);
    assertEquals('', el2.style.MozOpacity);
    assertEquals('', el3.style.filter);
  },

  testFramedPageOffset() {
    // Set up a complicated iframe ancestor chain.
    const iframe = googDom.getElement('test-frame-offset');
    const iframeDoc = googDom.getFrameContentDocument(iframe);
    const iframeWindow = googDom.getWindow(iframeDoc);

    const iframePos = 'style="display:block;position:absolute;' +
        'top:50px;left:50px;width:50px;height:50px;"';
    iframeDoc.write(
        `<iframe id="test-frame-offset-2" ${iframePos}></iframe>` +
        '<div id="test-element-2" ' +
        ' style="position:absolute;left:300px;top:300px">hi mom!</div>');
    iframeDoc.close();
    const iframe2 = iframeDoc.getElementById('test-frame-offset-2');
    const testElement2 = iframeDoc.getElementById('test-element-2');
    const iframeDoc2 = googDom.getFrameContentDocument(iframe2);
    const iframeWindow2 = googDom.getWindow(iframeDoc2);

    iframeDoc2.write(
        '<div id="test-element-3" ' +
        ' style="position:absolute;left:500px;top:500px">hi mom!</div>');
    iframeDoc2.close();
    const testElement3 = iframeDoc2.getElementById('test-element-3');

    assertCoordinateApprox(300, 300, 0, googStyle.getPageOffset(testElement2));
    assertCoordinateApprox(500, 500, 0, googStyle.getPageOffset(testElement3));

    assertCoordinateApprox(
        350, 350, 0, googStyle.getFramedPageOffset(testElement2, window));
    assertCoordinateApprox(
        300, 300, 0, googStyle.getFramedPageOffset(testElement2, iframeWindow));

    assertCoordinateApprox(
        600, 600, 0, googStyle.getFramedPageOffset(testElement3, window));
    assertCoordinateApprox(
        550, 550, 0, googStyle.getFramedPageOffset(testElement3, iframeWindow));
    assertCoordinateApprox(
        500, 500, 0,
        googStyle.getFramedPageOffset(testElement3, iframeWindow2));

    // Scroll the iframes a bit.
    window.scrollBy(0, 5);
    iframeWindow.scrollBy(0, 11);
    iframeWindow2.scrollBy(0, 18);

    // On Firefox 2, scrolling inner iframes causes off by one errors
    // in the page position, because we're using screen coords to compute them.
    assertCoordinateApprox(300, 300, 2, googStyle.getPageOffset(testElement2));
    assertCoordinateApprox(500, 500, 2, googStyle.getPageOffset(testElement3));

    assertCoordinateApprox(
        350, 350 - 11, 2, googStyle.getFramedPageOffset(testElement2, window));
    assertCoordinateApprox(
        300, 300, 2, googStyle.getFramedPageOffset(testElement2, iframeWindow));

    assertCoordinateApprox(
        600, 600 - 18 - 11, 2,
        googStyle.getFramedPageOffset(testElement3, window));
    assertCoordinateApprox(
        550, 550 - 18, 2,
        googStyle.getFramedPageOffset(testElement3, iframeWindow));
    assertCoordinateApprox(
        500, 500, 2,
        googStyle.getFramedPageOffset(testElement3, iframeWindow2));

    // In IE, if the element is in a frame that's been removed from the DOM and
    // relativeWin is not that frame's contentWindow, the contentWindow's parent
    // reference points to itself. We want to guarantee that we don't fall into
    // an infinite loop.
    const iframeParent = iframe.parentElement;
    iframeParent.removeChild(iframe);
    // We don't check the value returned as it differs by browser. 0,0 for
    // Chrome and FF. IE returns 30000 or 30198 for x in IE8-9 and 300 in
    // IE10-11
    googStyle.getFramedPageOffset(testElement2, window);
  },

  testTranslateRectForAnotherFrame() {
    let rect = new GoogRect(1, 2, 3, 4);
    const thisDom = googDom.getDomHelper();
    googStyle.translateRectForAnotherFrame(rect, thisDom, thisDom);
    assertEquals(1, rect.left);
    assertEquals(2, rect.top);
    assertEquals(3, rect.width);
    assertEquals(4, rect.height);

    let iframe = $('test-translate-frame-standard');
    let iframeDoc = googDom.getFrameContentDocument(iframe);
    let iframeDom = googDom.getDomHelper(iframeDoc);
    // Cannot rely on iframe starting at origin.
    iframeDom.getWindow().scrollTo(0, 0);
    // iframe is at (100, 150) and its body is not scrolled.
    rect = new GoogRect(1, 2, 3, 4);
    googStyle.translateRectForAnotherFrame(rect, iframeDom, thisDom);
    assertEquals(1 + 100, rect.left);
    assertRoughlyEquals(2 + 150, rect.top, .1);
    assertEquals(3, rect.width);
    assertEquals(4, rect.height);

    iframeDom.getWindow().scrollTo(11, 13);
    rect = new GoogRect(1, 2, 3, 4);
    googStyle.translateRectForAnotherFrame(rect, iframeDom, thisDom);
    assertEquals(1 + 100 - 11, rect.left);
    assertRoughlyEquals(2 + 150 - 13, rect.top, .1);
    assertEquals(3, rect.width);
    assertEquals(4, rect.height);

    iframe = $('test-translate-frame-quirk');
    iframeDoc = googDom.getFrameContentDocument(iframe);
    iframeDom = googDom.getDomHelper(iframeDoc);
    // Cannot rely on iframe starting at origin.
    iframeDom.getWindow().scrollTo(0, 0);
    // iframe is at (100, 350) and its body is not scrolled.
    rect = new GoogRect(1, 2, 3, 4);
    googStyle.translateRectForAnotherFrame(rect, iframeDom, thisDom);
    assertEquals(1 + 100, rect.left);
    assertRoughlyEquals(2 + 350, rect.top, .1);
    assertEquals(3, rect.width);
    assertEquals(4, rect.height);

    iframeDom.getWindow().scrollTo(11, 13);
    rect = new GoogRect(1, 2, 3, 4);
    googStyle.translateRectForAnotherFrame(rect, iframeDom, thisDom);
    assertEquals(1 + 100 - 11, rect.left);
    assertRoughlyEquals(2 + 350 - 13, rect.top, .1);
    assertEquals(3, rect.width);
    assertEquals(4, rect.height);
  },

  testGetVisibleRectForElement() {
    const container = googDom.getElement('test-visible');
    let el = googDom.getElement('test-visible-el');
    const dom = googDom.getDomHelper(el);
    const winScroll = dom.getDocumentScroll();
    const winSize = dom.getViewportSize();

    // Skip this test if the window size is small.  Firefox3/Linux in Selenium
    // sometimes fails without this check.
    if (winSize.width < 20 || winSize.height < 20) {
      return;
    }

    // Move the container element to the window's viewport.
    const h = winSize.height < 100 ? winSize.height / 2 : 100;
    googStyle.setSize(container, winSize.width / 2, h);
    googStyle.setPosition(container, 8, winScroll.y + winSize.height - h);
    let visible = googStyle.getVisibleRectForElement(el);
    let bounds = googStyle.getBounds(container);
    // VisibleRect == Bounds rect of the offsetParent
    assertNotNull(visible);
    assertEquals(bounds.left, visible.left);
    assertEquals(bounds.top, visible.top);
    assertEquals(bounds.left + bounds.width, visible.right);
    assertEquals(bounds.top + bounds.height, visible.bottom);

    // Move a part of the container element to outside of the viewpoert.
    googStyle.setPosition(container, 8, winScroll.y + winSize.height - h / 2);
    visible = googStyle.getVisibleRectForElement(el);
    bounds = googStyle.getBounds(container);
    // Confirm VisibleRect == Intersection of the bounds rect of the
    // offsetParent and the viewport.
    assertNotNull(visible);
    assertEquals(bounds.left, visible.left);
    assertEquals(bounds.top, visible.top);
    assertEquals(bounds.left + bounds.width, visible.right);
    assertEquals(winScroll.y + winSize.height, visible.bottom);

    // Move the container element to outside of the viewpoert.
    googStyle.setPosition(container, 8, winScroll.y + winSize.height * 2);
    visible = googStyle.getVisibleRectForElement(el);
    assertNull(visible);

    // Test the case with body element of height 0
    const iframe = googDom.getElement('test-visible-frame');
    const iframeDoc = googDom.getFrameContentDocument(iframe);
    el = iframeDoc.getElementById('test-visible');
    visible = googStyle.getVisibleRectForElement(el);

    const iframeViewportSize = googDom.getDomHelper(el).getViewportSize();
    // NOTE(chrishenry): For iframe, the clipping viewport is always the iframe
    // viewport, and not the actual browser viewport.
    assertNotNull(visible);
    assertEquals(0, visible.top);
    assertEquals(iframeViewportSize.height, visible.bottom);
    assertEquals(0, visible.left);
    assertEquals(iframeViewportSize.width, visible.right);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetVisibleRectForElementWithBodyScrolled() {
    const container = googDom.getElement('test-visible2');
    const dom = googDom.getDomHelper(container);
    const el = dom.createDom(TagName.DIV, undefined, 'Test');
    el.style.position = 'absolute';
    dom.append(container, el);

    container.style.position = 'absolute';
    googStyle.setPosition(container, 20, 500);
    googStyle.setSize(container, 100, 150);

    // Scroll body container such that container is exactly at top.
    window.scrollTo(0, 500);
    let visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(500, visibleRect.top, EPSILON);
    assertRoughlyEquals(20, visibleRect.left, EPSILON);
    assertRoughlyEquals(650, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(120, visibleRect.right, EPSILON);

    // Top 100px is clipped by window viewport.
    window.scrollTo(0, 600);
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(600, visibleRect.top, EPSILON);
    assertRoughlyEquals(20, visibleRect.left, EPSILON);
    assertRoughlyEquals(650, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(120, visibleRect.right, EPSILON);

    const winSize = dom.getViewportSize();

    // Left 50px is clipped by window viewport.
    // Right part is clipped by window viewport.
    googStyle.setSize(container, 10000, 150);
    window.scrollTo(70, 500);
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(500, visibleRect.top, EPSILON);
    assertRoughlyEquals(70, visibleRect.left, EPSILON);
    assertRoughlyEquals(650, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(70 + winSize.width, visibleRect.right, EPSILON);

    // Bottom part is clipped by window viewport.
    googStyle.setSize(container, 100, 2000);
    window.scrollTo(0, 500);
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(500, visibleRect.top, EPSILON);
    assertRoughlyEquals(20, visibleRect.left, EPSILON);
    assertRoughlyEquals(120, visibleRect.right, EPSILON);
    assertRoughlyEquals(500 + winSize.height, visibleRect.bottom, EPSILON);

    googStyle.setPosition(container, 10000, 10000);
    assertNull(googStyle.getVisibleRectForElement(el));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetVisibleRectForElementWithNestedAreaAndNonOffsetAncestor() {
    // IE7 quirks mode somehow consider container2 below as offset parent
    // of the element, which is incorrect.
    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(8) &&
        !googDom.isCss1CompatMode()) {
      return;
    }

    const container = googDom.getElement('test-visible2');
    const dom = googDom.getDomHelper(container);
    const container2 = dom.createDom(TagName.DIV);
    const el = dom.createDom(TagName.DIV, undefined, 'Test');
    el.style.position = 'absolute';
    dom.append(container, container2);
    dom.append(container2, el);

    container.style.position = 'absolute';
    googStyle.setPosition(container, 20, 500);
    googStyle.setSize(container, 100, 150);

    // container2 is a scrollable container but is not an offsetParent of
    // the element. It is ignored in the computation.
    container2.style.overflow = 'hidden';
    container2.style.marginTop = '50px';
    container2.style.marginLeft = '100px';
    googStyle.setSize(container2, 150, 100);

    // Scroll body container such that container is exactly at top.
    window.scrollTo(0, 500);
    let visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(500, visibleRect.top, EPSILON);
    assertRoughlyEquals(20, visibleRect.left, EPSILON);
    assertRoughlyEquals(650, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(120, visibleRect.right, EPSILON);

    // Top 100px is clipped by window viewport.
    window.scrollTo(0, 600);
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(600, visibleRect.top, EPSILON);
    assertRoughlyEquals(20, visibleRect.left, EPSILON);
    assertRoughlyEquals(650, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(120, visibleRect.right, EPSILON);

    const winSize = dom.getViewportSize();

    // Left 50px is clipped by window viewport.
    // Right part is clipped by window viewport.
    googStyle.setSize(container, 10000, 150);
    window.scrollTo(70, 500);
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(500, visibleRect.top, EPSILON);
    assertRoughlyEquals(70, visibleRect.left, EPSILON);
    assertRoughlyEquals(650, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(70 + winSize.width, visibleRect.right, EPSILON);

    // Bottom part is clipped by window viewport.
    googStyle.setSize(container, 100, 2000);
    window.scrollTo(0, 500);
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(500, visibleRect.top, EPSILON);
    assertRoughlyEquals(20, visibleRect.left, EPSILON);
    assertRoughlyEquals(120, visibleRect.right, EPSILON);
    assertRoughlyEquals(500 + winSize.height, visibleRect.bottom, EPSILON);

    googStyle.setPosition(container, 10000, 10000);
    assertNull(googStyle.getVisibleRectForElement(el));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetVisibleRectForElementInsideNestedScrollableArea() {
    const container = googDom.getElement('test-visible2');
    const dom = googDom.getDomHelper(container);
    const container2 = dom.createDom(TagName.DIV);
    const el = dom.createDom(TagName.DIV, undefined, 'Test');
    el.style.position = 'absolute';
    dom.append(container, container2);
    dom.append(container2, el);

    container.style.position = 'absolute';
    googStyle.setPosition(container, 100 /* left */, 500 /* top */);
    googStyle.setSize(container, 300 /* width */, 300 /* height */);

    container2.style.overflow = 'hidden';
    container2.style.position = 'relative';
    googStyle.setPosition(container2, 100, 50);
    googStyle.setSize(container2, 150, 100);

    // Scroll body container such that container is exactly at top.
    window.scrollTo(0, 500);
    let visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(550, visibleRect.top, EPSILON);
    assertRoughlyEquals(200, visibleRect.left, EPSILON);
    assertRoughlyEquals(650, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(350, visibleRect.right, EPSILON);

    // Left 50px is clipped by container.
    googStyle.setPosition(container2, -50, 50);
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(550, visibleRect.top, EPSILON);
    assertRoughlyEquals(100, visibleRect.left, EPSILON);
    assertRoughlyEquals(650, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(200, visibleRect.right, EPSILON);

    // Right part is clipped by container.
    googStyle.setPosition(container2, 100, 50);
    googStyle.setWidth(container2, 1000, 100);
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(550, visibleRect.top, EPSILON);
    assertRoughlyEquals(200, visibleRect.left, EPSILON);
    assertRoughlyEquals(650, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(400, visibleRect.right, EPSILON);

    // Top 50px is clipped by container.
    googStyle.setStyle(container2, 'width', '150px');
    googStyle.setStyle(container2, 'top', '-50px');
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(500, visibleRect.top, EPSILON);
    assertRoughlyEquals(200, visibleRect.left, EPSILON);
    assertRoughlyEquals(550, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(350, visibleRect.right, EPSILON);

    // Bottom part is clipped by container.
    googStyle.setStyle(container2, 'top', '50px');
    googStyle.setStyle(container2, 'height', '1000px');
    visibleRect = googStyle.getVisibleRectForElement(el);
    assertNotNull(visibleRect);
    assertRoughlyEquals(550, visibleRect.top, EPSILON);
    assertRoughlyEquals(200, visibleRect.left, EPSILON);
    assertRoughlyEquals(800, visibleRect.bottom, EPSILON);
    assertRoughlyEquals(350, visibleRect.right, EPSILON);

    // Outside viewport.
    googStyle.setStyle(container2, 'top', '10000px');
    googStyle.setStyle(container2, 'left', '10000px');
    assertNull(googStyle.getVisibleRectForElement(el));
  },

  testScrollIntoContainerViewQuirks() {
    if (googDom.isCss1CompatMode()) return;

    const container = googDom.getElement('scrollable-container');

    // Scroll the minimum amount to make the elements visible.
    googStyle.scrollIntoContainerView(googDom.getElement('item7'), container);
    assertEquals('scroll to item7', 79, container.scrollTop);
    googStyle.scrollIntoContainerView(googDom.getElement('item8'), container);
    assertEquals('scroll to item8', 100, container.scrollTop);
    googStyle.scrollIntoContainerView(googDom.getElement('item7'), container);
    assertEquals('item7 still visible', 100, container.scrollTop);
    googStyle.scrollIntoContainerView(googDom.getElement('item1'), container);
    assertEquals('scroll to item1', 17, container.scrollTop);

    // Center the element in the first argument.
    googStyle.scrollIntoContainerView(
        googDom.getElement('item1'), container, true);
    assertEquals('center item1', 0, container.scrollTop);
    googStyle.scrollIntoContainerView(
        googDom.getElement('item4'), container, true);
    assertEquals('center item4', 48, container.scrollTop);

    // The element is higher than the container.
    googDom.getElement('item3').style.height = '140px';
    googStyle.scrollIntoContainerView(googDom.getElement('item3'), container);
    assertEquals('show item3 with increased height', 59, container.scrollTop);
    googStyle.scrollIntoContainerView(
        googDom.getElement('item3'), container, true);
    assertEquals('center item3 with increased height', 87, container.scrollTop);
    googDom.getElement('item3').style.height = '';

    // Scroll to non-integer position.
    googDom.getElement('item4').style.height = '21px';
    googStyle.scrollIntoContainerView(
        googDom.getElement('item4'), container, true);
    assertEquals('scroll position is rounded down', 48, container.scrollTop);
    googDom.getElement('item4').style.height = '';
  },

  testScrollIntoContainerViewStandard() {
    if (!googDom.isCss1CompatMode()) return;

    const container = googDom.getElement('scrollable-container');

    // Scroll the minimum amount to make the elements visible.
    googStyle.scrollIntoContainerView(googDom.getElement('item7'), container);
    assertEquals('scroll to item7', 115, container.scrollTop);
    googStyle.scrollIntoContainerView(googDom.getElement('item8'), container);
    assertEquals('scroll to item8', 148, container.scrollTop);
    googStyle.scrollIntoContainerView(googDom.getElement('item7'), container);
    assertEquals('item7 still visible', 148, container.scrollTop);
    googStyle.scrollIntoContainerView(googDom.getElement('item1'), container);
    assertEquals('scroll to item1', 17, container.scrollTop);

    // Center the element in the first argument.
    googStyle.scrollIntoContainerView(
        googDom.getElement('item1'), container, true);
    assertEquals('center item1', 0, container.scrollTop);
    googStyle.scrollIntoContainerView(
        googDom.getElement('item4'), container, true);
    assertEquals('center item4', 66, container.scrollTop);

    // The element is higher than the container.
    googDom.getElement('item3').style.height = '140px';
    googStyle.scrollIntoContainerView(googDom.getElement('item3'), container);
    assertEquals('show item3 with increased height', 83, container.scrollTop);
    googStyle.scrollIntoContainerView(
        googDom.getElement('item3'), container, true);
    assertEquals('center item3 with increased height', 93, container.scrollTop);
    googDom.getElement('item3').style.height = '';

    // Scroll to non-integer position.
    googDom.getElement('item4').style.height = '21px';
    googStyle.scrollIntoContainerView(
        googDom.getElement('item4'), container, true);
    assertEquals('scroll position is rounded down', 66, container.scrollTop);
    googDom.getElement('item4').style.height = '';
  },

  testScrollIntoContainerViewSvg() {
    if (!googDom.isCss1CompatMode()) {
      return;
    }

    const svgEl = document.createElementNS &&
        document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    if (!svgEl || svgEl.getAttribute('transform') == '' ||
        (userAgent.WEBKIT && !userAgent.isVersionOrHigher(534.8))) {
      // SVG not supported, or getBoundingClientRect not supported on SVG
      // elements.
      return;
    }

    const assertEqualsForSvgPos = (expected, actual) => {
      if (userAgent.EDGE_OR_IE) {
        // The bounding size is 1 larger than the SVG element in IE. The
        // scrollTop value maybe 1 less or 1 more than the expected value
        // depending on the scroll direction.
        assertRoughlyEquals(expected, actual, 1);
      } else {
        assertEquals(expected, actual);
      }
    };

    const svgItem1 = googDom.getElement('svg-item1');
    const svgItem2 = googDom.getElement('svg-item2');
    const svgItem3 = googDom.getElement('svg-item3');

    // Scroll the minimum amount to make the elements visible.
    const container = googDom.getElement('svg-container');
    googStyle.scrollIntoContainerView(svgItem1, container);
    assertEquals(0, container.scrollTop);
    googStyle.scrollIntoContainerView(svgItem2, container);
    assertEqualsForSvgPos(50, container.scrollTop);
    googStyle.scrollIntoContainerView(svgItem3, container);
    assertEqualsForSvgPos(150, container.scrollTop);
    googStyle.scrollIntoContainerView(svgItem2, container);
    assertEqualsForSvgPos(100, container.scrollTop);

    // Center the element in the first argument.
    googStyle.scrollIntoContainerView(svgItem2, container, true);
    assertEqualsForSvgPos(75, container.scrollTop);
    googStyle.scrollIntoContainerView(svgItem3, container, true);
    assertEqualsForSvgPos(175, container.scrollTop);

    // The element is higher than the container.
    svgItem3.setAttribute('height', 200);
    googStyle.scrollIntoContainerView(svgItem3, container);
    assertEqualsForSvgPos(200, container.scrollTop);
    googStyle.scrollIntoContainerView(svgItem3, container, true);
    assertEqualsForSvgPos(225, container.scrollTop);

    // Scroll to non-integer position.
    svgItem3.setAttribute('height', 75);
    googStyle.scrollIntoContainerView(svgItem3, container, true);
    // Scroll position is rounded down from 162.5
    assertEqualsForSvgPos(162, container.scrollTop);
    svgItem3.setAttribute('height', 100);
  },

  testOffsetParent() {
    const parent = googDom.getElement('offset-parent');
    const child = googDom.getElement('offset-child');
    assertEquals(parent, googStyle.getOffsetParent(child));
  },

  testOverflowOffsetParent() {
    const parent = googDom.getElement('offset-parent-overflow');
    const child = googDom.getElement('offset-child-overflow');
    assertEquals(parent, googStyle.getOffsetParent(child));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testShadowDomOffsetParent() {
    // Ignore browsers that don't support shadowDOM.
    if (!document.createShadowRoot) {
      return;
    }

    const parent = googDom.createDom(TagName.DIV);
    parent.style.position = 'relative';
    const host = googDom.createDom(TagName.DIV);
    googDom.appendChild(parent, host);
    const root = host.createShadowRoot();
    const child = googDom.createDom(TagName.DIV);
    googDom.appendChild(root, child);

    assertEquals(parent, googStyle.getOffsetParent(child));
  },

  testGetViewportPageOffset() {
    try {
      const testViewport = googDom.getElement('test-viewport');
      testViewport.style.height = '5000px';
      testViewport.style.width = '5000px';
      let offset = googStyle.getViewportPageOffset(document);
      assertEquals(0, offset.x);
      assertEquals(0, offset.y);

      window.scrollTo(0, 100);
      offset = googStyle.getViewportPageOffset(document);
      assertEquals(0, offset.x);
      assertEquals(100, offset.y);

      window.scrollTo(100, 0);
      offset = googStyle.getViewportPageOffset(document);
      assertEquals(100, offset.x);
      assertEquals(0, offset.y);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testGetsTranslation() {
    const element = document.getElementById('translation');

    if (userAgent.IE) {
      if (!userAgent.isDocumentModeOrHigher(9) ||
          (!googDom.isCss1CompatMode() &&
           !userAgent.isDocumentModeOrHigher(10))) {
        // 'CSS transforms were introduced in IE9, but only in standards mode
        // later browsers support the translations in quirks mode.
        return;
      }
    }

    // First check the element is actually translated, and we haven't missed
    // one of the vendor-specific transform properties
    const position = googStyle.getClientPosition(element);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const translation = googStyle.getCssTranslation(element);
    const expectedTranslation = new Coordinate(20, 30);

    assertEquals(30, position.x);
    assertRoughlyEquals(40, position.y, .1);
    assertObjectEquals(expectedTranslation, translation);
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * with a vendor prefix for Webkit.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorStyleNameWebkit() {
    const mockElement = {'style': {'WebkitTransformOrigin': ''}};

    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    assertEquals(
        '-webkit-transform-origin',
        googStyle.getVendorStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * when it exists without a vendor prefix for Webkit.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorStyleNameWebkitNoPrefix() {
    const mockElement = {
      'style': {'WebkitTransformOrigin': '', 'transformOrigin': ''},
    };

    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    assertEquals(
        'transform-origin',
        googStyle.getVendorStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * with a vendor prefix for Gecko.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorStyleNameGecko() {
    const mockElement = {'style': {'MozTransformOrigin': ''}};

    assertUserAgent([UserAgents.GECKO], 'Gecko', 'Gecko');
    assertEquals(
        '-moz-transform-origin',
        googStyle.getVendorStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * when it exists without a vendor prefix for Gecko.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorStyleNameGeckoNoPrefix() {
    const mockElement = {
      'style': {'MozTransformOrigin': '', 'transformOrigin': ''},
    };

    assertUserAgent([UserAgents.GECKO], 'Gecko', 'Gecko');
    assertEquals(
        'transform-origin',
        googStyle.getVendorStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * with a vendor prefix for IE.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorStyleNameIE() {
    const mockElement = {'style': {'msTransformOrigin': ''}};

    assertUserAgent([UserAgents.IE], 'MSIE');
    assertEquals(
        '-ms-transform-origin',
        googStyle.getVendorStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * when it exists without a vendor prefix for IE.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorStyleNameIENoPrefix() {
    const mockElement = {
      'style': {'msTransformOrigin': '', 'transformOrigin': ''},
    };

    assertUserAgent([UserAgents.IE], 'MSIE');
    assertEquals(
        'transform-origin',
        googStyle.getVendorStyleName_(mockElement, 'transform-origin'));
  },


  /**
   * Test for the proper vendor style name for a CSS property
   * with a vendor prefix for Webkit.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorJsStyleNameWebkit() {
    const mockElement = {'style': {'WebkitTransformOrigin': ''}};

    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    assertEquals(
        'WebkitTransformOrigin',
        googStyle.getVendorJsStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * when it exists without a vendor prefix for Webkit.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorJsStyleNameWebkitNoPrefix() {
    const mockElement = {
      'style': {'WebkitTransformOrigin': '', 'transformOrigin': ''},
    };

    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    assertEquals(
        'transformOrigin',
        googStyle.getVendorJsStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * with a vendor prefix for Gecko.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorJsStyleNameGecko() {
    const mockElement = {'style': {'MozTransformOrigin': ''}};

    assertUserAgent([UserAgents.GECKO], 'Gecko', 'Gecko');
    assertEquals(
        'MozTransformOrigin',
        googStyle.getVendorJsStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * when it exists without a vendor prefix for Gecko.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorJsStyleNameGeckoNoPrefix() {
    const mockElement = {
      'style': {'MozTransformOrigin': '', 'transformOrigin': ''},
    };

    assertUserAgent([UserAgents.GECKO], 'Gecko', 'Gecko');
    assertEquals(
        'transformOrigin',
        googStyle.getVendorJsStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * with a vendor prefix for IE.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorJsStyleNameIE() {
    const mockElement = {'style': {'msTransformOrigin': ''}};

    assertUserAgent([UserAgents.IE], 'MSIE');
    assertEquals(
        'msTransformOrigin',
        googStyle.getVendorJsStyleName_(mockElement, 'transform-origin'));
  },

  /**
   * Test for the proper vendor style name for a CSS property
   * when it exists without a vendor prefix for IE.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testGetVendorJsStyleNameIENoPrefix() {
    const mockElement = {
      'style': {'msTransformOrigin': '', 'transformOrigin': ''},
    };

    assertUserAgent([UserAgents.IE], 'MSIE');
    assertEquals(
        'transformOrigin',
        googStyle.getVendorJsStyleName_(mockElement, 'transform-origin'));
  },



  /**
   * Test for the setting a style name for a CSS property
   * with a vendor prefix for Mozilla.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testSetVendorStyleGecko() {
    const mockElement = {'style': {'MozTransform': ''}};
    const styleValue = 'translate3d(0,0,0)';

    assertUserAgent([UserAgents.GECKO], 'Gecko', 'Gecko');
    googStyle.setStyle(mockElement, 'transform', styleValue);
    assertEquals(styleValue, mockElement.style.MozTransform);
  },

  /**
   * Test for the setting a style name for a CSS property
   * with a vendor prefix for IE.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testSetVendorStyleIE() {
    const mockElement = {'style': {'msTransform': ''}};
    const styleValue = 'translate3d(0,0,0)';

    assertUserAgent([UserAgents.IE], 'MSIE');
    googStyle.setStyle(mockElement, 'transform', styleValue);
    assertEquals(styleValue, mockElement.style.msTransform);
  },


  /**
   * Test for the getting a style name for a CSS property
   * with a vendor prefix for Webkit.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testGetVendorStyleWebkit() {
    const mockElement = {'style': {'WebkitTransform': ''}};
    const styleValue = 'translate3d(0,0,0)';

    assertUserAgent([UserAgents.WEBKIT], 'WebKit');
    googStyle.setStyle(mockElement, 'transform', styleValue);
    assertEquals(styleValue, googStyle.getStyle(mockElement, 'transform'));
  },

  /**
   * Test for the getting a style name for a CSS property
   * with a vendor prefix for Mozilla.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testGetVendorStyleGecko() {
    const mockElement = {'style': {'MozTransform': ''}};
    const styleValue = 'translate3d(0,0,0)';

    assertUserAgent([UserAgents.GECKO], 'Gecko', 'Gecko');
    googStyle.setStyle(mockElement, 'transform', styleValue);
    assertEquals(styleValue, googStyle.getStyle(mockElement, 'transform'));
  },

  /**
   * Test for the getting a style name for a CSS property
   * with a vendor prefix for IE.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testGetVendorStyleIE() {
    const mockElement = {'style': {'msTransform': ''}};
    const styleValue = 'translate3d(0,0,0)';

    assertUserAgent([UserAgents.IE], 'MSIE');
    googStyle.setStyle(mockElement, 'transform', styleValue);
    assertEquals(styleValue, googStyle.getStyle(mockElement, 'transform'));
  },


  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testParseStyleAttributeWithColon() {
    // Regression test for https://github.com/google/closure-library/issues/127.
    const cssObj = googStyle.parseStyleAttribute(
        'left: 0px; text-align: center; background-image: ' +
        'url(http://www.google.ca/Test.gif); -ms-filter: ' +
        'progid:DXImageTransform.Microsoft.MotionBlur(strength=50), ' +
        'progid:DXImageTransform.Microsoft.BasicImage(mirror=1);');
    assertEquals('url(http://www.google.ca/Test.gif)', cssObj.backgroundImage);
    assertEquals(
        'progid:DXImageTransform.Microsoft.MotionBlur(strength=50), ' +
            'progid:DXImageTransform.Microsoft.BasicImage(mirror=1)',
        cssObj.MsFilter);
  },
});
