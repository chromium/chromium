// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Delete files in MyFiles and ensure they are moved to /.Trash.
 */
testcase.trashMoveToTrash = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and confirm delete.
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Open the gear menu by clicking the gear button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for menu to not be hidden.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Wait for menu item to appear.
  await remoteCall.waitForElement(
      appId, '#gear-menu-toggle-hidden-files:not([disabled]):not([checked])');

  // Click the menu item.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-menu-toggle-hidden-files']);

  // Navigate to /My files/Downloads/.Trash/files.
  await navigateWithDirectoryTree(appId, '/My files/Downloads/.Trash/files');

  // Ensure hello.txt exists.
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /My files/.Trash/files.
  await navigateWithDirectoryTree(appId, '/My files/Downloads/.Trash/info');

  // Ensure hello.txt.trashinfo exists.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="hello.txt.trashinfo"]');
};

/**
 * Delete files then restore via toast 'Undo'.
 */
testcase.trashRestore = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and confirm delete.
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');

  // Wait for file to be removed from list.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Wait for the undo toast and click undo.
  await remoteCall.waitAndClickElement(
      appId, ['#toast', '#action:not([hidden])']);

  // Wait for file to reappear in list.
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
};
