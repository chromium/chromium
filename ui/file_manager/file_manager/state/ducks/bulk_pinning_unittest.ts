// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {setupStore} from '../for_tests.js';

import {updateBulkPinProgress} from './bulk_pinning.js';

/**
 * Tests that bulk pin progress updates the store and overwrites existing values
 * on each update.
 */
export async function testUpdateBulkPinProgress(done: () => void) {
  const want: chrome.fileManagerPrivate.BulkPinProgress = {
    stage: chrome.fileManagerPrivate.BulkPinStage.STOPPED,
    freeSpaceBytes: 100,
    requiredSpaceBytes: 100,
    bytesToPin: 100,
    pinnedBytes: 100,
    filesToPin: 100,
    remainingSeconds: 500,
    shouldPin: true,
    emptiedQueue: false,
    listedFiles: 100,
  };

  // Dispatch an action to update bulk pin progress.
  const store = setupStore();
  store.dispatch(updateBulkPinProgress(want));

  // Expect the bulk pin progress to be updated.
  const firstState = store.getState().bulkPinning;
  assertDeepEquals(
      want, firstState,
      `1. ${JSON.stringify(want)} !== ${JSON.stringify(firstState)}`);

  // Dispatch another action to change the stage to `SYNCING`.
  want.stage = chrome.fileManagerPrivate.BulkPinStage.SYNCING;
  store.dispatch(updateBulkPinProgress(want));

  // Expect the bulk pin progress to equal the new state.
  const secondState = store.getState().bulkPinning;
  assertDeepEquals(
      want, secondState,
      `2. ${JSON.stringify(want)} !== ${JSON.stringify(secondState)}`);

  done();
}
