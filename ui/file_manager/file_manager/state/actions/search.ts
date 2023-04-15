// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchData} from '../../externs/ts/state.js';
import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';


/** Action to update the search state. */
export interface SearchAction extends BaseAction {
  type: ActionType.SEARCH;
  payload: SearchData;
}

/**
 * Generates a search action based on the supplied data.
 * Query, status and options can be adjusted independently of each other.
 */
export function updateSearch(data: SearchData): SearchAction {
  return {
    type: ActionType.SEARCH,
    payload: {
      query: data.query,
      status: data.status,
      options: data.options,
    },
  };
}

/**
 * Clears all search settings.
 */
export function clearSearch(): SearchAction {
  return {
    type: ActionType.SEARCH,
    payload: {
      query: undefined,
      status: undefined,
      options: undefined,
    },
  };
}
