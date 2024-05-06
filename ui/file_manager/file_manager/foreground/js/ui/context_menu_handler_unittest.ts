// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';

import type {HideEvent, ShowEvent} from './context_menu_handler.js';
import {contextMenuHandler} from './context_menu_handler.js';
import {Menu} from './menu.js';

export async function testShowAndHideEvents() {
  // Keep original Date.now not to affect other code.
  const originalDateNow = Date.now;

  // Initial value is 1 since 0 becomes false.
  let currentTime = 1;

  // Overrides Date.now to simulate time.
  Date.now = function() {
    return currentTime;
  };

  const cmh = contextMenuHandler;

  // Create context menu.
  const rawDiv = document.createElement('div');
  const menu = crInjectTypeAndInit(rawDiv, Menu);
  document.body.appendChild(menu);

  menu.addMenuItem();

  // Create target elements.
  const elem1 = document.createElement('div');
  const elem2 = document.createElement('div');

  cmh.setContextMenu(elem1, menu);
  cmh.setContextMenu(elem2, menu);

  const events: Array<ShowEvent|HideEvent> = [];
  cmh.addEventListener('show', (e) => {
    events.push(e);
  });
  cmh.addEventListener('hide', (e) => {
    events.push(e);
  });

  // Show context menu of elem1.
  elem1.dispatchEvent(new MouseEvent('contextmenu'));
  assertEquals('show', events[0]!.type);
  assertEquals(elem1, events[0]!.detail.element);
  assertEquals(menu, events[0]!.detail.menu);

  // Show context menu of elem2.
  document.dispatchEvent(new MouseEvent('mousedown'));

  // On Windows to prevent context menu show again by mouse right button up,
  // we need to wait at least 50ms from the last hide of context menu.
  currentTime += 51;  // ms

  elem2.dispatchEvent(new MouseEvent('contextmenu'));
  assertEquals(3, events.length);
  assertEquals('hide', events[1]!.type);
  assertEquals(elem1, events[1]!.detail.element);
  assertEquals(menu, events[1]!.detail.menu);
  assertEquals('show', events[2]!.type);
  assertEquals(elem2, events[2]!.detail.element);
  assertEquals(menu, events[2]!.detail.menu);

  Date.now = originalDateNow;
}

/**
 * Tests that a keydown event that is not intended for the context menu will not
 * be consumed by the context menu.
 */
export async function testContextMenuDoesNotConsumeNonMenuEvent() {
  let eventConsumedByMenu = true;
  const contextMenuEventKey = 'ArrowDown';
  const nonContextMenuEventKey = 'AudioVolumeUp';

  // Create context menu.
  const rawDiv = document.createElement('div');
  const menu = crInjectTypeAndInit(rawDiv, Menu);
  document.body.appendChild(menu);

  // Show context menu.
  const elem = document.createElement('div');
  contextMenuHandler.setContextMenu(elem, menu);
  document.dispatchEvent(new MouseEvent('contextmenu'));

  // Only the event that is ignored by the context menu is received by
  // `document.body`.
  document.body.addEventListener('keydown', e => {
    eventConsumedByMenu = false;
    // Check this is the right keydown event.
    assertEquals(nonContextMenuEventKey, e.key);
    // Confirm this is not a menu event.
    assertFalse(contextMenuHandler.isMenuEvent(e));
  });

  // Send a keydown event expected to be consumed by the context menu.
  const contextMenuKeyDownEvent = new KeyboardEvent('keydown', {
    key: contextMenuEventKey,
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  });
  menu.dispatchEvent(contextMenuKeyDownEvent);
  assertTrue(eventConsumedByMenu);

  // Send a keydown event expected to be ignored by the context menu.
  const nonContextMenuKeyDownEvent = new KeyboardEvent('keydown', {
    key: nonContextMenuEventKey,
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  });
  menu.dispatchEvent(nonContextMenuKeyDownEvent);
  assertFalse(eventConsumedByMenu);
}
