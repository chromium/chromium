// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';

/**
 * Actions for Bulk Pinning.
 *
 * BulkPinProgress is the current state of files that are being pinned when the
 * BulkPinning feature is enabled. During bulk pinning, all the users items in
 * My drive are pinned and kept available offline. This tracks the progress of
 * both the initial operation and any subsequent updates along with any error
 * states that may occur.
 */

/** Action to update the bulk pin progress to the store. */
export interface UpdateBulkPinProgressAction extends BaseAction {
  type: ActionType.UPDATE_BULK_PIN_PROGRESS;
  payload: chrome.fileManagerPrivate.BulkPinProgress;
}

/** Action factory to update the bulk pin progress to the store. */
export function updateBulkPinProgress(
    payload: UpdateBulkPinProgressAction['payload']):
    UpdateBulkPinProgressAction {
  return {
    type: ActionType.UPDATE_BULK_PIN_PROGRESS,
    payload,
  };
}
