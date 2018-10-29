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
function waitAndAcceptDialog(appId) {
  const okButton = '.cr-dialog-ok';
  return new Promise(function(resolve) {
    // Wait for the Ok button to appear.
    remoteCall.waitForElement(appId, okButton).then(resolve);
  }).then(function() {
    // Click the Ok button.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [okButton]);
  }).then(function(result) {
    chrome.test.assertTrue(result, 'Dialog Ok button click failed');
    // Wait until the dialog closes.
    return remoteCall.waitForElementLost(appId, '.cr-dialog-container');
  });
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
  return repeatUntil(function() {
    return getVisibleDirectoryTreeItemNames(appId).then(function(names) {
      if (names.indexOf(name) !== -1) {
        return true;
      } else {
        return pending(caller, 'Directory tree item %s not found.', name);
      }
    });
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
  return repeatUntil(function() {
    return getVisibleDirectoryTreeItemNames(appId).then(function(names) {
      if (names.indexOf(name) === -1) {
        return true;
      } else {
        return pending(caller, 'Directory tree item %s still exists.', name);
      }
    });
  });
}

/**
 * Tests copying a file to the same file list.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 */
function keyboardCopy(path) {
  let appId;

  return new Promise(function(resolve) {
    setupAndWaitUntilReady(
        null, path, resolve, [ENTRIES.world], [ENTRIES.world]);
  }).then(function(results) {
    appId = results.windowId;
    // Copy the file into the same file list.
    return remoteCall.callRemoteTestUtil('copyFile', appId, ['world.ogv']);
  }).then(function(result) {
    chrome.test.assertTrue(result, 'copyFile failed');
    // Check: the copied file should appear in the file list.
    const expectedEntryRows = [ENTRIES.world.getExpectedRow()].concat(
        [['world (1).ogv', '59 KB', 'OGG video']]);
    return remoteCall.waitForFiles(
        appId, expectedEntryRows, {ignoreLastModifiedTime: true});
  });
}

/**
 * Tests deleting a file from the file list.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 */
function keyboardDelete(path) {
  let appId;

  return new Promise(function(resolve) {
    setupAndWaitUntilReady(
        null, path, resolve, [ENTRIES.hello], [ENTRIES.hello]);
  }).then(function(results) {
    appId = results.windowId;
    // Delete the file from the file list.
    return remoteCall.callRemoteTestUtil('deleteFile', appId, ['hello.txt']);
  }).then(function(result) {
    chrome.test.assertTrue(result, 'deleteFile failed');
    // Run the delete entry confirmation dialog.
    return waitAndAcceptDialog(appId);
  }).then(function() {
    // Check: the file list should be empty.
    return remoteCall.waitForFiles(appId, []);
  });
}

/**
 * Tests deleting a folder from the file list. The folder is also shown in the
 * Files app directory tree, and should not be shown there when deleted.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 * @param {string} treeItem The directory tree item selector.
 */
function keyboardDeleteFolder(path, treeItem) {
  let appId;

  return new Promise(function(resolve) {
    setupAndWaitUntilReady(
        null, path, resolve, [ENTRIES.photos], [ENTRIES.photos]);
  }).then(function(results) {
    appId = results.windowId;
    // Expand the directory tree |treeItem|.
    return expandRoot(appId, treeItem);
  }).then(function() {
    // Check: the folder should be shown in the directory tree.
    return waitForDirectoryTreeItem(appId, 'photos');
  }).then(function() {
    // Delete the folder entry from the file list.
    return remoteCall.callRemoteTestUtil('deleteFile', appId, ['photos']);
  }).then(function(result) {
    chrome.test.assertTrue(result, 'deleteFile failed');
    // Run the delete entry confirmation dialog.
    return waitAndAcceptDialog(appId);
  }).then(function() {
    // Check: the file list should be empty.
    return remoteCall.waitForFiles(appId, []);
  }).then(function() {
    // Check: the folder should not be shown in the directory tree.
    return waitForDirectoryTreeItemLost(appId, 'photos');
  });
}

/**
 * Renames a file.
 *
 * @param {string} appId The Files app windowId.
 * @param {string} oldName Old name of a file.
 * @param {string} newName New name of a file.
 * @return {Promise} Promise to be fulfilled on success.
 */
function renameFile(appId, oldName, newName) {
  const textInput = '#file-list .table-row[renaming] input.rename';

  return Promise.resolve().then(function() {
    // Select the file.
    return remoteCall.callRemoteTestUtil('selectFile', appId, [oldName]);
  }).then(function(result) {
    chrome.test.assertTrue(result, 'selectFile failed');
    // Press Ctrl+Enter key to rename the file.
    const key = ['#file-list', 'Enter', true, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Check: the renaming text input should be shown in the file list.
    return remoteCall.waitForElement(appId, textInput);
  }).then(function() {
    // Type new file name.
    return remoteCall.callRemoteTestUtil(
        'inputText', appId, [textInput, newName]);
  }).then(function() {
    // Send Enter key to the text input.
    const key = [textInput, 'Enter', false, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
  });
}

/**
 * Tests renaming a folder. An extra enter key is sent to the file list during
 * renaming to check the folder cannot be entered while it is being renamed.
 *
 * @param {string} path Initial path (Downloads or Drive).
 * @param {string} treeItem The directory tree item selector.
 * @return {Promise} Promise to be fulfilled on success.
 */
function testRenameFolder(path, treeItem) {
  let appId;

  const textInput = '#file-list .table-row[renaming] input.rename';

  return new Promise(function(resolve) {
    setupAndWaitUntilReady(
        null, path, resolve, [ENTRIES.photos], [ENTRIES.photos]);
  }).then(function(results) {
    // Expand the directory tree |treeItem|.
    appId = results.windowId;
    return expandRoot(appId, treeItem);
  }).then(function() {
    // Check: the photos folder should be shown in the directory tree.
    return waitForDirectoryTreeItem(appId, 'photos');
  }).then(function() {
    // Focus the file-list.
    return remoteCall.callRemoteTestUtil('focus', appId, ['#file-list']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Press ArrowDown to select the photos folder.
    const select = ['#file-list', 'ArrowDown', false, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', appId, select);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Await file list item selection.
    const selectedItem = '#file-list .table-row[selected]';
    return remoteCall.waitForElement(appId, selectedItem);
  }).then(function() {
    // Press Ctrl+Enter to rename the photos folder.
    const key = ['#file-list', 'Enter', true, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Check: the renaming text input should be shown in the file list.
    return remoteCall.waitForElement(appId, textInput);
  }).then(function() {
    // Type the new folder name.
    return remoteCall.callRemoteTestUtil(
        'inputText', appId, [textInput, 'bbq photos']);
  }).then(function() {
    // Send Enter to the list to attempt to enter the directory.
    const key = ['#list-container', 'Enter', false, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Send Enter to the text input to complete renaming.
    const key = [textInput, 'Enter', false, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Wait until renaming is complete.
    const renamingItem = '#file-list .table-row[renaming]';
    return remoteCall.waitForElementLost(appId, renamingItem);
  }).then(function() {
    // Check: the renamed folder should be shown in the file list.
    const expectedRows = [['bbq photos', '--', 'Folder', '']];
    return remoteCall.waitForFiles(
        appId, expectedRows, {ignoreLastModifiedTime: true});
  }).then(function() {
    // Check: the renamed folder should be shown in the directory tree.
    return waitForDirectoryTreeItem(appId, 'bbq photos');
  });
}

/**
 * Tests renaming a file.
 *
 * @param {string} path Initial path (Downloads or Drive).
 * @return {Promise} Promise to be fulfilled on success.
 */
function testRenameFile(path) {
  let appId;

  const newFile = [['New File Name.txt', '51 bytes', 'Plain text', '']];

  return new Promise(function(resolve) {
    setupAndWaitUntilReady(
        null, path, resolve, [ENTRIES.hello], [ENTRIES.hello]);
  }).then(function(results) {
    // Rename the file.
    appId = results.windowId;
    return renameFile(appId, 'hello.txt', 'New File Name.txt');
  }).then(function() {
    // Wait until renaming completes.
    return remoteCall.waitForElementLost(appId, '#file-list [renaming]');
  }).then(function() {
    // Check: the new file name should be shown in the file list.
    return remoteCall.waitForFiles(
        appId, newFile, {ignoreLastModifiedTime: true});
  }).then(function() {
    // Try renaming the new file to an invalid file name.
    return renameFile(appId, 'New File Name.txt', '.hidden file');
  }).then(function() {
    // Check: the error dialog should be shown.
    return waitAndAcceptDialog(appId);
  }).then(function() {
    // Check: the new file name should not be changed.
    return remoteCall.waitForFiles(
        appId, newFile, {ignoreLastModifiedTime: true});
  });
}

testcase.keyboardCopyDownloads = function() {
  testPromise(keyboardCopy(RootPath.DOWNLOADS));
};

testcase.keyboardCopyDrive = function() {
  testPromise(keyboardCopy(RootPath.DRIVE));
};

testcase.keyboardDeleteDownloads = function() {
  testPromise(keyboardDelete(RootPath.DOWNLOADS));
};

testcase.keyboardDeleteDrive = function() {
  testPromise(keyboardDelete(RootPath.DRIVE));
};

testcase.keyboardDeleteFolderDownloads = function() {
  testPromise(keyboardDeleteFolder(RootPath.DOWNLOADS, TREEITEM_DOWNLOADS));
};

testcase.keyboardDeleteFolderDrive = function() {
  testPromise(keyboardDeleteFolder(RootPath.DRIVE, TREEITEM_DRIVE));
};

testcase.renameFileDownloads = function() {
  testPromise(testRenameFile(RootPath.DOWNLOADS));
};

testcase.renameFileDrive = function() {
  testPromise(testRenameFile(RootPath.DRIVE));
};

testcase.renameNewFolderDownloads = function() {
  testPromise(testRenameFolder(RootPath.DOWNLOADS, TREEITEM_DOWNLOADS));
};

testcase.renameNewFolderDrive = function() {
  testPromise(testRenameFolder(RootPath.DRIVE, TREEITEM_DRIVE));
};
