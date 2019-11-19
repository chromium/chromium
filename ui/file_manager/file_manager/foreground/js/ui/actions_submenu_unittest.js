// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

let menu = null;
let submenu = null;
let separator = null;

document.write(`
    <command id="share" label="Share"></command>
    <command id="manage-in-drive" label="Manage in Drive"></command>
    <command id="toggle-pinned" label="Toggle pinned"></command>
    <command id="unpin-folder" label="Remove folder shortcut">
    </command>

    <cr-menu id="menu">
    <hr id="actions-separator" hidden>
    </cr-menu>`);

function queryRequiredElement(selectors, opt_context) {
  const element = (opt_context || document).querySelector(selectors);
  return assertInstanceof(
      element, HTMLElement, 'Missing required element: ' + selectors);
}

function setUp() {
  menu = util.queryDecoratedElement('#menu', cr.ui.Menu);
  separator = queryRequiredElement('#actions-separator', menu);
  submenu = new ActionsSubmenu(menu);
}

function tearDown() {
  const items = document.querySelectorAll('#menu cr-menu-item');
  for (let i = 0; i < items.length; i++) {
    items[i].parentNode.removeChild(items[i]);
  }
  separator.hidden = true;
}

function testSeparator() {
  assertTrue(separator.hidden);

  submenu.setActionsModel(
      new MockActionsModel({id: new MockActionModel('title', null)}));
  assertFalse(separator.hidden);

  submenu.setActionsModel(new MockActionsModel([]));
  assertTrue(separator.hidden);
}

function testNullModel() {
  submenu.setActionsModel(
      new MockActionsModel({id: new MockActionModel('title', null)}));
  let item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);

  submenu.setActionsModel(null);
  item = menu.querySelector('cr-menu-item');
  assertFalse(!!item);
}

function testCustomActionRendering() {
  submenu.setActionsModel(
      new MockActionsModel({id: new MockActionModel('title', null)}));
  const item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);
  assertEquals('title', item.textContent);
  assertEquals(null, item.command);
}

function testCommandActionRendering() {
  submenu.setActionsModel(new MockActionsModel(
      {SHARE: new MockActionModel('share with me!', null)}));
  const item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);
  assertEquals('Share', item.textContent);
  assertEquals('share', item.command.id);
}
