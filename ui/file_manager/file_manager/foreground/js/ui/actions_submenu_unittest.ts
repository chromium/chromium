// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {queryDecoratedElement, queryRequiredElement} from '../../../common/js/dom_utils.js';
import type {ActionsModel} from '../actions_model.js';
import {MockActionModel, MockActionsModel} from '../mock_actions_model.js';

import {ActionsSubmenu} from './actions_submenu.js';
import {Menu} from './menu.js';
import type {MenuItem} from './menu_item.js';

let menu: Menu;
let submenu: ActionsSubmenu;
let separator: HTMLElement;

export function setUp() {
  document.body.innerHTML = getTrustedHTML`
      <command id="share" label="Share"></command>
      <command id="manage-in-drive" label="Manage in Drive"></command>
      <command id="toggle-pinned" label="Toggle pinned"></command>
      <command id="unpin-folder" label="Remove folder shortcut">
      </command>

      <cr-menu id="menu">
      <hr id="actions-separator" hidden>
      </cr-menu>`;
  menu = queryDecoratedElement('#menu', Menu);
  separator = queryRequiredElement('#actions-separator', menu);
  submenu = new ActionsSubmenu(menu);
}

export function testSeparator() {
  assertTrue(separator.hidden);

  submenu.setActionsModel(
      new MockActionsModel({id: new MockActionModel('title')}) as unknown as
      ActionsModel);
  assertFalse(separator.hidden);

  submenu.setActionsModel(new MockActionsModel() as unknown as ActionsModel);
  assertTrue(separator.hidden);
}

export function testNullModel() {
  submenu.setActionsModel(
      new MockActionsModel({id: new MockActionModel('title')}) as unknown as
      ActionsModel);
  let item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);

  submenu.setActionsModel(null);
  item = menu.querySelector('cr-menu-item');
  assertFalse(!!item);
}

export function testCustomActionRendering() {
  submenu.setActionsModel(
      new MockActionsModel({id: new MockActionModel('title')}) as unknown as
      ActionsModel);
  const item = menu.querySelector<MenuItem>('cr-menu-item')!;
  assertTrue(!!item);
  assertEquals('title', item.textContent);
  assertEquals(null, item.command);
}

export function testCommandActionRendering() {
  submenu.setActionsModel(
      new MockActionsModel(
          {SAVE_FOR_OFFLINE: new MockActionModel('save for offline!')}) as
      unknown as ActionsModel);
  const item = menu.querySelector<MenuItem>('cr-menu-item')!;
  assertTrue(!!item);
  assertEquals('Toggle pinned', item.textContent);
  assertEquals('toggle-pinned', item.command?.id);
}
