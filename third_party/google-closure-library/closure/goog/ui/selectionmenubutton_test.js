/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.SelectionMenuButtonTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const SelectionMenuButton = goog.require('goog.ui.SelectionMenuButton');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let selectionMenuButton;
let clonedSelectionMenuButtonDom;

testSuite({
  setUp() {
    clonedSelectionMenuButtonDom =
        dom.getElement('demoSelectionMenuButton').cloneNode(true);

    selectionMenuButton = new SelectionMenuButton();
  },

  tearDown() {
    selectionMenuButton.dispose();

    const element = dom.getElement('demoSelectionMenuButton');
    element.parentNode.replaceChild(clonedSelectionMenuButtonDom, element);
  },

  /** Open the menu and click on the menu item inside. */
  testBasicButtonBehavior() {
    const node = dom.getElement('demoSelectionMenuButton');
    selectionMenuButton.decorate(node);
    testingEvents.fireClickSequence(node);

    assertTrue('Menu must open after click', selectionMenuButton.isOpen());

    let menuItemClicked = 0;
    let lastMenuItemClicked = null;
    events.listen(
        selectionMenuButton.getMenu(), Component.EventType.ACTION, (e) => {
          menuItemClicked++;
          lastMenuItemClicked = e.target;
        });

    const menuItem2 = dom.getElement('menuItem2');
    testingEvents.fireClickSequence(menuItem2);
    assertFalse(
        'Menu must close on clicking when open', selectionMenuButton.isOpen());
    assertEquals(
        'Number of menu items clicked should be 1', 1, menuItemClicked);
    assertEquals(
        'menuItem2 should be the last menuitem clicked', menuItem2,
        lastMenuItemClicked.getElement());
  },

  /**
     Tests that the checkbox fires the same events as the first 2 items.
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCheckboxFireEvents() {
    const node = dom.getElement('demoSelectionMenuButton');
    selectionMenuButton.decorate(node);

    let menuItemClicked = 0;
    let lastMenuItemClicked = null;
    events.listen(
        selectionMenuButton.getMenu(), Component.EventType.ACTION, (e) => {
          menuItemClicked++;
          lastMenuItemClicked = e.target;
        });

    const checkbox = dom.getElement('demoCheckbox');
    assertFalse(
        'Checkbox must be unchecked (i.e. unselected)', checkbox.checked);

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    checkbox.checked = true;
    testingEvents.fireClickSequence(checkbox);
    assertFalse(
        'Menu must be closed when clicking checkbox',
        selectionMenuButton.isOpen());
    assertEquals(
        'Number of menu items clicked should be 1', 1, menuItemClicked);
    assertEquals(
        'menuItem1 should be the last menuitem clicked',
        dom.getElement('menuItem1'), lastMenuItemClicked.getElement());

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    checkbox.checked = false;
    testingEvents.fireClickSequence(checkbox);
    assertFalse(
        'Menu must be closed when clicking checkbox',
        selectionMenuButton.isOpen());
    assertEquals(
        'Number of menu items clicked should be 2', 2, menuItemClicked);
    assertEquals(
        'menuItem2 should be the last menuitem clicked',
        dom.getElement('menuItem2'), lastMenuItemClicked.getElement());
  },

  /**
     Tests that the checkbox state gets updated when the first 2 events fire
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCheckboxReceiveEvents() {
    const node = dom.getElement('demoSelectionMenuButton');
    selectionMenuButton.decorate(node);

    const checkbox = dom.getElement('demoCheckbox');
    assertFalse(
        'Checkbox must be unchecked (i.e. unselected)', checkbox.checked);

    testingEvents.fireClickSequence(dom.getElement('menuItem1'));
    assertTrue('Checkbox must be checked (i.e. selected)', checkbox.checked);

    testingEvents.fireClickSequence(dom.getElement('menuItem2'));
    assertFalse(
        'Checkbox must be unchecked (i.e. unselected)', checkbox.checked);
  },

  /**
     Tests that set/getSelectionState correctly changes the state
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSelectionState() {
    const node = dom.getElement('demoSelectionMenuButton');
    selectionMenuButton.decorate(node);

    const checkbox = dom.getElement('demoCheckbox');
    assertFalse(
        'Checkbox must be unchecked (i.e. unselected)', checkbox.checked);

    selectionMenuButton.setSelectionState(
        SelectionMenuButton.SelectionState.ALL);
    assertTrue(
        'Checkbox should be checked when selecting all', checkbox.checked);
    assertEquals(
        'selectionState should be ALL', selectionMenuButton.getSelectionState(),
        SelectionMenuButton.SelectionState.ALL);

    selectionMenuButton.setSelectionState(
        SelectionMenuButton.SelectionState.NONE);
    assertFalse(
        'Checkbox should be checked when selecting all', checkbox.checked);
    assertEquals(
        'selectionState should be NONE',
        selectionMenuButton.getSelectionState(),
        SelectionMenuButton.SelectionState.NONE);

    selectionMenuButton.setSelectionState(
        SelectionMenuButton.SelectionState.SOME);
    assertTrue(
        'Checkbox should be checked when selecting all', checkbox.checked);
    assertEquals(
        'selectionState should be SOME',
        selectionMenuButton.getSelectionState(),
        SelectionMenuButton.SelectionState.SOME);
  },

  /**
     Tests that the checkbox gets disabled when the button is disabled
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCheckboxDisabled() {
    const node = dom.getElement('demoSelectionMenuButton');
    selectionMenuButton.decorate(node);

    const checkbox = dom.getElement('demoCheckbox');
    assertFalse('Checkbox must be enabled', checkbox.disabled);

    selectionMenuButton.setEnabled(false);
    assertTrue('Checkbox must be disabled', checkbox.disabled);

    selectionMenuButton.setEnabled(true);
    assertFalse('Checkbox must be enabled', checkbox.disabled);
  },

  /** Tests that clicking the checkbox does not open the menu */
  testCheckboxClickMenuClosed() {
    const node = dom.getElement('demoSelectionMenuButton');
    selectionMenuButton.decorate(node);

    const checkbox = dom.getElement('demoCheckbox');
    testingEvents.fireMouseDownEvent(checkbox);
    assertFalse(
        'Menu must be closed when mousedown checkbox',
        selectionMenuButton.isOpen());
    testingEvents.fireMouseUpEvent(checkbox);
    assertFalse(
        'Menu must remain closed when mouseup checkbox',
        selectionMenuButton.isOpen());

    selectionMenuButton.setOpen(true);
    testingEvents.fireClickSequence(checkbox);
    assertFalse(
        'Menu must close when clickin checkbox', selectionMenuButton.isOpen());
  },
});
