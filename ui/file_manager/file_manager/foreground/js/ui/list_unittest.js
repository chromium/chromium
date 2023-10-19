// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {decorate} from '../../../common/js/ui.js';

import {ArrayDataModel} from '../../../common/js/array_data_model.js';

import {List} from './list.js';

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

// clang-format on

export function testClearPinnedItem() {
  const list = document.createElement('ul');
  list.style.position = 'absolute';
  list.style.width = '800px';
  list.style.height = '800px';
  decorate(list, List);
  document.body.appendChild(list);

  const model = new ArrayDataModel(['Item A', 'Item B']);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLUListElement'.
  list.dataModel = model;
  // @ts-ignore: error TS2339: Property 'selectionModel' does not exist on type
  // 'HTMLUListElement'.
  list.selectionModel.setIndexSelected(0, true);
  // @ts-ignore: error TS2339: Property 'selectionModel' does not exist on type
  // 'HTMLUListElement'.
  list.selectionModel.leadIndex = 0;
  // @ts-ignore: error TS2339: Property 'ensureLeadItemExists' does not exist on
  // type 'HTMLUListElement'.
  list.ensureLeadItemExists();

  list.style.height = '0px';
  model.splice(0, 1);

  list.style.height = '800px';
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLUListElement'.
  list.redraw();
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('Item B', list.querySelectorAll('li')[0].textContent);
}

export function testClickOutsideListItem() {
  const list = document.createElement('ul');
  list.style.position = 'absolute';
  list.style.width = '800px';
  list.style.height = '800px';
  decorate(list, List);
  document.body.appendChild(list);

  // Add a header inside the list.
  const header = document.createElement('h1');
  header.innerText = 'Title inside the list';
  list.appendChild(header);

  const model = new ArrayDataModel(['Item A', 'Item B']);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLUListElement'.
  list.dataModel = model;

  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLUListElement'.
  list.redraw();

  const item = list.querySelector('li');
  const span = document.createElement('span');
  span.innerText = 'some text';
  // @ts-ignore: error TS18047: 'item' is possibly 'null'.
  item.appendChild(span);

  // Non-LI children should return null.
  // @ts-ignore: error TS2339: Property 'getListItemAncestor' does not exist on
  // type 'HTMLUListElement'.
  assertEquals(null, list.getListItemAncestor(header));

  // It should return null for the list itself.
  // @ts-ignore: error TS2339: Property 'getListItemAncestor' does not exist on
  // type 'HTMLUListElement'.
  assertEquals(null, list.getListItemAncestor(list));

  // Anything inside a LI should return the LI itself.
  // @ts-ignore: error TS2339: Property 'getListItemAncestor' does not exist on
  // type 'HTMLUListElement'.
  assertEquals(item, list.getListItemAncestor(item));
  // @ts-ignore: error TS2339: Property 'getListItemAncestor' does not exist on
  // type 'HTMLUListElement'.
  assertEquals(item, list.getListItemAncestor(span));
}
