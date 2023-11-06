// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {decorate} from '../../../common/js/ui.js';
import {Command} from './command.js';
import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
// clang-format on

/** @type {Menu} */
let menu;

/**
 * @param {number} x The screenX coord of the mouseup event.
 * @param {number} y The screenY coord of the mouseup event.
 * @return {boolean} The return value is false if event is cancelable and at
 *     least one of the event handlers which received event called
 *     Event.preventDefault(). Otherwise it returns true.
 *     https://developer.mozilla.org/en-US/docs/Web/API/EventTarget/dispatchEvent
 */
function mouseUpAt(x, y) {
  const mouseUpEvent = new MouseEvent('mouseup', {
    bubbles: true,
    cancelable: true,
    // @ts-ignore: error TS2345: Argument of type '{ bubbles: true; cancelable:
    // true; target: Menu; screenX: number; screenY: number; }' is not
    // assignable to parameter of type 'MouseEventInit'.
    target: menu,
    screenX: x,
    screenY: y,
  });
  // @ts-ignore: error TS2339: Property 'isTrustedForTesting' does not exist on
  // type 'MouseEvent'.
  mouseUpEvent.isTrustedForTesting = true;
  // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist on type
  // 'Menu'.
  return menu.dispatchEvent(mouseUpEvent);
}

export function setUp() {
  // @ts-ignore: error TS2673: Constructor of class 'Menu' is private and only
  // accessible within the class declaration.
  menu = new Menu();
}

/** @suppress {visibility} Allow test to reach to private properties. */
export function testHandleMouseOver() {
  let called = false;
  // @ts-ignore: error TS2339: Property 'findMenuItem_' does not exist on type
  // 'Menu'.
  menu.findMenuItem_ = function() {
    called = true;
    // @ts-ignore: error TS2339: Property 'findMenuItem_' does not exist on type
    // 'Menu'.
    return Menu.prototype.findMenuItem_.apply(this, arguments);
  };

  const over =
      // @ts-ignore: error TS2345: Argument of type '{ bubbles: true; target:
      // HTMLElement; }' is not assignable to parameter of type
      // 'MouseEventInit'.
      new MouseEvent('mouseover', {bubbles: true, target: document.body});
  assertFalse(called);
  // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist on type
  // 'Menu'.
  menu.dispatchEvent(over);
  assertTrue(called);
}

export function testHandleMouseUp() {
  const realNow = Date.now;
  Date.now = function() {
    return 10;
  };

  // @ts-ignore: error TS2339: Property 'show' does not exist on type 'Menu'.
  menu.show({x: 5, y: 5});

  // Stop mouseups at the same time and position.
  assertFalse(mouseUpAt(5, 5));

  // Allow mouseups with different positions but the same time.
  assertTrue(mouseUpAt(50, 50));

  // Alow mouseups with the same position but different times.
  Date.now = function() {
    return 1000;
  };
  assertTrue(mouseUpAt(5, 5));

  Date.now = realNow;
}

export function testShowViaKeyboardIgnoresMouseUps() {
  // @ts-ignore: error TS2339: Property 'show' does not exist on type 'Menu'.
  menu.show();
  assertTrue(mouseUpAt(0, 0));
}

/**
 * Tests that if the command attributes are spacified, they are copied to the
 * corresponding menuitem.
 */
export function testCommandMenuItem() {
  // Test 1: The case that the command label is set and other attributes copied.
  const command = new Command();
  command.id = 'the-command';
  command.label = 'CommandLabel';
  command.disabled = true;
  command.hidden = true;
  command.checked = true;
  document.body.appendChild(command);

  const menuItem = new MenuItem();
  // @ts-ignore: error TS2339: Property 'command' does not exist on type
  // 'MenuItem'.
  menuItem.command = '#the-command';

  // Confirms the label is copied from the command.
  // @ts-ignore: error TS2339: Property 'label' does not exist on type
  // 'MenuItem'.
  assertEquals('CommandLabel', menuItem.label);
  // Confirms the attributes are copied from the command.
  assertEquals(true, menuItem.disabled);
  assertEquals(true, menuItem.hidden);
  // @ts-ignore: error TS2339: Property 'checked' does not exist on type
  // 'MenuItem'.
  assertEquals(true, menuItem.checked);

  // Test 2: The case that the command label is not set, and other attributes
  // have default values.
  const command2 = new Command();
  command2.id = 'the-command2';
  document.body.appendChild(command2);

  const menuItem2 = new MenuItem();
  // @ts-ignore: error TS2339: Property 'label' does not exist on type
  // 'MenuItem'.
  menuItem2.label = 'MenuLabel';
  // @ts-ignore: error TS2339: Property 'command' does not exist on type
  // 'MenuItem'.
  menuItem2.command = '#the-command2';

  // Confirms the label is not copied, keeping the original label.
  // @ts-ignore: error TS2339: Property 'label' does not exist on type
  // 'MenuItem'.
  assertEquals('MenuLabel', menuItem2.label);
  // Confirms the attributes are copied from the command.
  assertEquals(false, menuItem2.disabled);
  assertEquals(false, menuItem2.hidden);
  // @ts-ignore: error TS2339: Property 'checked' does not exist on type
  // 'MenuItem'.
  assertEquals(false, menuItem2.checked);
}

/**
 * Mark all menu items other than |hiddenItems| as visible and check that the
 * expected number of separators are visible.
 */
// @ts-ignore: error TS7006: Parameter 'expectedSeparators' implicitly has an
// 'any' type.
function runSeparatorTest(items, hiddenItems, expectedSeparators) {
  // @ts-ignore: error TS2339: Property 'menuItems' does not exist on type
  // 'Menu'.
  for (const item of menu.menuItems) {
    item.hidden = false;
  }
  for (const i of hiddenItems) {
    items[i].hidden = true;
  }
  // @ts-ignore: error TS2339: Property 'updateCommands' does not exist on type
  // 'Menu'.
  menu.updateCommands();
  // @ts-ignore: error TS2339: Property 'hasVisibleItems' does not exist on type
  // 'Menu'.
  assertEquals(hiddenItems.length !== items.length, menu.hasVisibleItems());
  assertEquals(
      // @ts-ignore: error TS2339: Property 'querySelectorAll' does not exist on
      // type 'Menu'.
      expectedSeparators, menu.querySelectorAll('hr:not([hidden])').length);

  // The separators at the ends are always hidden.
  // @ts-ignore: error TS2339: Property 'menuItems' does not exist on type
  // 'Menu'.
  assertTrue(menu.menuItems[0].hidden);
  // @ts-ignore: error TS2339: Property 'menuItems' does not exist on type
  // 'Menu'.
  assertTrue(menu.menuItems[6].hidden);
}

/**
 * Tests that separators are only displayed when there is a visible
 * non-separator item on both sides of it. Further, ensure that multiple
 * separators will not be displayed adjacent to each other.
 */
export function testSeparators() {
  const menuItems = [];
  // @ts-ignore: error TS2339: Property 'addSeparator' does not exist on type
  // 'Menu'.
  menu.addSeparator();
  // @ts-ignore: error TS2339: Property 'addMenuItem' does not exist on type
  // 'Menu'.
  menuItems.push(menu.addMenuItem({label: 'a'}));
  // @ts-ignore: error TS2339: Property 'addSeparator' does not exist on type
  // 'Menu'.
  menu.addSeparator();
  // @ts-ignore: error TS2339: Property 'addMenuItem' does not exist on type
  // 'Menu'.
  menuItems.push(menu.addMenuItem({label: 'b'}));
  // @ts-ignore: error TS2339: Property 'addSeparator' does not exist on type
  // 'Menu'.
  menu.addSeparator();
  // @ts-ignore: error TS2339: Property 'addMenuItem' does not exist on type
  // 'Menu'.
  menuItems.push(menu.addMenuItem({label: 'c'}));
  // @ts-ignore: error TS2339: Property 'addSeparator' does not exist on type
  // 'Menu'.
  menu.addSeparator();

  runSeparatorTest(menuItems, [0, 1, 2], 0);
  runSeparatorTest(menuItems, [0, 1], 0);
  runSeparatorTest(menuItems, [0, 2], 0);
  runSeparatorTest(menuItems, [1, 2], 0);
  runSeparatorTest(menuItems, [0], 1);
  runSeparatorTest(menuItems, [1], 1);
  runSeparatorTest(menuItems, [2], 1);
  runSeparatorTest(menuItems, [], 2);
}

/**
 * Tests that focusSelectedItem() ignores hidden and disabled items.
 */
export function testFocusSelectedItems() {
  const menu = document.createElement('div');
  decorate(menu, Menu);
  // @ts-ignore: error TS2339: Property 'addMenuItem' does not exist on type
  // 'HTMLDivElement'.
  const item1 = menu.addMenuItem({label: 'item1'});
  // @ts-ignore: error TS2339: Property 'addSeparator' does not exist on type
  // 'HTMLDivElement'.
  menu.addSeparator();
  // @ts-ignore: error TS2339: Property 'addMenuItem' does not exist on type
  // 'HTMLDivElement'.
  const item2 = menu.addMenuItem({label: 'item2'});
  // @ts-ignore: error TS2339: Property 'addMenuItem' does not exist on type
  // 'HTMLDivElement'.
  const item3 = menu.addMenuItem({label: 'item3'});
  document.body.appendChild(menu);

  // Nothing is selected in the menu it should focus the first item.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  assertEquals(-1, menu.selectedIndex);
  // @ts-ignore: error TS2339: Property 'focusSelectedItem' does not exist on
  // type 'HTMLDivElement'.
  menu.focusSelectedItem();
  // Focus the first item.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  assertEquals(0, menu.selectedIndex);

  // Hide the first item, it should focus the second item.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  menu.selectedIndex = -1;
  item1.hidden = true;
  // @ts-ignore: error TS2339: Property 'focusSelectedItem' does not exist on
  // type 'HTMLDivElement'.
  menu.focusSelectedItem();
  // Focus the second item, index=1 is the separator.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  assertEquals(2, menu.selectedIndex);

  // First item is visible but disabled, it should focus the second item.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  menu.selectedIndex = -1;
  item1.hidden = false;
  item1.disabled = true;
  // @ts-ignore: error TS2339: Property 'focusSelectedItem' does not exist on
  // type 'HTMLDivElement'.
  menu.focusSelectedItem();
  // Focus the second item, index=1 is the separator.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  assertEquals(2, menu.selectedIndex);

  // All items are visible but disabled, it should focus the first item.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  menu.selectedIndex = -1;
  item1.disabled = true;
  item2.disabled = true;
  item3.disabled = true;
  // @ts-ignore: error TS2339: Property 'focusSelectedItem' does not exist on
  // type 'HTMLDivElement'.
  menu.focusSelectedItem();
  // Focus the first item.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  assertEquals(0, menu.selectedIndex);

  // If selectedIndex is already set, focusSelectedItem doesn't change it.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  assertEquals(0, menu.selectedIndex);
  item1.disabled = true;
  item2.disabled = false;
  item3.disabled = false;
  // @ts-ignore: error TS2339: Property 'focusSelectedItem' does not exist on
  // type 'HTMLDivElement'.
  menu.focusSelectedItem();
  // Focus remains in the first item.
  // @ts-ignore: error TS2339: Property 'selectedIndex' does not exist on type
  // 'HTMLDivElement'.
  assertEquals(0, menu.selectedIndex);
}

/**
 * Tests that MenuItem defaults to tabindex=-1.
 */
export function testMenuItemTabIndex() {
  // Defaults to -1.
  // @ts-ignore: error TS2339: Property 'addMenuItem' does not exist on type
  // 'Menu'.
  const item1 = menu.addMenuItem({label: 'item 1'});
  assertEquals('-1', item1.getAttribute('tabindex'));

  // Keeps previously set tabindex.
  const itemDiv = document.createElement('div');
  itemDiv.setAttribute('tabindex', '0');
  decorate(itemDiv, MenuItem);
  assertEquals('0', itemDiv.getAttribute('tabindex'));

  // Separator doesn't get tabindex.
  // @ts-ignore: error TS2339: Property 'addSeparator' does not exist on type
  // 'Menu'.
  menu.addSeparator();
  // @ts-ignore: error TS2339: Property 'menuItems' does not exist on type
  // 'Menu'.
  const separator = menu.menuItems[menu.menuItems.length - 1];
  assertTrue(separator.isSeparator());
  assertFalse(separator.hasAttribute('tabindex'));
}
