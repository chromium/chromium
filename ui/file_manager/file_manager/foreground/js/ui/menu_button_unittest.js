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
  menu = /** @type {!Menu} */ (menu);
  document.body.appendChild(menu);

  let menuButton = document.createElement('div');
  decorate(menuButton, MenuButton);
  menuButton = /** @type {!MenuButton} */ (menuButton);
  menuButton.menu = menu;
  document.body.appendChild(menuButton);

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
  assertEquals('menushow', events[0].type);
  assertEquals(true, events[0].bubbles);
  assertEquals(true, events[0].cancelable);
  assertEquals(window, events[0].view);

  // Click to hide menu by clicking the button.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  assertEquals(2, events.length);
  assertEquals('menuhide', events[1].type);
  assertEquals(true, events[1].bubbles);
  assertEquals(false, events[1].cancelable);
  assertEquals(window, events[1].view);
  // The button must not appear highlighted.
  assertTrue(menuButton.classList.contains('using-mouse'));

  // Click to show menu and hide by clicking the outside of the button.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  assertEquals(3, events.length);
  assertEquals('menushow', events[2].type);
  document.dispatchEvent(new MouseEvent('mousedown'));
  assertEquals(4, events.length);
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
  menu = /** @type {!Menu} */ (menu);
  menu.addMenuItem({});
  document.body.appendChild(menu);
  document.body.appendChild(otherButton);

  let menuButton = document.createElement('div');
  decorate(menuButton, MenuButton);
  menuButton = /** @type {!MenuButton} */ (menuButton);
  // Allow to put focus on the menu button by focus().
  menuButton.tabIndex = 1;
  menuButton.menu = menu;
  document.body.appendChild(menuButton);

  // Case 1: Close by mouse click outside the menu.
  otherButton.focus();
  // Click to show menu.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  assertTrue(menuButton.isMenuShown());
  // Click again to hide menu by clicking the button.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  assertFalse(menuButton.isMenuShown());
  // Focus should be kept on the original place.
  assertEquals(otherButton, document.activeElement);

  // Case 2: Activate menu item with mouse.
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  assertTrue(menuButton.isMenuShown());
  // Emulate choosing a menu item by mouse click.
  const menuItem = menu.menuItems[0];
  menuItem.selected = true;
  menuItem.dispatchEvent(new MouseEvent('mouseup', {buttons: 1}));
  assertFalse(menuButton.isMenuShown());
  // Focus should be kept on the original place.
  assertEquals(otherButton, document.activeElement);

  // Case 3: Open menu and activate by keyboard.
  menuButton.focus();
  assertEquals(menuButton, document.activeElement);
  // Open menu
  menuButton.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
  assertTrue(menuButton.isMenuShown(), 'menu opened');
  // Select an item and activate
  menu.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
  menu.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
  assertFalse(menuButton.isMenuShown(), 'menu closed');
  // Focus should be still on the menu button.
  assertEquals(menuButton, document.activeElement);
}
