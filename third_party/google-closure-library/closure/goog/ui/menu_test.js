/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.MenuTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const Coordinate = goog.require('goog.math.Coordinate');
const Menu = goog.require('goog.ui.Menu');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let menu;
let clonedMenuDom;

testSuite({
  setUp() {
    clonedMenuDom = dom.getElement('demoMenu').cloneNode(true);

    menu = new Menu();
  },

  tearDown() {
    menu.dispose();

    const element = dom.getElement('demoMenu');
    element.parentNode.replaceChild(clonedMenuDom, element);
  },

  /** @bug 1463524 */
  testMouseupDoesntActivateMenuItemImmediately() {
    menu.decorate(dom.getElement('demoMenu'));

    const fakeEvent = {clientX: 42, clientY: 42};
    const itemElem = dom.getElement('menuItem2');
    const coords = new Coordinate(42, 42);

    const menuItem = menu.getChildAt(1);
    let actionDispatched = false;
    events.listen(menuItem, Component.EventType.ACTION, (e) => {
      actionDispatched = true;
    });

    menu.setVisible(true, false, fakeEvent);
    // Makes the menu item active so it can be selected on mouseup.
    menuItem.setActive(true);

    testingEvents.fireMouseUpEvent(itemElem, undefined, coords);
    assertFalse(
        'ACTION should not be dispatched after the initial mouseup',
        actionDispatched);

    testingEvents.fireMouseUpEvent(itemElem, undefined, coords);
    assertTrue(
        'ACTION should be dispatched after the second mouseup',
        actionDispatched);
  },

  testHoverBehavior() {
    menu.decorate(dom.getElement('demoMenu'));

    testingEvents.fireMouseOverEvent(
        dom.getElement('menuItem2'), document.body);
    assertEquals(1, menu.getHighlightedIndex());

    menu.exitDocument();
    assertEquals(-1, menu.getHighlightedIndex());
  },

  testIndirectionDecoration() {
    menu.decorate(dom.getElement('complexMenu'));

    testingEvents.fireMouseOverEvent(
        dom.getElement('complexItem3'), document.body);
    assertEquals(2, menu.getHighlightedIndex());

    menu.exitDocument();
    assertEquals(-1, menu.getHighlightedIndex());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSetHighlightedIndex() {
    menu.decorate(dom.getElement('scrollableMenu'));
    assertEquals(0, menu.getElement().scrollTop);

    // Scroll down
    let element = dom.getElement('scrollableMenuItem4');
    menu.setHighlightedIndex(3);
    assertTrue(element.offsetTop >= menu.getElement().scrollTop);
    assertTrue(
        element.offsetTop <=
        menu.getElement().scrollTop + menu.getElement().offsetHeight);

    // Scroll up
    element = dom.getElement('scrollableMenuItem3');
    menu.setHighlightedIndex(2);
    assertTrue(element.offsetTop >= menu.getElement().scrollTop);
    assertTrue(
        element.offsetTop <=
        menu.getElement().scrollTop + menu.getElement().offsetHeight);

    menu.exitDocument();
    assertEquals(-1, menu.getHighlightedIndex());
  },
});
