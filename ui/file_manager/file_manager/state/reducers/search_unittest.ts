// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';

import {PropStatus, SearchData, SearchFileType, SearchLocation, SearchRecency} from '../../externs/ts/state.js';
import {clearSearch, updateSearch} from '../actions.js';
import {getEmptyState, getStore, Store} from '../store.js';

let store: Store;

export function setUp() {
  store = getStore();
  store.init(getEmptyState());
}

export function testSearchAction() {
  const firstState = store.getState().search;
  const want: SearchData = {
    query: undefined,
    status: undefined,
    options: undefined,
  };
  assertDeepEquals(
      want, firstState,
      `${JSON.stringify(want)} != ${JSON.stringify(firstState)}`);

  // Change the options only.
  const currentOptions = {
    location: SearchLocation.THIS_FOLDER,
    recency: SearchRecency.ANYTIME,
    type: SearchFileType.ALL_TYPES,
  };
  store.dispatch(updateSearch({
    query: undefined,
    status: undefined,
    options: currentOptions,
  }));

  // Checks that the search action mutated only options.
  want.options = currentOptions;
  const secondState = store.getState().search;
  assertDeepEquals(
      want, secondState,
      `${JSON.stringify(want)} != ${JSON.stringify(secondState)}`);
  // Check that changing options does not mutate firstState.
  assertFalse(firstState === secondState);

  // Change query and status, and verify that options did not change.
  want.query = 'query';
  want.status = PropStatus.STARTED;
  store.dispatch(updateSearch({
    query: want.query,
    status: want.status,
    options: undefined,
  }));
  const thirdState = store.getState().search;
  assertDeepEquals(
      want, thirdState,
      `${JSON.stringify(want)} != ${JSON.stringify(thirdState)}`);

  // Clear search.
  store.dispatch(clearSearch());
  const fourthState = store.getState().search;
  want.query = undefined;
  want.status = undefined;
  want.options = undefined;
  assertDeepEquals(
      want, fourthState,
      `${JSON.stringify(want)} != ${JSON.stringify(fourthState)}`);
}
