// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Waits until a dialog with an OK button is shown, and accepts it by clicking
 * on the dialog's OK button.
 *
 * @param {string} appId The Files app windowId.
 * @return {Promise} Promise to be fulfilled after clicking the OK button.
 */
async function waitAndAcceptDialog(appId) {
  const okButton = '.cr-dialog-ok';

  // Wait for the Ok button to appear.
  await remoteCall.waitForElement(appId, okButton);

  // Click the Ok button.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [okButton]),
      'Dialog Ok button click failed');

  // Wait until the dialog closes.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container');
}

/**
 * Returns the visible directory tree item names.
 *
 * @param {string} appId The Files app windowId.
 * @return {!Promise<!Array<string>>} List of visible item names.
 */
function getVisibleDirectoryTreeItemNames(appId) {
  return remoteCall.callRemoteTestUtil('getTreeItems', appId, []);
}

/**
 * Waits until the directory tree item |name| appears.
 *
 * @param {string} appId The Files app windowId.
 * @param {string} name Directory tree item name.
 * @return {!Promise}
 */
function waitForDirectoryTreeItem(appId, name) {
  let caller = getCaller();
  return repeatUntil(async () => {
    if ((await getVisibleDirectoryTreeItemNames(appId)).indexOf(name) !== -1) {
      return true;
    }
    return pending(caller, 'Directory tree item %s not found.', name);
  });
}

/**
 * Waits until the directory tree item |name| disappears.
 *
 * @param {string} appId The Files app windowId.
 * @param {string} name Directory tree item name.
 * @return {!Promise}
 */
function waitForDirectoryTreeItemLost(appId, name) {
  let caller = getCaller();
  return repeatUntil(async () => {
    if ((await getVisibleDirectoryTreeItemNames(appId)).indexOf(name) === -1) {
      return true;
    }
    return pending(caller, 'Directory tree item %s still exists.', name);
  });
}

/**
 * Tests copying a file to the same file list.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 */
async function keyboardCopy(path) {
  const appId =
      await setupAndWaitUntilReady(path, [ENTRIES.world], [ENTRIES.world]);

  // Copy the file into the same file list.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('copyFile', appId, ['world.ogv']),
      'copyFile failed');
  // Check: the copied file should appear in the file list.
  const expectedEntryRows = [ENTRIES.world.getExpectedRow()].concat(
      [['world (1).ogv', '59 KB', 'OGG video']]);
  await remoteCall.waitForFiles(
      appId, expectedEntryRows, {ignoreLastModifiedTime: true});
  const files = await remoteCall.callRemoteTestUtil('getFileList', appId, []);
  if (path == RootPath.DRIVE) {
    // DriveFs doesn't preserve mtimes so they shouldn't match.
    chrome.test.assertTrue(files[0][3] != files[1][3], files[0][3]);
  } else {
    // The mtimes should match for Local files.
    chrome.test.assertTrue(files[0][3] == files[1][3], files[0][3]);
  }
}

/**
 * Tests deleting a file from the file list.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 */
async function keyboardDelete(path) {
  const appId =
      await setupAndWaitUntilReady(path, [ENTRIES.hello], [ENTRIES.hello]);

  // Delete the file from the file list.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('deleteFile', appId, ['hello.txt']),
      'deleteFile failed');

  // Run the delete entry confirmation dialog.
  await waitAndAcceptDialog(appId);

  // Check: the file list should be empty.
  await remoteCall.waitForFiles(appId, []);
}

/**
 * Tests deleting a folder from the file list. The folder is also shown in the
 * Files app directory tree, and should not be shown there when deleted.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 * @param {string} treeItem The directory tree item selector.
 */
async function keyboardDeleteFolder(path, treeItem) {
  const appId =
      await setupAndWaitUntilReady(path, [ENTRIES.photos], [ENTRIES.photos]);

  // Expand the directory tree |treeItem|.
  await expandRoot(appId, treeItem);

  // Check: the folder should be shown in the directory tree.
  await waitForDirectoryTreeItem(appId, 'photos');

  // Delete the folder entry from the file list.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('deleteFile', appId, ['photos']),
      'deleteFile failed');

  // Run the delete entry confirmation dialog.
  await waitAndAcceptDialog(appId);

  // Check: the file list should be empty.
  await remoteCall.waitForFiles(appId, []);

  // Check: the folder should not be shown in the directory tree.
  await waitForDirectoryTreeItemLost(appId, 'photos');
}

/**
 * Renames a file.
 *
 * @param {string} appId The Files app windowId.
 * @param {string} oldName Old name of a file.
 * @param {string} newName New name of a file.
 * @return {Promise} Promise to be fulfilled on success.
 */
async function renameFile(appId, oldName, newName) {
  const textInput = '#file-list .table-row[renaming] input.rename';

  // Select the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('selectFile', appId, [oldName]),
      'selectFile failed');

  // Press Ctrl+Enter key to rename the file.
  const key = ['#file-list', 'Enter', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Check: the renaming text input should be shown in the file list.
  await remoteCall.waitForElement(appId, textInput);

  // Type new file name.
  await remoteCall.callRemoteTestUtil('inputText', appId, [textInput, newName]);

  // Send Enter key to the text input.
  const key2 = [textInput, 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key2));
}

/**
 * Tests renaming a folder. An extra enter key is sent to the file list during
 * renaming to check the folder cannot be entered while it is being renamed.
 *
 * @param {string} path Initial path (Downloads or Drive).
 * @param {string} treeItem The directory tree item selector.
 * @return {Promise} Promise to be fulfilled on success.
 */
async function testRenameFolder(path, treeItem) {
  const textInput = '#file-list .table-row[renaming] input.rename';
  const appId =
      await setupAndWaitUntilReady(path, [ENTRIES.photos], [ENTRIES.photos]);

  // Expand the directory tree |treeItem|.
  await expandRoot(appId, treeItem);

  // Check: the photos folder should be shown in the directory tree.
  await waitForDirectoryTreeItem(appId, 'photos');
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
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, [textInput, 'bbq photos']);

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
  await waitForDirectoryTreeItem(appId, 'bbq photos');
}


/**
 * Tests renaming a file.
 *
 * @param {string} path Initial path (Downloads or Drive).
 * @return {Promise} Promise to be fulfilled on success.
 */
async function testRenameFile(path) {
  const newFile = [['New File Name.txt', '51 bytes', 'Plain text', '']];

  const appId =
      await setupAndWaitUntilReady(path, [ENTRIES.hello], [ENTRIES.hello]);

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

testcase.keyboardCopyDownloads = () => {
  return keyboardCopy(RootPath.DOWNLOADS);
};

testcase.keyboardCopyDrive = () => {
  return keyboardCopy(RootPath.DRIVE);
};

testcase.keyboardDeleteDownloads = () => {
  return keyboardDelete(RootPath.DOWNLOADS);
};

testcase.keyboardDeleteDrive = () => {
  return keyboardDelete(RootPath.DRIVE);
};

testcase.keyboardDeleteFolderDownloads = () => {
  return keyboardDeleteFolder(RootPath.DOWNLOADS, TREEITEM_DOWNLOADS);
};

testcase.keyboardDeleteFolderDrive = () => {
  return keyboardDeleteFolder(RootPath.DRIVE, TREEITEM_DRIVE);
};

testcase.renameFileDownloads = () => {
  return testRenameFile(RootPath.DOWNLOADS);
};

testcase.renameFileDrive = () => {
  return testRenameFile(RootPath.DRIVE);
};

testcase.renameNewFolderDownloads = () => {
  return testRenameFolder(RootPath.DOWNLOADS, TREEITEM_DOWNLOADS);
};

testcase.renameNewFolderDrive = () => {
  return testRenameFolder(RootPath.DRIVE, TREEITEM_DRIVE);
};

/**
 * Test that selecting "Google Drive" in the directory tree with the keyboard
 * expands it and selects "My Drive".
 */
testcase.keyboardSelectDriveDirectoryTree = async () => {
  // Open Files app.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.world], [ENTRIES.hello]);

  // Focus the directory tree.
  await remoteCall.callRemoteTestUtil('focus', appId, ['#directory-tree']);

  // Select Google Drive in the directory tree; as of the time of writing, it's
  // the last item so this happens to work.
  await remoteCall.fakeKeyDown(
      appId, '#directory-tree', 'End', false, false, false);

  // Ensure it's selected.
  await remoteCall.waitForElement(appId, ['.drive-volume [selected]']);

  // Activate it.
  await remoteCall.fakeKeyDown(
      appId, '#directory-tree .drive-volume', 'Enter', false, false, false);

  // It should have expanded.
  await remoteCall.waitForElement(
      appId, ['.drive-volume .tree-children[expanded]']);

  // My Drive should be selected.
  await remoteCall.waitForElement(
      appId, ['[full-path-for-testing="/root"] [selected]']);
};

/**
 * Tests that while the delete dialog is displayed, it is not possible to press
 * CONTROL-C to copy a file.
 */
testcase.keyboardDisableCopyWhenDialogDisplayed = async () => {
  // Open Files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Select a file for deletion.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']),
      'selectFile failed');
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Click delete button in the toolbar.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['button#delete-button']);

  // Confirm that the delete confirmation dialog is shown.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Try to copy file. We need to use execCommand as the command handler that
  // interprets key strokes will drop events if there is a dialog on screen.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Press Cancel button to stop the delete operation.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['button.cr-dialog-cancel']));

  // Wait for dialog to disappear.
  chrome.test.assertTrue(
      await remoteCall.waitForElementLost(appId, '.cr-dialog-container.shown'));
  const key = ['#file-list', 'v', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Check no files were pasted.
  const files = TestEntryInfo.getExpectedRows([ENTRIES.hello]);
  await remoteCall.waitForFiles(appId, files);
};

/**
 * Tests Ctrl+N opens a new windows crbug.com/933302.
 */
testcase.keyboardOpenNewWindow = async () => {
  // Open Files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Grab the current open windows.
  const initialWindows =
      await remoteCall.callRemoteTestUtil('getWindows', null, []);
  const initialWindowsCount = Object.keys(initialWindows).length;
  console.log(JSON.stringify(initialWindows));

  // Send Ctrl+N to open a new window.
  const key = ['#file-list', 'n', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Wait for the new window to appear.
  return repeatUntil(async () => {
    const caller = getCaller();
    const currentWindows =
        await remoteCall.callRemoteTestUtil('getWindows', null, []);
    const currentWindowsIds = Object.keys(currentWindows);
    if (initialWindowsCount < currentWindowsIds.length) {
      return true;
    }
    return pending(
        caller,
        'Waiting for new window to open, current windows: ' +
            currentWindowsIds);
  });
};
