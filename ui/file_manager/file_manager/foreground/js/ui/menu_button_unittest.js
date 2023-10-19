// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {decorate} from '../../../common/js/ui.js';
import {Menu} from './menu.js';
import {MenuButton} from './menu_button.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

// clang-format on

export function testMenuShowAndHideEvents() {
  let menu = document.createElement('div');
  decorate(menu, Menu);
  // @ts-ignore: error TS2352: Conversion of type 'HTMLDivElement' to type
  // 'Menu' may be a mistake because neither type sufficiently overlaps with the
  // other. If this was intentional, convert the expression to 'unknown' first.
  menu = /** @type {!Menu} */ (menu);
  document.body.appendChild(menu);

  let menuButton = document.createElement('div');
  decorate(menuButton, MenuButton);
  // @ts-ignore: error TS2740: Type 'MenuButton' is missing the following
  // properties from type 'HTMLDivElement': align, addEventListener,
  // removeEventListener, accessKey, and 291 more.
  menuButton = /** @type {!MenuButton} */ (menuButton);
  // @ts-ignore: error TS2339: Property 'menu' does not exist on type
  // 'HTMLDivElement'.
  menuButton.menu = menu;
  document.body.appendChild(menuButton);

  // @ts-ignore: error TS7034: Variable 'events' implicitly has type 'any[]' in
  // some locations where its type cannot be determined.
  const events = [];
  menuButton.addEventListener('menushow', function(e) {
    events.push(e);
  });
  menuButton.addEventListener('menuhide', function(e) {
    events.push(e);
  });

  // Click to show menu.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  assertEquals(1, events.length);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals('menushow', events[0].type);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals(true, events[0].bubbles);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals(true, events[0].cancelable);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals(window, events[0].view);

  // Click to hide menu by clicking the button.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  assertEquals(2, events.length);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals('menuhide', events[1].type);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals(true, events[1].bubbles);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals(false, events[1].cancelable);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals(window, events[1].view);
  // The button must not appear highlighted.
  assertTrue(menuButton.classList.contains('using-mouse'));

  // Click to show menu and hide by clicking the outside of the button.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  assertEquals(3, events.length);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals('menushow', events[2].type);
  document.dispatchEvent(new MouseEvent('mousedown'));
  assertEquals(4, events.length);
  // @ts-ignore: error TS7005: Variable 'events' implicitly has an 'any[]' type.
  assertEquals('menuhide', events[3].type);
  // Emulate losing focus after clicking outside of the button.
  menuButton.dispatchEvent(new Event('blur'));
  // Focus highlight should not be suppressed anymore.
  assertFalse(menuButton.classList.contains('using-mouse'));
}

export function testFocusMoves() {
  let menu = document.createElement('div');
  const otherButton = document.createElement('button');
  decorate(menu, Menu);
  // @ts-ignore: error TS2352: Conversion of type 'HTMLDivElement' to type
  // 'Menu' may be a mistake because neither type sufficiently overlaps with the
  // other. If this was intentional, convert the expression to 'unknown' first.
  menu = /** @type {!Menu} */ (menu);
  // @ts-ignore: error TS2339: Property 'addMenuItem' does not exist on type
  // 'HTMLDivElement'.
  menu.addMenuItem({});
  document.body.appendChild(menu);
  document.body.appendChild(otherButton);

  let menuButton = document.createElement('div');
  decorate(menuButton, MenuButton);
  // @ts-ignore: error TS2322: Type 'MenuButton' is not assignable to type
  // 'HTMLDivElement'.
  menuButton = /** @type {!MenuButton} */ (menuButton);
  // Allow to put focus on the menu button by focus().
  menuButton.tabIndex = 1;
  // @ts-ignore: error TS2339: Property 'menu' does not exist on type
  // 'HTMLDivElement'.
  menuButton.menu = menu;
  document.body.appendChild(menuButton);

  // Case 1: Close by mouse click outside the menu.
  otherButton.focus();
  // Click to show menu.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  // @ts-ignore: error TS2339: Property 'isMenuShown' does not exist on type
  // 'HTMLDivElement'.
  assertTrue(menuButton.isMenuShown());
  // Click again to hide menu by clicking the button.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  // @ts-ignore: error TS2339: Property 'isMenuShown' does not exist on type
  // 'HTMLDivElement'.
  assertFalse(menuButton.isMenuShown());
  // Focus should be kept on the original place.
  assertEquals(otherButton, document.activeElement);

  // Case 2: Activate menu item with mouse.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  // @ts-ignore: error TS2339: Property 'isMenuShown' does not exist on type
  // 'HTMLDivElement'.
  assertTrue(menuButton.isMenuShown());
  // Emulate choosing a menu item by mouse click.
  // @ts-ignore: error TS2339: Property 'menuItems' does not exist on type
  // 'HTMLDivElement'.
  const menuItem = menu.menuItems[0];
  menuItem.selected = true;
  menuItem.dispatchEvent(new MouseEvent('mouseup', {buttons: 1}));
  // @ts-ignore: error TS2339: Property 'isMenuShown' does not exist on type
  // 'HTMLDivElement'.
  assertFalse(menuButton.isMenuShown());
  // Focus should be kept on the original place.
  assertEquals(otherButton, document.activeElement);

  // Case 3: Open menu and activate by keyboard.
  menuButton.focus();
  assertEquals(menuButton, document.activeElement);
  // Open menu
  menuButton.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
  // @ts-ignore: error TS2339: Property 'isMenuShown' does not exist on type
  // 'HTMLDivElement'.
  assertTrue(menuButton.isMenuShown(), 'menu opened');
  // Select an item and activate
  menu.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
  menu.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
  // @ts-ignore: error TS2339: Property 'isMenuShown' does not exist on type
  // 'HTMLDivElement'.
  assertFalse(menuButton.isMenuShown(), 'menu closed');
  // Focus should be still on the menu button.
  assertEquals(menuButton, document.activeElement);
}
