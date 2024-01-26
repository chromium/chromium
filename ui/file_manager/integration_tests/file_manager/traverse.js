// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {openNewWindow, remoteCall} from './background.js';
import {NESTED_ENTRY_SET} from './test_data.js';

/**
 * Test utility for traverse tests.
 * @param {string} path Root path to be traversed.
 */
async function traverseDirectories(path) {
  // Open Files app. Do not add initial files.
  const appId = await openNewWindow(path);

  // Check the initial view.
  await remoteCall.waitForElement(appId, '#detail-table');
  await addEntries(['local', 'drive'], NESTED_ENTRY_SET);
  // @ts-ignore: error TS4111: Property 'directoryA' comes from an index
  // signature, so it must be accessed with ['directoryA'].
  await remoteCall.waitForFiles(appId, [ENTRIES.directoryA.getExpectedRow()]);

  // Open the directory
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['A']));

  // Check the contents of current directory.
  // @ts-ignore: error TS4111: Property 'directoryB' comes from an index
  // signature, so it must be accessed with ['directoryB'].
  await remoteCall.waitForFiles(appId, [ENTRIES.directoryB.getExpectedRow()]);

  // Open the directory
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['B']));

  // Check the contents of current directory.
  // @ts-ignore: error TS4111: Property 'directoryC' comes from an index
  // signature, so it must be accessed with ['directoryC'].
  await remoteCall.waitForFiles(appId, [ENTRIES.directoryC.getExpectedRow()]);
}

/**
 * Tests to traverse local directories.
 */
// @ts-ignore: error TS4111: Property 'traverseDownloads' comes from an index
// signature, so it must be accessed with ['traverseDownloads'].
testcase.traverseDownloads = () => {
  return traverseDirectories(RootPath.DOWNLOADS);
};

/**
 * Tests to traverse drive directories.
 */
// @ts-ignore: error TS4111: Property 'traverseDrive' comes from an index
// signature, so it must be accessed with ['traverseDrive'].
testcase.traverseDrive = () => {
  return traverseDirectories(RootPath.DRIVE);
};
