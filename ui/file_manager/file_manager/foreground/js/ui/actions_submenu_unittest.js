// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {queryDecoratedElement, queryRequiredElement} from '../../../common/js/dom_utils.js';
import {util} from '../../../common/js/util.js';
import {MockActionModel, MockActionsModel} from '../mock_actions_model.js';

import {ActionsSubmenu} from './actions_submenu.js';
import {Menu} from './menu.js';

let menu = null;
let submenu = null;
let separator = null;

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
      new MockActionsModel({id: new MockActionModel('title', null)}));
  assertFalse(separator.hidden);

  submenu.setActionsModel(new MockActionsModel([]));
  assertTrue(separator.hidden);
}

export function testNullModel() {
  submenu.setActionsModel(
      new MockActionsModel({id: new MockActionModel('title', null)}));
  let item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);

  submenu.setActionsModel(null);
  item = menu.querySelector('cr-menu-item');
  assertFalse(!!item);
}

export function testCustomActionRendering() {
  submenu.setActionsModel(
      new MockActionsModel({id: new MockActionModel('title', null)}));
  const item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);
  assertEquals('title', item.textContent);
  assertEquals(null, item.command);
}

export function testCommandActionRendering() {
  submenu.setActionsModel(new MockActionsModel(
      {SHARE: new MockActionModel('share with me!', null)}));
  const item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);
  assertEquals('Share', item.textContent);
  assertEquals('share', item.command.id);
}
