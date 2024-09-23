// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, EntryType, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

/**
 * Select My files in directory tree and wait for load.
 *
 * @param appId ID of the app window.
 */
async function selectMyFiles(appId: string) {
  // Select My Files folder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('My files');

  // Wait for file list to display Downloads and Crostini.
  const downloadsRow = ['Downloads', '--', 'Folder'];
  const playFilesRow = ['Play files', '--', 'Folder'];
  const crostiniRow = ['Linux files', '--', 'Folder'];
  await remoteCall.waitForFiles(
      appId, [downloadsRow, playFilesRow, crostiniRow],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});
}

/**
 * Tests if MyFiles is displayed when flag is true.
 */
export async function showMyFiles() {
  const expectedElementLabels = [
    'Recent',
    'My files',
    'Downloads',
    'Linux files',
    'Play files',
    'Google Drive',
    'My Drive',
    'Shared with me',
    'Offline',
    'Trash',
  ];

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  const directoryTree = await DirectoryTreePageObject.create(appId);
  // Get the labels of the directory tree elements.
  const visibleElementLabels = await directoryTree.getVisibleItemLabels();
  chrome.test.assertEq(expectedElementLabels, visibleElementLabels);

  // Select Downloads folder.
  await directoryTree.selectItemByLabel('Downloads');

  // Check that My Files is displayed on breadcrumbs.
  const expectedBreadcrumbs = '/My files/Downloads';
  remoteCall.waitUntilCurrentDirectoryIsChanged(appId, expectedBreadcrumbs);
}

/**
 * Tests directory tree refresh doesn't hide Downloads folder.
 *
 * This tests a regression where Downloads folder would disappear because
 * MyFiles model and entry were being recreated on every update and
 * DirectoryTree expects NavigationModelItem to be the same instance through
 * updates.
 */
export async function directoryTreeRefresh() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('removable');

  // Select Downloads folder.
  await directoryTree.selectItemByLabel('Downloads');
}

/**
 * Tests My Files displaying Downloads on file list (RHS) and opening Downloads
 * from file list.
 */
export async function myFilesDisplaysAndOpensEntries() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Select My files in directory tree.
  await selectMyFiles(appId);

  // Double click on Download on file list.
  const downloadsFileListQuery = '#file-list [file-name="Downloads"]';
  await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId, [downloadsFileListQuery]);

  // Wait for file list to Downloads' content.
  await remoteCall.waitForFiles(
      appId, [ENTRIES.beautiful.getExpectedRow()],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Get the selected navigation tree item.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel('Downloads');
}

/**
 * Tests My files updating its children recursively.
 *
 * If it doesn't update its children recursively it can cause directory tree to
 * not show or hide sub-folders crbug.com/864453.
 */
export async function myFilesUpdatesChildren() {
  const hiddenFolder = new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: '.hidden-folder',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: '.hidden-folder',
    sizeText: '--',
    typeText: 'Folder',
  });

  // Add a hidden folder.
  // It can't be added via  remoteCall.setupAndWaitUntilReady, because it isn't
  // displayed and that function waits all entries to be displayed.
  await addEntries(['local'], [hiddenFolder]);

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Select Downloads folder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads');

  // Wait for gear menu to be displayed.
  await remoteCall.waitForElement(appId, '#gear-button');

  // Open the gear menu by clicking the gear button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for menu to not be hidden.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Wait for menu item to appear.
  await remoteCall.waitForElement(
      appId, '#gear-menu-toggle-hidden-files:not([disabled])');

  // Wait for menu item to appear.
  await remoteCall.waitForElement(
      appId, '#gear-menu-toggle-hidden-files:not([checked])');

  // Click the menu item.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-menu-toggle-hidden-files']);

  // Check the hidden folder to be displayed in RHS.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([hiddenFolder, ENTRIES.beautiful]),
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Wait for Downloads folder to have the expand icon because of hidden folder.
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'Downloads', /* hasChildren= */ true);

  // Expand Downloads to display the ".hidden-folder".
  await directoryTree.expandTreeItemByLabel('Downloads');

  // Check the hidden folder to be displayed in LHS.
  // Children of Downloads and named ".hidden-folder".
  await directoryTree.waitForChildItemByLabel('Downloads', '.hidden-folder');
}

/**
 * Check naming a folder after navigating inside MyFiles using file list (RHS).
 * crbug.com/889636.
 */
export async function myFilesFolderRename() {
  const textInput = '#file-list .table-row[renaming] input.rename';

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Select My files in directory tree.
  await selectMyFiles(appId);

  // Select Downloads via file list.
  await remoteCall.waitUntilSelected(appId, 'Downloads');

  // Open Downloads via file list.
  const fileListItem = '#file-list .table-row';
  const key = [fileListItem, 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Wait for Downloads to load.
  await remoteCall.waitForFiles(appId, [ENTRIES.photos.getExpectedRow()]);

  // Select photos via file list.
  await remoteCall.waitUntilSelected(appId, 'photos');

  // Press Ctrl+Enter for start renaming.
  const key2 = [fileListItem, 'Enter', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key2),
      'fakeKeyDown ctrl+Enter failed');

  // Wait for input for renaming to appear.
  // Check: the renaming text input should be shown in the file list.
  await remoteCall.waitForElement(appId, textInput);

  // Type new name.
  await remoteCall.inputText(appId, textInput, 'new name');

  // Send Enter key to the text input.
  const key3 = [textInput, 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key3),
      'fakeKeyDown failed');

  // Wait for new name to appear on the file list.
  const expectedRows2 = [['new name', '--', 'Folder', '']];
  await remoteCall.waitForFiles(
      appId, expectedRows2,
      {ignoreFileSize: true, ignoreLastModifiedTime: true});
}

/**
 * Tests that MyFiles only auto expands once.
 */
export async function myFilesAutoExpandOnce() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], [ENTRIES.beautiful]);
  const directoryTree = await DirectoryTreePageObject.create(appId);

  // Collapse MyFiles.
  await directoryTree.collapseTreeItemByLabel('My files');

  // Expand Google Drive and wait for its subtree to expand and display its
  // children.
  await directoryTree.expandTreeItemByLabel('Google Drive');

  // Click on My Drive
  await directoryTree.selectItemByLabel('My Drive');

  // Wait for My Drive to selected.
  await remoteCall.waitForFiles(
      appId, [ENTRIES.beautiful.getExpectedRow()],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Check that MyFiles is still collapsed.
  await directoryTree.waitForItemToCollapseByLabel('My files');
}

/**
 * Tests that My files refreshes its contents when PlayFiles is mounted.
 * crbug.com/946972.
 */
export async function myFilesUpdatesWhenAndroidVolumeMounts() {
  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Wait until Downloads is mounted.
  await remoteCall.waitForVolumesCount(1);

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

  // Click on My files and wait it to load.
  const downloadsRow = ['Downloads', '--', 'Folder'];
  const playFilesRow = ['Play files', '--', 'Folder'];
  const crostiniRow = ['Linux files', '--', 'Folder'];
  await directoryTree.selectItemByLabel('My files');
  await remoteCall.waitForFiles(
      appId, [downloadsRow, crostiniRow],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Mount Play files volume.
  await sendTestMessage({name: 'mountPlayFiles'});

  // Wait until it is mounted.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 2, []);

  // Android volume should automatically appear on directory tree and file list.
  await directoryTree.waitForItemByLabel('Play files');
  await remoteCall.waitForFiles(
      appId, [downloadsRow, playFilesRow, crostiniRow],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Un-mount Play files volume.
  await sendTestMessage({name: 'unmountPlayFiles'});

  // Wait until it is un-mounted.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Check: Play files should disappear from file list.
  await remoteCall.waitForFiles(
      appId, [downloadsRow, crostiniRow],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Check: Play files should disappear from directory tree.
  await directoryTree.waitForItemLostByLabel('Play files');
}

/**
 * Tests that toolbar delete is not shown for Downloads, or Linux files.
 */
export async function myFilesToolbarDelete() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Select My files in directory tree.
  await selectMyFiles(appId);

  // Select Downloads folder in list.
  await remoteCall.waitUntilSelected(appId, 'Downloads');

  // Test that the delete button isn't visible.
  const hiddenDeleteButton = '#delete-button[hidden]';
  await remoteCall.waitForElement(appId, hiddenDeleteButton);

  // Select fake entry Linux files folder in list.
  await remoteCall.waitUntilSelected(appId, 'Linux files');

  // Test that the delete button isn't visible.
  await remoteCall.waitForElement(appId, hiddenDeleteButton);

  // Mount crostini and test real root entry.
  await remoteCall.mountCrostini(appId);

  // Select My files in directory tree.
  await selectMyFiles(appId);

  // Select real Linux files folder in list.
  await remoteCall.waitUntilSelected(appId, 'Linux files');

  // Test that the delete button isn't visible.
  await remoteCall.waitForElement(appId, hiddenDeleteButton);
}
