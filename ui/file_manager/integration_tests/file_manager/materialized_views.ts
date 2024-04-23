// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

/**
 * Tests that the Directory Tree can display a materialized view.
 */
export async function mvDisplayInTree() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Wait for "Starred files" to appear.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Starred files');
}

/**
 * Tests that a materialized view can display its content in the file list.
 */
export async function mvScanner() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.hello], []);

  // Add hello to Tote/pin to shelf.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);

  // Open context menu.
  await remoteCall.waitAndRightClick(appId, ['.table-row[selected]']);

  // Add to Tote.
  await remoteCall.waitAndClickMenuItem(
      appId, 'context-menu', '#toggle-holding-space');

  // Wait for "Starred files" to appear.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Starred files');

  // Navigate to Starred files.
  await directoryTree.navigateToPath('/Starred files');

  // Check the content.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));
}
