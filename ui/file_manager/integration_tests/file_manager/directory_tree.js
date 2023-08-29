// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, EntryType, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Tests that when the current folder is changed, the 'selected' attribute
 * appears in the selected folder .tree-row and the "Current directory" aria
 * description is present on the corresponding tree item.
 */
testcase.directoryTreeActiveDirectory = async () => {
  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Change to My files folder.
  await directoryTree.navigateToPath('/My files');

  // Check: the My files folder should be the selected tree row.
  await directoryTree.waitForSelectedItemByLabel('My files');
  // Check: the corresponding tree item should be focused and have the "Current
  // directory" aria description.
  await directoryTree.waitForFocusedItemByLabel('My files');
  await directoryTree.waitForCurrentDirectoryItemByLabel('My files');

  // Change to Downloads folder.
  await directoryTree.navigateToPath('/My files/Downloads');

  // Check: the Downloads folder should be the selected tree row.
  await directoryTree.waitForSelectedItemByLabel('Downloads');
  // Check: the corresponding tree item should be focused and have the "Current
  // directory" aria description.
  await directoryTree.waitForFocusedItemByLabel('Downloads');
  await directoryTree.waitForCurrentDirectoryItemByLabel('Downloads');

  // Change to Google Drive volume's My Drive folder.
  await directoryTree.navigateToPath('/My Drive');

  // Check: the My Drive folder should be the selected tree row.
  await directoryTree.waitForSelectedItemByLabel('My Drive');
  // Check: the corresponding tree item should be focused and have the "Current
  // directory" aria description.
  await directoryTree.waitForFocusedItemByLabel('My Drive');
  await directoryTree.waitForCurrentDirectoryItemByLabel('My Drive');

  // Change to Recent folder.
  await directoryTree.navigateToPath('/Recent');

  // Check: the Recent folder should be the selected tree row.
  await directoryTree.waitForSelectedItemByLabel('Recent');
  // Check: the corresponding tree item should be focused and have the "Current
  // directory" aria description.
  await directoryTree.waitForFocusedItemByLabel('Recent');
  await directoryTree.waitForCurrentDirectoryItemByLabel('Recent');
};

/**
 * Tests that when the selected folder in the directory tree changes, the
 * selected folder and the tree item that has the "Current directory" aria
 * description do not change. Also tests that when the focused folder is
 * activated (via the Enter key for example), both the selected folder and the
 * tree item with a "Current directory" aria description are updated.
 */
testcase.directoryTreeSelectedDirectory = async () => {
  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Change to My files folder.
  await directoryTree.navigateToPath('/My files');

  // Check: the My files folder should be selected.
  await directoryTree.waitForSelectedItemByLabel('My files');
  // Check: the corresponding tree item should be focused and have the "Current
  // directory" aria description.
  await directoryTree.waitForFocusedItemByLabel('My files');
  await directoryTree.waitForCurrentDirectoryItemByLabel('My files');

  // Send ArrowUp key to change the focused folder.
  await directoryTree.focusPreviousItem();

  await directoryTree.waitForFocusedItemByLabel('Recent');
  // Check: the My files folder should be selected.
  await directoryTree.waitForSelectedItemByLabel('My files');
  // Check: the corresponding tree item should have the "Current directory" aria
  // description.
  await directoryTree.waitForCurrentDirectoryItemByLabel('My files');

  // Check: the Recent Media View folder should be focused.
  await directoryTree.waitForFocusedItemByLabel('Recent');

  // Send Enter key to activate the focused folder.
  await directoryTree.selectFocusedItem();

  // Check: the Recent folder should be focused and selected.
  await directoryTree.waitForSelectedItemByLabel('Recent');
  // Check: the corresponding tree item should be focused and have the "Current
  // directory" aria description.
  await directoryTree.waitForFocusedItemByLabel('Recent');
  await directoryTree.waitForCurrentDirectoryItemByLabel('Recent');
};

/**
 * Tests that the directory tree can be vertically scrolled.
 */
testcase.directoryTreeVerticalScroll = async () => {
  const folders = [ENTRIES.photos];

  // Add enough test entries to overflow the #directory-tree container.
  for (let i = 0; i < 30; i++) {
    folders.push(new TestEntryInfo({
      type: EntryType.DIRECTORY,
      targetPath: '' + i,
      lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
      nameText: '' + i,
      sizeText: '--',
      typeText: 'Folder',
    }));
  }

  // Open FilesApp on Downloads and expand the tree view of Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, folders, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.recursiveExpand('/My files/Downloads');

  // Verify the directory tree is not vertically scrolled.
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollTop']);
  chrome.test.assertTrue(original.scrollTop === 0);

  // Scroll the directory tree down (vertical scroll).
  await remoteCall.callRemoteTestUtil(
      'setScrollTop', appId, [directoryTree.rootSelector, 100]);

  // Check: the directory tree should be vertically scrolled.
  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollTop']);
  const scrolledDown = scrolled.scrollTop === 100;
  chrome.test.assertTrue(scrolledDown, 'Tree should scroll down');
};

/**
 * Tests that the directory tree does not horizontally scroll.
 */
testcase.directoryTreeHorizontalScroll = async () => {
  // Open FilesApp on Downloads and expand the tree view of Downloads.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.recursiveExpand('/My files/Downloads');

  // Verify the directory tree is not horizontally scrolled.
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);
  chrome.test.assertTrue(original.scrollLeft === 0);

  // Shrink the tree to 50px. TODO(files-ng): consider using 150px?
  await remoteCall.callRemoteTestUtil(
      'setElementStyles', appId,
      [directoryTree.containerSelector, {width: '50px'}]);

  // Scroll the directory tree left (horizontal scroll).
  await remoteCall.callRemoteTestUtil(
      'setScrollLeft', appId, [directoryTree.rootSelector, 100]);

  // Check: the directory tree should not be horizontally scrolled.
  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);
  const noScrollLeft = scrolled.scrollLeft === 0;
  chrome.test.assertTrue(noScrollLeft, 'Tree should not scroll left');
};

/**
 * Tests that the directory tree does not horizontally scroll when expanding
 * nested folder items.
 */
testcase.directoryTreeExpandHorizontalScroll = async () => {
  /**
   * Creates a folder test entry from a folder |path|.
   * @param {string} path The folder path.
   * @return {!TestEntryInfo}
   */
  function createFolderTestEntry(path) {
    const name = path.split('/').pop();
    return new TestEntryInfo({
      targetPath: path,
      nameText: name,
      type: EntryType.DIRECTORY,
      lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
      sizeText: '--',
      typeText: 'Folder',
    });
  }

  // Build an array of nested folder test entries.
  const nestedFolderTestEntries = [];
  for (let path = 'nested-folder1', i = 0; i < 6; ++i) {
    nestedFolderTestEntries.push(createFolderTestEntry(path));
    path += `/nested-folder${i + 1}`;
  }

  // Open FilesApp on Downloads containing the folder test entries.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, nestedFolderTestEntries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Verify the directory tree is not horizontally scrolled.
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);
  chrome.test.assertTrue(original.scrollLeft === 0);

  // Shrink the tree to 150px, enough to hide the deep folder names.
  await remoteCall.callRemoteTestUtil(
      'setElementStyles', appId,
      [directoryTree.containerSelector, {width: '150px'}]);

  // Expand the tree Downloads > nested-folder1 > nested-folder2 ...
  const lastFolderPath = nestedFolderTestEntries.pop().targetPath;
  await directoryTree.navigateToPath(`/My files/Downloads/${lastFolderPath}`);

  // Check: the directory tree should be showing the last test entry.
  const folder5Item = await directoryTree.waitForItemByLabel('nested-folder5');
  chrome.test.assertFalse(!!folder5Item.attributes.hidden);

  // Ensure the directory tree scroll event handling is complete.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('requestAnimationFrame', appId, []));

  // Check: the directory tree should not be horizontally scrolled.
  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);
  const notScrolled = scrolled.scrollLeft === 0;
  chrome.test.assertTrue(notScrolled, 'Tree should not scroll left');
};

/**
 * Creates a folder test entry from a folder |path|.
 * @param {string} path The folder path.
 * @return {!TestEntryInfo}
 */
function createFolderTestEntry(path) {
  const name = path.split('/').pop();
  return new TestEntryInfo({
    targetPath: path,
    nameText: name,
    type: EntryType.DIRECTORY,
    lastModifiedTime: 'Dec 28, 1962, 10:42 PM',
    sizeText: '--',
    typeText: 'Folder',
  });
}

/**
 * Tests that the directory tree does not horizontally scroll when expanding
 * nested folder items when the text direction is RTL.
 */
testcase.directoryTreeExpandHorizontalScrollRTL = async () => {
  // Build an array of nested folder test entries.
  const nestedFolderTestEntries = [];
  for (let path = 'nested-folder1', i = 0; i < 6; ++i) {
    nestedFolderTestEntries.push(createFolderTestEntry(path));
    path += `/nested-folder${i + 1}`;
  }

  // Open FilesApp on Downloads containing the folder test entries.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, nestedFolderTestEntries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Redraw FilesApp with text direction RTL.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'renderWindowTextDirectionRTL', appId, []));

  // Verify the directory tree is not horizontally scrolled.
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);
  chrome.test.assertTrue(original.scrollLeft === 0);

  // Shrink the tree to 150px, enough to hide the deep folder names.
  await remoteCall.callRemoteTestUtil(
      'setElementStyles', appId,
      [directoryTree.containerSelector, {width: '150px'}]);

  // Expand the tree Downloads > nested-folder1 > nested-folder2 ...
  const lastFolderPath = nestedFolderTestEntries.pop().targetPath;
  await directoryTree.navigateToPath(`/My files/Downloads/${lastFolderPath}`);

  // Check: the directory tree should be showing the last test entry.
  const folder5Item = await directoryTree.waitForItemByLabel('nested-folder5');
  chrome.test.assertFalse(!!folder5Item.attributes.hidden);

  // Ensure the directory tree scroll event handling is complete.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('requestAnimationFrame', appId, []));

  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);

  // Check: the directory tree should not be horizontally scrolled.
  const notScrolled = scrolled.scrollLeft === 0;
  chrome.test.assertTrue(notScrolled, 'Tree should not scroll right');
};

/**
 * Adds folders with the name prefix /path/to/sub-folders, it appends "-$X"
 * suffix for each folder.
 *
 * NOTE: It assumes the parent folders exist.
 *
 * @param {number} number Number of sub-folders to be created.
 * @param {string} namePrefix Prefix name to be used in the folder.
 */
function addSubFolders(number, namePrefix) {
  const result = new Array(number);
  const baseName = namePrefix.split('/').pop();

  for (let i = 0; i < number; i++) {
    const subFolderName = `${baseName}-${i}`;
    const targetPath = `${namePrefix}-${i}`;
    result[i] = new TestEntryInfo({
      targetPath: targetPath,
      nameText: subFolderName,
      type: EntryType.DIRECTORY,
      lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
      sizeText: '--',
      typeText: 'Folder',
    });
  }

  return result;
}

/**
 * Tests that expanding a folder updates the its sub-folders expand icons.
 */
testcase.directoryTreeExpandFolder = async () => {
  // Create a large-folder inside Downloads.
  let entries = addSubFolders(1, 'large-folder');

  // Create 20 sub-folders with 15 sub-sub-folders.
  const numberOfSubFolders = 20;
  const numberOfSubSubFolders = 15;
  entries = entries.concat(
      addSubFolders(numberOfSubFolders, `large-folder-0/sub-folder`));
  for (let i = 0; i < numberOfSubFolders; i++) {
    entries = entries.concat(addSubFolders(
        numberOfSubSubFolders,
        `large-folder-0/sub-folder-${i}/sub-sub-folder`));
  }

  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  const start = Date.now();

  // Expand the large-folder-0.
  await directoryTree.recursiveExpand('/My files/Downloads/large-folder-0');

  // Wait for all sub-folders to have the expand icon.
  await directoryTree.waitForChildItemsCountByLabel(
      'large-folder-0', numberOfSubFolders, /* excludeEmptyChild= */ true);

  // Expand a sub-folder.
  await directoryTree.recursiveExpand(
      '/My files/Downloads/large-folder-0/sub-folder-0');

  // Wait sub-folder to have its 1k sub-sub-folders.
  await directoryTree.waitForChildItemsCountByLabel(
      'sub-folder-0', numberOfSubSubFolders);

  const testTime = Date.now() - start;
  console.log(`[measurement] Test time: ${testTime}ms`);
};

/**
 * Tests to ensure expand icon does not show up if show hidden files is off
 * and a directory only contains hidden directories.
 */
testcase.directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOff =
    async () => {
  // Populate a normal folder entry with a hidden folder inside.
  const entries = [
    createFolderTestEntry('normal-folder'),
    createFolderTestEntry('normal-folder/.hidden-folder'),
  ];

  // Opens FilesApp on downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Expand all sub-directories in downloads.
  await directoryTree.recursiveExpand('/My files/Downloads');

  // Assert that the expand icon will not show up.
  await directoryTree.waitForItemToHaveChildren(
      'normal-folder', /* hasChildren= */ false);
};

/**
 * Tests to ensure expand icon shows up if show hidden files
 * is on and a directory only contains hidden directories.
 */
testcase.directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOn =
    async () => {
  // Populate a normal folder entry with a hidden folder inside.
  const entries = [
    createFolderTestEntry('normal-folder'),
    createFolderTestEntry('normal-folder/.hidden-folder'),
  ];

  // Opens FilesApp on downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Enable show hidden files.
  await remoteCall.waitAndClickElement(appId, '#gear-button');
  await remoteCall.waitAndClickElement(
      appId,
      '#gear-menu-toggle-hidden-files' +
          ':not([checked]):not([hidden])');

  // Expand all sub-directories in Downloads.
  await directoryTree.recursiveExpand('/My files/Downloads');

  // Assert that the expand icon shows up.
  await directoryTree.waitForItemToHaveChildren(
      'normal-folder', /* hasChildren= */ true);
};

/**
 * Tests that the "expand icon" on directory tree items is correctly
 * shown for directories on the "My Files" volume. Volumes such as
 * this, which do not delay expansion, eagerly traverse into children
 * directories of the current directory to see if the children have
 * children: if they do, an expand icon is shown, if not, it is hidden.
 */
testcase.directoryTreeExpandFolderOnNonDelayExpansionVolume = async () => {
  // Create a parent folder with a child folder inside it.
  const entries = [
    createFolderTestEntry('parent-folder'),
    createFolderTestEntry('parent-folder/empty-child-folder'),
    createFolderTestEntry('parent-folder/non-empty-child-folder'),
    createFolderTestEntry('parent-folder/non-empty-child-folder/child'),
  ];

  // Opens FilesApp on downloads with the folders above.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Expand the parent folder, which should check if the child folder
  // itself has children.
  await directoryTree.recursiveExpand('/My files/Downloads/parent-folder');

  // Check that the empty child folder has been checked for children, and was
  // found to have none. This ensures the expand icon is hidden.
  await directoryTree.waitForItemToHaveChildren(
      'empty-child-folder', /* hasChildren= */ false);

  // Check that the non-empty child folder has been checked for children, and
  // was found to have some. This ensures the expand icon is shown.
  await directoryTree.waitForItemToHaveChildren(
      'non-empty-child-folder', /* hasChildren= */ true);
};

/**
 * Tests that the "expand icon" on directory tree items is correctly
 * shown for directories on an SMB volume. Volumes such as this, which
 * do delay expansion, don't eagerly traverse into children directories
 * of the current directory to see if the children have children, but
 * instead give every child directory a 'tentative' expand icon. When
 * that icon is clicked, the child directory is read and the icon will
 * only remain if the child really has children.
 */
testcase.directoryTreeExpandFolderOnDelayExpansionVolume = async () => {
  // Create a parent folder with a child folder inside it, and another
  // where the child also has a child.
  const entries = [
    createFolderTestEntry('parent-folder'),
    createFolderTestEntry('parent-folder/child-folder'),
    createFolderTestEntry('grandparent-folder'),
    createFolderTestEntry('grandparent-folder/middle-child-folder'),
    createFolderTestEntry(
        'grandparent-folder/middle-child-folder/grandchild-folder'),
  ];

  // Open Files app.
  const appId = await setupAndWaitUntilReady(null, [], []);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Populate Smbfs with the directories.
  await addEntries(['smbfs'], entries);

  // Mount Smbfs volume.
  await sendTestMessage({name: 'mountSmbfs'});

  // Click to open the Smbfs volume.
  await directoryTree.selectItemByType('smb');

  // Expand the parent folder.
  await directoryTree.recursiveExpand('/SMB Share/parent-folder');

  // The child folder should have the 'may-have-children' attribute and
  // should not have the 'has-children' attribute. In this state, it
  // will display an expansion icon (until it's expanded and Files app
  // *knows* it has no children.
  await directoryTree.waitForItemToMayHaveChildren('child-folder');

  // Expand the child folder, which will discover it has no children and
  // remove its expansion icon.
  await directoryTree.expandTreeItemByLabel(
      'child-folder', /* allowEmpty= */ true);

  // The child folder should now have the 'has-children' attribute and
  // it should be false.
  await directoryTree.waitForItemToHaveChildren(
      'child-folder', /* hasChildren= */ false);

  // Expand the grandparent that has a middle child with a child.
  await directoryTree.recursiveExpand('/SMB Share/grandparent-folder');

  // The middle child should have an (eager) expand icon.
  await directoryTree.waitForItemToMayHaveChildren('middle-child-folder');

  // Expand the middle child, which will discover it does actually have
  // children.
  await directoryTree.expandTreeItemByLabel('middle-child-folder');

  // The middle child folder should now have the 'has-children' attribute
  // and it should be true (eg. it will retain its expand icon).
  await directoryTree.waitForItemToHaveChildren(
      'middle-child-folder', /* hasChildren= */ true);
};
