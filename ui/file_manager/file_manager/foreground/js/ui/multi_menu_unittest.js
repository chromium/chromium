// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {cr.ui.MultiMenuButton} */
let menubutton;

/** @type {cr.ui.Menu} */
let topMenu;

/** @type {cr.ui.Menu} */
let subMenu;

// Set up test components.
function setUp() {
  // Internals of WebUI reference this property when processing
  // keyboard events, so we need to prepare it to stop asserts.
  loadTimeData.data = {'SHORTCUT_ENTER': 'Enter'};
  // Install cr.ui <command> elements and <cr-menu>s on the page.
  document.body.innerHTML = [
    '<style>',
    '  cr-menu {',
    '    position: fixed;',
    '    padding: 8px;',
    '  }',
    '  cr-menu-item {',
    '    width: 10px;',
    '    height: 10px;',
    '    display: block;',
    '    background-color: blue;',
    '  }',
    '</style>',
    '<command id="default-task">',
    '<command id="more-actions">',
    '<command id="show-submenu" shortcut="Enter">',
    '<button id="test-menu-button" menu="#menu"></button>',
    '<cr-menu id="menu" hidden>',
    '  <cr-menu-item id="default" command="#default-task"></cr-menu-item>',
    '  <cr-menu-item id="more" command="#more-actions"></cr-menu-item>',
    '  <cr-menu-item id="host-sub-menu" command="#show-submenu"',
    'visibleif="full-page" class="hide-on-toolbar"',
    'sub-menu="#sub-menu" hidden></cr-menu-item>',
    '</cr-menu>',
    '<cr-menu id="sub-menu" hidden>',
    '  <cr-menu-item id="first" class="custom-appearance"></cr-menu-item>',
    '  <cr-menu-item id="second" class="custom-appearance"></cr-menu-item>',
    '</cr-menu>',
  ].join('');

  // Initialize cr.ui.Command with the <command>s.
  cr.ui.decorate('command', cr.ui.Command);
  menubutton =
      util.queryDecoratedElement('#test-menu-button', cr.ui.MultiMenuButton);
  topMenu = util.queryDecoratedElement('#menu', cr.ui.Menu);
  subMenu = util.queryDecoratedElement('#sub-menu', cr.ui.Menu);
}

/**
 * Send a 'mouseover' event to the element target of a query.
 * @param {string} targetQuery Query to specify the element.
 */
function sendMouseOver(targetQuery) {
  const event = new MouseEvent('mouseover', {
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  });
  const target = document.querySelector(targetQuery);
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 * Send a 'mouseover' event to the element target of a query.
 * @param {string} targetQuery Query to specify the element.
 * @param {number} x Position of the event in 'X'.
 * @param {number} y Position of the event in 'X'.
 */
function sendMouseOut(targetQuery, x, y) {
  const event = new MouseEvent('mouseout', {
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
    clientX: x,
    clientY: y,
  });
  const target = document.querySelector(targetQuery);
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 * Send a 'keydown' event to the element target of a query.
 * @param {string} targetQuery Query to specify the element.
 * @param {string} key property value for the key.
 */
function sendKeyDown(targetQuery, key) {
  const event = new KeyboardEvent('keydown', {
    key: key,
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  });
  const target = document.querySelector(targetQuery);
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 * Tests that making the top level menu visible doesn't
 * cause the sub-menu to become visible.
 */
function testShowMenuDoesntShowSubMenu() {
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
function testMouseOverNormalItemsDoesntShowSubMenu() {
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
function testMouseOverHostMenuShowsSubMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#host-sub-menu');
  assertFalse(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that 'mouseout' with the mouse over the top level
 * menu causes the sub-menu to hide.
 */
function testMouseoutFromHostMenuItemToHostMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#host-sub-menu');
  assertFalse(subMenu.hasAttribute('hidden'));
  // Get the location of one of our menu-items to send with the event.
  const item = document.querySelector('#default-task');
  const loc = item.getBoundingClientRect();
  sendMouseOut('#host-sub-menu', loc.left, loc.top);
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that 'mouseout' with the mouse over the sub-menu
 * doesn't hide the sub-menu.
 */
function testMouseoutFromHostMenuToSubMenu() {
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
function testSelectHostMenuItem() {
  menubutton.showMenu(true);
  topMenu.selectedIndex = 2;
  const hostItem = document.querySelector('#host-sub-menu');
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
function testSelectHostMenuItemAndCallShowSubMenu() {
  testSelectHostMenuItem();
  menubutton.showSubMenu();
  assertFalse(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that a mouse click outside of a menu and sub-menu causes
 * both menus to hide.
 */
function testClickOutsideVisibleMenuAndSubMenu() {
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
function testShrinkWindowSizesSubMenu() {
  testSelectHostMenuItemAndCallShowSubMenu();
  const subMenuPosition = subMenu.getBoundingClientRect();
  // Reduce window innerHeight so sub-menu won't fit.
  window.innerHeight = subMenuPosition.bottom - 10;
  // Navigate from sub-menu to the parent menu.
  sendKeyDown('#test-menu-button', 'ArrowLeft');
  // Call the internal hide method, then re-show it
  // to force the resizing behavior.
  menubutton.hideSubMenu_();
  menubutton.showSubMenu();
  const shrunkPosition = subMenu.getBoundingClientRect();
  assertTrue(shrunkPosition.bottom < window.innerHeight);
}

/**
 * Tests that growing the window height will increase
 * the height of the sub-menu.
 */
function testGrowWindowSizesSubMenu() {
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
  menubutton.hideSubMenu_();
  menubutton.showSubMenu();
  const grownPosition = subMenu.getBoundingClientRect();
  // Test that the height of the sub-menu is the same as
  // the height at the start of this test (before we
  // deliberately shrank it).
  assertTrue(grownPosition.bottom === subMenuPosition.bottom);
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
  document.querySelector('#default').removeAttribute('disabled');
  document.querySelector('#more').removeAttribute('disabled');
  document.querySelector('#host-sub-menu').removeAttribute('disabled');
}

/**
 * Tests that arrow navigates from main menu to sub-menu.
 */
function testNavigateFromMenuToSubMenu() {
  prepareForKeyboardNavigation();
  // Check that the hosting menu-item is not selected.
  const hostItem = document.querySelector('#host-sub-menu');
  assertFalse(hostItem.hasAttribute('selected'));
  // Check that the sub-menu has taken selection.
  const subItem = document.querySelector('#first');
  assertTrue(subItem.hasAttribute('selected'));
}

/**
 * Tests that arrow left moves back to the top level menu
 * only when the selected sub-menu item is the first one.
 */
function testNavigateFromSubMenuToParentMenu() {
  testNavigateFromMenuToSubMenu();
  // Use the arrow key to go to the next sub-menu item.
  sendKeyDown('#test-menu-button', 'ArrowDown');
  const secondItem = document.querySelector('#second');
  assertTrue(secondItem.hasAttribute('selected'));
  // Try to navigate from sub-menu to the parent menu.
  sendKeyDown('#test-menu-button', 'ArrowLeft');
  // Check that parent menu hosting item didn't get selected.
  const hostItem = document.querySelector('#host-sub-menu');
  assertFalse(hostItem.hasAttribute('selected'));
  // Check that selection is still on the sub-menu item.
  assertTrue(secondItem.hasAttribute('selected'));
  // Navigate up to the first sub-menu item.
  sendKeyDown('#test-menu-button', 'ArrowUp');
  // Check that the first sub-menu item is selected.
  const firstItem = document.querySelector('#first');
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
function testTopMenuArrowUpDismissesSubMenu() {
  prepareForKeyboardNavigation();
  // Check that the hosting menu-item is not selected.
  const hostItem = document.querySelector('#host-sub-menu');
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
function testShrinkWindowSizesTopMenu() {
  menubutton.showMenu(true);
  const menuPosition = topMenu.getBoundingClientRect();
  // Reduce window innerHeight so the menu won't fit.
  window.innerHeight = menuPosition.bottom - 10;
  // Call showMenu() which will first hide it, then re-open
  // it to force the resizing behavior.
  menubutton.showMenu(true);
  const shrunkPosition = topMenu.getBoundingClientRect();
  assertTrue(shrunkPosition.bottom < window.innerHeight);
}
