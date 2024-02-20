// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

/**
 * Waits until a dialog with an OK button is shown, and accepts it by clicking
 * on the dialog's OK button.
 *
 * @param appId The Files app windowId.
 * @return Promise to be fulfilled after clicking the OK button.
 */
async function waitAndAcceptDialog(appId: string): Promise<void> {
  const okButton = '.cr-dialog-ok';

  // Wait until the dialog is shown.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Click the dialog OK button.
  await remoteCall.waitAndClickElement(appId, okButton);

  // Wait until the dialog closes.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container');
}

/**
 * Tests copying a file to the same file list.
 *
 * @param path The path to be tested, Downloads or Drive.
 */
async function keyboardCopy(path: string) {
  const appId = await remoteCall.setupAndWaitUntilReady(
      path, [ENTRIES.world], [ENTRIES.world]);

  // Copy the file into the same file list.
  chrome.test.assertTrue(
      (await remoteCall.callRemoteTestUtil<boolean>(
          'copyFile', appId, ['world.ogv'])),
      'copyFile failed');
  // Check: the copied file should appear in the file list.
  const expectedEntryRows = [ENTRIES.world.getExpectedRow()].concat(
      [['world (1).ogv', '56 KB', 'OGG video', '']]);
  await remoteCall.waitForFiles(
      appId, expectedEntryRows, {ignoreLastModifiedTime: true});
  const files =
      await remoteCall.callRemoteTestUtil<string[]>('getFileList', appId, []);
  if (path === RootPath.DRIVE) {
    // DriveFs doesn't preserve mtimes so they shouldn't match.
    chrome.test.assertTrue(files[0]![3] !== files[1]![3], files[0]![3]);
  } else {
    // The mtimes should match for Local files.
    chrome.test.assertTrue(files[0]![3] === files[1]![3], files[0]![3]);
  }
}

/**
 * Tests deleting a file from the file list.
 *
 * @param path The path to be tested, Downloads or Drive.
 * @param confirmDeletion If the file system doesn't support trash, need to
 *     confirm the deletion.
 */
async function keyboardDelete(path: string, confirmDeletion: boolean = false) {
  const appId = await remoteCall.setupAndWaitUntilReady(
      path, [ENTRIES.hello], [ENTRIES.hello]);

  // Delete the file from the file list.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('deleteFile', appId, ['hello.txt']),
      'deleteFile failed');

  if (confirmDeletion) {
    await waitAndAcceptDialog(appId);
  }

  // Check: the file list should be empty.
  await remoteCall.waitForFiles(appId, []);
}

/**
 * Tests deleting a folder from the file list. The folder is also shown in the
 * Files app directory tree, and should not be shown there when deleted.
 *
 * @param path The path to be tested, Downloads or Drive.
 * @param parentLabel The directory tree item label.
 * @param confirmDeletion If the file system doesn't support trash, need to
 *     confirm the deletion.
 */
async function keyboardDeleteFolder(
    path: string, parentLabel: string, confirmDeletion: boolean = false) {
  const appId = await remoteCall.setupAndWaitUntilReady(
      path, [ENTRIES.photos], [ENTRIES.photos]);

  // Expand the directory tree |treeItem|.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel(parentLabel);

  // Check: the folder should be shown in the directory tree.
  await directoryTree.waitForChildItemByLabel(parentLabel, 'photos');

  // Delete the folder entry from the file list.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('deleteFile', appId, ['photos']),
      'deleteFile failed');

  if (confirmDeletion) {
    await waitAndAcceptDialog(appId);
  }

  // Check: the file list should be empty.
  await remoteCall.waitForFiles(appId, []);

  // Check: the folder should not be shown in the directory tree.
  await directoryTree.waitForChildItemLostByLabel(parentLabel, 'photos');
}

/**
 * Renames a file.
 *
 * @param appId The Files app windowId.
 * @param oldName Old name of a file.
 * @param newName New name of a file.
 * @return Promise to be fulfilled on success.
 */
async function renameFile(
    appId: string, oldName: string, newName: string): Promise<void> {
  const textInput = '#file-list .table-row[renaming] input.rename';

  // Select the file.
  await remoteCall.waitUntilSelected(appId, oldName);

  // Press Ctrl+Enter key to rename the file.
  const key = ['#file-list', 'Enter', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Check: the renaming text input should be shown in the file list.
  await remoteCall.waitForElement(appId, textInput);

  // Type new file name.
  await remoteCall.inputText(appId, textInput, newName);

  // Send Enter key to the text input.
  const key2 = [textInput, 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key2));
}

/**
 * Tests renaming a folder. An extra enter key is sent to the file list during
 * renaming to check the folder cannot be entered while it is being renamed.
 *
 * @param path Initial path (Downloads or Drive).
 * @param parentLabel The directory tree item label.
 * @return Promise to be fulfilled on success.
 */
async function testRenameFolder(
    path: string, parentLabel: string): Promise<void> {
  const textInput = '#file-list .table-row[renaming] input.rename';
  const appId = await remoteCall.setupAndWaitUntilReady(
      path, [ENTRIES.photos], [ENTRIES.photos]);

  // Expand the directory tree |treeItem|.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel(parentLabel);

  // Check: the photos folder should be shown in the directory tree.
  await directoryTree.waitForChildItemByLabel(parentLabel, 'photos');
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('focus', appId, ['#file-list']));

  // Press ArrowDown to select the photos folder.
  const select = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, select));

  // Await file list item selection.
  const selectedItem = '#file-list .table-row[selected]';
  await remoteCall.waitForElement(appId, selectedItem);

  // Press Ctrl+Enter to rename the photos folder.
  let key = ['#file-list', 'Enter', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Check: the renaming text input should be shown in the file list.
  await remoteCall.waitForElement(appId, textInput);

  // Type the new folder name.
  await remoteCall.inputText(appId, textInput, 'bbq photos');

  // Send Enter to the list to attempt to enter the directory.
  key = ['#list-container', 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Send Enter to the text input to complete renaming.
  key = [textInput, 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Wait until renaming is complete.
  const renamingItem = '#file-list .table-row[renaming]';
  await remoteCall.waitForElementLost(appId, renamingItem);

  // Check: the renamed folder should be shown in the file list.
  const expectedRows = [['bbq photos', '--', 'Folder', '']];
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Check: the renamed folder should be shown in the directory tree.
  await directoryTree.waitForChildItemByLabel(parentLabel, 'bbq photos');
}

/**
 * Tests renaming a file.
 *
 * @param path Initial path (Downloads or Drive).
 * @return Promise to be fulfilled on success.
 */
async function testRenameFile(path: string): Promise<void> {
  const newFile = [['New File Name.txt', '51 bytes', 'Plain text', '']];

  const appId = await remoteCall.setupAndWaitUntilReady(
      path, [ENTRIES.hello], [ENTRIES.hello]);

  // Rename the file.
  await renameFile(appId, 'hello.txt', 'New File Name.txt');

  // Wait until renaming completes.
  await remoteCall.waitForElementLost(appId, '#file-list [renaming]');

  // Check: the new file name should be shown in the file list.
  await remoteCall.waitForFiles(appId, newFile, {ignoreLastModifiedTime: true});

  // Try renaming the new file to an invalid file name.
  await renameFile(appId, 'New File Name.txt', '.hidden file');

  // Check: the error dialog should be shown.
  await waitAndAcceptDialog(appId);

  // Check: the new file name should not be changed.
  await remoteCall.waitForFiles(appId, newFile, {ignoreLastModifiedTime: true});
}

export function keyboardCopyDownloads() {
  return keyboardCopy(RootPath.DOWNLOADS);
}

export function keyboardCopyDrive() {
  return keyboardCopy(RootPath.DRIVE);
}

export function keyboardDeleteDownloads() {
  return keyboardDelete(RootPath.DOWNLOADS);
}

export function keyboardDeleteDrive() {
  return keyboardDelete(RootPath.DRIVE, /*confirmDeletion=*/ true);
}

export function keyboardDeleteFolderDownloads() {
  return keyboardDeleteFolder(RootPath.DOWNLOADS, 'Downloads');
}

export function keyboardDeleteFolderDrive() {
  return keyboardDeleteFolder(
      RootPath.DRIVE, 'My Drive', /*confirmDeletion=*/ true);
}

export function renameFileDownloads() {
  return testRenameFile(RootPath.DOWNLOADS);
}

export function renameFileDrive() {
  return testRenameFile(RootPath.DRIVE);
}

export function renameNewFolderDownloads() {
  return testRenameFolder(RootPath.DOWNLOADS, 'Downloads');
}

export function renameNewFolderDrive() {
  return testRenameFolder(RootPath.DRIVE, 'My Drive');
}

/**
 * Tests renaming partitions with the keyboard on the file list.
 */
export async function renameRemovableWithKeyboardOnFileList() {
  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.world]);

  // Mount removable device with partitions.
  await sendTestMessage({name: 'mountUsbWithMultiplePartitionTypes'});

  // Wait and select the removable group by clicking the label.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectGroupRootItemByType('removable');

  // Focus on the file list.
  await remoteCall.callRemoteTestUtil('focus', appId, ['#file-list']);

  // Wait for partitions to show up.
  const expectedRows = [
    ['partition-1', '--', 'ntfs', ''],
    ['partition-2', '--', 'ext4', ''],
    ['partition-3', '--', 'vfat', ''],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Attempt to rename partition with a label longer than permitted for fat32.
  const partitionToRename = 'partition-3';  // fat32
  await renameFile(appId, partitionToRename, 'very-long-partition-name');

  // Verify that an error was triggered.
  const errorTextElement =
      await remoteCall.waitForElement(appId, '.cr-dialog-text');
  chrome.test.assertEq(
      `Use a name that's 11 characters or less`, errorTextElement.text);

  // Dismiss the error dialog.
  await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');

  // Enter ctrl+A to select the old text so we can replace it.
  const textInput = '#file-list > li input';
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, [textInput, 'A', true, false, false]);

  // Enter a valid name this time.
  const smallerPartitionName = 'smaller';
  const enterKey = [textInput, 'Enter', false, false, false];
  await remoteCall.inputText(appId, textInput, smallerPartitionName);
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, enterKey);

  // Wait for the renaming input element to disappear.
  await remoteCall.waitForElementLost(appId, textInput);

  // verify the partition was successfully renamed.
  expectedRows[2]![0] = smallerPartitionName;
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});
}

/**
 * Tests that the root html element .focus-outline-visible class appears for
 * keyboard interaction and is removed on mouse interaction.
 */
export async function keyboardFocusOutlineVisible() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Check: the html element should have focus-outline-visible class.
  const htmlFocusOutlineVisible = ['html.focus-outline-visible'];
  await remoteCall.waitForElementsCount(appId, htmlFocusOutlineVisible, 1);

  // Send mousedown to the toolbar delete button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#move-to-trash-button', 'mousedown']));

  // Check: the html element should not have focus-outline-visible class.
  await remoteCall.waitForElementLost(appId, htmlFocusOutlineVisible);
}

/**
 * Tests that the root html element .pointer-active class is added and removed
 * for mouse interaction.
 */
export async function keyboardFocusOutlineVisibleMouse() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Send mousedown to the toolbar delete button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#move-to-trash-button', 'mousedown']));

  // Check: the html element should have pointer-active class.
  const htmlPointerActive = ['html.pointer-active'];
  await remoteCall.waitForElementsCount(appId, htmlPointerActive, 1);

  // Check: the html element should not have focus-outline-visible class.
  await remoteCall.waitForElementLost(appId, ['html.focus-outline-visible']);

  // Send mouseup to the toolbar delete button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#move-to-trash-button', 'mouseup']));

  // Check: the html element should not have pointer-active class.
  await remoteCall.waitForElementLost(appId, htmlPointerActive);
}

/**
 * Tests that the root html element .pointer-active class will be removed with
 * pointerup event triggered by touch.
 */
export async function pointerActiveRemovedByTouch() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Send pointerdown to the list container.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#list-container', 'pointerdown']));

  // Check: the html element should have pointer-active class.
  const htmlPointerActive = ['html.pointer-active'];
  await remoteCall.waitForElementsCount(appId, htmlPointerActive, 1);

  // Send pointerup with touch to the list container.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId,
      ['#list-container', 'pointerup', {pointerType: 'touch'}]));

  // Check: the html element should not have pointer-active class.
  await remoteCall.waitForElementLost(appId, htmlPointerActive);
}

/**
 * Tests that the root html element .pointer-active class should not be added if
 * the PointerDown event is triggered by touch.
 */
export async function noPointerActiveOnTouch() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Send pointerdown with touch to the list container.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId,
      ['#list-container', 'pointerdown', {pointerType: 'touch'}]));

  // Check: the html element should not have pointer-active class.
  const htmlPointerActive = ['html.pointer-active'];
  await remoteCall.waitForElementLost(appId, htmlPointerActive);
}

/**
 * Test that selecting "Google Drive" in the directory tree with the keyboard
 * expands it and selects "My Drive".
 */
export async function keyboardSelectDriveDirectoryTree() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.world], [ENTRIES.hello]);

  // Focus the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.focusTree();

  // Wait for Google Drive root to be available.
  await directoryTree.waitForItemByLabel('Google Drive');

  // The directory tree is the first element focused, so pressing down whilst
  // focused should move through all the volumes until it reaches the drive
  // volume.
  const caller = getCaller();
  await repeatUntil(async () => {
    await directoryTree.focusNextItem();
    const focusedItem = await directoryTree.getFocusedItem();
    if (focusedItem &&
        directoryTree.getItemLabel(focusedItem) === 'Google Drive') {
      return true;
    }
    return pending(caller, 'Moving down until drive volume selected');
  });

  // Ensure it's focused.
  await directoryTree.waitForFocusedItemByLabel('Google Drive');

  // Activate it.
  await directoryTree.selectFocusedItem();

  // It should have expanded.
  await directoryTree.waitForItemToExpandByLabel('Google Drive');

  // My Drive should be selected.
  await directoryTree.waitForSelectedItemByLabel('My Drive');
}

/**
 * Tests that while the delete dialog is displayed, it is not possible to press
 * CONTROL-C to copy a file.
 */
export async function keyboardDisableCopyWhenDialogDisplayed() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);

  // Select a file for deletion.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Click delete button in the toolbar.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#delete-button']);

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Check: the dialog 'Cancel' button should be focused by default.
  const defaultButton =
      await remoteCall.waitForElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', defaultButton.text);

  // Try to copy file. We need to use execCommand as the command handler that
  // interprets key strokes will drop events if there is a dialog on screen.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Click the delete confirm dialog 'Cancel' button to cancel the deletion.
  await remoteCall.waitAndClickElement(appId, '.cr-dialog-cancel');

  // Check: the delete confirm dialog should close.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container.shown');

  // Send a paste command to the file-list.
  const key = ['#file-list', 'v', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Check: no files should be pasted.
  const files = TestEntryInfo.getExpectedRows([ENTRIES.hello]);
  await remoteCall.waitForFiles(appId, files);
}

/**
 * Tests Ctrl+N opens a new windows crbug.com/933302.
 */
export async function keyboardOpenNewWindow() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Grab the current open windows.
  const initialWindows = await remoteCall.getWindows();
  const initialWindowsCount = Object.keys(initialWindows).length;

  // Send Ctrl+N to open a new window.
  const key = ['#file-list', 'n', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Wait for the new window to appear.
  return repeatUntil(async () => {
    const caller = getCaller();
    const currentWindows = await remoteCall.getWindows();
    const currentWindowsIds = Object.keys(currentWindows);
    if (initialWindowsCount < currentWindowsIds.length) {
      return true;
    }
    return pending(
        caller,
        'Waiting for new window to open, current windows: ' +
            currentWindowsIds);
  });
}
