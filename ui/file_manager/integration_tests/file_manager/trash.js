// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Delete files in MyFiles and ensure they are moved to /.Trash.
 * Then delete items from /.Trash/files and /.Trash/info, then delete /.Trash.
 */
testcase.trashMoveToTrash = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#delete-button');
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

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and confirm delete (dialog shown).
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /My files/Downloads/.Trash/info.
  await navigateWithDirectoryTree(appId, '/My files/Downloads/.Trash/info');

  // Select hello.txt.trashinfo.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt.trashinfo"]');

  // Delete item and confirm delete (dialog shown).
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt.trashinfo"]');

  // Navigate to /My files/Downloads.
  await navigateWithDirectoryTree(appId, '/My files/Downloads');

  // Select .Trash.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name=".Trash"]');

  // Delete item and confirm delete (dialog shown).
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(appId, '#file-list [file-name=".Trash"]');

  // Delete photos dir (no dialog),
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');
  await remoteCall.waitAndClickElement(appId, '#delete-button');

  // Wait for photos to be removed, and .Trash to be recreated.
  await remoteCall.waitForElementLost(appId, '#file-list [file-name="photos"]');
  await remoteCall.waitForElement(appId, '#file-list [file-name=".Trash"]');
};

/**
 * Delete files then restore via toast 'Undo'.
 */
testcase.trashRestoreFromToast = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Wait for the undo toast and click undo.
  await remoteCall.waitAndClickElement(
      appId, ['#toast', '#action:not([hidden])']);

  // Wait for file to reappear in list.
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
};

/**
 * Delete files then restore via Trash file context menu.
 */
testcase.trashRestoreFromTrash = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="My files › Downloads › hello.txt"]');

  // Restore item.
  await remoteCall.waitAndClickElement(appId, '#restore-from-trash-button');

  // Wait for completion of file restore.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="My files › Downloads › hello.txt"]');

  // Navigate to /My files/Downloads and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/My files/Downloads');
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
};

/**
 * Delete files (move them into trash) then permanently delete.
 */
testcase.trashDeleteFromTrash = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="My files › Downloads › hello.txt"]');

  // Delete item and confirm delete (dialog shown).
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="My files › Downloads › hello.txt"]');
};
