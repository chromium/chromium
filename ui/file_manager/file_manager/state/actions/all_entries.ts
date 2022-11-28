// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';

/**
 * Processes the allEntries and removes any entry that isn't in use any more.
 */
export interface ClearStaleCachedEntriesAction extends BaseAction {
  type: ActionType.CLEAR_STALE_CACHED_ENTRIES;
  payload?: undefined;
}
