// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {PropStatus, type SearchData, SearchLocation, SearchRecency} from '../../state/state.js';
import {getEmptyState, getStore, type Store} from '../store.js';

import {clearSearch, updateSearch} from './search.js';

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
      `1. ${JSON.stringify(want)} !== ${JSON.stringify(firstState)}`);

  // Change the options only.
  const currentOptions = {
    location: SearchLocation.THIS_FOLDER,
    recency: SearchRecency.ANYTIME,
    fileCategory: chrome.fileManagerPrivate.FileCategory.ALL,
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
      `2. ${JSON.stringify(want)} !== ${JSON.stringify(secondState)}`);
  // Check that changing options does not mutate firstState.
  assertFalse(firstState === secondState);

  // Send the same options again, to verify that unchanged options do not change
  // the state.
  store.dispatch(updateSearch({
    query: undefined,
    status: undefined,
    options: currentOptions,
  }));
  const unchangedState = store.getState().search;
  assertTrue(unchangedState === secondState);

  // Change the options a bit to verify that one property change inside options
  // causes search state update.
  const freshRecencyOptions = {
    location: SearchLocation.THIS_FOLDER,
    recency: SearchRecency.LAST_WEEK,
    fileCategory: chrome.fileManagerPrivate.FileCategory.ALL,
  };
  store.dispatch(updateSearch({
    query: undefined,
    status: undefined,
    options: freshRecencyOptions,
  }));

  want.options = freshRecencyOptions;
  const freshRecencyOptionsState = store.getState().search;
  assertDeepEquals(
      want, freshRecencyOptionsState,
      `3. ${JSON.stringify(want)} !== ${
          JSON.stringify(freshRecencyOptionsState)}`);
  // Check that changing options does not mutate firstState.
  assertFalse(unchangedState === freshRecencyOptionsState);

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
      `4. ${JSON.stringify(want)} !== ${JSON.stringify(thirdState)}`);

  // Clear search.
  store.dispatch(clearSearch());
  const fourthState = store.getState().search;
  want.query = undefined;
  want.status = undefined;
  want.options = undefined;
  assertDeepEquals(
      want, fourthState,
      `${JSON.stringify(want)} !== ${JSON.stringify(fourthState)}`);
}
