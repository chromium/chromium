// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {decorate} from '../../../common/js/ui.js';
import {contextMenuHandler} from './context_menu_handler.js';
import {Menu} from './menu.js';

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

// clang-format on

export function testShowAndHideEvents() {
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
  const menu = document.createElement('div');
  decorate(menu, Menu);
  document.body.appendChild(menu);

  const menuItem = document.createElement('div');
  menu.addMenuItem(menuItem);

  // Create target elements.
  const elem1 = document.createElement('div');
  const elem2 = document.createElement('div');

  cmh.setContextMenu(elem1, menu);
  cmh.setContextMenu(elem2, menu);

  const events = [];
  cmh.addEventListener('show', function(e) {
    events.push(e);
  });
  cmh.addEventListener('hide', function(e) {
    events.push(e);
  });

  // Show context menu of elem1.
  elem1.dispatchEvent(new MouseEvent('contextmenu'));
  assertEquals(1, events.length);
  assertEquals('show', events[0].type);
  assertEquals(elem1, events[0].element);
  assertEquals(menu, events[0].menu);

  // Show context menu of elem2.
  document.dispatchEvent(new MouseEvent('mousedown'));

  // On Windows to prevent context menu show again by mouse right button up,
  // we need to wait at least 50ms from the last hide of context menu.
  currentTime += 51;  // ms

  elem2.dispatchEvent(new MouseEvent('contextmenu'));
  assertEquals(3, events.length);
  assertEquals('hide', events[1].type);
  assertEquals(elem1, events[1].element);
  assertEquals(menu, events[1].menu);
  assertEquals('show', events[2].type);
  assertEquals(elem2, events[2].element);
  assertEquals(menu, events[2].menu);

  Date.now = originalDateNow;
}
