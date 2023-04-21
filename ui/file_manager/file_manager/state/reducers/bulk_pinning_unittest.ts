// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {updateBulkPinProgress} from '../actions/bulk_pinning.js';
import {setupStore, waitDeepEquals} from '../for_tests.js';

/**
 * Tests that bulk pin progress updates the store and overwrites existing values
 * on each update.
 */
export async function testUpdateBulkPinProgress(done: () => void) {
  const bulkPinProgress: chrome.fileManagerPrivate.BulkPinProgress = {
    stage: chrome.fileManagerPrivate.BulkPinStage.STOPPED,
    freeSpaceBytes: 100,
    requiredSpaceBytes: 100,
    bytesToPin: 100,
    pinnedBytes: 100,
    filesToPin: 100,
  };

  // Dispatch an action to update bulk pin progress.
  const store = setupStore();
  store.dispatch(updateBulkPinProgress(bulkPinProgress));

  // Expect the bulk pin progress to be updated.
  waitDeepEquals(store, bulkPinProgress, (state) => state.bulkPinning);

  // Dispatch another action to change the stage to `SYNCING`.
  bulkPinProgress.stage = chrome.fileManagerPrivate.BulkPinStage.SYNCING;
  store.dispatch(updateBulkPinProgress(bulkPinProgress));

  // Expect the bulk pin progress to equal the new state.
  waitDeepEquals(store, bulkPinProgress, (state) => state.bulkPinning);

  done();
}
