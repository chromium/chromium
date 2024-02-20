// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootPath, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Constants for interacting with the directory tree on the LHS of Files app.
 * When we are not in guest mode, we fill Google Drive with the basic entry set
 * which causes an extra tree-item to be added.
 */
const TREEITEM_DRIVE = 'My Drive';
const TREEITEM_DOWNLOADS = 'Downloads';

/**
 * Selects the first item in the file list.
 *
 * @param appId The Files app windowId.
 * @return Promise to be fulfilled on success.
 */
async function selectFirstFileListItem(appId: string): Promise<void> {
  // Ensure no file list items are selected.
  await remoteCall.waitForElementLost(appId, '#file-list [selected]');

  // Press DownArrow key to select an item.
  const key = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Await file list item selection.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Retrieve all selected items in the file list.
  const elements =
      await remoteCall.queryElements(appId, ['#file-list [selected]']);

  // Check: the first list item only should be selected.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertEq('listitem-1', elements[0]!.attributes['id']);
}

/*
 * Searches for the file being renamed and gets current value for the renaming
 * field. Throws test assertion error if fails to find one.
 *
 * @param appId The Files app windowId.
 * @return Current value of the name.
 */
async function getFileRenamingValue(appId: string): Promise<string> {
  const renamingInput = await remoteCall.queryElements(
      appId, ['#file-list .table-row[renaming] input.rename']);
  chrome.test.assertEq(1, renamingInput.length);
  return renamingInput[0]!.value!;
}

/**
 * Creates a new folder in the file list.
 *
 * @param appId The Files app windowId.
 * @param initialEntrySet Initial set of entries.
 * @param label Downloads or Drive directory tree item label.
 * @return Promise to be fulfilled on success.
 */
async function createNewFolder(
    appId: string, initialEntrySet: TestEntryInfo[],
    label: string): Promise<void> {
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

  // Check: the text input should be shown in the file list.
  await remoteCall.waitForElement(appId, textInput);

  chrome.test.assertEq(newFolderName, await getFileRenamingValue(appId));

  // Get all file list rows that have attribute 'renaming'.
  const renamingFileListRows = ['#file-list .table-row[renaming]'];
  let elements = await remoteCall.queryElements(appId, renamingFileListRows);

  // Check: the new folder only should be 'renaming'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertTrue('selected' in elements[0]!.attributes);

  chrome.test.assertEq(newFolderName, await getFileRenamingValue(appId));

  // Get all file list rows that have attribute 'selected'.
  const selectedFileListRows = ['#file-list .table-row[selected]'];
  elements = await remoteCall.queryElements(appId, selectedFileListRows);

  // Check: the new folder only should be 'selected'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertTrue('renaming' in elements[0]!.attributes);

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

  // Wait for the new folder to become selected in the file list.
  await remoteCall.waitForElement(
      appId, [`#file-list .table-row[file-name="${newFolderName}"][selected]`]);

  // Check: a new folder should be present in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel(label);
  await directoryTree.waitForChildItemByLabel(label, newFolderName);
}

export async function selectCreateFolderDownloads() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.recursiveExpand('/My files/Downloads');
  await selectFirstFileListItem(appId);
  await createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
}

export async function createFolderDownloads() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.recursiveExpand('/My files/Downloads');
  await createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
}

export async function createFolderNestedDownloads() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.recursiveExpand('/My files/Downloads');
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await createNewFolder(appId, [], 'photos');
}

export async function createFolderDrive() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.recursiveExpand('/Google Drive/My Drive');
  await createNewFolder(appId, BASIC_DRIVE_ENTRY_SET, TREEITEM_DRIVE);
}
