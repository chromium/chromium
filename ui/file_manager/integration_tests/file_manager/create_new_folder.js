// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Constants for interacting with the directory tree on the LHS of Files app.
 * When we are not in guest mode, we fill Google Drive with the basic entry set
 * which causes an extra tree-item to be added.
 */
const TREEITEM_DRIVE = '#directory-tree [entry-label="My Drive"]';
const TREEITEM_DOWNLOADS = '#directory-tree [entry-label="Downloads"]';

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

/**
 * Creates a new folder in the file list.
 *
 * @param {string} appId The Files app windowId.
 * @param {Array<TestEntryInfo>} initialEntrySet Initial set of entries.
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

  // Check: a new folder should be shown in the file list.
  let files = [['New folder', '--', 'Folder', '']].concat(
      TestEntryInfo.getExpectedRows(initialEntrySet));

  // Check: a new folder should be present in the directory tree.
  const newSubtreeChildItem =
      selector + ' .tree-children .tree-item[entry-label="New folder"]';
  await remoteCall.waitForElement(appId, newSubtreeChildItem);

  // Check: the text input should be shown in the file list.
  await remoteCall.waitForElement(appId, textInput);

  // Get all file list rows that have attribute 'renaming'.
  const renamingFileListRows = ['#file-list .table-row[renaming]'];
  let elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, renamingFileListRows);

  // Check: the new folder only should be 'renaming'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertEq(0, elements[0].text.indexOf('New folder--'));
  chrome.test.assertTrue('selected' in elements[0].attributes);

  // Get all file list rows that have attribute 'selected'.
  const selectedFileListRows = ['#file-list .table-row[selected]'];
  elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, selectedFileListRows);

  // Check: the new folder only should be 'selected'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertEq(0, elements[0].text.indexOf('New folder--'));
  chrome.test.assertTrue('renaming' in elements[0].attributes);

  // Type the test folder name.
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, [textInput, 'Test Folder Name']);

  // Press the Enter key.
  key = [textInput, 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Wait until renaming is complete.
  const renamingItem = '#file-list .table-row[renaming]';
  await remoteCall.waitForElementLost(appId, renamingItem);

  // Check: the test folder should be shown in the file list.
  files = [['Test Folder Name', '--', 'Folder', '']].concat(
      TestEntryInfo.getExpectedRows(initialEntrySet));

  // Check: the test folder should be present in the directory tree.
  const testSubtreeChildItem =
      selector + ' .tree-children .tree-item[entry-label="Test Folder Name"]';
  await remoteCall.waitForElement(appId, testSubtreeChildItem);

  // Get all file list rows that have attribute 'selected'.
  elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, selectedFileListRows);

  // Check: the test folder only should be 'selected'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertEq(
      0, elements[0].text.indexOf('Test Folder Name--'),
      'Actual text was: ' + elements[0].text);
}

/**
 * Expands the directory tree item given by |selector| (Downloads or Drive)
 * to reveal its subtree child items.
 *
 * @param {string} appId The Files app windowId.
 * @param {string} selector Downloads or Drive directory tree item selector.
 * @return {Promise} Promise fulfilled on success.
 */
async function expandRoot(appId, selector) {
  const expandIcon =
      selector + ' > .tree-row[has-children=true] > .expand-icon';

  // Wait for the subtree expand icon to appear.
  await remoteCall.waitForElement(appId, expandIcon);

  // Click the expand icon to expand the subtree.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [expandIcon]));

  // Wait for the subtree to expand and display its children.
  const expandedSubtree = selector + ' > .tree-children[expanded]';
  const element = await remoteCall.waitForElement(appId, expandedSubtree);

  // Verify expected subtree child item name.
  if (element.text.indexOf('photos') === -1) {
    chrome.test.fail('directory subtree child item "photos" not found');
  }
}

testcase.selectCreateFolderDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await expandRoot(appId, TREEITEM_DOWNLOADS);
  await selectFirstFileListItem(appId);
  await createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
};

testcase.createFolderDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await expandRoot(appId, TREEITEM_DOWNLOADS);
  await createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
};

testcase.createFolderNestedDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await expandRoot(appId, TREEITEM_DOWNLOADS);
  await remoteCall.navigateWithDirectoryTree(
      appId, '/Downloads/photos', 'My files/Downloads');
  await createNewFolder(appId, [], TREEITEM_DOWNLOADS);
};

testcase.createFolderDrive = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
  await expandRoot(appId, TREEITEM_DRIVE);
  await createNewFolder(appId, BASIC_DRIVE_ENTRY_SET, TREEITEM_DRIVE);
};
