// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';

import {Command} from './command.js';
import {Menu} from './menu.js';
import {createMenuItem, MenuItem} from './menu_item.js';


let menu: Menu;

/**
 * @param x The screenX coord of the mouseup event.
 * @param y The screenY coord of the mouseup event.
 * @return The return value is false if event is cancelable and at
 *     least one of the event handlers which received event called
 *     Event.preventDefault(). Otherwise it returns true.
 *     https://developer.mozilla.org/en-US/docs/Web/API/EventTarget/dispatchEvent
 */
function mouseUpAt(x: number, y: number): boolean {
  const mouseUpEvent = new MouseEvent('mouseup', {
    bubbles: true,
    cancelable: true,
    screenX: x,
    screenY: y,
  });
  (mouseUpEvent as any).isTrustedForTesting = true;
  return menu.dispatchEvent(mouseUpEvent);
}

export function setUp() {
  menu = document.createElement('cr-menu') as Menu;
  crInjectTypeAndInit(menu, Menu);
}

export function testHandleMouseOver() {
  let called = false;
  menu['findMenuItem'] = function(node: Node) {
    called = true;
    return Menu.prototype['findMenuItem'].apply(this, [node]);
  };

  const over = new MouseEvent('mouseover', {bubbles: true});
  assertFalse(called);
  menu.dispatchEvent(over);
  assertTrue(called);
}

export function testHandleMouseUp() {
  const realNow = Date.now;
  Date.now = function() {
    return 10;
  };

  menu.show({x: 5, y: 5});

  // Stop mouseups at the same time and position.
  assertFalse(mouseUpAt(5, 5));

  // Allow mouseups with different positions but the same time.
  assertTrue(mouseUpAt(50, 50));

  // Allow mouseups with the same position but different times.
  Date.now = function() {
    return 1000;
  };
  assertTrue(mouseUpAt(5, 5));

  Date.now = realNow;
}

export function testShowViaKeyboardIgnoresMouseUps() {
  menu.show();
  assertTrue(mouseUpAt(0, 0));
}

/**
 * Tests that if the command attributes are specified, they are copied to the
 * corresponding menuitem.
 */
export function testCommandMenuItem() {
  // Test 1: The case that the command label is set and other attributes copied.
  const command = document.createElement('command') as Command;
  crInjectTypeAndInit(command, Command);
  command.id = 'the-command';
  command.label = 'CommandLabel';
  command.disabled = true;
  command.hidden = true;
  command.checked = true;
  document.body.appendChild(command);

  const menuItem = createMenuItem();
  menuItem.command = '#the-command';

  // Confirms the label is copied from the command.
  assertEquals('CommandLabel', menuItem.label);
  // Confirms the attributes are copied from the command.
  assertEquals(true, menuItem.disabled);
  assertEquals(true, menuItem.hidden);
  assertEquals(true, menuItem.checked);

  // Test 2: The case that the command label is not set, and other attributes
  // have default values.
  const command2 = document.createElement('command') as Command;
  crInjectTypeAndInit(command2, Command);
  command2.id = 'the-command2';
  document.body.appendChild(command2);

  const menuItem2 = createMenuItem();
  menuItem2.label = 'MenuLabel';
  menuItem2.command = '#the-command2';

  // Confirms the label is not copied, keeping the original label.
  assertEquals('MenuLabel', menuItem2.label);
  // Confirms the attributes are copied from the command.
  assertEquals(false, menuItem2.disabled);
  assertEquals(false, menuItem2.hidden);
  assertEquals(false, menuItem2.checked);
}

/**
 * Mark all menu items other than |hiddenItems| as visible and check that the
 * expected number of separators are visible.
 */
function runSeparatorTest(
    items: MenuItem[], hiddenItems: number[], expectedSeparators: number) {
  for (const item of menu.menuItems) {
    item.hidden = false;
  }
  for (const i of hiddenItems) {
    items[i]!.hidden = true;
  }
  menu.updateCommands();
  assertEquals(hiddenItems.length !== items.length, menu.hasVisibleItems());
  assertEquals(
      expectedSeparators, menu.querySelectorAll('hr:not([hidden])').length);

  // The separators at the ends are always hidden.
  assertTrue(menu.menuItems[0]!.hidden);
  assertTrue(menu.menuItems[6]!.hidden);
}

/**
 * Tests that separators are only displayed when there is a visible
 * non-separator item on both sides of it. Further, ensure that multiple
 * separators will not be displayed adjacent to each other.
 */
export function testSeparators() {
  const menuItems = [];
  menu.addSeparator();
  menuItems.push(menu.addMenuItem({label: 'a'}));
  menu.addSeparator();
  menuItems.push(menu.addMenuItem({label: 'b'}));
  menu.addSeparator();
  menuItems.push(menu.addMenuItem({label: 'c'}));
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
  const menu = document.createElement('div') as unknown as Menu;
  crInjectTypeAndInit(menu, Menu);
  const item1 = menu.addMenuItem({label: 'item1'});
  menu.addSeparator();
  const item2 = menu.addMenuItem({label: 'item2'});
  const item3 = menu.addMenuItem({label: 'item3'});
  document.body.appendChild(menu);

  // Nothing is selected in the menu it should focus the first item.
  assertEquals(-1, menu.selectedIndex);
  menu.focusSelectedItem();
  // Focus the first item.
  assertEquals(0, menu.selectedIndex);

  // Hide the first item, it should focus the second item.
  menu.selectedIndex = -1;
  item1.hidden = true;
  menu.focusSelectedItem();
  // Focus the second item, index=1 is the separator.
  assertEquals(2, menu.selectedIndex);

  // First item is visible but disabled, it should focus the second item.
  menu.selectedIndex = -1;
  item1.hidden = false;
  item1.disabled = true;
  menu.focusSelectedItem();
  // Focus the second item, index=1 is the separator.
  assertEquals(2, menu.selectedIndex);

  // All items are visible but disabled, it should focus the first item.
  menu.selectedIndex = -1;
  item1.disabled = true;
  item2.disabled = true;
  item3.disabled = true;
  menu.focusSelectedItem();
  // Focus the first item.
  assertEquals(0, menu.selectedIndex);

  // If selectedIndex is already set, focusSelectedItem doesn't change it.
  assertEquals(0, menu.selectedIndex);
  item1.disabled = true;
  item2.disabled = false;
  item3.disabled = false;
  menu.focusSelectedItem();
  // Focus remains in the first item.
  assertEquals(0, menu.selectedIndex);
}

/**
 * Tests that MenuItem defaults to tabindex=-1.
 */
export function testMenuItemTabIndex() {
  // Defaults to -1.
  const item1 = menu.addMenuItem({label: 'item 1'});
  assertEquals('-1', item1.getAttribute('tabindex'));

  // Keeps previously set tabindex.
  const itemDiv = document.createElement('div');
  itemDiv.setAttribute('tabindex', '0');
  crInjectTypeAndInit(itemDiv, MenuItem);
  assertEquals('0', itemDiv.getAttribute('tabindex'));

  // Separator doesn't get tabindex.
  menu.addSeparator();
  const separator = menu.menuItems[menu.menuItems.length - 1]!;
  assertTrue(separator.isSeparator());
  assertFalse(separator.hasAttribute('tabindex'));
}
