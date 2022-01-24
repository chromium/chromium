/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} swapping userAgent
 */

goog.module('goog.events.MouseWheelHandlerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const MouseWheelEvent = goog.require('goog.events.MouseWheelEvent');
const MouseWheelHandler = goog.require('goog.events.MouseWheelHandler');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
/** @suppress {extraRequire} */
const userAgent = goog.require('goog.userAgent');

let log;
const stubs = new PropertyReplacer();

const DEFAULT_TYPE = 'mousewheel';
const GECKO_TYPE = 'DOMMouseScroll';

const HORIZONTAL = 'h';
const VERTICAL = 'v';

let mouseWheelEvent;
let mouseWheelEventRtl;
let mouseWheelHandler;
let mouseWheelHandlerRtl;

function runWebKitContinuousAndDiscreteEventsTest() {
  userAgent.isVersionOrHigher = functions.TRUE;

  createHandlerAndListen();

  // IE-style wheel events.
  handleEvent(createFakeWebkitMouseWheelEvent(0, -40));
  assertMouseWheelEvent(1, 0, 1);

  handleEvent(createFakeWebkitMouseWheelEvent(80, -40));
  assertMouseWheelEvent(-2, -2, 1);

  // Even in Webkit versions that usually behave in IE style, sometimes wheel
  // events don't behave; this has been observed for instance with Macbook
  // and Chrome OS touchpads in Webkit 534.10+.
  handleEvent(createFakeWebkitMouseWheelEvent(-3, 5));
  assertMouseWheelEvent(-5, 3, -5);

  handleEvent(createFakeWebkitMouseWheelEvent(4, -7));
  assertMouseWheelEvent(7, -4, 7);
}

// Be sure to call this after setting up goog.userAgent mock and not before.
function createHandlerAndListen() {
  mouseWheelHandler = new MouseWheelHandler(dom.getElement('foo'));

  events.listen(
      mouseWheelHandler, MouseWheelHandler.EventType.MOUSEWHEEL, (e) => {
        mouseWheelEvent = e;
      });

  mouseWheelHandlerRtl = new MouseWheelHandler(dom.getElement('fooRtl'));

  events.listen(
      mouseWheelHandlerRtl, MouseWheelHandler.EventType.MOUSEWHEEL, (e) => {
        mouseWheelEventRtl = e;
      });
}

function handleEvent(event) {
  mouseWheelHandler.handleEvent(event);
  mouseWheelHandlerRtl.handleEvent(event);
}

function assertMouseWheelEvent(expectedDetail, expectedDeltaX, expectedDeltaY) {
  assertTrue('event should be non-null', !!mouseWheelEvent);
  assertTrue(
      'event should have correct JS type',
      mouseWheelEvent instanceof MouseWheelEvent);
  assertEquals(
      'event should have correct detail property', expectedDetail,
      mouseWheelEvent.detail);
  assertEquals(
      'event should have correct deltaX property', expectedDeltaX,
      mouseWheelEvent.deltaX);
  assertEquals(
      'event should have correct deltaY property', expectedDeltaY,
      mouseWheelEvent.deltaY);

  // RTL
  assertTrue('event should be non-null', !!mouseWheelEventRtl);
  assertTrue(
      'event should have correct JS type',
      mouseWheelEventRtl instanceof MouseWheelEvent);
  assertEquals(
      'event should have correct detail property', expectedDetail,
      mouseWheelEventRtl.detail);
  assertEquals(
      'event should have correct deltaX property', -expectedDeltaX,
      mouseWheelEventRtl.deltaX);
  assertEquals(
      'event should have correct deltaY property', expectedDeltaY,
      mouseWheelEventRtl.deltaY);
}

/** @suppress {checkTypes} suppression added to enable type checking */
function createFakeMouseWheelEvent(
    type, opt_wheelDelta, opt_detail, opt_axis, opt_wheelDeltaX,
    opt_wheelDeltaY) {
  const event = {
    type: type,
    wheelDelta: opt_wheelDelta !== undefined ? opt_wheelDelta : undefined,
    detail: opt_detail !== undefined ? opt_detail : undefined,
    axis: opt_axis || undefined,
    wheelDeltaX: opt_wheelDeltaX !== undefined ? opt_wheelDeltaX : undefined,
    wheelDeltaY: opt_wheelDeltaY !== undefined ? opt_wheelDeltaY : undefined,

    // These two are constants defined on the event in FF3.1 and later.
    // It doesn't matter exactly what they are, and it doesn't affect
    // our simulations of other browsers.
    HORIZONTAL_AXIS: HORIZONTAL,
    VERTICAL_AXIS: VERTICAL,
  };
  return new BrowserEvent(event);
}

function createFakeWebkitMouseWheelEvent(wheelDeltaX, wheelDeltaY) {
  return createFakeMouseWheelEvent(
      DEFAULT_TYPE,
      Math.abs(wheelDeltaX) > Math.abs(wheelDeltaY) ? wheelDeltaX : wheelDeltaY,
      undefined, undefined, wheelDeltaX, wheelDeltaY);
}

testSuite({
  setUpPage() {
    log = dom.getElement('log');
  },

  setUp() {
    stubs.remove(goog, 'userAgent');
  },

  tearDown() {
    stubs.reset();
    goog.dispose(mouseWheelHandler);
    goog.dispose(mouseWheelHandlerRtl);
    mouseWheelHandlerRtl = null;
    mouseWheelHandler = null;
    mouseWheelEvent = null;
    mouseWheelEventRtl = null;
  },

  tearDownPage() {
    // Create interactive demo.
    mouseWheelHandler = new MouseWheelHandler(document.body);

    events.listen(
        mouseWheelHandler, MouseWheelHandler.EventType.MOUSEWHEEL, (e) => {
          log.append(
              document.createElement('br'),
              googString.subs(
                  '(deltaX, deltaY): (%s, %s)', e.deltaX, e.deltaY));
        });
  },

  testIeStyleMouseWheel() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent =
        {OPERA: false, EDGE_OR_IE: true, GECKO: false, WEBKIT: false};

    createHandlerAndListen();

    // Non-gecko, non-webkit events get wheelDelta divided by -40 to get detail.
    handleEvent(createFakeMouseWheelEvent(DEFAULT_TYPE, 120));
    assertMouseWheelEvent(-3, 0, -3);

    handleEvent(createFakeMouseWheelEvent(DEFAULT_TYPE, -120));
    assertMouseWheelEvent(3, 0, 3);

    handleEvent(createFakeMouseWheelEvent(DEFAULT_TYPE, 1200));
    assertMouseWheelEvent(-30, 0, -30);
  },

  testNullBody() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent = {OPERA: false, IE: true, GECKO: false, WEBKIT: false};
    const documentObjectWithNoBody = {};
    testingEvents.mixinListenable(documentObjectWithNoBody);
    /** @suppress {checkTypes} suppression added to enable type checking */
    mouseWheelHandler = new MouseWheelHandler(documentObjectWithNoBody);
  },

  testGeckoStyleMouseWheel() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent = {OPERA: false, IE: false, GECKO: true, WEBKIT: false};

    createHandlerAndListen();

    handleEvent(createFakeMouseWheelEvent(GECKO_TYPE, null, 3));
    assertMouseWheelEvent(3, 0, 3);

    handleEvent(createFakeMouseWheelEvent(GECKO_TYPE, null, -12));
    assertMouseWheelEvent(-12, 0, -12);

    // Really big values should get truncated to +-3.
    handleEvent(createFakeMouseWheelEvent(GECKO_TYPE, null, 1200));
    assertMouseWheelEvent(3, 0, 3);

    handleEvent(createFakeMouseWheelEvent(GECKO_TYPE, null, -1200));
    assertMouseWheelEvent(-3, 0, -3);

    // Test scrolling with the additional axis property.
    handleEvent(createFakeMouseWheelEvent(GECKO_TYPE, null, 3, VERTICAL));
    assertMouseWheelEvent(3, 0, 3);

    handleEvent(createFakeMouseWheelEvent(GECKO_TYPE, null, 3, HORIZONTAL));
    assertMouseWheelEvent(3, 3, 0);

    handleEvent(createFakeMouseWheelEvent(GECKO_TYPE, null, -3, HORIZONTAL));
    assertMouseWheelEvent(-3, -3, 0);
  },

  testWebkitStyleMouseWheel_ieStyle() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent =
        {OPERA: false, IE: false, GECKO: false, WEBKIT: true, WINDOWS: true};

    createHandlerAndListen();

    // IE-style Webkit events get wheelDelta divided by -40 to get detail.
    handleEvent(createFakeWebkitMouseWheelEvent(-40, 0));
    assertMouseWheelEvent(1, 1, 0);

    handleEvent(createFakeWebkitMouseWheelEvent(120, 0));
    assertMouseWheelEvent(-3, -3, 0);

    handleEvent(createFakeWebkitMouseWheelEvent(0, 120));
    assertMouseWheelEvent(-3, 0, -3);

    handleEvent(createFakeWebkitMouseWheelEvent(0, -40));
    assertMouseWheelEvent(1, 0, 1);

    handleEvent(createFakeWebkitMouseWheelEvent(80, -40));
    assertMouseWheelEvent(-2, -2, 1);
  },

  testWebkitStyleMouseWheel_ieStyleOnLinux() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent = {
      OPERA: false,
      IE: false,
      GECKO: false,
      WEBKIT: true,
      WINDOWS: false,
      LINUX: true,
    };
    runWebKitContinuousAndDiscreteEventsTest();
  },

  testWebkitStyleMouseWheel_ieStyleOnMac() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent = {
      OPERA: false,
      IE: false,
      GECKO: false,
      WEBKIT: true,
      WINDOWS: false,
      MAC: true,
    };
    runWebKitContinuousAndDiscreteEventsTest();
  },

  testWebkitStyleMouseWheel_nonIeStyle() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent =
        {OPERA: false, IE: false, GECKO: false, WEBKIT: true, WINDOWS: false};

    userAgent.isVersionOrHigher = functions.FALSE;

    createHandlerAndListen();

    // non-IE-style Webkit events do not get wheelDelta scaled
    handleEvent(createFakeWebkitMouseWheelEvent(-40, 0));
    assertMouseWheelEvent(1, 1, 0);

    handleEvent(createFakeWebkitMouseWheelEvent(120, 0));
    assertMouseWheelEvent(-3, -3, 0);

    handleEvent(createFakeWebkitMouseWheelEvent(0, 120));
    assertMouseWheelEvent(-3, 0, -3);

    handleEvent(createFakeWebkitMouseWheelEvent(0, -40));
    assertMouseWheelEvent(1, 0, 1);

    handleEvent(createFakeWebkitMouseWheelEvent(80, -40));
    assertMouseWheelEvent(-2, -2, 1);
  },

  testMaxDeltaX() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent =
        {OPERA: false, IE: false, GECKO: false, WEBKIT: true, WINDOWS: true};

    createHandlerAndListen();

    // IE-style Webkit events get wheelDelta divided by -40 to get detail.
    handleEvent(createFakeWebkitMouseWheelEvent(-120, 0));
    assertMouseWheelEvent(3, 3, 0);

    mouseWheelHandler.setMaxDeltaX(3);
    mouseWheelHandlerRtl.setMaxDeltaX(3);
    handleEvent(createFakeWebkitMouseWheelEvent(-120, 0));
    assertMouseWheelEvent(3, 3, 0);

    mouseWheelHandler.setMaxDeltaX(2);
    mouseWheelHandlerRtl.setMaxDeltaX(2);
    handleEvent(createFakeWebkitMouseWheelEvent(-120, 0));
    assertMouseWheelEvent(3, 2, 0);

    handleEvent(createFakeWebkitMouseWheelEvent(0, -120));
    assertMouseWheelEvent(3, 0, 3);
  },

  testMaxDeltaY() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent =
        {OPERA: false, IE: false, GECKO: false, WEBKIT: true, WINDOWS: true};

    createHandlerAndListen();

    // IE-style Webkit events get wheelDelta divided by -40 to get detail.
    handleEvent(createFakeWebkitMouseWheelEvent(0, -120));
    assertMouseWheelEvent(3, 0, 3);

    mouseWheelHandler.setMaxDeltaY(3);
    mouseWheelHandlerRtl.setMaxDeltaY(3);
    handleEvent(createFakeWebkitMouseWheelEvent(0, -120));
    assertMouseWheelEvent(3, 0, 3);

    mouseWheelHandler.setMaxDeltaY(2);
    mouseWheelHandlerRtl.setMaxDeltaY(2);
    handleEvent(createFakeWebkitMouseWheelEvent(0, -120));
    assertMouseWheelEvent(3, 0, 2);

    handleEvent(createFakeWebkitMouseWheelEvent(-120, 0));
    assertMouseWheelEvent(3, 3, 0);
  },
});
