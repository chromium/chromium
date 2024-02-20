// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';


/**
 * Tests restoring the sorting order.
 */
export async function restoreSortColumn() {
  const EXPECTED_FILES = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,     // 'photos' (directory)
    ENTRIES.world,      // 'world.ogv', 56758 bytes
    ENTRIES.beautiful,  // 'Beautiful Song.ogg', 13410 bytes
    ENTRIES.desktop,    // 'My Desktop Background.png', 272 bytes
    ENTRIES.hello,      // 'hello.txt', 51 bytes
  ]);

  // Set up Files app.
  let appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Sort by name.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(1)']);

  // Check the sorted style of the header.
  const iconSortedAsc =
      '.table-header-cell .sorted [iron-icon="files16:arrow_up_small"]';
  await remoteCall.waitForElement(appId, iconSortedAsc);

  // Sort by size (in descending order).
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(2)']);

  // Check the sorted style of the header.
  const iconSortedDesc =
      '.table-header-cell .sorted [iron-icon="files16:arrow_down_small"]';
  await remoteCall.waitForElement(appId, iconSortedDesc);

  // Check the sorted files.
  await remoteCall.waitForFiles(appId, EXPECTED_FILES, {orderCheck: true});

  // Open another window, where the sorted column should be restored.
  appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check the sorted style of the header.
  await remoteCall.waitForElement(appId, iconSortedDesc);

  // Check the sorted files.
  await remoteCall.waitForFiles(appId, EXPECTED_FILES, {orderCheck: true});
}

/**
 * Tests restoring the current view (the file list or the thumbnail grid).
 */
export async function restoreCurrentView() {
  // Set up Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check the initial view.
  await remoteCall.waitForElement(appId, '.thumbnail-grid[hidden]');

  // Change the current view.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#view-button']));

  // Check the new current view.
  await remoteCall.waitForElement(appId, '.detail-table[hidden]');

  // Open another window, where the current view is restored.
  const appId2 = await remoteCall.openNewWindow(RootPath.DOWNLOADS);

  // Check the current view.
  await remoteCall.waitForElement(appId2, '.detail-table[hidden]');
}
