// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, EntryType, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {expandTreeItem, IGNORE_APP_ERRORS, mountCrostini, navigateWithDirectoryTree, openNewWindow, remoteCall, setupAndWaitUntilReady} from './background.js';
import {DOWNLOADS_FAKE_TASKS} from './tasks.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';

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
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
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

  // Delete selected item.
  await remoteCall.waitAndClickElement(appId, '#delete-button');

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Check: the dialog 'Cancel' button should be focused by default.
  const dialogDefaultButton =
      await remoteCall.waitForElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', dialogDefaultButton.text);

  // Click the delete confirm dialog 'Delete' button.
  let dialogDeleteButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');
  chrome.test.assertEq('Delete', dialogDeleteButton.text);

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /My files/Downloads/.Trash/info.
  await navigateWithDirectoryTree(appId, '/My files/Downloads/.Trash/info');

  // Select hello.txt.trashinfo.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt.trashinfo"]');

  // Delete selected item.
  await remoteCall.waitAndClickElement(appId, '#delete-button');

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Click the delete confirm dialog 'Delete' button.
  dialogDeleteButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');
  chrome.test.assertEq('Delete', dialogDeleteButton.text);

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt.trashinfo"]');

  // Navigate to /My files/Downloads.
  await navigateWithDirectoryTree(appId, '/My files/Downloads');

  // Select .Trash.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name=".Trash"]');

  // Delete selected item.
  await remoteCall.waitAndClickElement(appId, '#delete-button');

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Click the delete confirm dialog 'Delete' button.
  dialogDeleteButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');
  chrome.test.assertEq('Delete', dialogDeleteButton.text);

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(appId, '#file-list [file-name=".Trash"]');

  // Delete photos dir (no dialog),
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');

  // Wait for photos to be removed, and .Trash to be recreated.
  await remoteCall.waitForElementLost(appId, '#file-list [file-name="photos"]');
  await remoteCall.waitForElement(appId, '#file-list [file-name=".Trash"]');
};

/**
 * Permanently delete files in MyFiles.
 */
testcase.trashPermanentlyDelete = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Send Shift+Delete to permanently delete, shows delete confirm dialog.
  const shiftDeleteKey = ['#quick-view', 'Delete', false, true, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, shiftDeleteKey),
      'Pressing Shift+Delete failed.');
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');
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
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
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
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Right-click the selected file to validate context menu.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check that 'Restore from Trash' and 'Delete' are shown.
  const checkMenu = async command => {
    await remoteCall.waitForElement(
        appId,
        `#file-context-menu:not([hidden]) [command="${
            command}"]:not([hidden])`);
  };
  await checkMenu('#restore-from-trash');
  await checkMenu('#delete');

  // Restore item.
  await remoteCall.waitAndClickElement(appId, '#restore-from-trash-button');

  // Wait for completion of file restore.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /My files/Downloads and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/My files/Downloads');
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
};

/**
 * Delete files then restore via keyboard shortcut.
 */
testcase.trashRestoreFromTrashShortcut = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash.
  await navigateWithDirectoryTree(appId, '/Trash');

  // Select file.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');

  // Press 'Delete' key.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['#file-list', 'Delete', false, false, false]));

  // Wait for completion of file restore.
  await remoteCall.waitForElementLost(
      appId, '.tre-row input [file-name="hello.txt"]');

  // Navigate to /My files/Downloads and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/My files/Downloads');
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
};

/**
 * Delete files (move them into trash) then empty trash using toolbar.
 */
testcase.trashEmptyTrash = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Empty trash and confirm delete (dialog shown).
  await remoteCall.waitAndClickElement(appId, '#empty-trash-button');
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');
};

/**
 * Delete files (move them into trash) then empty trash using shortcut.
 */
testcase.trashEmptyTrashShortcut = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Press Ctrl+Shift+Delete key.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['#file-list', 'Delete', /*ctrl=*/ true, /*shift=*/ true, false]));

  // Confirm dialog.
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');
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
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete selected item.
  await remoteCall.waitAndClickElement(appId, '#delete-button');

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Check: the dialog 'Cancel' button should be focused by default.
  const dialogDefaultButton =
      await remoteCall.waitForElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', dialogDefaultButton.text);

  // Click the delete confirm dialog 'Delete' button.
  const dialogDeleteButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');
  chrome.test.assertEq('Delete', dialogDeleteButton.text);

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');
};

/**
 * When selecting items whilst in the trash root, no files tasks should be
 * available.
 */
testcase.trashNoTasksInTrashRoot = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);

  // Select hello.txt and make sure tasks are visible.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.waitForElement(appId, '#tasks:not([hidden])');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown and the tasks button is
  // hidden.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.waitForElement(appId, '#tasks[hidden]');
};

/**
 * Double clicking on a file while in Trash shows a disallowed alert dialog.
 */
testcase.trashDoubleClickOnFileInTrashRootShowsDialog = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);

  // Select hello.txt and make sure a default task is executed when double
  // clicking.
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId, ['#file-list [file-name="hello.txt"]']));
  await remoteCall.waitUntilTaskExecutes(
      appId, DOWNLOADS_FAKE_TASKS[0].descriptor);

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.waitForElement(appId, '#tasks[hidden]');

  // Double-click the file and ensure an alert dialog is displayed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId, ['#file-list [file-name="hello.txt"]']);
  await remoteCall.waitForElement(appId, '.files-alert-dialog');
};

/**
 * Double clicking on a file while in Trash shows a disallowed alert dialog.
 */
testcase.trashTraversingFolderShowsDisallowedDialog = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select the Photos folder and trash the whole thing.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(appId, '#file-list [file-name="photos"]');

  // Navigate to /Trash and ensure the "photos" folder is shown.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');

  // Double-click the folder and ensure an alert dialog is displayed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId, ['#file-list [file-name="photos"]']);
  await remoteCall.waitForElement(appId, '.files-alert-dialog');

  // Dismiss the alert dialog.
  await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');

  // Select the element and press enter, outside of Trash this would navigate to
  // the folder but in Trash it should show an alert dialog.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');
  const enterKey = ['#file-list', 'Enter', false, false, false];
  await remoteCall.fakeKeyDown(appId, ...enterKey);
  await remoteCall.waitForElement(appId, '.files-alert-dialog');
};

/**
 * Tests that dragging an accepted file over Trash shows that it accepts the
 * action and performs a trash operation (move a move).
 */
testcase.trashDragDropRootAcceptsEntries = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source = '#file-list [file-name="hello.txt"] .entry-name';

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  const target = '#directory-tree [entry-label="Trash"]';
  await remoteCall.waitForElement(appId, target);

  // Drag the source and hover it over the target.
  const skipDrop = true;
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeDragAndDrop', appId, [source, target, skipDrop]),
      'fakeDragAndDrop failed');

  // Check: drag hovering should navigate the file list.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Trash');

  // Check: the target should have accepts class.
  const willAcceptDrop = '#directory-tree [entry-label="Trash"].accepts';
  await remoteCall.waitForElement(appId, willAcceptDrop);

  // Check: the target should not have denies class.
  const willDenyDrop = '#directory-tree [entry-label="Trash"].denies';
  await remoteCall.waitForElementLost(appId, willDenyDrop);

  // Send a dragdrop event to the target to start a trash operation.
  const dragLeave = false;
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeDragLeaveOrDrop', appId, ['#file-list', target, dragLeave]),
      'fakeDragLeaveOrDrop failed');

  // The Trash root should not have either accepts nor denies after stopping the
  // dragdrop event.
  await remoteCall.waitForElementLost(appId, willAcceptDrop);
  await remoteCall.waitForElementLost(appId, willDenyDrop);
};

/**
 * Tests that dragging a file from a location that Trash is not enabled (Android
 * files in this case) shows it is denied.
 */
testcase.trashDragDropFromDisallowedRootsFails = async () => {
  // Open Files app on Play Files.
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET);
  const appId = await openNewWindow(RootPath.ANDROID_FILES);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Set the source of the drag event to the name of the file.
  const source = `#file-list li[file-name="${
      BASIC_ANDROID_ENTRY_SET[0].nameText}"] .entry-name`;

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target to be visible.
  const target = '#directory-tree [entry-label="Trash"]';
  await remoteCall.waitForElement(appId, target);

  // Drag the source and hover it over the target.
  const skipDrop = true;
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeDragAndDrop', appId, [source, target, skipDrop]),
      'fakeDragAndDrop failed');

  // Wait for the directory to change to the Trash root.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Trash');

  // The Trash root in the directory tree shouldn't have the accepts class.
  const willAcceptDrop = '#directory-tree [entry-label="Trash"].accepts';
  await remoteCall.waitForElementLost(appId, willAcceptDrop);

  // The Trash root should have a denies class.
  const willDenyDrop = '#directory-tree [entry-label="Trash"].denies';
  await remoteCall.waitForElement(appId, willDenyDrop);

  // Send a dragleave event to the target to stop the drag event.
  const dragLeave = true;
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeDragLeaveOrDrop', appId, ['#file-list', target, dragLeave]),
      'fakeDragLeaveOrDrop failed');

  // The Trash root should not have either accepts nor denies after stopping the
  // dragdrop event.
  await remoteCall.waitForElementLost(appId, willAcceptDrop);
  await remoteCall.waitForElementLost(appId, willDenyDrop);
};

/**
 * Tests that dragging and dropping on the Trash root actually trashes the item
 * and it appears in Trash after drop completed.
 */
testcase.trashDragDropRootPerformsTrashAction = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source = '#file-list [file-name="hello.txt"] .entry-name';

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  const target = '#directory-tree [entry-label="Trash"]';
  await remoteCall.waitForElement(appId, target);

  // Send a dragdrop event to the target to start a trash operation.
  const skipDrop = false;
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeDragAndDrop', appId, [source, target, skipDrop]),
      'fakeDragLeaveOrDrop failed');

  // Wait for element to disappear from the "Downloads" view, this indicates it
  // should be in trash.
  await remoteCall.waitForElementLost(appId, source);

  // Navigate to the Trash root.
  await navigateWithDirectoryTree(appId, '/Trash');

  // Wait for the element to appear in the Trash.
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
};

/**
 * Tests that dragging an entry that is non-modifiable (Downloads in this case)
 * should not be allowed despite residing in a trashable location.
 */
testcase.trashDragDropNonModifiableEntriesCantBeTrashed = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Navigate to My files.
  await navigateWithDirectoryTree(appId, '/My files');

  // Use Downloads entry as the drag source. Although this is technically a
  // folder and resides on a trashable location "My files", it is a special
  // entry so we should disallow this from being acceptable as a drop target on
  // Trash.
  const source = '#file-list [file-name="Downloads"] .entry-name';

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  const target = '#directory-tree [entry-label="Trash"]';
  await remoteCall.waitForElement(appId, target);

  // Send a dragdrop event to the target to try Trash the downloads folder.
  const skipDrop = false;
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeDragAndDrop', appId, [source, target, skipDrop]),
      'fakeDragLeaveOrDrop failed');

  // Navigate to Trash to ensure Downloads wasn't sent there.
  await navigateWithDirectoryTree(appId, '/Trash');

  // Ensure the Downloads entry doesn't exist in Trash.
  await remoteCall.waitForElement(appId, `[scan-completed="Trash"]`);
  await remoteCall.waitForFiles(appId, []);
};
