// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchData, State} from '../../externs/ts/state.js';
import {SearchAction} from '../actions.js';

export function search(state: State, action: SearchAction): State {
  const payload = action.payload;
  const blankSearch = {
    query: undefined,
    status: undefined,
    options: undefined,
  };
  // Special case: if none of the fields are set, the action clears the search
  // state in the store.
  if (Object.values(payload).every(field => field === undefined)) {
    return {
      ...state,
      search: blankSearch,
    };
  }
  const currentSearch = state.search || blankSearch;
  // Create a clone of current search. We must not modify the original object,
  // as store customers are free to cache it and check for changes. If we modify
  // the original object the check for changes incorrectly return false.
  const search: SearchData = {...currentSearch};
  if (payload.query !== undefined) {
    search.query = payload.query;
  }
  if (payload.status !== undefined) {
    search.status = payload.status;
  }
  if (payload.options !== undefined) {
    search.options = {...payload.options};
  }
  return {...state, search};
}
