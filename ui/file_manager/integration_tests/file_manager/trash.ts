// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../prod/file_manager/shared_types.js';
import {addEntries, createNestedTestFolders, ENTRIES, RootPath, sendTestMessage} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, DOWNLOADS_FAKE_TASKS, NESTED_ENTRY_SET} from './test_data.js';

/**
 * Clicks the enabled and visible delete button and ensures the move to trash
 * button is hidden.
 */
async function clickDeleteButton(appId: string) {
  await remoteCall.waitForElement(appId, '#move-to-trash[hidden]');
  await remoteCall.waitAndClickElement(
      appId, '#delete-button:not([hidden]):not([disabled])');
}

/**
 * Confirm the deletion happens and assert the dialog has the correct text.
 * @param okText expected OK text
 */
async function confirmPermanentDeletion(appId: string, okText: string) {
  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Check: the dialog 'Cancel' button should be focused by default.
  const dialogDefaultButton =
      await remoteCall.waitForElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', dialogDefaultButton.text);

  // Click the delete confirm dialog 'Delete' button.
  const dialogDeleteButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');
  chrome.test.assertEq(okText, dialogDeleteButton.text);

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');
}

/**
 * Clicks the delete button and confirms the deletion.
 */
async function clickDeleteButtonAndConfirmDeletion(appId: string) {
  await clickDeleteButton(appId);
  await confirmPermanentDeletion(appId, 'Delete forever');
}

/**
 * Delete files in MyFiles and ensure they are moved to /.Trash.
 * Then delete items from /.Trash/files and /.Trash/info, then delete /.Trash.
 */
export async function trashMoveToTrash() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Enable hidden files to be shown.
  await remoteCall.showHiddenFiles(appId);

  // Navigate to /My files/Downloads/.Trash/files.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/.Trash/files');

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete selected item.
  await clickDeleteButtonAndConfirmDeletion(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /My files/Downloads/.Trash/info.
  await directoryTree.navigateToPath('/My files/Downloads/.Trash/info');

  // Select hello.txt.trashinfo.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt.trashinfo"]');

  // Delete selected item.
  await clickDeleteButtonAndConfirmDeletion(appId);

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt.trashinfo"]');

  // Navigate to /My files/Downloads.
  await directoryTree.navigateToPath('/My files/Downloads');

  // Select .Trash.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name=".Trash"]');

  // Delete selected item.
  await clickDeleteButtonAndConfirmDeletion(appId);

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(appId, '#file-list [file-name=".Trash"]');

  // Delete photos dir (no dialog),
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');
  await remoteCall.clickTrashButton(appId);

  // Wait for photos to be removed, and .Trash to be recreated.
  await remoteCall.waitForElementLost(appId, '#file-list [file-name="photos"]');
  await remoteCall.waitForElement(appId, '#file-list [file-name=".Trash"]');
}

/**
 * Selects a file and a folder at the same time then deletes both.
 */
export async function trashMultipleEntries() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello, ENTRIES.photos], []);

  // Select all (both the file and the folder).
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA);

  // Delete both entries.
  await remoteCall.clickTrashButton(appId);

  // Wait for both entries to be removed.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.waitForElementLost(appId, '#file-list [file-name="photos"]');
}

/**
 * Selects a non-empty folder and deletes it.
 */
export async function trashNonEmptyFolder() {
  // Build folder structure nested-folder0/nested-folder1.
  const entries = createNestedTestFolders(2);

  // Open files app to a Downloads folder containing nested-folder0.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Select the folder.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="nested-folder0"]');

  // Delete the folder.
  await remoteCall.clickTrashButton(appId);

  // Wait for the folder to be removed.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="nested-folder0"]');
}

/**
 * Permanently delete files in Downloads.
 */
export async function trashPermanentlyDelete() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Send Shift+Delete to permanently delete, shows delete confirm dialog.
  const shiftDeleteKey = ['#quick-view', 'Delete', false, true, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, shiftDeleteKey),
      'Pressing Shift+Delete failed.');

  // Confirm the permanent deletion of the "hello.txt" file.
  await confirmPermanentDeletion(appId, 'Delete forever');
}

/**
 * Files send to the Trash from ~/MyFiles should be able to be deleted once they
 * are in Trash.
 */
export async function trashDeleteFromTrashOriginallyFromMyFiles() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Cut the file "hello.txt" in preparation to move to ~/MyFiles.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['cut']));

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files');

  // Paste the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete selected item.
  await clickDeleteButtonAndConfirmDeletion(appId);
}

/**
 * Delete files then restore via progress center panel button 'Undo'.
 */
export async function trashRestoreFromToast() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Wait for the a success progress panel item to appear.
  await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item[status="success"]']);

  // Press the "Undo"" button on the success feedback panel.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      [['#progress-panel', 'xf-panel-item', 'xf-button#primary-action']]));

  // Wait for file to reappear in list.
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
}

/**
 * Delete files then restore via Trash file context menu.
 */
export async function trashRestoreFromTrash() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Right-click the selected file to validate context menu.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check that 'Restore from Trash' and 'Delete' are shown.
  const checkMenu = async (command: string) => {
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
  await directoryTree.navigateToPath('/My files/Downloads');
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
}

/**
 * Delete files then restore via keyboard shortcut.
 */
export async function trashRestoreFromTrashShortcut() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');

  // Select file.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');

  // Press 'Delete' key.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['#file-list', 'Delete', false, false, false]));

  // Wait for completion of file restore.
  await remoteCall.waitForElementLost(
      appId, '.tre-row input [file-name="hello.txt"]');

  // Navigate to /My files/Downloads and ensure the file is shown.
  await directoryTree.navigateToPath('/My files/Downloads');
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
}

/**
 * Delete files (move them into trash) then empty trash using the banner.
 */
export async function trashEmptyTrash() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const fileNameSelector = '#file-list [file-name="hello.txt"]';
  const emptyTrashButtonSelector =
      ['trash-banner', 'cr-button[command="#empty-trash"]'];

  // Select hello.txt.
  await remoteCall.waitAndClickElement(appId, fileNameSelector);

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, fileNameSelector);

  // Navigate to /Trash and ensure the file is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitForElement(appId, fileNameSelector);
  // SimulateUiClick ensures that the focus gets updated, which resets
  // canExecute.
  await remoteCall.simulateUiClick(appId, fileNameSelector);

  // Click the empty trash button. SimulateUiClick ensures that the focus is set
  // to the empty trash button before bringing up the confirmation dialog.
  await remoteCall.waitForElement(appId, emptyTrashButtonSelector);
  await remoteCall.simulateUiClick(appId, emptyTrashButtonSelector);
  // Cancel delete.
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-cancel');
  // Wait for the dialog to be hidden.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container.shown');
  // Check: the file is still present.
  await remoteCall.waitForElement(appId, fileNameSelector);
  // Empty trash and confirm delete (dialog shown).
  await remoteCall.waitForElement(appId, emptyTrashButtonSelector);
  await remoteCall.simulateUiClick(appId, emptyTrashButtonSelector);
  // Wait for the dialog to be shown again.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');
  // Confirm the permanent deletion.
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog .cr-dialog-ok');
  // Wait for the dialog to be hidden.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container.shown');

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(appId, fileNameSelector);
}

/**
 * Delete files (move them into trash) then empty trash using shortcut.
 */
export async function trashEmptyTrashShortcut() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
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
}

/**
 * Delete files (move them into trash) then permanently delete.
 */
export async function trashDeleteFromTrash() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete selected item.
  await clickDeleteButtonAndConfirmDeletion(appId);
}

/**
 * Delete files (move them into trash) then permanently delete.
 */
export async function trashDeleteFromTrashOriginallyFromDrive() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Confirm the permanent deletion of the file does not say 'forever'.
  await clickDeleteButton(appId);
  await confirmPermanentDeletion(appId, 'Delete');
}

/**
 * When selecting items whilst in the trash root, no files tasks should be
 * available.
 */
export async function trashNoTasksInTrashRoot() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);

  // Select hello.txt and make sure tasks are visible.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.waitForElement(appId, '#tasks:not([hidden])');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown and the tasks button is
  // hidden.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.waitForElement(appId, '#tasks[hidden]');
}

/**
 * Double clicking on a file while in Trash shows a disallowed alert dialog.
 */
export async function trashDoubleClickOnFileInTrashRootShowsDialog() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);

  // Select hello.txt and make sure a default task is executed when double
  // clicking.
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId, ['#file-list [file-name="hello.txt"]']));
  await remoteCall.waitUntilTaskExecutes(
      appId, DOWNLOADS_FAKE_TASKS[0]!.descriptor, ['hello.txt']);

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.waitForElement(appId, '#tasks[hidden]');

  // Double-click the file and ensure an alert dialog is displayed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId, ['#file-list [file-name="hello.txt"]']);
  await remoteCall.waitForElement(appId, '.files-confirm-dialog');
}

/**
 * Pressing Enter on a file while in Trash shows a disallowed confirm dialog
 * with a restore button that performs restoration on the file.
 */
export async function
trashPressingEnterOnFileInTrashRootShowsDialogWithRestoreButton() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitUntilSelected(appId, 'hello.txt');
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.waitForElement(appId, '#tasks[hidden]');

  // Press "Enter" on the file and ensure an alert dialog is displayed.
  const enterKey = ['#file-list', 'Enter', false, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...enterKey);
  await remoteCall.waitForElement(appId, '.files-confirm-dialog');

  // Click the "Restore" button on the error message and ensure it restores the
  // file back to the Downloads directory.
  await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');
  await directoryTree.navigateToPath('/My files/Downloads');
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
}

/**
 * Double clicking on a file while in Trash shows a disallowed alert dialog.
 */
export async function trashTraversingFolderShowsDisallowedDialog() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select the Photos folder and trash the whole thing.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, '#file-list [file-name="photos"]');

  // Navigate to /Trash and ensure the "photos" folder is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');

  // Double-click the folder and ensure an alert dialog is displayed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId, ['#file-list [file-name="photos"]']);
  await remoteCall.waitForElement(appId, '.files-confirm-dialog');

  // Dismiss the alert dialog.
  await remoteCall.waitAndClickElement(appId, '.cr-dialog-cancel');

  // Select the element and press enter, outside of Trash this would navigate to
  // the folder but in Trash it should show a confirm dialog.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');
  const enterKey = ['#file-list', 'Enter', false, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...enterKey);
  await remoteCall.waitForElement(appId, '.files-confirm-dialog');
}

/**
 * Tests that dragging an accepted file over Trash shows that it accepts the
 * action and performs a trash operation (move a move).
 */
export async function trashDragDropRootAcceptsEntries() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source = '#file-list [file-name="hello.txt"] .entry-name';

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Trash');

  // Drag the source and hover it over the target.
  const finishDrop = await directoryTree.dragFilesToItemByLabel(
      source, 'Trash', /* skipDrop= */ true);

  // Check: drag hovering should navigate the file list.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Trash');

  // Check: the target should have accepts class and should not have denies
  // class.
  await directoryTree.waitForItemToAcceptDropByLabel('Trash');

  // Send a dragdrop event to the target to start a trash operation.
  await finishDrop('#file-list', /* dragLeave= */ false);

  // The Trash root should not have either accepts nor denies after stopping the
  // dragdrop event.
  await directoryTree.waitForItemToFinishDropByLabel('Trash');
}

/**
 * Tests that dragging a file from a location that Trash is not enabled (Android
 * files in this case) shows it is denied.
 */
export async function trashDragDropFromDisallowedRootsFails() {
  // Open Files app on Play Files.
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET);
  const appId = await remoteCall.openNewWindow(RootPath.ANDROID_FILES);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Set the source of the drag event to the name of the file.
  const source = `#file-list li[file-name="${
      BASIC_ANDROID_ENTRY_SET[0]!.nameText}"] .entry-name`;

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target to be visible.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Trash');

  // Drag the source and hover it over the target.
  const finishDrop = await directoryTree.dragFilesToItemByLabel(
      source, 'Trash', /* skipDrop= */ true);

  // Wait for the directory to change to the Trash root.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Trash');

  // The Trash root in the directory tree shouldn't have the accepts class and
  // should have a denies class.
  await directoryTree.waitForItemToDenyDropByLabel('Trash');

  // Send a dragleave event to the target to stop the drag event.
  await finishDrop('#file-list', /* dropLeave= */ true);

  // The Trash root should not have either accepts nor denies after stopping the
  // dragdrop event.
  await directoryTree.waitForItemToFinishDropByLabel('Trash');
}

/**
 * Tests that dragging and dropping on the Trash root actually trashes the item
 * and it appears in Trash after drop completed.
 */
export async function trashDragDropRootPerformsTrashAction() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source = '#file-list [file-name="hello.txt"] .entry-name';

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Trash');

  // Send a dragdrop event to the target to start a trash operation.
  await directoryTree.dragFilesToItemByLabel(
      source, 'Trash', /* skipDrop= */ false);

  // Wait for element to disappear from the "Downloads" view, this indicates it
  // should be in trash.
  await remoteCall.waitForElementLost(appId, source);

  // Navigate to the Trash root.
  await directoryTree.navigateToPath('/Trash');

  // Wait for the element to appear in the Trash.
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
}

/**
 * Tests that dragging an entry that is non-modifiable (Downloads in this case)
 * should not be allowed despite residing in a trashable location.
 */
export async function trashDragDropNonModifiableEntriesCantBeTrashed() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Navigate to My files.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files');

  // Use Downloads entry as the drag source. Although this is technically a
  // folder and resides on a trashable location "My files", it is a special
  // entry so we should disallow this from being acceptable as a drop target on
  // Trash.
  const source = '#file-list [file-name="Downloads"] .entry-name';

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  await directoryTree.waitForItemByLabel('Trash');

  // Send a dragdrop event to the target to try Trash the downloads folder.
  await directoryTree.dragFilesToItemByLabel(
      source, 'Trash', /* skipDrop= */ false);

  // Navigate to Trash to ensure Downloads wasn't sent there.
  await directoryTree.navigateToPath('/Trash');

  // Ensure the Downloads entry doesn't exist in Trash.
  await remoteCall.waitForElement(appId, `[scan-completed="Trash"]`);
  await remoteCall.waitForFiles(appId, []);
}

/**
 * Tests the Trash root is not visible when opening Files app as a select file
 * dialog.
 */
export async function trashDontShowTrashRootOnSelectFileDialog() {
  // Open Files app on Downloads as a select file dialog.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, [],
      {type: DialogType.SELECT_OPEN_FILE});

  // Navigate to the My files directory to ensure the directory tree has fully
  // loaded and wait for My files to finish scanning.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files');
  await remoteCall.waitForElement(appId, `[scan-completed="My files"]`);

  // Ensure the Trash root entry is not visible on the page.
  await directoryTree.waitForItemLostByLabel('Trash');
}

/**
 * Tests the Trash root is not visible when Files app is used as a select file
 * dialog within Android applications.
 */
export async function trashDontShowTrashRootWhenOpeningAsAndroidFilePicker() {
  // Open Files app on Downloads as an Android file picker.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, [],
      {volumeFilter: ['media-store-files-only']});

  // Navigate to the My files directory to ensure the directory tree has fully
  // loaded and wait for My files to finish scanning.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files');
  await remoteCall.waitForElement(appId, `[scan-completed="My files"]`);

  // Ensure the Trash root entry is not visible on the page.
  await directoryTree.waitForItemLostByLabel('Trash');
}

/**
 * Tests that a trashed file with a deletion date >30 days gets permanently
 * removed.
 */
export async function trashEnsureOldEntriesArePeriodicallyRemoved() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);
  const fileNameSelector = '#file-list [file-name="hello.txt"]';

  // Select hello.txt and make sure a default task is executed when double
  // clicking.
  await remoteCall.waitAndClickElement(appId, fileNameSelector);
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, fileNameSelector);

  // Navigate to /Trash and ensure the file is there and has not been deleted,
  // the deletion date is well within the periodic deletion boundaries.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitForElement(appId, fileNameSelector);

  // Navigate away from /Trash (to /My files) as the periodic removal will only
  // be kicked off on initial directory scan.
  await directoryTree.navigateToPath('/My files');

  // Overwrite the existing .trashinfo file with an older one that is outside
  // the 30 day window and should trigger periodic removal.
  await addEntries(['local'], [
    ENTRIES.trashRootDirectory,
    ENTRIES.trashInfoDirectory,
    ENTRIES.oldTrashInfoFile,
  ]);

  // Navigate to /Trash and ensure the file has been removed.
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitForElement(appId, `[scan-completed="Trash"]`);
  await remoteCall.waitForElementLost(appId, fileNameSelector);

  // Expect no feedback panel element to appear as the IOTask was kicked off
  // with notifications disabled.
  await remoteCall.waitForElementLost(
      appId, ['#progress-panel', 'xf-panel-item']);
}

/**
 * Tests that dragging and dropping out of the Trash root restore files to the
 * location that was requested (i.e. the drop target).
 */
export async function trashDragDropOutOfTrashPerformsRestoration() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt and send it to the Trash.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to the Trash root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');

  // Wait for the element to appear in the Trash.
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');

  // Send a dragdrop event that emulates dragging the hello.txt onto the "My
  // files" root in the directory tree.
  await directoryTree.dragFilesToItemByLabel(
      '#file-list [file-name="hello.txt"] .entry-name', 'My files',
      /* skipDrop= */ false);

  // Wait for the element to disappear from the file list.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // A feedback panel item should be created to indicate the restoring function.
  await remoteCall.waitForElement(appId, ['#progress-panel', 'xf-panel-item']);

  // Navigate to the "My files" root and ensure the file exists there now.
  await directoryTree.navigateToPath('/My files');
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
}

/**
 * Tests that the "Moving to trash" visual signal that is shown whilst a trash
 * operation is in progress, does not contain the "Undo" button.
 */
export async function trashRestorationDialogInProgressDoesntShowUndo() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Tell the progress center to never finish the operation which leaves the in
  // progress visual signal visible.
  await remoteCall.callRemoteTestUtil(
      'progressCenterNeverNotifyCompleted', appId, []);

  // Select hello.txt and send it to the Trash.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // A feedback panel item should be created to indicate the item is being sent
  // to the trash with only a secondary action.
  const cancelButton = await remoteCall.waitForElement(
      appId,
      ['#progress-panel', 'xf-panel-item', 'xf-button#secondary-action']);
  await remoteCall.waitForElementLost(
      appId, ['#progress-panel', 'xf-panel-item', 'xf-button#primary-action']);

  // Ensure the secondary action is of the category cancel.
  chrome.test.assertEq(cancelButton.attributes['data-category'], 'cancel');
}

/**
 * Tests that the `TrashEnabled` preference adds and removes the trash root
 * from the directory tree.
 */
export async function trashTogglingTrashEnabledPrefUpdatesDirectoryTree() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt and send it to the Trash, this file should not be removed
  // in between enabling and disabling the feature.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Wait for Trash root to be visible.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Trash');

  // Disable trash.
  await sendTestMessage({name: 'setTrashEnabled', enabled: false});

  // Wait for the Trash root to disappear.
  await directoryTree.waitForItemLostByLabel('Trash');

  // Ensure the delete button shows up instead of the move to trash button.
  await remoteCall.waitUntilSelected(appId, 'world.ogv');
  await clickDeleteButton(appId);
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Cancel the dialog.
  await remoteCall.waitAndClickElement(appId, '.cr-dialog-cancel');

  // Enable trash.
  await sendTestMessage({name: 'setTrashEnabled', enabled: true});

  // Wait for the Trash root to appear again.
  await directoryTree.waitForItemByLabel('Trash');

  // Navigate to the "Trash" root and ensure the file exists there now.
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitForElement(appId, '#file-list [file-name="hello.txt"]');
}

/**
 * Tests that the `TrashEnabled` preference adds and removes the trash root
 * from the directory tree and when navigated on the Trash root, removal
 * navigates the user back to My files.
 */
export async function trashTogglingTrashEnabledNavigatesAwayFromTrashRoot() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Navigate to the Trash root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');

  // Disable trash.
  await sendTestMessage({name: 'setTrashEnabled', enabled: false});

  // Wait for the Trash root to disappear.
  await directoryTree.waitForItemLostByLabel('Trash');

  // Ensure the new root is now at My files.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');
}

/**
 * Verify that files that have their parents trashed show an alert dialog to
 * indicate that restoration is not possible.
 */
export async function trashCantRestoreWhenParentDoesntExist() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, NESTED_ENTRY_SET, []);

  // Navigate to the "A" directory.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/A');

  // Ensure the "B" directory exists within "A".
  await remoteCall.waitForFiles(appId, [ENTRIES.directoryB.getExpectedRow()]);

  // Select the "B" directory.
  await remoteCall.waitAndClickElement(appId, '#file-list [file-name="B"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, '#file-list [file-name="B"]');

  // Navigate to /My files/Downloads and click the "A" directory.
  await directoryTree.navigateToPath('/My files/Downloads');
  await remoteCall.waitAndClickElement(appId, '#file-list [file-name="A"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, '#file-list [file-name="A"]');

  // Navigate to Trash and click the "B" directory of which the parent "A"
  // directory has been removed.
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(appId, '#file-list [file-name="B"]');

  // Right-click the selected file to validate context menu.
  await remoteCall.waitAndRightClick(appId, '.table-row[selected]');
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Restore item and expect the alert dialog is shown as the parent has been
  // removed.
  await remoteCall.waitAndClickElement(appId, '#restore-from-trash-button');
  await remoteCall.waitForElement(appId, '.files-alert-dialog');
}

/**
 * Verify that infeasible actions within Trash root are disabled and hidden when
 * right clicking a file. Verify that feasible actions are enabled.
 */
export async function
trashInfeasibleActionsForFileDisabledAndHiddenInTrashRoot() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  const fileSelector = '#file-list [file-name="hello.txt"]';

  // Select hello.txt and send it to the Trash.
  await remoteCall.waitAndClickElement(appId, fileSelector);
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, fileSelector);

  // Navigate to the Trash root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');

  // Wait for the element to appear in the Trash and right click it to get
  // access to the context menu.
  await remoteCall.waitAndRightClick(appId, fileSelector);

  const contextMenuSelector = '#file-context-menu:not([hidden])';
  await remoteCall.waitForElement(appId, contextMenuSelector);

  // Ensure each infeasible action is disabled and hidden.
  const infeasibleActions = [
    'rename',
    'new-folder',
    'paste',
    'copy',
    'zip-selection',
    'set-wallpaper',
    'invoke-sharesheet',
  ];
  for (const action of infeasibleActions) {
    await remoteCall.waitForElement(
        appId,
        contextMenuSelector + ' [command="#' + action + '"][disabled][hidden]');
  }

  // Ensure each feasible action not disabled and not hidden.
  const feasibleActions = [
    'cut',
    'get-info',
    'delete',
    'restore-from-trash',
  ];
  for (const action of feasibleActions) {
    await remoteCall.waitForElement(
        appId,
        contextMenuSelector + ' [command="#' + action +
            '"]:not([disabled]):not([hidden])');
  }
}

/**
 * Verify that infeasible actions within Trash root are disabled and hidden when
 * right clicking a folder. Verify that feasible actions are enabled.
 */
export async function
trashInfeasibleActionsForFolderDisabledAndHiddenInTrashRoot() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello, ENTRIES.directoryA], []);

  // Select and copy hello.txt into the clipboard to check the Paste Into Folder
  // action.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']),
      'execCommand failed');

  const fileSelector = '#file-list [file-name="A"]';

  // Select folder A and send it to the Trash.
  await remoteCall.waitAndClickElement(appId, fileSelector);
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, fileSelector);

  // Navigate to the Trash root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');

  // Wait for the element to appear in the Trash and right click it to get
  // access to the context menu.
  await remoteCall.waitAndRightClick(appId, fileSelector);

  const contextMenuSelector = '#file-context-menu:not([hidden])';
  await remoteCall.waitForElement(appId, contextMenuSelector);

  // Ensure each infeasible action is disabled and hidden.
  const infeasibleActions = [
    'rename',
    'new-folder',
    'copy',
    'zip-selection',
    'paste-into-folder',
  ];
  for (const action of infeasibleActions) {
    await remoteCall.waitForElement(
        appId,
        contextMenuSelector + ' [command="#' + action + '"][disabled][hidden]');
  }

  // Ensure each feasible action not disabled and not hidden.
  const feasibleActions = [
    'cut',
    'get-info',
    'delete',
    'restore-from-trash',
  ];
  for (const action of feasibleActions) {
    await remoteCall.waitForElement(
        appId,
        contextMenuSelector + ' [command="#' + action +
            '"]:not([disabled]):not([hidden])');
  }
}

/**
 * Verify that Extract All within Trash root is disabled and hidden when right
 * clicking a zip file.
 */
export async function trashExtractAllForZipHiddenAndDisabledInTrashRoot() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchive], []);

  const fileSelector = '#file-list [file-name="archive.zip"]';

  // Select tera.zip and send it to the Trash.
  await remoteCall.waitAndClickElement(appId, fileSelector);
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, fileSelector);

  // Navigate to the Trash root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');

  // Wait for the element to appear in the Trash and right click it to get
  // access to the context menu.
  await remoteCall.waitAndRightClick(appId, fileSelector);

  const contextMenuSelector = '#file-context-menu:not([hidden])';
  await remoteCall.waitForElement(appId, contextMenuSelector);
  // Ensure extract all action is disabled and hidden.
  await remoteCall.waitForElement(
      appId,
      contextMenuSelector + ' [command="#extract-all"][disabled][hidden]');
}

/**
 * Verify that infeasible actions within Trash root are disabled and hidden when
 * right clicking a blank space. Verify that Cut is disabled but not hidden.
 */
export async function trashAllActionsDisabledForBlankSpaceInTrashRoot() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Select and copy hello.txt into the clipboard to check the Paste action.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']),
      'execCommand failed');

  // Navigate to the Trash root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');

  // Click blank space.
  await remoteCall.rightClickFileListBlankSpace(appId);

  // Ensure the context menu is not hidden.
  const contextMenuSelector = '#file-context-menu:not([hidden])';
  await remoteCall.waitForElement(appId, contextMenuSelector);
  // Ensure each infeasible action is disabled and hidden.
  const infeasibleActions = [
    'paste',
    'new-folder',
    'copy',
  ];
  for (const action of infeasibleActions) {
    await remoteCall.waitForElement(
        appId,
        contextMenuSelector + ' [command="#' + action + '"][disabled][hidden]');
  }

  // Ensure Cut is disabled and not hidden.
  await remoteCall.waitForElement(
      appId, contextMenuSelector + ' [command="#cut"][disabled]:not([hidden])');
}

export async function trashStaleTrashInfoFilesAreRemovedAfterOneHour() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  const fileSelector = '#file-list [file-name="hello.txt"]';
  const trashInfoSelector = '#file-list [file-name="hello.txt.trashinfo"]';

  // Select hello.txt and send it to the Trash.
  await remoteCall.waitAndClickElement(appId, fileSelector);
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, fileSelector);

  // Enable hidden files to be shown.
  await remoteCall.showHiddenFiles(appId);

  // Navigate to /My files/Downloads/.Trash/files.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/.Trash/files');

  // Select hello.txt.
  await remoteCall.waitAndClickElement(appId, fileSelector);

  // Delete selected item.
  await clickDeleteButtonAndConfirmDeletion(appId);
  await remoteCall.waitForElementLost(appId, fileSelector);

  // Navigate to /My files/Downloads/.Trash/info and ensure the .trashinfo file
  // is still there.
  await directoryTree.navigateToPath('/My files/Downloads/.Trash/info');
  await remoteCall.waitForElement(appId, trashInfoSelector);

  // Update the modification date for the .trashinfo file.
  chrome.test.assertEq(
      await sendTestMessage({
        name: 'updateModificationDate',
        localPath: 'Downloads/.Trash/info/hello.txt.trashinfo',
        modificationDate: ((new Date()).getTime() - 120 * 60 * 1000),
      }),
      'true');

  // Navigate to the Trash directory which should kick off the removal of the
  // stale .trashinfo file.
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitForElementLost(appId, fileSelector);

  // Navigate back to the .Trash/info directory and ensure the .trashinfo file
  // has been removed.
  await directoryTree.navigateToPath('/My files/Downloads/.Trash/info');
  await remoteCall.waitForElementLost(appId, trashInfoSelector);
}

/**
 * If the current directory is .Trash or sub folders of .Trash (e.g.
 * files/info), after disable "show hidden files" menu, the current directory
 * should navigate back to MyFiles.
 */
export async function trashTogglingHiddenFilesNavigatesAwayFromTrash() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  const helloFileSelector = '#file-list [file-name="hello.txt"]';
  await remoteCall.waitAndClickElement(appId, helloFileSelector);

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(appId, helloFileSelector);

  // Enable hidden files to be shown.
  await remoteCall.showHiddenFiles(appId);

  // Navigate to /My files/Downloads/.Trash.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/.Trash');

  // Uncheck "show hidden files".
  await remoteCall.showHiddenFiles(appId, /* check= */ false);

  // Expect current directory goes back to MyFiles.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');
  await directoryTree.waitForItemLostByLabel('.Trash');

  // Show hidden files again.
  await remoteCall.showHiddenFiles(appId);

  // Navigate to /My files/Downloads/.Trash/files this time.
  await directoryTree.navigateToPath('/My files/Downloads/.Trash/files');

  // Uncheck "show hidden files".
  await remoteCall.showHiddenFiles(appId, /* check= */ false);

  // Expect current directory goes back to MyFiles.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');
  await directoryTree.waitForItemLostByLabel('.Trash');
}
