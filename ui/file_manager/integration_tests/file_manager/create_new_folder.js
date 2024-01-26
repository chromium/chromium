// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootPath, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Constants for interacting with the directory tree on the LHS of Files app.
 * When we are not in guest mode, we fill Google Drive with the basic entry set
 * which causes an extra tree-item to be added.
 */
export const TREEITEM_DRIVE = 'My Drive';
export const TREEITEM_DOWNLOADS = 'Downloads';

/**
 * Selects the first item in the file list.
 *
 * @param {string} appId The Files app windowId.
 * @return {Promise<void>} Promise to be fulfilled on success.
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
// @ts-ignore: error TS7006: Parameter 'appId' implicitly has an 'any' type.
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
 * @param {string} label Downloads or Drive directory tree item label.
 * @return {Promise<void>} Promise to be fulfilled on success.
 */
async function createNewFolder(appId, initialEntrySet, label) {
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
  // @ts-ignore: error TS2345: Argument of type '{ ignoreLastModifiedTime: true;
  // }' is not assignable to parameter of type '{ orderCheck: boolean | null |
  // undefined; ignoreFileSize: boolean | null | undefined;
  // ignoreLastModifiedTime: boolean | null | undefined; }'.
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

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
  // @ts-ignore: error TS2345: Argument of type '{ ignoreLastModifiedTime: true;
  // }' is not assignable to parameter of type '{ orderCheck: boolean | null |
  // undefined; ignoreFileSize: boolean | null | undefined;
  // ignoreLastModifiedTime: boolean | null | undefined; }'.
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Wait for the new folder to become selected in the file list.
  await remoteCall.waitForElement(
      appId, [`#file-list .table-row[file-name="${newFolderName}"][selected]`]);

  // Check: a new folder should be present in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.expandTreeItemByLabel(label);
  await directoryTree.waitForChildItemByLabel(label, newFolderName);
}

// @ts-ignore: error TS4111: Property 'selectCreateFolderDownloads' comes from
// an index signature, so it must be accessed with
// ['selectCreateFolderDownloads'].
testcase.selectCreateFolderDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.recursiveExpand('/My files/Downloads');
  await selectFirstFileListItem(appId);
  await createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
};

// @ts-ignore: error TS4111: Property 'createFolderDownloads' comes from an
// index signature, so it must be accessed with ['createFolderDownloads'].
testcase.createFolderDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.recursiveExpand('/My files/Downloads');
  await createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
};

// @ts-ignore: error TS4111: Property 'createFolderNestedDownloads' comes from
// an index signature, so it must be accessed with
// ['createFolderNestedDownloads'].
testcase.createFolderNestedDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.recursiveExpand('/My files/Downloads');
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await createNewFolder(appId, [], 'photos');
};

// @ts-ignore: error TS4111: Property 'createFolderDrive' comes from an index
// signature, so it must be accessed with ['createFolderDrive'].
testcase.createFolderDrive = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.recursiveExpand('/Google Drive/My Drive');
  await createNewFolder(appId, BASIC_DRIVE_ENTRY_SET, TREEITEM_DRIVE);
};
