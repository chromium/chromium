/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.PopupMenuTest');
goog.setTestOnly();

const Box = goog.require('goog.math.Box');
const BrowserEvent = goog.require('goog.events.BrowserEvent');
const Coordinate = goog.require('goog.math.Coordinate');
const Corner = goog.require('goog.positioning.Corner');
const EventHandler = goog.require('goog.events.EventHandler');
const EventType = goog.require('goog.events.EventType');
const KeyCodes = goog.require('goog.events.KeyCodes');
const Menu = goog.require('goog.ui.Menu');
const MenuItem = goog.require('goog.ui.MenuItem');
const PopupMenu = goog.require('goog.ui.PopupMenu');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let anchor;
let menu;
let menuitem;
let menuitem1;
let menuitem3;

// Event handler
let handler;
let showPopup;
let beforeShowPopupCalled;
let popup;

/**
 * Asserts properties of `target` matches the expected value.
 * @param {Object} target The target specifying how the popup menu should be
 *     attached to an anchor.
 * @param {Element} expectedElement The expected anchoring element.
 * @param {Corner} expectedTargetCorner The expected value of the
 *     `target.targetCorner_` property.
 * @param {Corner} expectedMenuCorner The expected value of the
 *     `target.menuCorner_` property.
 * @param {EventType} expectedEventType The expected value of the
 *     `target.eventType_` property.
 * @param {Box} expectedMargin The expected value of the `target.margin_`
 *     property.
 */
function assertTarget(
    target, expectedElement, expectedTargetCorner, expectedMenuCorner,
    expectedEventType, expectedMargin) {
  const expectedTarget = {
    element_: expectedElement,
    targetCorner_: expectedTargetCorner,
    menuCorner_: expectedMenuCorner,
    eventType_: expectedEventType,
    margin_: expectedMargin,
  };

  assertObjectEquals('Target does not match.', expectedTarget, target);
}

testSuite({
  setUp() {
    anchor = dom.getElement('popup-anchor');
    menu = dom.getElement('menu');
    menuitem1 = dom.getElement('menuitem_1');
    menuitem3 = dom.getElement('menuitem_3');
    handler = new EventHandler();
    popup = new PopupMenu();
  },

  tearDown() {
    handler.dispose();
    popup.dispose();
  },

  /**
     Test menu receives BEFORE_SHOW event before it's displayed.
     @suppress {visibility} suppression added to enable type checking
   */
  testBeforeShowEvent() {
    popup.render();
    /** @suppress {visibility} suppression added to enable type checking */
    const target = popup.createAttachTarget(anchor);
    popup.attach(anchor);

    function beforeShowPopup(e) {
      // Ensure that the element is not yet visible.
      assertFalse(
          'The element should not be shown when BEFORE_SHOW event is ' +
              'being handled',
          style.isElementShown(popup.getElement()));
      // Verify that current anchor is set before dispatching BEFORE_SHOW.
      assertNotNullNorUndefined(popup.getAttachedElement());
      assertEquals(
          'The attached anchor element is incorrect', target.element_,
          popup.getAttachedElement());
      beforeShowPopupCalled = true;
      return showPopup;
    }
    function onShowPopup(e) {
      assertEquals(
          'The attached anchor element is incorrect', target.element_,
          popup.getAttachedElement());
    }

    handler.listen(popup, Menu.EventType.BEFORE_SHOW, beforeShowPopup);
    handler.listen(popup, Menu.EventType.SHOW, onShowPopup);

    beforeShowPopupCalled = false;
    showPopup = false;
    popup.showMenu(target, 0, 0);
    assertTrue(
        'BEFORE_SHOW event handler should be called on #showMenu',
        beforeShowPopupCalled);
    assertFalse(
        'The element should not be shown when BEFORE_SHOW handler ' +
            'returned false',
        style.isElementShown(popup.getElement()));

    beforeShowPopupCalled = false;
    showPopup = true;
    popup.showMenu(target, 0, 0);
    assertTrue(
        'The element should be shown when BEFORE_SHOW handler ' +
            'returned true',
        style.isElementShown(popup.getElement()));
  },

  /**
     Test the behavior of {@link PopupMenu.isAttachTarget}.
     @suppress {visibility} suppression added to enable type checking
   */
  testIsAttachTarget() {
    popup.render();
    // Before 'attach' is called.
    assertFalse(
        'Menu should not be attached to the element',
        popup.isAttachTarget(anchor));

    popup.attach(anchor);
    assertTrue(
        'Menu should be attached to the anchor', popup.isAttachTarget(anchor));

    popup.detach(anchor);
    assertFalse(
        'Menu is expected to be detached from the element',
        popup.isAttachTarget(anchor));
  },

  /**
     Tests the behavior of {@link PopupMenu.createAttachTarget}.
     @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testCreateAttachTarget() {
    // Randomly picking parameters.
    const targetCorner = Corner.TOP_END;
    const menuCorner = Corner.BOTTOM_LEFT;
    const contextMenu = false;  // Show menu on mouse down event.
    const margin = new Box(0, 10, 5, 25);

    // Simply setting the required parameters.
    /** @suppress {visibility} suppression added to enable type checking */
    let target = popup.createAttachTarget(anchor);
    assertTrue(popup.isAttachTarget(anchor));
    assertTarget(
        target, anchor, undefined, undefined, EventType.MOUSEDOWN, undefined);

    // Creating another target with all the parameters.
    /** @suppress {visibility} suppression added to enable type checking */
    target = popup.createAttachTarget(
        anchor, targetCorner, menuCorner, contextMenu, margin);
    assertTrue(popup.isAttachTarget(anchor));
    assertTarget(
        target, anchor, targetCorner, menuCorner, EventType.MOUSEDOWN, margin);

    // Finally, switch up the 'contextMenu'
    /** @suppress {visibility} suppression added to enable type checking */
    target = popup.createAttachTarget(
        anchor, undefined, undefined, true /*opt_contextMenu*/, undefined);
    assertTarget(
        target, anchor, undefined, undefined, EventType.CONTEXTMENU, undefined);
  },

  /** Tests the behavior of {@link PopupMenu.getAttachTarget}. */
  testGetAttachTarget() {
    popup.render();
    // Before the menu is attached to the anchor.
    /** @suppress {visibility} suppression added to enable type checking */
    let target = popup.getAttachTarget(anchor);
    assertTrue(
        'Not expecting a target before the element is attach to the menu',
        target == null);

    // Randomly picking parameters.
    const targetCorner = Corner.TOP_END;
    const menuCorner = Corner.BOTTOM_LEFT;
    const contextMenu = false;  // Show menu on mouse down event.
    const margin = new Box(0, 10, 5, 25);

    popup.attach(anchor, targetCorner, menuCorner, contextMenu, margin);
    /** @suppress {visibility} suppression added to enable type checking */
    target = popup.getAttachTarget(anchor);
    assertTrue(
        'Failed to get target after attaching element to menu', target != null);

    // Make sure we got the right target back.
    assertTarget(
        target, anchor, targetCorner, menuCorner, EventType.MOUSEDOWN, margin);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSmallViewportSliding() {
    popup.render();
    popup.getElement().style.position = 'absolute';
    popup.getElement().style.outline = '1px solid blue';
    const item = new MenuItem('Test Item');
    popup.addChild(item, true);
    item.getElement().style.overflow = 'hidden';

    const viewport = style.getClientViewportElement();
    const viewportRect = style.getVisibleRectForElement(viewport);

    const middlePos = Math.floor((viewportRect.right - viewportRect.left) / 2);
    const leftwardPos =
        Math.floor((viewportRect.right - viewportRect.left) / 3);
    const rightwardPos =
        Math.floor((viewportRect.right - viewportRect.left) / 3 * 2);

    // Can interpret these positions as widths relative to the viewport as well.
    const smallWidth = leftwardPos;
    const mediumWidth = middlePos;
    const largeWidth = rightwardPos;

    // Test small menu first.  This should be small enough that it will display
    // its upper left corner where we tell it to in all three positions.
    popup.getElement().style.width = `${smallWidth}px`;

    /** @suppress {visibility} suppression added to enable type checking */
    let target = popup.createAttachTarget(anchor);
    popup.attach(anchor);

    popup.showMenu(target, leftwardPos, 0);
    assertObjectEquals(
        'Popup in wrong position: small size, leftward pos',
        new Coordinate(leftwardPos, 0), style.getPosition(popup.getElement()));

    popup.showMenu(target, middlePos, 0);
    assertObjectEquals(
        'Popup in wrong position: small size, middle pos',
        new Coordinate(middlePos, 0), style.getPosition(popup.getElement()));

    popup.showMenu(target, rightwardPos, 0);
    assertObjectEquals(
        'Popup in wrong position: small size, rightward pos',
        new Coordinate(rightwardPos, 0), style.getPosition(popup.getElement()));

    // Test medium menu next.  This should display with its upper left corner
    // at the target when leftward and middle, but on the right it should
    // position its upper right corner at the target instead.
    popup.getElement().style.width = `${mediumWidth}px`;

    popup.showMenu(target, leftwardPos, 0);
    assertObjectEquals(
        'Popup in wrong position: medium size, leftward pos',
        new Coordinate(leftwardPos, 0), style.getPosition(popup.getElement()));

    popup.showMenu(target, middlePos, 0);
    assertObjectEquals(
        'Popup in wrong position: medium size, middle pos',
        new Coordinate(middlePos, 0), style.getPosition(popup.getElement()));

    popup.showMenu(target, rightwardPos, 0);
    assertObjectEquals(
        'Popup in wrong position: medium size, rightward pos',
        new Coordinate(rightwardPos - mediumWidth, 0),
        style.getPosition(popup.getElement()));

    // Test large menu next.  This should display with its upper left corner at
    // the target when leftward, and its upper right corner at the target when
    // rightward, but right in the middle neither corner can be at the target
    // and keep the entire menu onscreen, so it should place its upper right
    // corner at the very right edge of the viewport.
    popup.getElement().style.width = `${largeWidth}px`;
    popup.showMenu(target, leftwardPos, 0);
    assertObjectEquals(
        'Popup in wrong position: large size, leftward pos',
        new Coordinate(leftwardPos, 0), style.getPosition(popup.getElement()));

    popup.showMenu(target, middlePos, 0);
    assertObjectEquals(
        'Popup in wrong position: large size, middle pos',
        new Coordinate(viewportRect.right - viewportRect.left - largeWidth, 0),
        style.getPosition(popup.getElement()));

    popup.showMenu(target, rightwardPos, 0);
    assertObjectEquals(
        'Popup in wrong position: large size, rightward pos',
        new Coordinate(rightwardPos - largeWidth, 0),
        style.getPosition(popup.getElement()));

    // Make sure that the menu still displays correctly if we give the target
    // a target corner.  We can't set the overflow policy in that case, but it
    // should still display.
    popup.detach(anchor);
    anchor.style.position = 'absolute';
    anchor.style.left = '24px';
    anchor.style.top = '24px';
    const targetCorner = Corner.TOP_END;
    /** @suppress {visibility} suppression added to enable type checking */
    target = popup.createAttachTarget(anchor, targetCorner);
    popup.attach(anchor, targetCorner);
    popup.getElement().style.width = `${smallWidth}px`;
    popup.showMenu(target, leftwardPos, 0);
    assertObjectEquals(
        'Popup in wrong position: small size, leftward pos, with target corner',
        new Coordinate(24, 24), style.getPosition(popup.getElement()));
  },

  /**
   * Tests that the menu is shown if the SPACE or ENTER keys are pressed, and
   * that none of the menu items are highlighted (PopupMenu.highlightedIndex ==
   * -1).
   */
  testKeyboardEventsShowMenu() {
    popup.decorate(menu);
    popup.attach(anchor);
    popup.hide();
    assertFalse(popup.isVisible());
    events.fireKeySequence(anchor, KeyCodes.SPACE);
    assertTrue(popup.isVisible());
    assertEquals(-1, popup.getHighlightedIndex());
    popup.hide();
    assertFalse(popup.isVisible());
    events.fireKeySequence(anchor, KeyCodes.ENTER);
    assertTrue(popup.isVisible());
    assertEquals(-1, popup.getHighlightedIndex());
  },

  /**
   * Tests that the menu is shown and the first item is highlighted if the DOWN
   * key is pressed.
   */
  testDownKey() {
    popup.decorate(menu);
    popup.attach(anchor);
    popup.hide();
    assertFalse(popup.isVisible());
    events.fireKeySequence(anchor, KeyCodes.DOWN);
    assertTrue(popup.isVisible());
    assertEquals(0, popup.getHighlightedIndex());
  },

  /** Tests activation of menu items by keyboard. */
  testMenuItemKeyboardActivation() {
    popup.decorate(menu);
    popup.attach(anchor);
    // Check that if the ESC key is pressed the focus is on
    // the anchor element.
    events.fireKeySequence(menu, KeyCodes.ESC);
    assertEquals(anchor, document.activeElement);

    let menuitemListenerFired = false;
    function onMenuitemAction(event) {
      if (event.keyCode == KeyCodes.SPACE || event.keyCode == KeyCodes.ENTER) {
        menuitemListenerFired = true;
      }
    }
    handler.listen(menuitem1, EventType.KEYDOWN, onMenuitemAction);
    // Simulate opening a menu using the DOWN key, and pressing the SPACE/ENTER
    // key in order to activate the first menuitem.
    events.fireKeySequence(anchor, KeyCodes.DOWN);
    events.fireKeySequence(menu, KeyCodes.SPACE);
    assertTrue(menuitemListenerFired);
    menuitemListenerFired = false;
    events.fireKeySequence(anchor, KeyCodes.DOWN);
    events.fireKeySequence(menu, KeyCodes.ENTER);
    assertTrue(menuitemListenerFired);
    // Make sure the menu item's listener doesn't fire for any key.
    menuitemListenerFired = false;
    events.fireKeySequence(anchor, KeyCodes.DOWN);
    events.fireKeySequence(menu, KeyCodes.SHIFT);
    assertFalse(menuitemListenerFired);

    // Simulate opening menu and moving down to the third menu item using the
    // DOWN key, and then activating it using the SPACE key.
    menuitemListenerFired = false;
    handler.listen(menuitem3, EventType.KEYDOWN, onMenuitemAction);
    events.fireKeySequence(anchor, KeyCodes.DOWN);
    events.fireKeySequence(anchor, KeyCodes.DOWN);
    events.fireKeySequence(anchor, KeyCodes.DOWN);
    events.fireKeySequence(menu, KeyCodes.SPACE);
    assertTrue(menuitemListenerFired);
  },

  /**
     Tests that a context menu isn't shown if the SPACE or ENTER keys are
     pressed.
   */
  testContextMenuKeyboard() {
    popup.attach(anchor, null, null, true);
    popup.hide();
    assertFalse(popup.isVisible());
    events.fireKeySequence(anchor, KeyCodes.SPACE);
    assertFalse(popup.isVisible());
    events.fireKeySequence(anchor, KeyCodes.ENTER);
    assertFalse(popup.isVisible());
  },

  /**
   * Tests that there is no crash when hitting a key when no menu item is
   * highlighted.
   */
  testKeyPressWithNoHighlightedItem() {
    popup.decorate(menu);
    popup.attach(anchor);
    events.fireKeySequence(anchor, KeyCodes.SPACE);
    assertTrue(popup.isVisible());
    try {
      events.fireKeySequence(menu, KeyCodes.SPACE);
    } catch (e) {
      fail(
          'Crash attempting to reference null selected menu item after ' +
          'keyboard event.');
    }
  },

  /**
   * Tests that the menu is not shown (i.e. the browser context menu overrides
   * the menu) if the SHIFT key is pressed when the menu is right-clicked and
   * the popup has shiftOverride set.
   */
  testShiftOverride() {
    popup.decorate(menu);
    popup.attach(
        anchor,
        /* opt_targetCorner */ undefined,
        /* opt_menuCorner */ undefined,
        /* opt_contextMenu */ false);

    popup.setShiftOverride(true);
    events.fireMouseDownEvent(
        anchor, BrowserEvent.MouseButton.RIGHT,
        /* opt_coords */ null,
        /* opt_eventProperties */ {shiftKey: true});
    assertFalse(popup.isVisible());

    popup.setShiftOverride(false);
    events.fireMouseDownEvent(
        anchor, BrowserEvent.MouseButton.RIGHT,
        /* opt_coords */ null,
        /* opt_eventProperties */ {shiftKey: true});
    assertTrue(popup.isVisible());
  },
});
