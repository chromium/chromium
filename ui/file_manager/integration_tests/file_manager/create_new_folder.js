// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootPath, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, recursiveExpand, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Constants for interacting with the directory tree on the LHS of Files app.
 * When we are not in guest mode, we fill Google Drive with the basic entry set
 * which causes an extra tree-item to be added.
 */
export const TREEITEM_DRIVE = '#directory-tree [entry-label="My Drive"]';
export const TREEITEM_DOWNLOADS = '#directory-tree [entry-label="Downloads"]';

/**
 * Selects the first item in the file list.
 *
 * @param {string} appId The Files app windowId.
 * @return {Promise} Promise to be fulfilled on success.
 */
async function selectFirstFileListItem(appId) {
  // Ensure no file list items are selected.
  await remoteCall.waitForElementLost(appId, '#file-list [selected]');

  // Press DownArrow key to select an item.
  const key = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Await file list item selection.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Retrieve all selected items in the file list.
  const elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, ['#file-list [selected]']);

  // Check: the first list item only should be selected.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertEq('listitem-1', elements[0].attributes['id']);
}

/*
 * Searches for the file being renamed and gets current value for the renaming
 * field. Throws test assertion error if fails to find one.
 *
 * @param {string} appId The Files app windowId.
 * @return {string} Current value of the name.
 */
async function getFileRenamingValue(appId) {
  const renamingInput = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId,
      ['#file-list .table-row[renaming] input.rename']);
  chrome.test.assertEq(1, renamingInput.length);
  return renamingInput[0].value;
}

/**
 * Creates a new folder in the file list.
 *
 * @param {string} appId The Files app windowId.
 * @param {!Array<!TestEntryInfo>} initialEntrySet Initial set of entries.
 * @param {string} selector Downloads or Drive directory tree item selector.
 * @return {Promise} Promise to be fulfilled on success.
 */
async function createNewFolder(appId, initialEntrySet, selector) {
  const textInput = '#file-list .table-row[renaming] input.rename';

  // Focus the file-list.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('focus', appId, ['#file-list']));

  // Press Ctrl+E to create a new folder.
  let key = ['#file-list', 'e', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  let newFolderName = 'New folder';
  // Check: a new folder should be shown in the file list.
  let files = [[newFolderName, '--', 'Folder', '']].concat(
      TestEntryInfo.getExpectedRows(initialEntrySet));
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Check: a new folder should be present in the directory tree.
  const newSubtreeChildItem =
      `${selector} .tree-children .tree-item[entry-label="${newFolderName}"]`;
  await remoteCall.waitForElement(appId, newSubtreeChildItem);

  // Check: the text input should be shown in the file list.
  await remoteCall.waitForElement(appId, textInput);

  chrome.test.assertEq(newFolderName, await getFileRenamingValue(appId));

  // Get all file list rows that have attribute 'renaming'.
  const renamingFileListRows = ['#file-list .table-row[renaming]'];
  let elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, renamingFileListRows);

  // Check: the new folder only should be 'renaming'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertTrue('selected' in elements[0].attributes);

  chrome.test.assertEq(newFolderName, await getFileRenamingValue(appId));

  // Get all file list rows that have attribute 'selected'.
  const selectedFileListRows = ['#file-list .table-row[selected]'];
  elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, selectedFileListRows);

  // Check: the new folder only should be 'selected'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertTrue('renaming' in elements[0].attributes);

  // Type the test folder name.
  newFolderName = 'Test Folder Name';
  await remoteCall.inputText(appId, textInput, newFolderName);

  // Press the Enter key.
  key = [textInput, 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Wait until renaming is complete.
  const renamingItem = '#file-list .table-row[renaming]';
  await remoteCall.waitForElementLost(appId, renamingItem);

  // Check: the test folder should be shown in the file list.
  files = [[newFolderName, '--', 'Folder', '']].concat(
      TestEntryInfo.getExpectedRows(initialEntrySet));
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Check: the test folder should be present in the directory tree.
  const testSubtreeChildItem =
      `${selector} .tree-children .tree-item[entry-label="${newFolderName}"]`;
  await remoteCall.waitForElement(appId, testSubtreeChildItem);

  // Wait for the new folder to become selected in the file list.
  await remoteCall.waitForElement(
      appId, [`#file-list .table-row[file-name="${newFolderName}"][selected]`]);
}

testcase.selectCreateFolderDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await recursiveExpand(appId, '/My files/Downloads');
  await selectFirstFileListItem(appId);
  await createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
};

testcase.createFolderDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await recursiveExpand(appId, '/My files/Downloads');
  await createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
};

testcase.createFolderNestedDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await recursiveExpand(appId, '/My files/Downloads');
  await navigateWithDirectoryTree(appId, '/My files/Downloads/photos');
  await createNewFolder(appId, [], TREEITEM_DOWNLOADS);
};

testcase.createFolderDrive = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
  await recursiveExpand(appId, '/Google Drive/My Drive');
  await createNewFolder(appId, BASIC_DRIVE_ENTRY_SET, TREEITEM_DRIVE);
};
