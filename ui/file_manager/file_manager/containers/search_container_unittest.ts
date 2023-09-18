// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

import {waitUntil} from '../common/js/test_error_reporting.js';
import {EntryLocation} from '../externs/entry_location.js';
import {State} from '../externs/ts/state.js';
import {VolumeManager} from '../externs/volume_manager.js';
import {waitDeepEquals} from '../state/for_tests.js';
import {getEmptyState, getStore, type Store} from '../state/store.js';

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
  // The following setup is needed to generate transitionend events in the input
  // element. Currently, openSearch transitions to open state only on transition
  // end.
  document.body.innerHTML = getTrustedHTML`
    <style>
      #root {
        display: flex;
        flex-direction: row;
      }
      #filler {
        flex-grow: 1;
      }
      #search-box cr-input {
        transition: width 10ms ease;
        width: 50px;
      }
      #search-box.has-text cr-input {
        width: 200px;
      }
      #search-wrapper {
        width: 10px;
        display: flex;
        flex-direction: row;
      }
      #search-wrapper.has-text {
        width: 400px;
      }
    </style>
    <div id="root">
      <div id="filler"></div>
    </div>
  `;
  const root = document.querySelector('#root')!;
  searchWrapper.innerHTML = getTrustedHTML`
      <cr-button id="search-button" tabindex="0">Open</cr-button>
      <div id="search-box">
        <cr-input type="search" disabled placeholder="Search">
          <cr-button class="clear" slot="suffix" tabindex="0" has-tooltip>
            Search
          </cr-button>
        </cr-input>
      </div>`;
  searchWrapper.id = 'search-wrapper';
  searchWrapper.toggleAttribute('collapsed');
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

/**
 * Resets flags state.
 */
export function tearDown() {
  loadTimeData.resetForTesting();
}

export async function testQueryUpdated() {
  // Manually open the search container; without this the container is in the
  // closed state and does not clean up query or option values on close.
  searchContainer!.openSearch();
  // Wait for search to open. Otherwise closing of search does not work.
  await waitUntil(() => {
    return !searchWrapper.hasAttribute('collapsed');
  });

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
  // Wait for search to close.
  await waitUntil(() => {
    return searchWrapper.hasAttribute('collapsed');
  });
  const want2 = {
    query: undefined,
    status: undefined,
    options: undefined,
  };
  await waitDeepEquals(store!, want2, (state: State) => {
    return state.search;
  });
}

// TODO(b:241868453): Add test for V2
