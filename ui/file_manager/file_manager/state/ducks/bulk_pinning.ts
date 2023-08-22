// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {addReducer, BaseAction, Reducer, ReducersMap} from '../../lib/base_store.js';
import {Action, ActionType} from '../actions.js';

/**
 * Actions and reducers for Bulk Pinning.
 *
 * BulkPinProgress is the current state of files that are being pinned when the
 * BulkPinning feature is enabled. During bulk pinning, all the users items in
 * My drive are pinned and kept available offline. This tracks the progress of
 * both the initial operation and any subsequent updates along with any error
 * states that may occur.
 *
 * This file is checked via TS, so we suppress Closure checks.
 * @suppress {checkTypes}
 */

/** Map of actions to reducers for the bulk pinning slice. */
export const bulkPinningReducersMap: ReducersMap<State, Action> = new Map();

/** Action to update the bulk pin progress to the store. */
export interface UpdateBulkPinProgressAction extends BaseAction {
  type: ActionType.UPDATE_BULK_PIN_PROGRESS;
  payload: chrome.fileManagerPrivate.BulkPinProgress;
}

const updateBulkPinningReducer =
    (currentState: State,
     bulkPinning: UpdateBulkPinProgressAction['payload']) => ({
      ...currentState,
      bulkPinning,
    });

/** Action factory to update the bulk pin progress to the store. */
export const updateBulkPinProgress = addReducer(
    ActionType.UPDATE_BULK_PIN_PROGRESS,
    updateBulkPinningReducer as Reducer<State, Action>, bulkPinningReducersMap);
