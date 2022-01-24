/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.SelectTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const CustomButtonRenderer = goog.require('goog.ui.CustomButtonRenderer');
const Menu = goog.require('goog.ui.Menu');
const MenuItem = goog.require('goog.ui.MenuItem');
const Role = goog.require('goog.a11y.aria.Role');
const Select = goog.require('goog.ui.Select');
const Separator = goog.require('goog.ui.Separator');
const State = goog.require('goog.a11y.aria.State');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

const defaultCaption = 'initial caption';
let sandboxEl;
let select;

// Confirms that aria roles for select conform to spec:
// http://www.w3.org/TR/wai-aria/roles#listbox
// Basically the select should have a role of LISTBOX and all the items should
// have a role of OPTION.

testSuite({
  setUp() {
    sandboxEl = dom.getElement('sandbox');
    select = new Select(defaultCaption);
  },

  tearDown() {
    select.dispose();
    dom.removeChildren(sandboxEl);
  },

  /**
   * Checks that the default caption passed in the constructor and in the setter
   * is returned by getDefaultCaption, and acts as a default caption, i.e. is
   * shown as a caption when no items are selected.
   */
  testDefaultCaption() {
    select.render(sandboxEl);
    const item1 = new MenuItem('item 1');
    select.addItem(item1);
    select.addItem(new MenuItem('item 2'));
    assertEquals(defaultCaption, select.getDefaultCaption());
    assertEquals(defaultCaption, select.getCaption());

    const newCaption = 'new caption';
    select.setDefaultCaption(newCaption);
    assertEquals(newCaption, select.getDefaultCaption());
    assertEquals(newCaption, select.getCaption());

    select.setSelectedItem(item1);
    assertNotEquals(newCaption, select.getCaption());

    select.setSelectedItem(null);
    assertEquals(newCaption, select.getCaption());
  },

  testNoDefaultCaption() {
    assertNull(new Select().getDefaultCaption());
    assertEquals('', new Select('').getDefaultCaption());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAriaRoles() {
    select.render(sandboxEl);
    const item1 = new MenuItem('item 1');
    select.addItem(item1);
    // Added a separator to make sure that the SETSIZE ignores the separator
    // items.
    const separator = new Separator();
    select.addItem(separator);
    const item2 = new MenuItem('item 2');
    select.addItem(item2);
    assertNotNull(select.getElement());
    assertNotNull(item1.getElement());
    assertNotNull(item2.getElement());
    assertEquals(Role.LISTBOX, aria.getRole(select.getElement()));
    assertEquals(Role.OPTION, aria.getRole(item1.getElement()));
    assertEquals(Role.OPTION, aria.getRole(item2.getElement()));
    assertNotNull(aria.getState(select.getElement(), State.ACTIVEDESCENDANT));
    const contentElement =
        select.getRenderer().getContentElement(select.getElement());
    assertEquals('2', aria.getState(contentElement, State.SETSIZE));
    assertEquals('0', aria.getState(contentElement, State.POSINSET));
    select.setSelectedItem(item1);
    assertEquals('1', aria.getState(contentElement, State.POSINSET));
    select.setSelectedItem(item2);
    assertEquals('2', aria.getState(contentElement, State.POSINSET));
  },

  /** Checks that the select control handles ACTION events from its items. */
  testHandlesItemActions() {
    select.render(sandboxEl);
    const item1 = new MenuItem('item 1');
    const item2 = new MenuItem('item 2');
    select.addItem(item1);
    select.addItem(item2);

    item1.dispatchEvent(Component.EventType.ACTION);
    assertEquals(item1, select.getSelectedItem());
    assertEquals(item1.getCaption(), select.getCaption());

    item2.dispatchEvent(Component.EventType.ACTION);
    assertEquals(item2, select.getSelectedItem());
    assertEquals(item2.getCaption(), select.getCaption());
  },

  /** Tests Select.prototype.setValue. */
  testSetValue() {
    select.render(sandboxEl);
    const item1 = new MenuItem('item 1', 1);
    const item2 = new MenuItem('item 2', 2);
    select.addItem(item1);
    select.addItem(item2);

    select.setValue(1);
    assertEquals(item1, select.getSelectedItem());

    select.setValue(2);
    assertEquals(item2, select.getSelectedItem());

    select.setValue(3);
    assertNull(select.getSelectedItem());
  },

  /**
   * Checks that the current selection is cleared when the selected item is
   * removed.
   */
  testSelectionIsClearedWhenSelectedItemIsRemoved() {
    select.render(sandboxEl);
    const item1 = new MenuItem('item 1');
    select.addItem(item1);
    select.addItem(new MenuItem('item 2'));

    select.setSelectedItem(item1);
    select.removeItem(item1);
    assertNull(select.getSelectedItem());
  },

  /**
   * Check that the select control is subscribed to its selection model events
   * after being added, removed and added back again into the document.
   */
  testExitAndEnterDocument() {
    const component = new Component();
    component.render(sandboxEl);

    const item1 = new MenuItem('item 1');
    const item2 = new MenuItem('item 2');
    const item3 = new MenuItem('item 3');

    select.addItem(item1);
    select.addItem(item2);
    select.addItem(item3);

    component.addChild(select, true);
    item2.dispatchEvent(Component.EventType.ACTION);
    assertEquals(item2.getCaption(), select.getCaption());

    component.removeChild(select, true);
    item1.dispatchEvent(Component.EventType.ACTION);
    assertEquals(item2.getCaption(), select.getCaption());

    component.addChild(select, true);
    item3.dispatchEvent(Component.EventType.ACTION);
    assertEquals(item3.getCaption(), select.getCaption());
  },

  testSelectEventFiresForProgrammaticChange() {
    select.render();
    const item1 = new MenuItem('item 1');
    const item2 = new MenuItem('item 2');
    select.addItem(item1);
    select.addItem(item2);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const recordingHandler = new recordFunction();
    events.listen(select, Component.EventType.CHANGE, recordingHandler);

    select.setSelectedItem(item2);
    assertEquals(
        'Selecting new item should fire CHANGE event.', 1,
        recordingHandler.getCallCount());

    select.setSelectedItem(item2);
    assertEquals(
        'Selecting the same item should not fire CHANGE event.', 1,
        recordingHandler.getCallCount());

    select.setSelectedIndex(0);
    assertEquals(
        'Selecting new item should fire CHANGE event.', 2,
        recordingHandler.getCallCount());

    select.setSelectedIndex(0);
    assertEquals(
        'Selecting the same item should not fire CHANGE event.', 2,
        recordingHandler.getCallCount());
  },

  testSelectEventFiresForUserInitiatedAction() {
    select.render();
    const item1 = new MenuItem('item 1');
    const item2 = new MenuItem('item 2');
    select.addItem(item1);
    select.addItem(item2);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const recordingHandler = new recordFunction();
    events.listen(select, Component.EventType.CHANGE, recordingHandler);

    select.setOpen(true);

    item2.dispatchEvent(Component.EventType.ACTION);
    assertEquals(
        'Selecting new item should fire CHANGE event.', 1,
        recordingHandler.getCallCount());
    assertFalse(select.isOpen());

    select.setOpen(true);

    item2.dispatchEvent(Component.EventType.ACTION);
    assertEquals(
        'Selecting the same item should not fire CHANGE event.', 1,
        recordingHandler.getCallCount());
    assertFalse(select.isOpen());
  },

  /**
   * Checks that if an item is selected before decorate is called, the selection
   * is preserved after decorate.
   */
  testSetSelectedItemBeforeRender() {
    select.addItem(new MenuItem('item 1'));
    select.addItem(new MenuItem('item 2'));
    const item3 = new MenuItem('item 3');
    select.addItem(item3);
    select.setSelectedItem(item3);
    assertEquals(2, select.getSelectedIndex());

    select.decorate(sandboxEl);
    assertEquals(2, select.getSelectedIndex());
  },

  /**
   * Checks that if a value is set before decorate is called, the value is
   * preserved after decorate.
   */
  testSetValueBeforeRender() {
    select.addItem(new MenuItem('item 1', 1));
    select.addItem(new MenuItem('item 2', 2));
    select.setValue(2);
    assertEquals(2, select.getValue());

    select.decorate(sandboxEl);
    assertEquals(2, select.getValue());
  },

  testUpdateCaption_aria() {
    select.render(sandboxEl);

    // Verify default state.
    assertEquals(defaultCaption, select.getCaption());
    assertFalse(!!aria.getLabel(
        select.getRenderer().getContentElement(select.getElement())));

    // Add and select an item with aria-label.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const item1 = new MenuItem();
    select.addItem(item1);
    item1.getElement().setAttribute('aria-label', 'item1');
    select.setSelectedIndex(0);
    assertEquals(
        'item1',
        aria.getLabel(
            select.getRenderer().getContentElement(select.getElement())));

    // Add and select an item without a label.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const item2 = new MenuItem();
    select.addItem(item2);
    select.setSelectedIndex(1);
    assertFalse(!!aria.getLabel(
        select.getRenderer().getContentElement(select.getElement())));
  },

  testDisposeWhenInnerHTMLHasBeenClearedInIE10() {
    assertNotThrows(() => {
      const customSelect =
          new Select(null /* label */, new Menu(), new CustomButtonRenderer());
      customSelect.render(sandboxEl);

      // In IE10 setting the innerHTML of a node invalidates the parent child
      // relation of all its child nodes (unlike removeNode).
      dom.removeChildren(sandboxEl);

      // goog.ui.Select's disposeInternal trigger's goog.ui.Component's
      // disposeInternal, which triggers goog.ui.MenuButton's exitDocument,
      // which closes the associated menu and updates the activeDescendant.
      // In the case of a CustomMenuButton the contentElement is referenced by
      // element.firstChild.firstChild, an invalid relation in IE 10.
      customSelect.dispose();
    });
  },
});
