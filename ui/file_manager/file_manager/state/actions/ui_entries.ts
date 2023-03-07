// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {FileKey} from '../../externs/ts/state.js';
import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';

/**
 * Actions for UI entries.
 *
 * UI entries represents entries shown on UI only (aka FakeEntry, e.g.
 * Recents/Trash/Google Drive wrapper), they don't have a real entry backup in
 * the file system.
 */


/** Action add single UI entry into the store. */
export interface AddUiEntryAction extends BaseAction {
  type: ActionType.ADD_UI_ENTRY;
  payload: {
    entry: FakeEntryImpl,
  };
}

/** Action remove single UI entry from the store. */
export interface RemoveUiEntryAction extends BaseAction {
  type: ActionType.REMOVE_UI_ENTRY;
  payload: {
    key: FileKey,
  };
}

/** Action factory to add single UI entry into the store. */
export function addUiEntry(payload: AddUiEntryAction['payload']):
    AddUiEntryAction {
  return {
    type: ActionType.ADD_UI_ENTRY,
    payload,
  };
}

/** Action factory to remove single UI entry from the store. */
export function removeUiEntry(payload: RemoveUiEntryAction['payload']):
    RemoveUiEntryAction {
  return {
    type: ActionType.REMOVE_UI_ENTRY,
    payload,
  };
}
