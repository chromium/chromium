/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.fx.DragListGroupTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const BrowserFeature = goog.require('goog.events.BrowserFeature');
const Coordinate = goog.require('goog.math.Coordinate');
const DragListDirection = goog.require('goog.fx.DragListDirection');
const DragListGroup = goog.require('goog.fx.DragListGroup');
const DragListPermission = goog.require('goog.fx.DragListPermission');
const Dragger = goog.require('goog.fx.Dragger');
const EventType = goog.require('goog.events.EventType');
const FxDragEvent = goog.require('goog.fx.DragEvent');
const GoogEvent = goog.require('goog.events.Event');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googArray = goog.require('goog.array');
const googObject = goog.require('goog.object');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

/** @type {DragListGroup} */
let dlg;

/** @type {?} */
let list;

/** @type {?} */
let list2;

/** @type {BrowserEvent} */
let event;

/**
 * The number of event listeners registered by the DragListGroup after the
 * init() call.
 * @type {number}
 */
let initialListenerCount;

/**
 * Type of events fired by the DragListGroup.
 * @type {!Array<string>}
 */
let firedEventTypes;

/**
 * Asserts that the DragListGroup is in idle state.
 * @param {!DragListGroup} dlg The DragListGroup to examine.
 * @suppress {visibility} suppression added to enable type checking
 */
function assertIdle(dlg) {
  assertFalse(dlg.isDragging());
  assertNull('dragger element has been cleaned up', dlg.draggerEl_);
  assertNull('dragger has been cleaned up', dlg.dragger_);
  assertEquals(
      'the additional event listeners have been removed', initialListenerCount,
      googObject.getCount(dlg.eventHandler_.keys_));
}

/**
 * @param {!DragListPermission} dragListPermission
 * @suppress {checkTypes} suppression added to enable type checking
 */
function setUpWithDragListPermission(dragListPermission) {
  dlg.dispose();
  const sandbox = dom.getElement('sandbox');
  dom.removeChildren(sandbox);

  list = dom.createDom(TagName.DIV, {'id': 'horiz_div'});
  list.appendChild(dom.createDom(TagName.DIV, null, dom.createTextNode('1')));
  list.appendChild(dom.createDom(TagName.DIV, null, dom.createTextNode('2')));
  list.appendChild(dom.createDom(TagName.DIV, null, dom.createTextNode('3')));
  sandbox.appendChild(list);

  list2 = dom.createDom(TagName.DIV, {'id': 'horiz_div2'});
  list2.appendChild(dom.createDom(TagName.DIV, null, dom.createTextNode('A')));
  list2.appendChild(dom.createDom(TagName.DIV, null, dom.createTextNode('B')));
  list2.appendChild(dom.createDom(TagName.DIV, null, dom.createTextNode('C')));
  sandbox.appendChild(list2);

  dlg = new DragListGroup();
  dlg.addDragList(
      list, DragListDirection.RIGHT, null /** opt_unused */, 'test_hover_class',
      dragListPermission);
  dlg.addDragList(list2, DragListDirection.RIGHT);
  dlg.init();

  /** @suppress {visibility} suppression added to enable type checking */
  initialListenerCount = googObject.getCount(dlg.eventHandler_.keys_);

  event = new BrowserEvent();
  event.currentTarget = dom.getElementsByTagName(TagName.DIV, list)[0];

  firedEventTypes = [];
  events.listen(dlg, googObject.getValues(DragListGroup.EventType), (e) => {
    firedEventTypes.push(e.type);
  });
}

/**
 * Method that starts the dragging motion and wait in order for the
 * event based DragListGroup internal logic to execute.
 * @param {!DragListGroup|null} dlg The DragListGroup to examine
 * @param {boolean} correctPosition Whethere to correct draggedEL position
 * @return {!Coordinate} The mouse cursor position where the dragginig
 *     started at.
 * @suppress {visibility,checkTypes} Suppression added to enable type checking
 */
function startDrag(dlg, correctPosition) {
  dlg.overrideCorrectDraggedElementInitialPos(correctPosition);
  const fistChildBoundingRect = list.firstChild.getBoundingClientRect();
  // 450px is chosen so the click is always placed outside of the dragged
  // element (which is only 50px wide), but still on an element of the dragged
  // list group (which takes up the whole page width).
  const firstChildClickPositionX = 450;
  const firstChildClickPositionY =
      fistChildBoundingRect.top + fistChildBoundingRect.height / 2;

  const dragStartPosition =
      new Coordinate(firstChildClickPositionX, firstChildClickPositionY);
  testingEvents.fireMouseDownEvent(
      list.firstChild, BrowserEvent.MouseButton.LEFT, dragStartPosition);

  // Firing an additional DragStart event (on top of the one triggered by
  // the mousedown above) is necessary as the test framework interrupts the
  // event handling execution flow by executing the handler methods directly
  // after the event fires, leading to overriding of the initial position
  // computation. This duplication solves the issue.
  const duplicateEvent = new BrowserEvent();
  duplicateEvent.currentTarget = list.firstChild;
  duplicateEvent.clientX = firstChildClickPositionX;
  duplicateEvent.clientY = firstChildClickPositionY;
  dlg.dragger_.fireDragStart_(duplicateEvent);

  const draggedElement = dom.getElementByTagNameAndClass('div', 'cursor_move');
  const firstMovedMousePosition = new Coordinate(
      firstChildClickPositionX + 2, firstChildClickPositionY + 2);
  testingEvents.fireMouseMoveEvent(draggedElement, firstMovedMousePosition);

  const newMovedMousePosition = new Coordinate(
      firstMovedMousePosition.x + 2, firstMovedMousePosition.y + 2);
  testingEvents.fireMouseMoveEvent(draggedElement, newMovedMousePosition);

  return newMovedMousePosition;
}

/**
 * Whether the mouse is over the currently dragged element.
 * @param {!DragListGroup|null} dlg The DragListGroup to examine
 * @param {!Coordinate} newMousePosition The mouse cursor position
 * @return {boolean} Whether the cursor is over the draggerEl or not
 * @suppress {visibility} suppression added to enable type checking
 */
function isCursorOverDragger(dlg, newMousePosition) {
  const draggedElRect = dlg.draggerEl_.getBoundingClientRect();
  const isContainedInWidth = draggedElRect.left <= newMousePosition.x &&
      draggedElRect.right >= newMousePosition.x;
  const isContainedInHeight = draggedElRect.top <= newMousePosition.y &&
      draggedElRect.bottom >= newMousePosition.y;

  return isContainedInWidth && isContainedInHeight;
}

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  setUp() {
    const sandbox = dom.getElement('sandbox');
    list = dom.createDom(TagName.DIV, {'id': 'horiz_div'});
    list.appendChild(dom.createDom(TagName.DIV, null, dom.createTextNode('1')));
    list.appendChild(dom.createDom(TagName.DIV, null, dom.createTextNode('2')));
    list.appendChild(dom.createDom(TagName.DIV, null, dom.createTextNode('3')));
    sandbox.appendChild(list);

    dlg = new DragListGroup();
    dlg.setDragItemHoverClass('opacity_40', 'cursor_move');
    dlg.setDragItemHandleHoverClass('opacity_40', 'cursor_pointer');
    dlg.setCurrDragItemClass('blue_bg', 'opacity_40');
    dlg.setDraggerElClass('cursor_move blue_bg reduced-size');
    dlg.addDragList(list, DragListDirection.RIGHT);
    dlg.init();

    /** @suppress {visibility} suppression added to enable type checking */
    initialListenerCount = googObject.getCount(dlg.eventHandler_.keys_);

    event = new BrowserEvent();
    event.currentTarget = dom.getElementsByTagName(TagName.DIV, list)[0];

    firedEventTypes = [];
    events.listen(dlg, googObject.getValues(DragListGroup.EventType), (e) => {
      firedEventTypes.push(e.type);
    });
  },

  tearDown() {
    // Finish the potential on-going drag sequence to ensure no test state
    // bleeding.
    testingEvents.fireMouseUpEvent(list.firstChild);
    dlg.dispose();
    dom.removeChildren(dom.getElement('sandbox'));
  },

  /**
   * Test the initial assumptions.
   * Verify that the setter methods work properly, i.e., the CSS classes are
   * stored in the private arrays after init() but are not added yet to target.
   * (Since initially, we are not yet hovering over any list, in particular,
   * over this target.)
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testSettersAfterInit() {
    assertTrue(googArray.equals(
        dlg.dragItemHoverClasses_, ['opacity_40', 'cursor_move']));
    assertTrue(googArray.equals(
        dlg.dragItemHandleHoverClasses_, ['opacity_40', 'cursor_pointer']));
    assertTrue(
        googArray.equals(dlg.currDragItemClasses_, ['blue_bg', 'opacity_40']));

    assertFalse(
        'Should have no cursor_move class after init',
        classlist.contains(event.currentTarget, 'cursor_move'));
    assertFalse(
        'Should have no cursor_pointer class after init',
        classlist.contains(event.currentTarget, 'cursor_pointer'));
    assertFalse(
        'Should have no opacity_40 class after init',
        classlist.contains(event.currentTarget, 'opacity_40'));
    assertFalse(
        'Should not have blue_bg class after init',
        classlist.contains(event.currentTarget, 'blue_bg'));
  },

  /**
   * Test the effect of hovering over a list.
   * Check that after the MOUSEOVER browser event these classes are added to
   * the current target of the event.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testAddDragItemHoverClasses() {
    dlg.handleDragItemMouseover_(event);

    assertTrue(
        'Should have cursor_move class after MOUSEOVER',
        classlist.contains(event.currentTarget, 'cursor_move'));
    assertTrue(
        'Should have opacity_40 class after MOUSEOVER',
        classlist.contains(event.currentTarget, 'opacity_40'));
    assertFalse(
        'Should not have cursor_pointer class after MOUSEOVER',
        classlist.contains(event.currentTarget, 'cursor_pointer'));
    assertFalse(
        'Should not have blue_bg class after MOUSEOVER',
        classlist.contains(event.currentTarget, 'blue_bg'));
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testAddDragItemHandleHoverClasses() {
    dlg.handleDragItemHandleMouseover_(event);

    assertFalse(
        'Should not have cursor_move class after MOUSEOVER',
        classlist.contains(event.currentTarget, 'cursor_move'));
    assertTrue(
        'Should have opacity_40 class after MOUSEOVER',
        classlist.contains(event.currentTarget, 'opacity_40'));
    assertTrue(
        'Should have cursor_pointer class after MOUSEOVER',
        classlist.contains(event.currentTarget, 'cursor_pointer'));
    assertFalse(
        'Should not have blue_bg class after MOUSEOVER',
        classlist.contains(event.currentTarget, 'blue_bg'));
  },

  /**
   * Test the effect of stopping hovering over a list.
   * Check that after the MOUSEOUT browser event all CSS classes are removed
   * from the target (as we are no longer hovering over the it).
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testRemoveDragItemHoverClasses() {
    dlg.handleDragItemMouseover_(event);
    dlg.handleDragItemMouseout_(event);

    assertFalse(
        'Should have no cursor_move class after MOUSEOUT',
        classlist.contains(event.currentTarget, 'cursor_move'));
    assertFalse(
        'Should have no cursor_pointer class after MOUSEOUT',
        classlist.contains(event.currentTarget, 'cursor_pointer'));
    assertFalse(
        'Should have no opacity_40 class after MOUSEOUT',
        classlist.contains(event.currentTarget, 'opacity_40'));
    assertFalse(
        'Should have no blue_bg class after MOUSEOUT',
        classlist.contains(event.currentTarget, 'blue_bg'));
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testRemoveDragItemHandleHoverClasses() {
    dlg.handleDragItemHandleMouseover_(event);
    dlg.handleDragItemHandleMouseout_(event);

    assertFalse(
        'Should have no cursor_move class after MOUSEOUT',
        classlist.contains(event.currentTarget, 'cursor_move'));
    assertFalse(
        'Should have no cursor_pointer class after MOUSEOUT',
        classlist.contains(event.currentTarget, 'cursor_pointer'));
    assertFalse(
        'Should have no opacity_40 class after MOUSEOUT',
        classlist.contains(event.currentTarget, 'opacity_40'));
    assertFalse(
        'Should have no blue_bg class after MOUSEOUT',
        classlist.contains(event.currentTarget, 'blue_bg'));
  },

  /**
   * Test the effect of dragging an item. (DRAGSTART event.)
   * Check that after the MOUSEDOWN browser event is handled by the
   * handlePotentialDragStart_() method the currDragItem has the CSS classes
   * set by the setter method.
   * @suppress {visibility} suppression added to enable type checking
   */
  testAddCurrentDragItemClasses() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const be = new BrowserEvent({
      type: EventType.MOUSEDOWN,
      button: BrowserFeature.HAS_W3C_BUTTON ? 0 : 1,
    });
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    event.event_ = be;

    dlg.handlePotentialDragStart_(event);

    assertFalse(
        'Should have no cursor_move class after MOUSEDOWN',
        classlist.contains(dlg.currDragItem_, 'cursor_move'));
    assertFalse(
        'Should have no cursor_pointer class after MOUSEDOWN',
        classlist.contains(dlg.currDragItem_, 'cursor_pointer'));
    assertTrue(
        'Should have opacity_40 class after MOUSEDOWN',
        classlist.contains(dlg.currDragItem_, 'opacity_40'));
    assertTrue(
        'Should have blue_bg class after MOUSEDOWN',
        classlist.contains(dlg.currDragItem_, 'blue_bg'));
  },

  /**
   * Test the effect of dragging an item. (DRAGEND event.)
   * Check that after the MOUSEUP browser event handled by the handleDragEnd_()
   * method the currDragItem has no CSS classes set in the dispatched event.
   * @suppress {visibility} suppression added to enable type checking
   */
  testRemoveCurrentDragItemClasses() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const be = new BrowserEvent({
      type: EventType.MOUSEDOWN,
      button: BrowserFeature.HAS_W3C_BUTTON ? 0 : 1,
    });
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    event.event_ = be;
    dlg.handlePotentialDragStart_(event);

    // Need to catch the dispatched event because the temporary fields
    // including dlg.currentDragItem_ are cleared after the dragging has ended.
    let currDragItem = dom.createDom(
        TagName.DIV, ['cursor_move', 'blue_bg'], dom.createTextNode('4'));
    events.listen(dlg, DragListGroup.EventType.DRAGEND, (e) => {
      /** @suppress {visibility} suppression added to enable type checking */
      currDragItem = dlg.currDragItem_;
    });

    /** @suppress {checkTypes} suppression added to enable type checking */
    const dragger = new Dragger(event.currentTarget);
    be.type = EventType.MOUSEUP;
    be.clientX = 1;
    be.clientY = 2;
    const dragEvent = new FxDragEvent(
        Dragger.EventType.END, dragger, be.clientX, be.clientY, be);
    dlg.handleDragEnd_(dragEvent);  // this method dispatches the DRAGEND event
    dragger.dispose();

    assertFalse(
        'Should have no cursor_move class after MOUSEUP',
        classlist.contains(currDragItem, 'cursor_move'));
    assertFalse(
        'Should have no cursor_pointer class after MOUSEUP',
        classlist.contains(currDragItem, 'cursor_pointer'));
    assertFalse(
        'Should have no opacity_40 class after MOUSEUP',
        classlist.contains(currDragItem, 'opacity_40'));
    assertFalse(
        'Should have no blue_bg class after MOUSEUP',
        classlist.contains(currDragItem, 'blue_bg'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testFiredEvents() {
    testingEvents.fireClickSequence(list.firstChild);
    assertArrayEquals(
        'event types in case of zero distance dragging',
        [
          DragListGroup.EventType.DRAGGERCREATED.toString(),
          DragListGroup.EventType.BEFOREDRAGSTART.toString(),
          DragListGroup.EventType.DRAGSTART.toString(),
          DragListGroup.EventType.BEFOREDRAGEND.toString(),
          DragListGroup.EventType.DRAGGERREMOVED.toString(),
          DragListGroup.EventType.DRAGEND.toString(),
        ],
        firedEventTypes);
    assertIdle(dlg);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testFiredEventsWithHysteresis() {
    dlg.setHysteresis(2);

    testingEvents.fireClickSequence(list.firstChild);
    assertArrayEquals(
        'only clone events are fired on click if hysteresis is enabled',
        [
          DragListGroup.EventType.DRAGGERCREATED.toString(),
          DragListGroup.EventType.DRAGGERREMOVED.toString(),
        ],
        firedEventTypes);
    firedEventTypes.length = 0;
    assertIdle(dlg);

    testingEvents.fireMouseDownEvent(
        list.firstChild, null, new Coordinate(0, 0));
    testingEvents.fireMouseMoveEvent(list.firstChild, new Coordinate(1, 0));
    assertArrayEquals(
        'only potential-start event is fired on click if hysteresis is enabled',
        [DragListGroup.EventType.DRAGGERCREATED.toString()], firedEventTypes);
    firedEventTypes.length = 0;

    testingEvents.fireMouseMoveEvent(list.firstChild, new Coordinate(3, 0));
    assertArrayEquals(
        'start+move events are fired over hysteresis distance',
        [
          DragListGroup.EventType.BEFOREDRAGSTART.toString(),
          DragListGroup.EventType.DRAGSTART.toString(),
          DragListGroup.EventType.BEFOREDRAGMOVE.toString(),
          DragListGroup.EventType.DRAGMOVE.toString(),
        ],
        firedEventTypes);
    assertTrue(dlg.isDragging());

    firedEventTypes.length = 0;
    testingEvents.fireMouseUpEvent(list.firstChild, null, new Coordinate(3, 0));
    assertArrayEquals(
        'end events are fired on mouseup',
        [
          DragListGroup.EventType.BEFOREDRAGEND.toString(),
          DragListGroup.EventType.DRAGGERREMOVED.toString(),
          DragListGroup.EventType.DRAGEND.toString(),
        ],
        firedEventTypes);
    assertIdle(dlg);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPreventDefaultBeforeDragStart() {
    events.listen(
        dlg, DragListGroup.EventType.BEFOREDRAGSTART, GoogEvent.preventDefault);

    testingEvents.fireMouseDownEvent(list.firstChild);
    assertArrayEquals(
        'event types if dragging is prevented',
        [
          DragListGroup.EventType.DRAGGERCREATED.toString(),
          DragListGroup.EventType.BEFOREDRAGSTART.toString(),
          DragListGroup.EventType.DRAGGERREMOVED.toString(),
        ],
        firedEventTypes);
    assertIdle(dlg);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPreventDefaultBeforeDragStartWithHysteresis() {
    dlg.setHysteresis(5);
    events.listen(
        dlg, DragListGroup.EventType.BEFOREDRAGSTART, GoogEvent.preventDefault);

    testingEvents.fireMouseDownEvent(
        list.firstChild, null, new Coordinate(0, 0));
    testingEvents.fireMouseMoveEvent(list.firstChild, new Coordinate(10, 0));
    assertArrayEquals(
        'event types if dragging is prevented',
        [
          DragListGroup.EventType.DRAGGERCREATED.toString(),
          DragListGroup.EventType.BEFOREDRAGSTART.toString(),
          DragListGroup.EventType.DRAGGERREMOVED.toString(),
        ],
        firedEventTypes);
    assertIdle(dlg);
  },

  /**
   * Tests that the dragged item initial position is untouched if the item would
   * not be under the mouse cursor and the correction behavior is not enabled.
   */
  testPositionCorrectionOnDragItemNotApplied() {
    const cursorPosition = startDrag(dlg, false);
    assertFalse(isCursorOverDragger(dlg, cursorPosition));
  },

  /**
   * Tests that the dragged item initial position is corrected if the item would
   * not be under the mouse cursor.
   */
  testPositionCorrectionOnDragItemApplied() {
    const cursorPosition = startDrag(dlg, true);
    assertTrue(isCursorOverDragger(dlg, cursorPosition));
  },

  /**
   * Tests that a new item can be added to a drag list after the control has
   * been initialized.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testAddItemToDragList() {
    const item =
        dom.createDom(TagName.DIV, null, dom.createTextNode('newItem'));

    dlg.addItemToDragList(list, item);

    assertEquals(item, list.lastChild);
    assertEquals(4, dom.getChildren(list).length);

    events.listen(
        dlg, DragListGroup.EventType.BEFOREDRAGSTART, GoogEvent.preventDefault);

    testingEvents.fireMouseDownEvent(item);
    assertArrayEquals(
        'Should fire beforedragstart event when clicked',
        [
          DragListGroup.EventType.DRAGGERCREATED.toString(),
          DragListGroup.EventType.BEFOREDRAGSTART.toString(),
          DragListGroup.EventType.DRAGGERREMOVED.toString(),
        ],
        firedEventTypes);
  },

  /**
   * Tests that a new item added to a drag list after the control has been
   * initialized is inserted at the correct position.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testInsertItemInDragList() {
    const item =
        dom.createDom(TagName.DIV, null, dom.createTextNode('newItem'));

    dlg.addItemToDragList(list, item, 0);

    assertEquals(item, list.firstChild);
    assertEquals(4, dom.getChildren(list).length);

    events.listen(
        dlg, DragListGroup.EventType.BEFOREDRAGSTART, GoogEvent.preventDefault);

    testingEvents.fireMouseDownEvent(item);
    assertArrayEquals(
        'Should fire beforedragstart event when clicked',
        [
          DragListGroup.EventType.DRAGGERCREATED.toString(),
          DragListGroup.EventType.BEFOREDRAGSTART.toString(),
          DragListGroup.EventType.DRAGGERREMOVED.toString(),
        ],
        firedEventTypes);
  },

  testOnlyDropDragPermission_noItemDragEvents() {
    setUpWithDragListPermission(DragListPermission.ONLY_DROP);

    testingEvents.fireMouseDownEvent(list.firstChild);
    assertArrayEquals(
        'Expect no events to be fired on a list with only drop permission.', [],
        firedEventTypes);
    firedEventTypes.length = 0;

    testingEvents.fireMouseDownEvent(list2.firstChild);
    assertArrayEquals(
        'Expect normal events to be fired on a list with the default permission.',
        [
          DragListGroup.EventType.DRAGGERCREATED.toString(),
          DragListGroup.EventType.BEFOREDRAGSTART.toString(),
          DragListGroup.EventType.DRAGSTART.toString(),
        ],
        firedEventTypes);
    firedEventTypes.length = 0;
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testOnlyDropDragPermission_allowsDropOnList() {
    setUpWithDragListPermission(DragListPermission.ONLY_DROP);

    // When the user starts a drag on the first item of the second list.
    testingEvents.fireMouseDownEvent(list2.firstChild);
    assertEquals(
        'Expect the current drag item to be the first child of the second list.',
        dlg.currDragItem_, list2.firstChild);
    firedEventTypes.length = 0;

    /** @suppress {checkTypes} suppression added to enable type checking */
    const be = new BrowserEvent({
      type: EventType.MOUSEMOVE,
      button: BrowserFeature.HAS_W3C_BUTTON ? 0 : 1,
    });
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    event.event_ = be;

    const posList2 = style.getPosition(list2.children[1]);
    be.clientX = posList2.x + 2;
    be.clientY = posList2.y + 2;

    /** @suppress {visibility} suppression added to enable type checking */
    let dragEvent = new FxDragEvent(
        Dragger.EventType.DRAG, dlg.dragger_, be.clientX, be.clientY, be);
    dlg.handleDragMove_(dragEvent);

    assertArrayEquals(
        'Expect drag events to be fired.',
        [
          DragListGroup.EventType.BEFOREDRAGMOVE.toString(),
          DragListGroup.EventType.DRAGMOVE.toString(),
        ],
        firedEventTypes);
    firedEventTypes.length = 0;

    assertNotEquals(
        'Expect the current drag item to not start display: none.', 'none',
        dlg.currDragItem_.style.display);

    // When the user drags the item over the first list.
    const posList = style.getPosition(list.children[1]);
    be.clientX = posList.x + 2;
    /** @suppress {visibility} suppression added to enable type checking */
    dlg.draggerEl_.style.left = be.clientX + 'px';
    be.clientY = posList.y + 2;
    /** @suppress {visibility} suppression added to enable type checking */
    dlg.draggerEl_.style.top = be.clientY + 'px';

    /** @suppress {visibility} suppression added to enable type checking */
    dragEvent = new FxDragEvent(
        Dragger.EventType.DRAG, dlg.dragger_, be.clientX, be.clientY, be);

    dlg.handleDragMove_(dragEvent);
    assertArrayEquals(
        'Expect drag events to be fired.',
        [
          DragListGroup.EventType.BEFOREDRAGMOVE.toString(),
          DragListGroup.EventType.DRAGMOVE.toString(),
        ],
        firedEventTypes);
    firedEventTypes.length = 0;
    assertTrue(dlg.isDragging());

    assertNotEquals(
        'Expect the current drag item to still not be shown.', 'none',
        dlg.currDragItem_.style.display);

    assertTrue(
        'Expect the first list to have the list hover class.',
        classlist.contains(list, 'test_hover_class'));
  },

  testOnlyDragOutDragPermission_hasItemDragEvents() {
    setUpWithDragListPermission(DragListPermission.ONLY_DRAG_OUT);

    testingEvents.fireMouseDownEvent(list.firstChild);
    assertArrayEquals(
        'Expect normal events to be fired on a list with the only drag out ' +
            'permission.',
        [
          DragListGroup.EventType.DRAGGERCREATED.toString(),
          DragListGroup.EventType.BEFOREDRAGSTART.toString(),
          DragListGroup.EventType.DRAGSTART.toString(),
        ],
        firedEventTypes);
    firedEventTypes.length = 0;
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testOnlyDragOutDragPermission_doesNotAllowDropOnList() {
    setUpWithDragListPermission(DragListPermission.ONLY_DRAG_OUT);

    // When the user starts a drag on the first item of the second list.
    testingEvents.fireMouseDownEvent(list2.firstChild);
    assertEquals(
        'Expect the current drag item to be the first child of the second list.',
        dlg.currDragItem_, list2.firstChild);
    firedEventTypes.length = 0;

    /** @suppress {checkTypes} suppression added to enable type checking */
    const be = new BrowserEvent({
      type: EventType.MOUSEMOVE,
      button: BrowserFeature.HAS_W3C_BUTTON ? 0 : 1,
    });
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    event.event_ = be;

    const posList2 = style.getPosition(list2.children[1]);
    be.clientX = posList2.x + 2;
    be.clientY = posList2.y + 2;

    /** @suppress {visibility} suppression added to enable type checking */
    let dragEvent = new FxDragEvent(
        Dragger.EventType.DRAG, dlg.dragger_, be.clientX, be.clientY, be);
    dlg.handleDragMove_(dragEvent);

    assertArrayEquals(
        'Expect drag events to be fired.',
        [
          DragListGroup.EventType.BEFOREDRAGMOVE.toString(),
          DragListGroup.EventType.DRAGMOVE.toString(),
        ],
        firedEventTypes);
    firedEventTypes.length = 0;

    assertNotEquals(
        'Expect the current drag item to not start display: none.', 'none',
        dlg.currDragItem_.style.display);

    // When the user drags the item over the first list.
    const posList = style.getPosition(list.children[1]);
    be.clientX = posList.x + 2;
    be.clientY = posList.y + 2;
    /** @suppress {visibility} suppression added to enable type checking */
    dlg.draggerEl_.style.left = be.clientX + 'px';
    /** @suppress {visibility} suppression added to enable type checking */
    dlg.draggerEl_.style.top = be.clientY + 'px';

    /** @suppress {visibility} suppression added to enable type checking */
    dragEvent = new FxDragEvent(
        Dragger.EventType.DRAG, dlg.dragger_, be.clientX, be.clientY, be);

    dlg.handleDragMove_(dragEvent);
    assertArrayEquals(
        'Expect drag events to be fired.',
        [
          DragListGroup.EventType.BEFOREDRAGMOVE.toString(),
          DragListGroup.EventType.DRAGMOVE.toString(),
        ],
        firedEventTypes);
    firedEventTypes.length = 0;
    assertTrue(dlg.isDragging());

    assertEquals(
        'Expect the current drag item to now be display: none.', 'none',
        dlg.currDragItem_.style.display);

    assertFalse(
        'Expect the first list to not have the list hover class.',
        classlist.contains(list, 'test_hover_class'));
  },
});
