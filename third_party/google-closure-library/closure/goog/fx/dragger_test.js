/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.fx.DraggerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const Dragger = goog.require('goog.fx.Dragger');
const EventType = goog.require('goog.events.EventType');
const GoogEvent = goog.require('goog.events.Event');
const GoogRect = goog.require('goog.math.Rect');
const StrictMock = goog.require('goog.testing.StrictMock');
const TagName = goog.require('goog.dom.TagName');
const bidi = goog.require('goog.style.bidi');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

/** @suppress {visibility} suppression added to enable type checking */
const HAS_SET_CAPTURE = Dragger.HAS_SET_CAPTURE_;

let target;
let targetRtl;

/**
 * @suppress {missingProperties,checkTypes} suppression added to enable type
 * checking
 */
function runStartDragTest(handleId, targetElement) {
  let dragger = new Dragger(targetElement, dom.getElement(handleId));
  if (handleId == 'handle_rtl') {
    dragger.enableRightPositioningForRtl(true);
  }
  const e = new StrictMock(BrowserEvent);
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  e.type = EventType.MOUSEDOWN;
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  e.clientX = 1;
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  e.clientY = 2;
  e.isMouseActionButton().$returns(true);
  e.preventDefault();
  e.isMouseActionButton().$returns(true);
  e.preventDefault();
  e.$replay();

  events.listen(dragger, Dragger.EventType.START, () => {
    targetElement.style.display = 'block';
  });

  dragger.startDrag(e);

  assertTrue(
      'Start drag with no hysteresis must actually start the drag.',
      dragger.isDragging());
  if (handleId == 'handle_rtl') {
    assertEquals(10, bidi.getOffsetStart(targetElement));
  }
  assertEquals(
      'Dragger startX must match event\'s clientX.', 1, dragger.startX);
  assertEquals(
      'Dragger clientX must match event\'s clientX', 1, dragger.clientX);
  assertEquals(
      'Dragger startY must match event\'s clientY.', 2, dragger.startY);
  assertEquals(
      'Dragger clientY must match event\'s clientY', 2, dragger.clientY);
  assertEquals(
      'Dragger deltaX must match target\'s offsetLeft', 10, dragger.deltaX);
  assertEquals(
      'Dragger deltaY must match target\'s offsetTop', 15, dragger.deltaY);

  dragger = new Dragger(targetElement, dom.getElement(handleId));
  dragger.setHysteresis(1);
  dragger.startDrag(e);
  assertFalse(
      'Start drag with a valid non-zero hysteresis should not start ' +
          'the drag.',
      dragger.isDragging());
  e.$verify();
}

testSuite({
  setUp() {
    const sandbox = dom.getElement('sandbox');
    target = dom.createDom(TagName.DIV, {
      'id': 'target',
      'style': 'display:none;position:absolute;top:15px;left:10px',
    });
    sandbox.appendChild(target);
    sandbox.appendChild(dom.createDom(TagName.DIV, {id: 'handle'}));

    const sandboxRtl = dom.getElement('sandbox_rtl');
    targetRtl = dom.createDom(TagName.DIV, {
      'id': 'target_rtl',
      'style': 'position:absolute; top:15px; right:10px; width:10px; ' +
          'height: 10px; background: green;',
    });
    sandboxRtl.appendChild(targetRtl);
    sandboxRtl.appendChild(dom.createDom(TagName.DIV, {
      'id': 'background_rtl',
      'style': 'width: 10000px;height:50px;position:absolute;color:blue;',
    }));
    sandboxRtl.appendChild(dom.createDom(TagName.DIV, {id: 'handle_rtl'}));
  },

  tearDown() {
    dom.removeChildren(dom.getElement('sandbox'));
    dom.removeChildren(dom.getElement('sandbox_rtl'));
    events.removeAll(document);
  },

  testStartDrag() {
    runStartDragTest('handle', target);
  },

  testStartDrag_rtl() {
    runStartDragTest('handle_rtl', targetRtl);
  },

  /**
     @bug 1381317 Cancelling start drag didn't end the attempt to drag.
     @suppress {missingProperties,checkTypes,visibility} suppression added to
     enable type checking
   */
  testStartDrag_Cancel() {
    const dragger = new Dragger(target);

    const e = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.type = EventType.MOUSEDOWN;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 1;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 2;
    e.isMouseActionButton().$returns(true);
    e.$replay();

    events.listen(dragger, Dragger.EventType.START, (e) => {
      // Cancel drag.
      e.preventDefault();
    });

    dragger.startDrag(e);

    assertFalse('Start drag must have been cancelled.', dragger.isDragging());
    assertFalse(
        'Dragger must not have registered mousemove handlers.',
        events.hasListener(
            dragger.document_, EventType.MOUSEMOVE, !HAS_SET_CAPTURE));
    assertFalse(
        'Dragger must not have registered mouseup handlers.',
        events.hasListener(
            dragger.document_, EventType.MOUSEUP, !HAS_SET_CAPTURE));
    e.$verify();
  },

  /**
     Tests that start drag happens on left mousedown.
     @suppress {missingProperties,checkTypes,visibility} suppression added to
     enable type checking
   */
  testStartDrag_LeftMouseDownOnly() {
    const dragger = new Dragger(target);

    const e = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.type = EventType.MOUSEDOWN;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 1;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 2;
    e.isMouseActionButton().$returns(false);
    e.$replay();

    events.listen(dragger, Dragger.EventType.START, (e) => {
      fail('No drag START event should have been dispatched');
    });

    dragger.startDrag(e);

    assertFalse('Start drag must have been cancelled.', dragger.isDragging());
    assertFalse(
        'Dragger must not have registered mousemove handlers.',
        events.hasListener(dragger.document_, EventType.MOUSEMOVE, true));
    assertFalse(
        'Dragger must not have registered mouseup handlers.',
        events.hasListener(dragger.document_, EventType.MOUSEUP, true));
    e.$verify();
  },

  /**
     Tests that start drag happens on other event type than MOUSEDOWN.
     @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testStartDrag_MouseMove() {
    const dragger = new Dragger(target);

    const e = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.type = EventType.MOUSEMOVE;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 1;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 2;
    // preventDefault is not called.
    e.$replay();

    let startDragFired = false;
    events.listen(dragger, Dragger.EventType.START, (e) => {
      startDragFired = true;
    });

    dragger.startDrag(e);

    assertTrue('Dragging should be in progress.', dragger.isDragging());
    assertTrue('Start drag event should have fired.', startDragFired);
    assertTrue(
        'Dragger must have registered mousemove handlers.',
        events.hasListener(
            dragger.document_, EventType.MOUSEMOVE, !HAS_SET_CAPTURE));
    assertTrue(
        'Dragger must have registered mouseup handlers.',
        events.hasListener(
            dragger.document_, EventType.MOUSEUP, !HAS_SET_CAPTURE));
    e.$verify();
  },

  /**
     Tests that preventDefault is not called for TOUCHSTART event.
     @suppress {checkTypes} suppression added to enable type checking
   */
  testStartDrag_TouchStart() {
    const dragger = new Dragger(target);

    const e = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.type = EventType.TOUCHSTART;
    // preventDefault is not called.
    e.$replay();

    let startDragFired = false;
    events.listen(dragger, Dragger.EventType.START, (e) => {
      startDragFired = true;
    });

    dragger.startDrag(e);

    assertTrue('Dragging should be in progress.', dragger.isDragging());
    assertTrue('Start drag event should have fired.', startDragFired);
    assertTrue(
        'Dragger must have registered touchstart listener.',
        events.hasListener(
            dragger.handle, EventType.TOUCHSTART, false /*opt_cap*/));
    e.$verify();
  },

  /**
   * Tests that preventDefault is not called for TOUCHSTART event when
   * hysteresis is set to be greater than zero.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testStartDrag_TouchStart_NonZeroHysteresis() {
    const dragger = new Dragger(target);
    dragger.setHysteresis(5);
    const e = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.type = EventType.TOUCHSTART;
    // preventDefault is not called.
    e.$replay();

    let startDragFired = false;
    events.listen(dragger, Dragger.EventType.START, (e) => {
      startDragFired = true;
    });

    dragger.startDrag(e);

    assertFalse(
        'Start drag must not start drag because of hysterisis.',
        dragger.isDragging());
    assertTrue(
        'Dragger must have registered touchstart listener.',
        events.hasListener(
            dragger.handle, EventType.TOUCHSTART, false /*opt_cap*/));
    e.$verify();
  },

  /**
     @bug 1381317 Cancelling start drag didn't end the attempt to drag.
     @suppress {missingProperties,checkTypes,visibility} suppression added to
     enable type checking
   */
  testHandleMove_Cancel() {
    const dragger = new Dragger(target);
    dragger.setHysteresis(5);

    events.listen(dragger, Dragger.EventType.START, (e) => {
      // Cancel drag.
      e.preventDefault();
    });

    const e = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 1;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 2;
    e.isMouseActionButton().$returns(true).$anyTimes();
    // preventDefault is not called.
    e.$replay();
    dragger.startDrag(e);
    assertFalse(
        'Start drag must not start drag because of hysterisis.',
        dragger.isDragging());
    assertTrue(
        'Dragger must have registered mousemove handlers.',
        events.hasListener(
            dragger.document_, EventType.MOUSEMOVE, !HAS_SET_CAPTURE));
    assertTrue(
        'Dragger must have registered mouseup handlers.',
        events.hasListener(
            dragger.document_, EventType.MOUSEUP, !HAS_SET_CAPTURE));

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 10;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 10;
    dragger.handleMove_(e);
    assertFalse('Drag must be cancelled.', dragger.isDragging());
    assertFalse(
        'Dragger must unregistered mousemove handlers.',
        events.hasListener(dragger.document_, EventType.MOUSEMOVE, true));
    assertFalse(
        'Dragger must unregistered mouseup handlers.',
        events.hasListener(dragger.document_, EventType.MOUSEUP, true));
    e.$verify();
  },

  /**
     @bug 1714667 IE<9 built in drag and drop handling stops dragging.
     @suppress {checkTypes} suppression added to enable type checking
   */
  testIeDragStartCancelling() {
    // Testing only IE<9.
    if (!userAgent.IE || userAgent.isVersionOrHigher(9)) {
      return;
    }

    // Built in 'dragstart' cancelling not enabled.
    let dragger = new Dragger(target);

    let e = new GoogEvent(EventType.MOUSEDOWN);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 1;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 2;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.button = 1;  // IE only constant for left button.
    /** @suppress {checkTypes} suppression added to enable type checking */
    let be = new BrowserEvent(e);
    dragger.startDrag(be);
    assertTrue('The drag should have started.', dragger.isDragging());

    e = new GoogEvent(EventType.DRAGSTART);
    /** @suppress {visibility} suppression added to enable type checking */
    e.target = dragger.document_.documentElement;
    assertTrue(
        'The event should not be canceled.', testingEvents.fireBrowserEvent(e));

    dragger.dispose();

    // Built in 'dragstart' cancelling enabled.
    dragger = new Dragger(target);
    dragger.setCancelIeDragStart(true);

    e = new GoogEvent(EventType.MOUSEDOWN);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 1;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 2;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.button = 1;  // IE only constant for left button.
    /** @suppress {checkTypes} suppression added to enable type checking */
    be = new BrowserEvent(e);
    dragger.startDrag(be);
    assertTrue('The drag should have started.', dragger.isDragging());

    e = new GoogEvent(EventType.DRAGSTART);
    /** @suppress {visibility} suppression added to enable type checking */
    e.target = dragger.document_.documentElement;
    assertFalse(
        'The event should be canceled.', testingEvents.fireBrowserEvent(e));

    dragger.dispose();
  },

  /**
     @suppress {missingProperties,checkTypes} suppression added to enable type
     checking
   */
  testPreventMouseDown() {
    const dragger = new Dragger(target);
    dragger.setPreventMouseDown(false);

    const e = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.type = EventType.MOUSEDOWN;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 1;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 2;
    e.isMouseActionButton().$returns(true);
    // preventDefault is not called.
    e.$replay();

    dragger.startDrag(e);

    assertTrue('Dragging should be in progess.', dragger.isDragging());
    e.$verify();
  },

  testLimits() {
    const dragger = new Dragger(target);

    assertEquals(100, dragger.limitX(100));
    assertEquals(100, dragger.limitY(100));

    dragger.setLimits(new GoogRect(10, 20, 30, 40));

    assertEquals(10, dragger.limitX(0));
    assertEquals(40, dragger.limitX(100));
    assertEquals(20, dragger.limitY(0));
    assertEquals(60, dragger.limitY(100));
  },

  /**
     @suppress {missingProperties,checkTypes} suppression added to enable type
     checking
   */
  testWindowBlur() {
    const dragger = new Dragger(target);
    dragger.setAllowSetCapture(false);

    let dragEnded = false;
    events.listen(dragger, Dragger.EventType.END, (e) => {
      dragEnded = true;
    });

    let e = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.type = EventType.MOUSEDOWN;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 1;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 2;
    e.isMouseActionButton().$returns(true);
    e.preventDefault();
    e.$replay();
    dragger.startDrag(e);
    e.$verify();

    assertTrue(dragger.isDragging());

    e = new BrowserEvent();
    e.type = EventType.BLUR;
    /** @suppress {checkTypes} suppression added to enable type checking */
    e.target = window;
    /** @suppress {checkTypes} suppression added to enable type checking */
    e.currentTarget = window;
    testingEvents.fireBrowserEvent(e);

    assertTrue(dragEnded);
  },

  /**
     @suppress {missingProperties,checkTypes} suppression added to enable type
     checking
   */
  testBlur() {
    const dragger = new Dragger(target);
    dragger.setAllowSetCapture(false);

    let dragEnded = false;
    events.listen(dragger, Dragger.EventType.END, (e) => {
      dragEnded = true;
    });

    let e = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.type = EventType.MOUSEDOWN;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientX = 1;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.clientY = 2;
    e.isMouseActionButton().$returns(true);
    e.preventDefault();
    e.$replay();
    dragger.startDrag(e);
    e.$verify();

    assertTrue(dragger.isDragging());

    e = new BrowserEvent();
    e.type = EventType.BLUR;
    e.target = document.body;
    e.currentTarget = document.body;
    // Blur events do not bubble but the test event system does not emulate that
    // part so we add a capturing listener on the target and stops the
    // propagation at the target, preventing any event from bubbling.
    events.listen(document.body, EventType.BLUR, (e) => {
      e.propagationStopped_ = true;
    }, true);
    testingEvents.fireBrowserEvent(e);

    assertFalse(dragEnded);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCloneNode() {
    const element = dom.createDom(TagName.DIV);
    element.innerHTML = '<input type="hidden" value="v0">' +
        '<textarea>v1</textarea>' +
        '<textarea>v2</textarea>';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    element.childNodes[0].value = '\'new\'\n"value"';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    element.childNodes[1].value = '<' +
        '/textarea>&lt;3';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    element.childNodes[2].value = '<script>\n\talert("oops!");<' +
        '/script>';
    let clone = Dragger.cloneNode(element);
    assertEquals(element.childNodes[0].value, clone.childNodes[0].value);
    assertEquals(element.childNodes[1].value, clone.childNodes[1].value);
    assertEquals(element.childNodes[2].value, clone.childNodes[2].value);
    /** @suppress {checkTypes} suppression added to enable type checking */
    clone = Dragger.cloneNode(element.childNodes[2]);
    assertEquals(element.childNodes[2].value, clone.value);
  },
});
