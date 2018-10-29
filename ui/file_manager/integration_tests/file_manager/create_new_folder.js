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
function selectFirstFileListItem(appId) {
  return Promise.resolve().then(function() {
    // Ensure no file list items are selected.
    return remoteCall.waitForElementLost(appId, '#file-list [selected]');
  }).then(function() {
    // Press DownArrow key to select an item.
    const key = ['#file-list', 'ArrowDown', false, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Await file list item selection.
    return remoteCall.waitForElement(appId, '.table-row[selected]');
  }).then(function() {
    // Retrieve all selected items in the file list.
    return remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, ['#file-list [selected]']);
  }).then(function(elements) {
    // Check: the first list item only should be selected.
    chrome.test.assertEq(1, elements.length);
    chrome.test.assertEq('listitem-1', elements[0].attributes['id']);
  });
}

/**
 * Creates a new folder in the file list.
 *
 * @param {string} appId The Files app windowId.
 * @param {Array<TestEntryInfo>} initialEntrySet Initial set of entries.
 * @param {string} selector Downloads or Drive directory tree item selector.
 * @return {Promise} Promise to be fulfilled on success.
 */
function createNewFolder(appId, initialEntrySet, selector) {
  const textInput = '#file-list .table-row[renaming] input.rename';

  return new Promise(function(resolve) {
    // Focus the file-list.
    remoteCall.callRemoteTestUtil('focus', appId, ['#file-list'], resolve);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Press Ctrl+E to create a new folder.
    const key = ['#file-list', 'e', true, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Check: a new folder should be shown in the file list.
    const files = [['New folder', '--', 'Folder', '']].concat(
        TestEntryInfo.getExpectedRows(initialEntrySet));
    return remoteCall.waitForFiles(
        appId, files, {ignoreLastModifiedTime: true});
  }).then(function() {
    // Check: a new folder should be present in the directory tree.
    const newSubtreeChildItem =
        selector + ' .tree-children .tree-item[entry-label="New folder"]';
    return remoteCall.waitForElement(appId, newSubtreeChildItem);
  }).then(function() {
    // Check: the text input should be shown in the file list.
    return remoteCall.waitForElement(appId, textInput);
  }).then(function() {
    // Get all file list rows that have attribute 'renaming'.
    const renamingFileListRows = ['#file-list .table-row[renaming]'];
    return remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, renamingFileListRows);
  }).then(function(elements) {
    // Check: the new folder only should be 'renaming'.
    chrome.test.assertEq(1, elements.length);
    chrome.test.assertEq(0, elements[0].text.indexOf('New folder--'));
    chrome.test.assertTrue('selected' in elements[0].attributes);
  }).then(function() {
    // Get all file list rows that have attribute 'selected'.
    const selectedFileListRows = ['#file-list .table-row[selected]'];
    return remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, selectedFileListRows);
  }).then(function(elements) {
    // Check: the new folder only should be 'selected'.
    chrome.test.assertEq(1, elements.length);
    chrome.test.assertEq(0, elements[0].text.indexOf('New folder--'));
    chrome.test.assertTrue('renaming' in elements[0].attributes);
  }).then(function() {
    // Type the test folder name.
    return remoteCall.callRemoteTestUtil(
        'inputText', appId, [textInput, 'Test Folder Name']);
  }).then(function() {
    // Press the Enter key.
    const key = [textInput, 'Enter', false, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Wait until renaming is complete.
    const renamingItem = '#file-list .table-row[renaming]';
    return remoteCall.waitForElementLost(appId, renamingItem);
  }).then(function() {
    // Check: the test folder should be shown in the file list.
    const files = [['Test Folder Name', '--', 'Folder', '']].concat(
        TestEntryInfo.getExpectedRows(initialEntrySet));
    return remoteCall.waitForFiles(
        appId, files, {ignoreLastModifiedTime: true});
  }).then(function(elements) {
    // Check: the test folder should be present in the directory tree.
    const testSubtreeChildItem = selector +
        ' .tree-children .tree-item[entry-label="Test Folder Name"]';
    return remoteCall.waitForElement(appId, testSubtreeChildItem);
  }).then(function() {
    // Get all file list rows that have attribute 'selected'.
    const selectedFileListRows = ['#file-list .table-row[selected]'];
    return remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, selectedFileListRows);
  }).then(function(elements) {
    // Check: the test folder only should be 'selected'.
    chrome.test.assertEq(1, elements.length);
    chrome.test.assertEq(
        0, elements[0].text.indexOf('Test Folder Name--'),
        'Actual text was: ' + elements[0].text);
  });
}

/**
 * Expands the directory tree item given by |selector| (Downloads or Drive)
 * to reveal its subtree child items.
 *
 * @param {string} appId The Files app windowId.
 * @param {string} selector Downloads or Drive directory tree item selector.
 * @return {Promise} Promise fulfilled on success.
 */
function expandRoot(appId, selector) {
  const expandIcon = selector + ' > .tree-row > .expand-icon';

  return new Promise(function(resolve) {
    // Wait for the subtree expand icon to appear.
    remoteCall.waitForElement(appId, expandIcon).then(resolve);
  }).then(function() {
    // Click the expand icon to expand the subtree.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [expandIcon]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Wait for the subtree to expand and display its children.
    const expandedSubtree = selector + ' > .tree-children[expanded]';
    return remoteCall.waitForElement(appId, expandedSubtree);
  }).then(function(element) {
    // Verify expected subtree child item name.
    if (element.text.indexOf('photos') === -1)
      chrome.test.fail('directory subtree child item "photos" not found');
  });
}

testcase.selectCreateFolderDownloads = function() {
  let appId;

  const promise = new Promise(function(resolve) {
    setupAndWaitUntilReady(
        null, RootPath.DOWNLOADS, resolve, BASIC_LOCAL_ENTRY_SET, []);
  }).then(function(results) {
    appId = results.windowId;
    return expandRoot(appId, TREEITEM_DOWNLOADS);
  }).then(function() {
    return selectFirstFileListItem(appId);
  }).then(function() {
    return createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
  });

  testPromise(promise);
};

testcase.createFolderDownloads = function() {
  let appId;

  const promise = new Promise(function(resolve) {
    setupAndWaitUntilReady(
        null, RootPath.DOWNLOADS, resolve, BASIC_LOCAL_ENTRY_SET, []);
  }).then(function(results) {
    appId = results.windowId;
    return expandRoot(appId, TREEITEM_DOWNLOADS);
  }).then(function() {
    return createNewFolder(appId, BASIC_LOCAL_ENTRY_SET, TREEITEM_DOWNLOADS);
  });

  testPromise(promise);
};

testcase.createFolderNestedDownloads = function() {
  let appId;

  const promise =
      new Promise(function(resolve) {
        setupAndWaitUntilReady(
            null, RootPath.DOWNLOADS, resolve, BASIC_LOCAL_ENTRY_SET, []);
      })
          .then(function(results) {
            appId = results.windowId;
            return expandRoot(appId, TREEITEM_DOWNLOADS);
          })
          .then(function() {
            return remoteCall.navigateWithDirectoryTree(
                appId, '/photos', 'My files/Downloads');
          })
          .then(function() {
            return remoteCall.waitForFiles(
                appId, [], {ignoreLastModifiedTime: true});
          })
          .then(function() {
            return createNewFolder(appId, [], TREEITEM_DOWNLOADS);
          });

  testPromise(promise);
};

testcase.createFolderDrive = function() {
  let appId;

  const promise = new Promise(function(resolve) {
    setupAndWaitUntilReady(
        null, RootPath.DRIVE, resolve, [], BASIC_DRIVE_ENTRY_SET);
  }).then(function(results) {
    appId = results.windowId;
    return expandRoot(appId, TREEITEM_DRIVE);
  }).then(function() {
    return createNewFolder(appId, BASIC_DRIVE_ENTRY_SET, TREEITEM_DRIVE);
  });

  testPromise(promise);
};
