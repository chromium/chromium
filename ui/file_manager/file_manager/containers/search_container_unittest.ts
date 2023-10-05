// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

import {EntryLocation} from '../externs/entry_location.js';
import {State} from '../externs/ts/state.js';
import {VolumeManager} from '../externs/volume_manager.js';
import {waitDeepEquals} from '../state/for_tests.js';
import {getEmptyState, getStore, type Store} from '../state/store.js';

import {SearchContainer} from './search_container.js';


let searchWrapper: HTMLElement|undefined;
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
  document.body.innerHTML = getTrustedHTML`
    <div id="root">
      <div id="search-wrapper" collapsed>
        <cr-button id="search-button" tabindex="0">Open</cr-button>
        <div id="search-box">
          <cr-input type="search" disabled placeholder="Search">
            <cr-button class="clear" slot="suffix" tabindex="0" has-tooltip>
              Search
            </cr-button>
          </cr-input>
        </div>
        <div id="options-container"></div>
        <div id="path-container"></div>
      </div>
    </div>`;
  searchWrapper = document.querySelector('#search-wrapper') as HTMLElement;

  store = setupStore();
  const volumeManager: VolumeManager = {
    getLocationInfo: (_entry: Entry): EntryLocation => {
      return new EntryLocation();
    },
  } as unknown as VolumeManager;

  searchContainer = new SearchContainer(
      volumeManager, searchWrapper,
      document.querySelector('#options-container') as HTMLElement,
      document.querySelector('#path-container') as HTMLElement);
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

  // Test 1: Enter a query.
  const input = searchWrapper!.querySelector('cr-input') as CrInputElement;
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
    query: undefined,
    status: undefined,
    options: undefined,
  };
  await waitDeepEquals(store!, want2, (state: State) => {
    return state.search;
  });
}

// TODO(b:241868453): Add test for V2
