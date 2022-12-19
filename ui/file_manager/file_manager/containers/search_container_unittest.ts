// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {EntryLocation} from '../externs/entry_location.js';
import {State} from '../externs/ts/state.js';
import {VolumeManager} from '../externs/volume_manager.js';
import {waitDeepEquals} from '../state/for_tests.js';
import {getEmptyState, getStore, Store} from '../state/store.js';

import {SearchContainer} from './search_container.js';


const searchWrapper: HTMLElement = document.createElement('div');
const optionsContainer: HTMLElement = document.createElement('div');
const pathContainer: HTMLElement = document.createElement('div');

let store: Store|undefined;
let searchContainer: SearchContainer|undefined;

function setupStore(): Store {
  const store = getStore();
  store.init(getEmptyState());
  return store;
}

/**
 * Creates new <search-container> element for each test.
 */
export function setUp() {
  const root = document.createElement('div');
  document.body.replaceChildren(root);
  searchWrapper.innerHTML = `
      <cr-button id="search-button" tabindex="0">
        <div class="icon"></div>
      </cr-button>
      <div id="search-box">
        <cr-input type="search" disabled placeholder="Search">
          <cr-button class="clear" slot="suffix" tabindex="0" has-tooltip>
            <div class="icon"></div>
          </cr-button>
        </cr-input>
      </div>`;
  root.appendChild(searchWrapper);
  root.appendChild(optionsContainer);
  root.appendChild(pathContainer);

  store = setupStore();
  const volumeManager: VolumeManager = {
    getLocationInfo: (_entry: Entry): EntryLocation => {
      return new EntryLocation();
    },
  } as unknown as VolumeManager;

  searchContainer = new SearchContainer(
      volumeManager, searchWrapper, optionsContainer, pathContainer);
}

export async function testQueryUpdated() {
  // Test 1: Enter a query.
  const input = searchWrapper.querySelector('cr-input') as CrInputElement;
  input.value = 'hello';
  input.dispatchEvent(new Event('input', {
    bubbles: true,
    cancelable: true,
  }));
  const want1 = {
    query: 'hello',
    status: undefined,
    options: undefined,
  };
  await waitDeepEquals(store!, want1, (state: State) => {
    return state.search;
  });

  // Test 2: Clear the query.
  searchContainer!.clear();
  const want2 = {
    query: '',
    status: undefined,
    options: undefined,
  };
  await waitDeepEquals(store!, want2, (state: State) => {
    return state.search;
  });
}
