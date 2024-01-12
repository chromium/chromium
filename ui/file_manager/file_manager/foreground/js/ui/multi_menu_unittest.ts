// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';
import {queryDecoratedElement} from '../../../common/js/dom_utils.js';

import {Command} from './command.js';
import {Menu} from './menu.js';
import type {MultiMenu} from './multi_menu.js';
import {MultiMenuButton} from './multi_menu_button.js';

let menubutton: MultiMenuButton;
let topMenu: Menu;
let subMenu: Menu;
let secondSubMenu: Menu;
let initialWindowHeight: number;

// Set up test components.
export function setUp() {
  // Multiple tests rely on the window height, reset between tests to avoid
  // interference.
  if (!initialWindowHeight) {
    initialWindowHeight = window.innerHeight;
  }
  window.innerHeight = initialWindowHeight;

  // Install cr.ui <command> elements and <cr-menu>s on the page.
  document.body.innerHTML = getTrustedHTML`
    <style>
      cr-menu {
        position: fixed;
        padding: 8px;
      }
      cr-menu-item {
        width: 10px;
        height: 10px;
        display: block;
        background-color: blue;
      }
    </style>
    <command id="default-task">
    <command id="more-actions">
    <command id="show-submenu" shortcut="Enter">
    <button id="test-menu-button" menu="#menu"></button>
    <cr-menu id="menu" hidden>
      <cr-menu-item id="default" command="#default-task"></cr-menu-item>
      <cr-menu-item id="more" command="#more-actions"></cr-menu-item>
      <cr-menu-item id="host-sub-menu" command="#show-submenu"
    visibleif="full-page" class="hide-on-toolbar"
    sub-menu="#sub-menu" hidden></cr-menu-item>
      <cr-menu-item id="host-second-sub-menu" command="#show-submenu"
    visibleif="full-page" class="hide-on-toolbar"
    sub-menu="#second-sub-menu" hidden></cr-menu-item>
    </cr-menu>
    <cr-menu id="sub-menu" hidden>
      <cr-menu-item id="first" class="custom-appearance"></cr-menu-item>
      <cr-menu-item id="second" class="custom-appearance"></cr-menu-item>
    </cr-menu>
    <cr-menu id="second-sub-menu" hidden>
      <cr-menu-item id="secondone" class="custom-appearance"></cr-menu-item>
    </cr-menu>
    <div id="focus-div" tabindex="1"/>
    <button id="focus-button" tabindex="2"/>
    <cr-input id="focus-input" input-tabindex="3">
    </cr-input>
  `;

  // Initialize cr.ui.Command with the <command>s.
  for (const command of document.querySelectorAll<HTMLElement>('command')) {
    crInjectTypeAndInit(command, Command);
  }
  menubutton = queryDecoratedElement('#test-menu-button', MultiMenuButton);
  topMenu = queryDecoratedElement('#menu', Menu);
  subMenu = queryDecoratedElement('#sub-menu', Menu);
  secondSubMenu = queryDecoratedElement('#second-sub-menu', Menu);
}

/**
 * Send a 'mouseover' event to the element target of a query.
 * @param targetQuery Query to specify the element.
 */
function sendMouseOver(targetQuery: string) {
  const event = new MouseEvent('mouseover', {
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  });
  const target = document.querySelector(targetQuery)!;
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 * Send a 'mousedown' event to the element target of a query.
 * @param targetQuery Query to specify the element.
 */
function sendMouseDown(targetQuery: string) {
  const event = new MouseEvent('mousedown', {
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  });
  const target = document.querySelector(targetQuery)!;
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 * Send a 'mouseover' event to the element target of a query.
 * @param targetQuery Query to specify the element.
 * @param x Position of the event in 'X'.
 * @param y Position of the event in 'X'.
 */
function sendMouseOut(targetQuery: string, x: number, y: number) {
  const event = new MouseEvent('mouseout', {
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
    clientX: x,
    clientY: y,
  });
  const target = document.querySelector(targetQuery)!;
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 * Send a 'keydown' event to the element target of a query.
 * @param targetQuery Query to specify the element.
 * @param key property value for the key.
 */
function sendKeyDown(targetQuery: string, key: string) {
  const event = new KeyboardEvent('keydown', {
    key: key,
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  });
  const target = document.querySelector(targetQuery)!;
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 * Tests that making the top level menu visible doesn't
 * cause the sub-menu to become visible.
 */
export function testShowMenuDoesntShowSubMenu() {
  menubutton.showMenu(true);
  // Check the top level menu is not hidden.
  assertFalse(topMenu.hasAttribute('hidden'));
  // Check the sub-menu is hidden
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that a 'mouseover' event on top of normal menu-items
 * doesn't cause the sub-menu to become visible.
 */
export function testMouseOverNormalItemsDoesntShowSubMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#default-task');
  assertTrue(subMenu.hasAttribute('hidden'));
  sendMouseOver('#more-actions');
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that 'mouseover' on a menu-item with 'show-submenu' command
 * causes the sub-menu to become visible.
 */
export function testMouseOverHostMenuShowsSubMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#host-sub-menu');
  assertFalse(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that 'mouseout' with the mouse over the top level
 * menu causes the sub-menu to hide.
 */
export function testMouseoutFromHostMenuItemToHostMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#host-sub-menu');
  assertFalse(subMenu.hasAttribute('hidden'));
  // Get the location of one of our menu-items to send with the event.
  const item = document.querySelector('#default-task')!;
  const loc = item.getBoundingClientRect();
  sendMouseOut('#host-sub-menu', loc.left, loc.top);
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that 'mouseout' with the mouse over the sub-menu
 * doesn't hide the sub-menu.
 */
export function testMouseoutFromHostMenuToSubMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#host-sub-menu');
  assertFalse(subMenu.hasAttribute('hidden'));
  // Get the location of our sub-menu to send with the event.
  const loc = subMenu.getBoundingClientRect();
  sendMouseOut('#host-sub-menu', loc.left, loc.top);
  assertFalse(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that selecting a menu-item with a 'show-submenu' command
 * doesn't cause the sub-menu to become visible.
 */
export function testSelectHostMenuItem() {
  menubutton.showMenu(true);
  topMenu.selectedIndex = 2;
  const hostItem = document.querySelector('#host-sub-menu')!;
  assert(hostItem.hasAttribute('selected'));
  assertEquals(hostItem.getAttribute('selected'), 'selected');
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that selecting a menu-item with a 'show-submenu' command
 * followed by calling the showSubMenu() method causes the
 * sub-menu to become visible.
 * (Note: in an application, this would happen from a command
 * being executed rather than a direct showSubMenu() call.)
 */
export function testSelectHostMenuItemAndCallShowSubMenu() {
  testSelectHostMenuItem();
  (menubutton.menu as MultiMenu).showSubMenu();
  assertFalse(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that a mouse click outside of a menu and sub-menu causes
 * both menus to hide.
 */
export function testClickOutsideVisibleMenuAndSubMenu() {
  testSelectHostMenuItemAndCallShowSubMenu();
  const event = new MouseEvent('mousedown', {
    bubbles: true,
    cancelable: true,
    view: window,
    composed: true,  // Allow the event to bubble past shadow DOM root.
    clientX: 0,      // 0, 0 is in the padding area of the viewport
    clientY: 0,
  });
  menubutton.dispatchEvent(event);
  assertTrue(topMenu.hasAttribute('hidden'));
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that shrinking the window height will limit
 * the height of the sub-menu.
 */
export function testShrinkWindowSizesSubMenu() {
  testSelectHostMenuItemAndCallShowSubMenu();
  const subMenuPosition = subMenu.getBoundingClientRect();
  // Reduce window innerHeight so sub-menu won't fit.
  window.innerHeight = subMenuPosition.bottom - 10;
  // Navigate from sub-menu to the parent menu.
  sendKeyDown('#test-menu-button', 'ArrowLeft');
  // Call the internal hide method, then re-show it
  // to force the resizing behavior.
  (menubutton.menu as MultiMenu)['hideSubMenu_']();
  (menubutton.menu as MultiMenu).showSubMenu();
  const shrunkPosition = subMenu.getBoundingClientRect();
  assertTrue(shrunkPosition.bottom < window.innerHeight);
}

/**
 * Tests that growing the window height will increase
 * the height of the sub-menu.
 */
export function testGrowWindowSizesSubMenu() {
  // Remember the full size of the sub-menu
  testSelectHostMenuItemAndCallShowSubMenu();
  const subMenuPosition = subMenu.getBoundingClientRect();
  // Make sure the sub-menu has been reduced in height.
  testShrinkWindowSizesSubMenu();
  // Make the window taller than the sub-menu plus padding.
  window.innerHeight = subMenuPosition.bottom + 20;
  // Navigate from sub-menu to the parent menu.
  sendKeyDown('#test-menu-button', 'ArrowLeft');
  // Call the internal hide method, then re-show it
  // to force the resizing behavior.
  (menubutton.menu as MultiMenu)['hideSubMenu_']();
  (menubutton.menu as MultiMenu).showSubMenu();
  const grownPosition = subMenu.getBoundingClientRect();
  // Test that the height of the sub-menu is the same as
  // the height at the start of this test (before we
  // deliberately shrank it).
  assertTrue(grownPosition.height === subMenuPosition.height);
}

/**
 * Utility function to prepare the menu and sub-menu for keyboard tests.
 */
function prepareForKeyboardNavigation() {
  // Make sure the both of the menus are active.
  testSelectHostMenuItemAndCallShowSubMenu();
  // Re-enable the menu-items, since showMenu() disables
  // all of them due to the canExecute() tests all returning
  // false since we're just a unit test harness. This is
  // needed since the arrow key handlers skip over disabled items.
  document.querySelector('#default')?.removeAttribute('disabled');
  document.querySelector('#more')?.removeAttribute('disabled');
  document.querySelector('#host-sub-menu')?.removeAttribute('disabled');
}

/**
 * Tests that arrow navigates from main menu to sub-menu.
 */
export function testNavigateFromMenuToSubMenu() {
  prepareForKeyboardNavigation();
  // Check that the hosting menu-item is not selected.
  const hostItem = document.querySelector('#host-sub-menu')!;
  assertFalse(hostItem.hasAttribute('selected'));
  // Check that the sub-menu has taken selection.
  const subItem = document.querySelector('#first')!;
  assertTrue(subItem.hasAttribute('selected'));
}

/**
 * Tests that arrow left moves back to the top level menu
 * only when the selected sub-menu item is the first one.
 */
export function testNavigateFromSubMenuToParentMenu() {
  testNavigateFromMenuToSubMenu();
  // Use the arrow key to go to the next sub-menu item.
  sendKeyDown('#test-menu-button', 'ArrowDown');
  const secondItem = document.querySelector('#second')!;
  assertTrue(secondItem.hasAttribute('selected'));
  // Try to navigate from sub-menu to the parent menu.
  sendKeyDown('#test-menu-button', 'ArrowLeft');
  // Check that parent menu hosting item didn't get selected.
  const hostItem = document.querySelector('#host-sub-menu')!;
  assertFalse(hostItem.hasAttribute('selected'));
  // Check that selection is still on the sub-menu item.
  assertTrue(secondItem.hasAttribute('selected'));
  // Navigate up to the first sub-menu item.
  sendKeyDown('#test-menu-button', 'ArrowUp');
  // Check that the first sub-menu item is selected.
  const firstItem = document.querySelector('#first')!;
  assertTrue(firstItem.hasAttribute('selected'));
  // Navigate back to the parent menu.
  sendKeyDown('#test-menu-button', 'ArrowLeft');
  // Check selection has moved back to the parent menu.
  assertTrue(hostItem.hasAttribute('selected'));
  assertFalse(firstItem.hasAttribute('selected'));
}

/**
 * Tests that arrow up on the top level menu hides the
 * sub menu when the sub-menu is visible.
 */
export function testTopMenuArrowUpDismissesSubMenu() {
  prepareForKeyboardNavigation();
  // Check that the hosting menu-item is not selected.
  const hostItem = document.querySelector('#host-sub-menu')!;
  assertFalse(hostItem.hasAttribute('selected'));
  // Navigate from sub-menu to the parent menu.
  sendKeyDown('#test-menu-button', 'ArrowLeft');
  // Check that the hosting menu-item is not selected.
  assertTrue(hostItem.hasAttribute('selected'));
  // Navigate up the main menu.
  sendKeyDown('#test-menu-button', 'ArrowUp');
  // Check that the sub-menu has been hidden.
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that the top level menu is resized when the parent
 * window is too small to fit in without clipping.
 */
export function testShrinkWindowSizesTopMenu() {
  menubutton.showMenu(true);
  const menuPosition = topMenu.getBoundingClientRect();
  // Reduce window innerHeight so the menu won't fit.
  window.innerHeight = menuPosition.height - 10;
  // Call showMenu() which will first hide it, then re-open
  // it to force the resizing behavior.
  menubutton.showMenu(true);
  const shrunkPosition = topMenu.getBoundingClientRect();
  assertTrue(shrunkPosition.height === (window.innerHeight - 2));
}

/**
 * Tests that mousedown the menu button grabs focus.
 */
export function testFocusMenuButtonWithMouse() {
  // Set focus on a div element.
  const divElement = document.querySelector<HTMLElement>('#focus-div')!;
  divElement.focus();

  // Send mousedown event to the menu button.
  sendMouseDown('#test-menu-button');

  // Verify that the previously focused element still has focus.
  assertTrue(document.hasFocus() && document.activeElement === divElement);

  // Set focus on a button element.
  //*   */
  const buttonElement = document.querySelector<HTMLElement>('#focus-button')!;
  buttonElement.focus();

  // Send mousedown event to the menu button.
  sendMouseDown('#test-menu-button');

  // Verify that the previously focused button has lost focus.
  assertFalse(document.hasFocus() && document.activeElement === buttonElement);

  // Verify the menu button has taken focus.
  assertTrue(document.hasFocus() && document.activeElement === menubutton);

  // Set focus on a cr-input element.
  //*   */
  const inputElement = document.querySelector<HTMLElement>('#focus-input')!;
  inputElement.focus();

  // Send mousedown event to the menu button.
  sendMouseDown('#test-menu-button');

  // Verify the cr-input element has lost focus.
  assertFalse(document.hasFocus() && document.activeElement === inputElement);

  // Verify the menu button has taken focus.
  assertTrue(document.hasFocus() && document.activeElement === menubutton);
}

/**
 * Tests that opening a sub menu hides any showing sub menu.
 */
export function testShowSubMenuHidesExisting() {
  testMouseOverHostMenuShowsSubMenu();
  sendMouseOver('#host-second-sub-menu');
  // Check the previously shown sub menu is hidden.
  assertTrue(subMenu.hasAttribute('hidden'));
  // Check the second sub menu is visible.
  assertFalse(secondSubMenu.hasAttribute('hidden'));
}

/**
 * Tests that a keydown event that is not intended for the menu will not be
 * consumed by the menu.
 */
export async function testMenuDoesNotConsumeNonMenuEvent() {
  let eventConsumedByMenu = true;
  const nonMenuEventKey = 'AudioVolumeUp';
  // The event should be received by the `document.body` after it is ignored by
  // the menu.
  document.body.addEventListener('keydown', e => {
    eventConsumedByMenu = false;
    // Check this is the right keydown event.
    assertEquals(nonMenuEventKey, e.key);
    // Confirm this is not a menu event.
    assertFalse(menubutton.isMenuEvent(e));
  });
  // Send the event to the menu.
  sendKeyDown('#test-menu-button', nonMenuEventKey);
  // Wait for the event to be received by the `document.body`.
  await new Promise<void>(
      resolve => window.requestAnimationFrame(() => resolve()));
  assertFalse(eventConsumedByMenu);
}
