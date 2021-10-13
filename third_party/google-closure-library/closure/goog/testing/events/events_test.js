/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.eventsTest');
goog.setTestOnly();

const Coordinate = goog.require('goog.math.Coordinate');
const EventType = goog.require('goog.events.EventType');
const InputType = goog.require('goog.dom.InputType');
const KeyCodes = goog.require('goog.events.KeyCodes');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googString = goog.require('goog.string');
const recordFunction = goog.require('goog.testing.recordFunction');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

let firedEventTypes;
let firedEventCoordinates;
let firedScreenCoordinates;
let firedShiftKeys;
let firedKeyCodes;
let root;
let log;
let input;
let testButton;
let parentEl;
let childEl;
const coordinate = new Coordinate(123, 456);
const stubs = new PropertyReplacer();
let eventCount;

// For a double click, IE fires selectstart instead of the second mousedown,
// but we don't simulate selectstart. Also, IE doesn't fire the second click.
const DBLCLICK_SEQ =
    (userAgent.IE ? ['mousedown', 'mouseup', 'click', 'mouseup', 'dblclick'] : [
      'mousedown',
      'mouseup',
      'click',
      'mousedown',
      'mouseup',
      'click',
      'dblclick',
    ]);

const DBLCLICK_SEQ_COORDS = (new Array(DBLCLICK_SEQ.length)).fill(coordinate);

const CONTEXTMENU_SEQ = userAgent.WINDOWS ?
    ['mousedown', 'mouseup', 'contextmenu'] :
    userAgent.GECKO ? ['mousedown', 'contextmenu', 'mouseup'] :
                      userAgent.WEBKIT && userAgent.MAC ?
                      ['mousedown', 'contextmenu', 'mouseup', 'click'] :
                      ['mousedown', 'contextmenu', 'mouseup'];

/** Assert that the list of events given was fired, in that order. */
function assertEventTypes(list) {
  assertArrayEquals(list, firedEventTypes);
}

/**
 * Assert that the list of event coordinates given was caught, in that order.
 */
function assertCoordinates(list) {
  assertArrayEquals(list, firedEventCoordinates);
  assertArrayEquals(list, firedScreenCoordinates);
}

/** Prevent default the event of the given type on the root element. */
function preventDefaultEventType(type) {
  events.listen(root, type, preventDefault);
}

function preventDefault(e) {
  e.preventDefault();
}
testSuite({
  setUpPage() {
    root = dom.getElement('root');
    log = dom.getElement('log');
    input = dom.getElement('input');
    testButton = dom.getElement('testButton');
    parentEl = dom.getElement('parentEl');
    childEl = dom.getElement('childEl');
  },

  setUp() {
    stubs.reset();
    events.removeAll(root);
    events.removeAll(log);
    events.removeAll(input);
    events.removeAll(testButton);
    events.removeAll(parentEl);
    events.removeAll(childEl);

    dom.removeChildren(root);
    firedEventTypes = [];
    firedEventCoordinates = [];
    firedScreenCoordinates = [];
    firedShiftKeys = [];
    firedKeyCodes = [];

    for (let key in EventType) {
      events.listen(root, EventType[key], (e) => {
        firedEventTypes.push(e.type);
        const coord = new Coordinate(e.clientX, e.clientY);
        firedEventCoordinates.push(coord);

        firedScreenCoordinates.push(new Coordinate(e.screenX, e.screenY));

        firedShiftKeys.push(!!e.shiftKey);
        firedKeyCodes.push(e.keyCode);
      });
    }

    eventCount =
        {parentBubble: 0, parentCapture: 0, childCapture: 0, childBubble: 0};
    // Event listeners for the capture/bubble test.
    events.listen(parentEl, EventType.CLICK, (e) => {
      eventCount.parentCapture++;
      assertEquals(parentEl, e.currentTarget);
      assertEquals(childEl, e.target);
    }, true);
    events.listen(childEl, EventType.CLICK, (e) => {
      eventCount.childCapture++;
      assertEquals(childEl, e.currentTarget);
      assertEquals(childEl, e.target);
    }, true);
    events.listen(childEl, EventType.CLICK, (e) => {
      eventCount.childBubble++;
      assertEquals(childEl, e.currentTarget);
      assertEquals(childEl, e.target);
    });
    events.listen(parentEl, EventType.CLICK, (e) => {
      eventCount.parentBubble++;
      assertEquals(parentEl, e.currentTarget);
      assertEquals(childEl, e.target);
    });
  },

  tearDownPage() {
    for (let key in EventType) {
      const type = EventType[key];
      if (type == 'mousemove' || type == 'mouseout' || type == 'mouseover') {
        continue;
      }
      dom.appendChild(
          input,
          dom.createDom(
              TagName.LABEL, null,
              dom.createDom(
                  TagName.INPUT, {'id': type, 'type': InputType.CHECKBOX}),
              type, dom.createDom(TagName.BR)));
      events.listen(
          testButton, type, /**
                               @suppress {strictMissingProperties} suppression
                               added to enable type checking
                             */
          (e) => {
            if (dom.getElement(e.type).checked) {
              e.preventDefault();
            }

            log.append(
                document.createElement('br'),
                googString.subs('%s (%s, %s)', e.type, e.clientX, e.clientY));
          });
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMouseEnter() {
    testingEvents.fireMouseEnterEvent(root, null);
    testingEvents.fireMouseEnterEvent(root, null, coordinate);
    assertEventTypes(['mouseenter', 'mouseenter']);
    assertCoordinates([style.getClientPosition(root), coordinate]);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMouseLeave() {
    testingEvents.fireMouseLeaveEvent(root, null);
    testingEvents.fireMouseLeaveEvent(root, null, coordinate);
    assertEventTypes(['mouseleave', 'mouseleave']);
    assertCoordinates([style.getClientPosition(root), coordinate]);
  },

  testMouseOver() {
    testingEvents.fireMouseOverEvent(root, null);
    testingEvents.fireMouseOverEvent(root, null, coordinate);
    assertEventTypes(['mouseover', 'mouseover']);
    assertCoordinates([style.getClientPosition(root), coordinate]);
  },

  testMouseOut() {
    testingEvents.fireMouseOutEvent(root, null);
    testingEvents.fireMouseOutEvent(root, null, coordinate);
    assertEventTypes(['mouseout', 'mouseout']);
    assertCoordinates([style.getClientPosition(root), coordinate]);
  },

  testFocus() {
    testingEvents.fireFocusEvent(root);
    assertEventTypes(['focus']);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testFocusIn() {
    testingEvents.fireFocusInEvent(root);
    assertEventTypes([EventType.FOCUSIN]);
  },

  testBlur() {
    testingEvents.fireBlurEvent(root);
    assertEventTypes(['blur']);
  },

  testClickSequence() {
    assertTrue(testingEvents.fireClickSequence(root));
    assertEventTypes(['mousedown', 'mouseup', 'click']);
    const rootPosition = style.getClientPosition(root);
    assertCoordinates([rootPosition, rootPosition, rootPosition]);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testClickSequenceWithCoordinate() {
    assertTrue(testingEvents.fireClickSequence(root, null, coordinate));
    assertCoordinates([coordinate, coordinate, coordinate]);
    assertArrayEquals([false, false, false], firedShiftKeys);
  },

  testTouchStart() {
    testingEvents.fireTouchStartEvent(root);
    testingEvents.fireTouchStartEvent(root, coordinate);
    assertEventTypes(['touchstart', 'touchstart']);
    assertCoordinates([style.getClientPosition(root), coordinate]);
  },

  testTouchMove() {
    testingEvents.fireTouchMoveEvent(root);
    testingEvents.fireTouchMoveEvent(root, coordinate, {touches: []});
    assertEventTypes(['touchmove', 'touchmove']);
    assertCoordinates([style.getClientPosition(root), coordinate]);
  },

  testTouchEnd() {
    testingEvents.fireTouchEndEvent(root);
    testingEvents.fireTouchEndEvent(root, coordinate);
    assertEventTypes(['touchend', 'touchend']);
    assertCoordinates([style.getClientPosition(root), coordinate]);
  },

  testTouchSequence() {
    assertTrue(testingEvents.fireTouchSequence(root));
    assertEventTypes(['touchstart', 'touchend']);
    const rootPosition = style.getClientPosition(root);
    assertCoordinates([rootPosition, rootPosition]);
  },

  testTouchSequenceWithCoordinate() {
    assertTrue(testingEvents.fireTouchSequence(root, coordinate));
    assertCoordinates([coordinate, coordinate]);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testClickSequenceWithEventProperty() {
    assertTrue(testingEvents.fireClickSequence(
        root, null, undefined, {shiftKey: true}));
    assertArrayEquals([true, true, true], firedShiftKeys);
  },

  testClickSequenceCancellingMousedown() {
    preventDefaultEventType('mousedown');
    assertFalse(testingEvents.fireClickSequence(root));
    assertEventTypes(['mousedown', 'mouseup', 'click']);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testClickSequenceCancellingMousedownWithCoordinate() {
    preventDefaultEventType('mousedown');
    assertFalse(testingEvents.fireClickSequence(root, null, coordinate));
    assertCoordinates([coordinate, coordinate, coordinate]);
  },

  testClickSequenceCancellingMouseup() {
    preventDefaultEventType('mouseup');
    assertFalse(testingEvents.fireClickSequence(root));
    assertEventTypes(['mousedown', 'mouseup', 'click']);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testClickSequenceCancellingMouseupWithCoordinate() {
    preventDefaultEventType('mouseup');
    assertFalse(testingEvents.fireClickSequence(root, null, coordinate));
    assertCoordinates([coordinate, coordinate, coordinate]);
  },

  testClickSequenceCancellingClick() {
    preventDefaultEventType('click');
    assertFalse(testingEvents.fireClickSequence(root));
    assertEventTypes(['mousedown', 'mouseup', 'click']);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testClickSequenceCancellingClickWithCoordinate() {
    preventDefaultEventType('click');
    assertFalse(testingEvents.fireClickSequence(root, null, coordinate));
    assertCoordinates([coordinate, coordinate, coordinate]);
  },

  testDoubleClickSequence() {
    assertTrue(testingEvents.fireDoubleClickSequence(root));
    assertEventTypes(DBLCLICK_SEQ);
  },

  testDoubleClickSequenceWithCoordinate() {
    assertTrue(testingEvents.fireDoubleClickSequence(root, coordinate));
    assertCoordinates(DBLCLICK_SEQ_COORDS);
  },

  testDoubleClickSequenceCancellingMousedown() {
    preventDefaultEventType('mousedown');
    assertFalse(testingEvents.fireDoubleClickSequence(root));
    assertEventTypes(DBLCLICK_SEQ);
  },

  testDoubleClickSequenceCancellingMousedownWithCoordinate() {
    preventDefaultEventType('mousedown');
    assertFalse(testingEvents.fireDoubleClickSequence(root, coordinate));
    assertCoordinates(DBLCLICK_SEQ_COORDS);
  },

  testDoubleClickSequenceCancellingMouseup() {
    preventDefaultEventType('mouseup');
    assertFalse(testingEvents.fireDoubleClickSequence(root));
    assertEventTypes(DBLCLICK_SEQ);
  },

  testDoubleClickSequenceCancellingMouseupWithCoordinate() {
    preventDefaultEventType('mouseup');
    assertFalse(testingEvents.fireDoubleClickSequence(root, coordinate));
    assertCoordinates(DBLCLICK_SEQ_COORDS);
  },

  testDoubleClickSequenceCancellingClick() {
    preventDefaultEventType('click');
    assertFalse(testingEvents.fireDoubleClickSequence(root));
    assertEventTypes(DBLCLICK_SEQ);
  },

  testDoubleClickSequenceCancellingClickWithCoordinate() {
    preventDefaultEventType('click');
    assertFalse(testingEvents.fireDoubleClickSequence(root, coordinate));
    assertCoordinates(DBLCLICK_SEQ_COORDS);
  },

  testDoubleClickSequenceCancellingDoubleClick() {
    preventDefaultEventType('dblclick');
    assertFalse(testingEvents.fireDoubleClickSequence(root));
    assertEventTypes(DBLCLICK_SEQ);
  },

  testDoubleClickSequenceCancellingDoubleClickWithCoordinate() {
    preventDefaultEventType('dblclick');
    assertFalse(testingEvents.fireDoubleClickSequence(root, coordinate));
    assertCoordinates(DBLCLICK_SEQ_COORDS);
  },

  testKeySequence() {
    assertTrue(testingEvents.fireKeySequence(root, KeyCodes.ZERO));
    assertEventTypes(['keydown', 'keypress', 'keyup']);
  },

  testKeySequenceCancellingKeydown() {
    preventDefaultEventType('keydown');
    assertFalse(testingEvents.fireKeySequence(root, KeyCodes.ZERO));
    assertEventTypes(['keydown', 'keyup']);
  },

  testKeySequenceCancellingKeypress() {
    preventDefaultEventType('keypress');
    assertFalse(testingEvents.fireKeySequence(root, KeyCodes.ZERO));
    assertEventTypes(['keydown', 'keypress', 'keyup']);
  },

  testKeySequenceCancellingKeyup() {
    preventDefaultEventType('keyup');
    assertFalse(testingEvents.fireKeySequence(root, KeyCodes.ZERO));
    assertEventTypes(['keydown', 'keypress', 'keyup']);
  },

  testKeySequenceWithEscapeKey() {
    assertTrue(testingEvents.fireKeySequence(root, KeyCodes.ESC));
    if (userAgent.EDGE || userAgent.GECKO || userAgent.WEBKIT) {
      assertEventTypes(['keydown', 'keyup']);
    } else {
      assertEventTypes(['keydown', 'keypress', 'keyup']);
    }
  },

  testKeySequenceForMacActionKeys() {
    stubs.set(userAgent, 'GECKO', true);
    stubs.set(userAgent, 'MAC', true);
    testingEvents.fireKeySequence(root, KeyCodes.C, {'metaKey': true});
    assertEventTypes(['keydown', 'keyup']);
  },

  testKeySequenceForOptionKeysOnMac() {
    // Mac uses an option (or alt) key to type non-ASCII characters. This test
    // verifies we can emulate key events sent when typing such non-ASCII
    // characters.
    stubs.set(userAgent, 'WEBKIT', true);
    stubs.set(userAgent, 'MAC', true);

    const optionKeyCodes = [
      [0xc0, 0x00e6],  // option+'
      [0xbc, 0x2264],  // option+,
      [0xbd, 0x2013],  // option+-
      [0xbe, 0x2265],  // option+.
      [0xbf, 0x00f7],  // option+/
      [0x30, 0x00ba],  // option+0
      [0x31, 0x00a1],  // option+1
      [0x32, 0x2122],  // option+2
      [0x33, 0x00a3],  // option+3
      [0x34, 0x00a2],  // option+4
      [0x35, 0x221e],  // option+5
      [0x36, 0x00a7],  // option+6
      [0x37, 0x00b6],  // option+7
      [0x38, 0x2022],  // option+8
      [0x39, 0x00aa],  // option+9
      [0xba, 0x2026],  // option+;
      [0xbb, 0x2260],  // option+=
      [0xdb, 0x201c],  // option+[
      [
        0xdc,
        0x00ab,
      ],               // option+\
      [0xdd, 0x2018],  // option+]
      [0x41, 0x00e5],  // option+a
      [0x42, 0x222b],  // option+b
      [0x43, 0x00e7],  // option+c
      [0x44, 0x2202],  // option+d
      [0x45, 0x00b4],  // option+e
      [0x46, 0x0192],  // option+f
      [0x47, 0x00a9],  // option+g
      [0x48, 0x02d9],  // option+h
      [0x49, 0x02c6],  // option+i
      [0x4a, 0x2206],  // option+j
      [0x4b, 0x02da],  // option+k
      [0x4c, 0x00ac],  // option+l
      [0x4d, 0x00b5],  // option+m
      [0x4e, 0x02dc],  // option+n
      [0x4f, 0x00f8],  // option+o
      [0x50, 0x03c0],  // option+p
      [0x51, 0x0153],  // option+q
      [0x52, 0x00ae],  // option+r
      [0x53, 0x00df],  // option+s
      [0x54, 0x2020],  // option+t
      [0x56, 0x221a],  // option+v
      [0x57, 0x2211],  // option+w
      [0x58, 0x2248],  // option+x
      [0x59, 0x00a5],  // option+y
      [0x5a, 0x03a9]   // option+z
    ];

    for (let i = 0; i < optionKeyCodes.length; ++i) {
      firedEventTypes = [];
      firedKeyCodes = [];
      const keyCode = optionKeyCodes[i][0];
      const keyPressKeyCode = optionKeyCodes[i][1];
      testingEvents.fireNonAsciiKeySequence(
          root, keyCode, keyPressKeyCode, {'altKey': true});
      assertEventTypes(['keydown', 'keypress', 'keyup']);
      assertArrayEquals([keyCode, keyPressKeyCode, keyCode], firedKeyCodes);
    }
  },

  testContextMenuSequence() {
    assertTrue(testingEvents.fireContextMenuSequence(root));
    assertEventTypes(CONTEXTMENU_SEQ);
  },

  testContextMenuSequenceWithCoordinate() {
    assertTrue(testingEvents.fireContextMenuSequence(root, coordinate));
    assertEventTypes(CONTEXTMENU_SEQ);
    assertCoordinates((new Array(CONTEXTMENU_SEQ.length)).fill(coordinate));
  },

  testContextMenuSequenceCancellingMousedown() {
    preventDefaultEventType('mousedown');
    assertFalse(testingEvents.fireContextMenuSequence(root));
    assertEventTypes(CONTEXTMENU_SEQ);
  },

  testContextMenuSequenceCancellingMouseup() {
    preventDefaultEventType('mouseup');
    assertFalse(testingEvents.fireContextMenuSequence(root));
    assertEventTypes(CONTEXTMENU_SEQ);
  },

  testContextMenuSequenceCancellingContextMenu() {
    preventDefaultEventType('contextmenu');
    assertFalse(testingEvents.fireContextMenuSequence(root));
    assertEventTypes(CONTEXTMENU_SEQ);
  },

  testContextMenuSequenceFakeMacWebkit() {
    stubs.set(userAgent, 'WINDOWS', false);
    stubs.set(userAgent, 'MAC', true);
    stubs.set(userAgent, 'WEBKIT', true);
    assertTrue(testingEvents.fireContextMenuSequence(root));
    assertEventTypes(['mousedown', 'contextmenu', 'mouseup', 'click']);
  },

  testCaptureBubble_simple() {
    assertTrue(testingEvents.fireClickEvent(childEl));
    assertObjectEquals(
        {parentCapture: 1, childCapture: 1, childBubble: 1, parentBubble: 1},
        eventCount);
  },

  testCaptureBubble_preventDefault() {
    events.listen(childEl, EventType.CLICK, (e) => {
      e.preventDefault();
    });
    assertFalse(testingEvents.fireClickEvent(childEl));
    assertObjectEquals(
        {parentCapture: 1, childCapture: 1, childBubble: 1, parentBubble: 1},
        eventCount);
  },

  testCaptureBubble_stopPropagationParentCapture() {
    events.listen(parentEl, EventType.CLICK, (e) => {
      e.stopPropagation();
    }, true /* capture */);
    assertTrue(testingEvents.fireClickEvent(childEl));
    assertObjectEquals(
        {parentCapture: 1, childCapture: 0, childBubble: 0, parentBubble: 0},
        eventCount);
  },

  testCaptureBubble_stopPropagationChildCapture() {
    events.listen(childEl, EventType.CLICK, (e) => {
      e.stopPropagation();
    }, true /* capture */);
    assertTrue(testingEvents.fireClickEvent(childEl));
    assertObjectEquals(
        {parentCapture: 1, childCapture: 1, childBubble: 0, parentBubble: 0},
        eventCount);
  },

  testCaptureBubble_stopPropagationChildBubble() {
    events.listen(childEl, EventType.CLICK, (e) => {
      e.stopPropagation();
    });
    assertTrue(testingEvents.fireClickEvent(childEl));
    assertObjectEquals(
        {parentCapture: 1, childCapture: 1, childBubble: 1, parentBubble: 0},
        eventCount);
  },

  testCaptureBubble_stopPropagationParentBubble() {
    events.listen(parentEl, EventType.CLICK, (e) => {
      e.stopPropagation();
    });
    assertTrue(testingEvents.fireClickEvent(childEl));
    assertObjectEquals(
        {parentCapture: 1, childCapture: 1, childBubble: 1, parentBubble: 1},
        eventCount);
  },

  /**
     @suppress {checkTypes,missingProperties,strictMissingProperties}
     suppression added to enable type checking
   */
  testMixinListenable() {
    const obj = {};
    obj.doFoo = recordFunction();

    testingEvents.mixinListenable(obj);

    obj.doFoo();
    assertEquals(1, obj.doFoo.getCallCount());

    const handler = recordFunction();
    events.listen(obj, 'test', handler);
    obj.dispatchEvent('test');
    assertEquals(1, handler.getCallCount());
    assertEquals(obj, handler.getLastCall().getArgument(0).target);

    events.unlisten(obj, 'test', handler);
    obj.dispatchEvent('test');
    assertEquals(1, handler.getCallCount());

    events.listen(obj, 'test', handler);
    obj.dispose();
    obj.dispatchEvent('test');
    assertEquals(1, handler.getCallCount());
  },
});
