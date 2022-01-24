/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} swapping userAgent
 */

goog.module('goog.events.WheelHandlerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const EventsWheelEvent = goog.require('goog.events.WheelEvent');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const WheelHandler = goog.require('goog.events.WheelHandler');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
/** @suppress {extraRequire} */
const userAgent = goog.require('goog.userAgent');

let log;
const stubs = new PropertyReplacer();

const PREFERRED_TYPE = 'wheel';
const LEGACY_TYPE = 'mousewheel';
const GECKO_TYPE = 'DOMMouseScroll';

const HORIZONTAL = 'h';
const VERTICAL = 'v';

const DeltaMode = EventsWheelEvent.DeltaMode;

let mouseWheelEvent;
let mouseWheelEventRtl;
let mouseWheelHandler;
let mouseWheelHandlerRtl;

// Be sure to call this after setting up goog.userAgent mock and not before.
function createHandlerAndListen() {
  /** @suppress {checkTypes} suppression added to enable type checking */
  mouseWheelHandler = new WheelHandler(dom.getElement('foo'));

  events.listen(mouseWheelHandler, EventsWheelEvent.EventType.WHEEL, (e) => {
    mouseWheelEvent = e;
  });

  /** @suppress {checkTypes} suppression added to enable type checking */
  mouseWheelHandlerRtl = new WheelHandler(dom.getElement('fooRtl'));

  events.listen(mouseWheelHandlerRtl, EventsWheelEvent.EventType.WHEEL, (e) => {
    mouseWheelEventRtl = e;
  });
}

function handleEvent(event) {
  mouseWheelHandler.handleEvent(event);
  mouseWheelHandlerRtl.handleEvent(event);
}

function assertWheelEvent(deltaMode, deltaX, deltaY, deltaZ) {
  assertTrue('event should be non-null', !!mouseWheelEvent);
  assertTrue(
      'event should have correct JS type',
      mouseWheelEvent instanceof EventsWheelEvent);
  assertEquals(
      'event should have correct deltaMode property', deltaMode,
      mouseWheelEvent.deltaMode);
  assertEquals(
      'event should have correct deltaX property', deltaX,
      mouseWheelEvent.deltaX);
  assertEquals(
      'event should have correct deltaY property', deltaY,
      mouseWheelEvent.deltaY);
  assertEquals(
      'event should have correct deltaZ property', deltaZ,
      mouseWheelEvent.deltaZ);

  // RTL
  assertTrue('event should be non-null', !!mouseWheelEventRtl);
  assertTrue(
      'event should have correct JS type',
      mouseWheelEventRtl instanceof EventsWheelEvent);
  assertEquals(
      'event should have correct deltaMode property', deltaMode,
      mouseWheelEventRtl.deltaMode);
  assertEquals(
      'event should have correct deltaX property', -deltaX,
      mouseWheelEventRtl.deltaX);
  assertEquals(
      'event should have correct deltaY property', deltaY,
      mouseWheelEventRtl.deltaY);
  assertEquals(
      'event should have correct deltaZ property', deltaZ,
      mouseWheelEventRtl.deltaZ);
}

function assertPixelDeltas(scale) {
  assertEquals(mouseWheelEvent.deltaX * scale, mouseWheelEvent.pixelDeltaX);
  assertEquals(mouseWheelEvent.deltaY * scale, mouseWheelEvent.pixelDeltaY);
  assertEquals(mouseWheelEvent.deltaZ * scale, mouseWheelEvent.pixelDeltaZ);

  // RTL
  assertEquals(
      mouseWheelEventRtl.deltaX * scale, mouseWheelEventRtl.pixelDeltaX);
  assertEquals(
      mouseWheelEventRtl.deltaY * scale, mouseWheelEventRtl.pixelDeltaY);
  assertEquals(
      mouseWheelEventRtl.deltaZ * scale, mouseWheelEventRtl.pixelDeltaZ);
}

/** @suppress {checkTypes} suppression added to enable type checking */
function createFakePreferredEvent(
    opt_deltaMode, opt_deltaX, opt_deltaY, opt_deltaZ) {
  const event = {
    type: PREFERRED_TYPE,
    deltaMode: opt_deltaMode,
    deltaX: opt_deltaX,
    deltaY: opt_deltaY,
    deltaZ: opt_deltaZ,
  };
  return new BrowserEvent(event);
}

/** @suppress {checkTypes} suppression added to enable type checking */
function createFakeLegacyEvent(
    opt_wheelDelta, opt_wheelDeltaX, opt_wheelDeltaY) {
  const event = {
    type: LEGACY_TYPE,
    wheelDelta: opt_wheelDelta,
    wheelDeltaX: opt_wheelDeltaX,
    wheelDeltaY: opt_wheelDeltaY,
  };
  return new BrowserEvent(event);
}

/** @suppress {checkTypes} suppression added to enable type checking */
function createFakeGeckoEvent(opt_detail, opt_axis) {
  const event = {
    type: GECKO_TYPE,
    detail: opt_detail,
    axis: opt_axis,
    HORIZONTAL_AXIS: HORIZONTAL,
    VERTICAL_AXIS: VERTICAL,
  };
  return new BrowserEvent(event);
}
testSuite({
  setUpPage() {
    log = dom.getElement('log');
  },

  setUp() {
    stubs.remove(goog, 'userAgent');
    /** @suppress {checkTypes} suppression added to enable type checking */
    goog.userAgent = {
      product: {
        CHROME: false,
        version: 0,
        isVersion: function(version) {
          return googString.compareVersions(this.version, version) >= 0;
        },
      },
      GECKO: false,
      IE: false,
      version: 0,
      isVersionOrHigher: function(version) {
        return googString.compareVersions(this.version, version) >= 0;
      },
    };
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
    mouseWheelHandler = new WheelHandler(document.body);

    events.listen(mouseWheelHandler, EventsWheelEvent.EventType.WHEEL, (e) => {
      log.append(
          document.createElement('br'),
          googString.subs('(deltaX, deltaY): (%s, %s)', e.deltaX, e.deltaY));
    });
  },

  testGetDomEventType() {
    // Defaults to legacy non-gecko event.
    assertEquals(LEGACY_TYPE, WheelHandler.getDomEventType());

    // Gecko start to support wheel with version 17.
    goog.userAgent.GECKO = true;
    goog.userAgent.version = 16;
    assertEquals(GECKO_TYPE, WheelHandler.getDomEventType());
    goog.userAgent.version = 17;
    assertEquals(PREFERRED_TYPE, WheelHandler.getDomEventType());
    goog.userAgent.GECKO = false;

    // IE started with version 9.
    goog.userAgent.IE = true;
    goog.userAgent.version = 8;
    assertEquals(LEGACY_TYPE, WheelHandler.getDomEventType());
    goog.userAgent.version = 9;
    assertEquals(PREFERRED_TYPE, WheelHandler.getDomEventType());
    goog.userAgent.IE = false;

    // Chrome started with version 31.
    goog.userAgent.product.CHROME = true;
    goog.userAgent.product.version = 30;
    assertEquals(LEGACY_TYPE, WheelHandler.getDomEventType());
    goog.userAgent.product.version = 31;
    assertEquals(PREFERRED_TYPE, WheelHandler.getDomEventType());
    goog.userAgent.product.CHROME = false;
  },

  testPreferredStyleWheel() {
    // Enable 'wheel'
    goog.userAgent.IE = true;
    goog.userAgent.version = 9;
    createHandlerAndListen();

    handleEvent(createFakePreferredEvent(DeltaMode.PIXEL, 10, 20, 30));
    assertWheelEvent(DeltaMode.PIXEL, 10, 20, 30);
    assertPixelDeltas(1);

    handleEvent(createFakePreferredEvent(DeltaMode.LINE, 10, 20, 30));
    assertWheelEvent(DeltaMode.LINE, 10, 20, 30);
    assertPixelDeltas(15);

    handleEvent(createFakePreferredEvent(DeltaMode.PAGE, 10, 20, 30));
    assertWheelEvent(DeltaMode.PAGE, 10, 20, 30);
    assertPixelDeltas(30 * 15);
  },

  testLegacyStyleWheel() {
    // 'mousewheel' enabled by default
    createHandlerAndListen();

    // Test one dimensional.
    handleEvent(createFakeLegacyEvent(10));
    assertWheelEvent(DeltaMode.PIXEL, 0, -10, 0);
    assertPixelDeltas(1);

    // Test two dimensional.
    handleEvent(createFakeLegacyEvent(/* ignored */ 10, 20, 30));
    assertWheelEvent(DeltaMode.PIXEL, -20, -30, 0);
    assertPixelDeltas(1);
  },

  testLegacyGeckoStyleWheel() {
    goog.userAgent.GECKO = true;
    createHandlerAndListen();

    // Test no axis.
    handleEvent(createFakeGeckoEvent(10));
    assertWheelEvent(DeltaMode.LINE, 0, 10, 0);
    assertPixelDeltas(15);

    // Vertical axis.
    handleEvent(createFakeGeckoEvent(10, VERTICAL));
    assertWheelEvent(DeltaMode.LINE, 0, 10, 0);
    assertPixelDeltas(15);

    // Horizontal axis.
    handleEvent(createFakeGeckoEvent(10, HORIZONTAL));
    assertWheelEvent(DeltaMode.LINE, 10, 0, 0);
    assertPixelDeltas(15);
  },

  testLegacyIeStyleWheel() {
    goog.userAgent.IE = true;

    createHandlerAndListen();

    // Non-gecko, non-webkit events get wheelDelta divided by -40 to get detail.
    handleEvent(createFakeLegacyEvent(120));
    assertWheelEvent(DeltaMode.PIXEL, 0, -120, 0);

    handleEvent(createFakeLegacyEvent(-120));
    assertWheelEvent(DeltaMode.PIXEL, 0, 120, 0);

    handleEvent(createFakeLegacyEvent(1200));
    assertWheelEvent(DeltaMode.PIXEL, 0, -1200, 0);
  },

  testNullBody() {
    goog.userAgent.IE = true;
    const documentObjectWithNoBody = {};
    testingEvents.mixinListenable(documentObjectWithNoBody);
    /** @suppress {checkTypes} suppression added to enable type checking */
    mouseWheelHandler = new WheelHandler(documentObjectWithNoBody);
  },
});
