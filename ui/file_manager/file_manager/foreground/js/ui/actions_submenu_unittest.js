// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {queryDecoratedElement, queryRequiredElement} from '../../../common/js/dom_utils.js';
import {MockActionModel, MockActionsModel} from '../mock_actions_model.js';

import {ActionsSubmenu} from './actions_submenu.js';
import {Menu} from './menu.js';

// @ts-ignore: error TS7034: Variable 'menu' implicitly has type 'any' in some
// locations where its type cannot be determined.
let menu = null;
// @ts-ignore: error TS7034: Variable 'submenu' implicitly has type 'any' in
// some locations where its type cannot be determined.
let submenu = null;
// @ts-ignore: error TS7034: Variable 'separator' implicitly has type 'any' in
// some locations where its type cannot be determined.
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
  // @ts-ignore: error TS2345: Argument of type 'typeof Menu' is not assignable
  // to parameter of type 'new (...args: any) => Menu'.
  menu = queryDecoratedElement('#menu', Menu);
  // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable to
  // parameter of type 'Document | Element | HTMLElement | DocumentFragment |
  // undefined'.
  separator = queryRequiredElement('#actions-separator', menu);
  submenu = new ActionsSubmenu(menu);
}

export function testSeparator() {
  // @ts-ignore: error TS7005: Variable 'separator' implicitly has an 'any'
  // type.
  assertTrue(separator.hidden);

  // @ts-ignore: error TS7005: Variable 'submenu' implicitly has an 'any' type.
  submenu.setActionsModel(
      // @ts-ignore: error TS2345: Argument of type 'null' is not assignable to
      // parameter of type 'FileSystemEntry[]'.
      new MockActionsModel({id: new MockActionModel('title', null)}));
  // @ts-ignore: error TS7005: Variable 'separator' implicitly has an 'any'
  // type.
  assertFalse(separator.hidden);

  // @ts-ignore: error TS7005: Variable 'submenu' implicitly has an 'any' type.
  submenu.setActionsModel(new MockActionsModel([]));
  // @ts-ignore: error TS7005: Variable 'separator' implicitly has an 'any'
  // type.
  assertTrue(separator.hidden);
}

export function testNullModel() {
  // @ts-ignore: error TS7005: Variable 'submenu' implicitly has an 'any' type.
  submenu.setActionsModel(
      // @ts-ignore: error TS2345: Argument of type 'null' is not assignable to
      // parameter of type 'FileSystemEntry[]'.
      new MockActionsModel({id: new MockActionModel('title', null)}));
  // @ts-ignore: error TS7005: Variable 'menu' implicitly has an 'any' type.
  let item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);

  // @ts-ignore: error TS7005: Variable 'submenu' implicitly has an 'any' type.
  submenu.setActionsModel(null);
  // @ts-ignore: error TS7005: Variable 'menu' implicitly has an 'any' type.
  item = menu.querySelector('cr-menu-item');
  assertFalse(!!item);
}

export function testCustomActionRendering() {
  // @ts-ignore: error TS7005: Variable 'submenu' implicitly has an 'any' type.
  submenu.setActionsModel(
      // @ts-ignore: error TS2345: Argument of type 'null' is not assignable to
      // parameter of type 'FileSystemEntry[]'.
      new MockActionsModel({id: new MockActionModel('title', null)}));
  // @ts-ignore: error TS7005: Variable 'menu' implicitly has an 'any' type.
  const item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);
  assertEquals('title', item.textContent);
  assertEquals(null, item.command);
}

export function testCommandActionRendering() {
  // @ts-ignore: error TS7005: Variable 'submenu' implicitly has an 'any' type.
  submenu.setActionsModel(new MockActionsModel(
      // @ts-ignore: error TS2345: Argument of type 'null' is not assignable to
      // parameter of type 'FileSystemEntry[]'.
      {SHARE: new MockActionModel('share with me!', null)}));
  // @ts-ignore: error TS7005: Variable 'menu' implicitly has an 'any' type.
  const item = menu.querySelector('cr-menu-item');
  assertTrue(!!item);
  assertEquals('Share', item.textContent);
  assertEquals('share', item.command.id);
}
