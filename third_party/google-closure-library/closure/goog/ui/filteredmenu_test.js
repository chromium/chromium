/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.FilteredMenuTest');
goog.setTestOnly();

const AutoCompleteValues = goog.require('goog.a11y.aria.AutoCompleteValues');
const EventType = goog.require('goog.events.EventType');
const FilteredMenu = goog.require('goog.ui.FilteredMenu');
const GoogRect = goog.require('goog.math.Rect');
const KeyCodes = goog.require('goog.events.KeyCodes');
const MenuItem = goog.require('goog.ui.MenuItem');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
const transform = goog.require('goog.style.transform');


let sandbox;

function isHighlightedVisible(menu) {
  let contRect = style.getBounds(menu.getContentElement());
  // Expands the containing rectangle by 1px on top and bottom. The test
  // sometime fails with 1px out of bound on FF6/Linux. This is not
  // consistently reproducible.
  contRect = new GoogRect(
      contRect.left, contRect.top - 1, contRect.width, contRect.height + 2);
  const itemRect = style.getBounds(menu.getHighlighted().getElement());
  return contRect.contains(itemRect);
}


testSuite({
  setUp() {
    sandbox = dom.getElement('sandbox');
  },

  tearDown() {
    dom.removeChildren(sandbox);
  },

  testRender() {
    let menu = new FilteredMenu();
    menu.addItem(new MenuItem('Menu Item 1'));
    menu.addItem(new MenuItem('Menu Item 2'));
    menu.render(sandbox);

    assertEquals('Menu should contain two items.', 2, menu.getChildCount());
    assertEquals(
        'Caption of first menu item should match supplied value.',
        'Menu Item 1', menu.getItemAt(0).getCaption());
    assertEquals(
        'Caption of second menu item should match supplied value.',
        'Menu Item 2', menu.getItemAt(1).getCaption());
    assertTrue(
        'Caption of first item should be in document.',
        sandbox.innerHTML.indexOf('Menu Item 1') != -1);
    assertTrue(
        'Caption of second item should be in document.',
        sandbox.innerHTML.indexOf('Menu Item 2') != -1);

    menu.dispose();
  },

  testDecorate() {
    let menu = new FilteredMenu();
    menu.decorate(dom.getElement('testmenu'));

    assertEquals('Menu should contain four items.', 4, menu.getChildCount());
    assertEquals(
        'Caption of menu item should match decorated element', 'Apple',
        menu.getItemAt(0).getCaption());
    assertEquals(
        'Accelerator of menu item should match accelerator element', 'A',
        menu.getItemAt(0).getAccelerator());
    assertEquals(
        'Caption of menu item should match decorated element', 'Lemon',
        menu.getItemAt(1).getCaption());
    assertEquals(
        'Caption of menu item should match decorated element', 'Orange',
        menu.getItemAt(2).getCaption());
    assertEquals(
        'Caption of menu item should match decorated element', 'Strawberry',
        menu.getItemAt(3).getCaption());

    menu.dispose();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testFilter() {
    let menu = new FilteredMenu();
    menu.addItem(new MenuItem('Family'));
    menu.addItem(new MenuItem('Friends'));
    menu.addItem(new MenuItem('Photos'));
    menu.addItem(new MenuItem([
      dom.createTextNode('Work'),
      dom.createDom(TagName.SPAN, MenuItem.ACCELERATOR_CLASS, 'W'),
    ]));

    menu.render(sandbox);

    // Check menu items.
    assertEquals(
        'Family should be the first label in the move to menu', 'Family',
        menu.getChildAt(0).getCaption());
    assertEquals(
        'Friends should be the second label in the move to menu', 'Friends',
        menu.getChildAt(1).getCaption());
    assertEquals(
        'Photos should be the third label in the move to menu', 'Photos',
        menu.getChildAt(2).getCaption());
    assertEquals(
        'Work should be the fourth label in the move to menu', 'Work',
        menu.getChildAt(3).getCaption());

    // Filter menu.
    menu.setFilter('W');
    assertFalse(
        'Family should not be visible when the menu is filtered',
        menu.getChildAt(0).isVisible());
    assertFalse(
        'Friends should not be visible when the menu is filtered',
        menu.getChildAt(1).isVisible());
    assertFalse(
        'Photos should not be visible when the menu is filtered',
        menu.getChildAt(2).isVisible());
    assertTrue(
        'Work should be visible when the menu is filtered',
        menu.getChildAt(3).isVisible());
    // Check accelerator.
    assertEquals(
        'The accelerator for Work should be present', 'W',
        menu.getChildAt(3).getAccelerator());

    menu.setFilter('W,');
    for (let i = 0; i < menu.getChildCount(); i++) {
      assertFalse(
          'W, should not match anything with allowMultiple set to false',
          menu.getChildAt(i).isVisible());
    }

    // Clear filter.
    menu.setFilter('');
    for (let i = 0; i < menu.getChildCount(); i++) {
      assertTrue('All items should be visible', menu.getChildAt(i).isVisible());
    }

    menu.dispose();
  },

  testFilterAllowMultiple() {
    let menu = new FilteredMenu();
    menu.setAllowMultiple(true);
    menu.addItem(new MenuItem('Family'));
    menu.addItem(new MenuItem('Friends'));
    menu.addItem(new MenuItem('Photos'));
    menu.addItem(new MenuItem('Work'));

    menu.render(sandbox);

    // Filter menu.
    menu.setFilter('W,');
    for (let i = 0; i < menu.getChildCount(); i++) {
      assertTrue(
          'W, should show all items with allowMultiple set to true',
          menu.getChildAt(i).isVisible());
    }

    // Filter second label.
    menu.setFilter('Work,P');
    assertFalse(
        'Family should not be visible when the menu is filtered',
        menu.getChildAt(0).isVisible());
    assertFalse(
        'Friends should not be visible when the menu is filtered',
        menu.getChildAt(1).isVisible());
    assertTrue(
        'Photos should be visible when the menu is filtered',
        menu.getChildAt(2).isVisible());
    assertFalse(
        'Work should not be visible when the menu is filtered',
        menu.getChildAt(3).isVisible());

    // Clear filter.
    menu.setFilter('');
    for (let i = 0; i < menu.getChildCount(); i++) {
      assertTrue('All items should be visible', menu.getChildAt(i).isVisible());
    }

    menu.dispose();
  },

  testFilterWordBoundary() {
    let menu = new FilteredMenu();
    menu.addItem(new MenuItem('Vacation Photos'));
    menu.addItem(new MenuItem('Work'));
    menu.addItem(new MenuItem('Receipts & Invoices'));
    menu.addItem(new MenuItem('Invitations'));
    menu.addItem(new MenuItem('3.Family'));
    menu.addItem(new MenuItem('No:Farm'));
    menu.addItem(new MenuItem('Syd/Family'));

    menu.render(sandbox);

    // Filter menu.
    menu.setFilter('Photos');
    assertTrue(
        'Vacation Photos should be visible when the menu is filtered',
        menu.getChildAt(0).isVisible());
    assertFalse(
        'Work should not be visible when the menu is filtered',
        menu.getChildAt(1).isVisible());
    assertFalse(
        'Receipts & Invoices should not be visible when the menu is ' +
            'filtered',
        menu.getChildAt(2).isVisible());
    assertFalse(
        'Invitations should not be visible when the menu is filtered',
        menu.getChildAt(3).isVisible());

    menu.setFilter('I');
    assertFalse(
        'Vacation Photos should not be visible when the menu is filtered',
        menu.getChildAt(0).isVisible());
    assertFalse(
        'Work should not be visible when the menu is filtered',
        menu.getChildAt(1).isVisible());
    assertTrue(
        'Receipts & Invoices should be visible when the menu is filtered',
        menu.getChildAt(2).isVisible());
    assertTrue(
        'Invitations should be visible when the menu is filtered',
        menu.getChildAt(3).isVisible());

    menu.setFilter('Fa');
    assertTrue(
        '3.Family should be visible when the menu is filtered',
        menu.getChildAt(4).isVisible());
    assertTrue(
        'No:Farm should be visible when the menu is filtered',
        menu.getChildAt(5).isVisible());
    assertTrue(
        'Syd/Family should be visible when the menu is filtered',
        menu.getChildAt(6).isVisible());

    menu.dispose();
  },

  testScrollIntoView() {
    let menu = new FilteredMenu();
    menu.addItem(new MenuItem('Family'));
    menu.addItem(new MenuItem('Friends'));
    menu.addItem(new MenuItem('Photos'));
    menu.addItem(new MenuItem('Work'));
    menu.render(sandbox);

    menu.setHighlightedIndex(0);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));
    menu.setHighlightedIndex(1);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));
    menu.setHighlightedIndex(2);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));
    menu.setHighlightedIndex(3);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));
    menu.setHighlightedIndex(0);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));

    menu.dispose();
  },

  testScrollIntoView_cssTransformApplied() {
    // Applying a transform property on an element affects whether it is set
    // as an offsetParent, which influences calculation of offsetTop and
    // offsetLeft for child elements. This test is to help ensure that reliance
    // on offsetParent is avoided when performing scroll-on-highlight as the
    // behavior of offsetParent is not well-defined.
    let menu = new FilteredMenu();
    menu.addItem(new MenuItem('Family'));
    menu.addItem(new MenuItem('Friends'));
    menu.addItem(new MenuItem('Photos'));
    menu.addItem(new MenuItem('Work'));
    menu.render(sandbox);

    transform.setTranslation(menu.getContentElement(), 0, 0);

    menu.setHighlightedIndex(0);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));
    menu.setHighlightedIndex(1);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));
    menu.setHighlightedIndex(2);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));
    menu.setHighlightedIndex(3);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));
    menu.setHighlightedIndex(0);
    assertTrue(
        'Highlighted item should be visible', isHighlightedVisible(menu));

    menu.dispose();
  },

  testEscapeKeyHandling() {
    let menu = new FilteredMenu();
    menu.addItem(new MenuItem('Family'));
    menu.addItem(new MenuItem('Friends'));
    menu.render(sandbox);

    let gotKeyCode = false;
    const wrapper = document.getElementById('wrapper');
    events.listenOnce(wrapper, EventType.KEYPRESS, (e) => {
      gotKeyCode = true;
    });
    testingEvents.fireKeySequence(menu.getFilterInputElement(), KeyCodes.ESC);
    assertFalse('ESC key should not propagate out to parent', gotKeyCode);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAriaRoles() {
    let menu = new FilteredMenu();
    menu.addItem(new MenuItem('Item 1'));
    menu.render(sandbox);

    const input = menu.getFilterInputElement();
    assertEquals(
        AutoCompleteValues.LIST, aria.getState(input, State.AUTOCOMPLETE));
    assertEquals(menu.getContentElement().id, aria.getState(input, State.OWNS));
    assertEquals('true', aria.getState(input, State.EXPANDED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testInputActiveDescendant() {
    let menu = new FilteredMenu();
    const menuItem1 = new MenuItem('Item 1');
    const menuItem2 = new MenuItem('Item 2');
    menu.addItem(menuItem1);
    menu.addItem(menuItem2);
    menu.render(sandbox);

    assertNull(aria.getActiveDescendant(menu.getFilterInputElement()));
    menu.setHighlightedIndex(0);
    assertEquals(
        menuItem1.getElementStrict(),
        aria.getActiveDescendant(menu.getFilterInputElement()));
    menu.setHighlightedIndex(1);
    assertEquals(
        menuItem2.getElementStrict(),
        aria.getActiveDescendant(menu.getFilterInputElement()));
  },
});
