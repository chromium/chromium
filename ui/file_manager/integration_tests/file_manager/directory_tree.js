// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, EntryType, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, recursiveExpand, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Tests that when the current folder is changed, the 'active' attribute
 * appears in the active folder .tree-row and the "Current directory" aria
 * description is present on the corresponding tree item.
 */
testcase.directoryTreeActiveDirectory = async () => {
  const activeRow = '.tree-row[selected][active]';
  const selectedAriaCurrentDirectory =
      '.tree-item[selected][aria-description="Current directory"]';

  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Change to My files folder.
  await navigateWithDirectoryTree(appId, '/My files');

  // Check: the My files folder should be the active tree row.
  const myFiles = await remoteCall.waitForElement(appId, activeRow);
  chrome.test.assertTrue(myFiles.text.includes('My files'));
  // Check: the corresponding tree item should be selected and have the "Current
  // directory" aria description.
  let currentDirectoryTreeItem =
      await remoteCall.waitForElement(appId, selectedAriaCurrentDirectory);
  chrome.test.assertEq(
      currentDirectoryTreeItem.attributes['entry-label'], 'My files');

  // Change to Downloads folder.
  await navigateWithDirectoryTree(appId, '/My files/Downloads');

  // Check: the Downloads folder should be the active tree row.
  const downloads = await remoteCall.waitForElement(appId, activeRow);
  chrome.test.assertTrue(downloads.text.includes('Downloads'));
  // Check: the corresponding tree item should be selected and have the "Current
  // directory" aria description.
  currentDirectoryTreeItem =
      await remoteCall.waitForElement(appId, selectedAriaCurrentDirectory);
  chrome.test.assertEq(
      currentDirectoryTreeItem.attributes['entry-label'], 'Downloads');

  // Change to Google Drive volume's My Drive folder.
  await navigateWithDirectoryTree(appId, '/My Drive');

  // Check: the My Drive folder should be the active tree row.
  const myDrive = await remoteCall.waitForElement(appId, activeRow);
  chrome.test.assertTrue(myDrive.text.includes('My Drive'));
  // Check: the corresponding tree item should be selected and have the "Current
  // directory" aria description.
  currentDirectoryTreeItem =
      await remoteCall.waitForElement(appId, selectedAriaCurrentDirectory);
  chrome.test.assertEq(
      currentDirectoryTreeItem.attributes['entry-label'], 'My Drive');

  // Change to Recent folder.
  await navigateWithDirectoryTree(appId, '/Recent');

  // Check: the Recent folder should be the active tree row.
  const recent = await remoteCall.waitForElement(appId, activeRow);
  chrome.test.assertTrue(recent.text.includes('Recent'));
  // Check: the corresponding tree item should be selected and have the "Current
  // directory" aria description.
  currentDirectoryTreeItem =
      await remoteCall.waitForElement(appId, selectedAriaCurrentDirectory);
  chrome.test.assertEq(
      currentDirectoryTreeItem.attributes['entry-label'], 'Recent');
};

/**
 * Tests that when the selected folder in the directory tree changes, the active
 * folder and the tree item that has the "Current directory" aria description do
 * not change. Also tests that when the selected folder is activated (via the
 * Enter key for example), both the active folder and the tree item with a
 * "Current directory" aria description are updated.
 */
testcase.directoryTreeSelectedDirectory = async () => {
  const selectedActiveRow = '.tree-row[selected][active]';
  const selectedAriaCurrentDirectory =
      '.tree-item[selected][aria-description="Current directory"]';

  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Change to My files folder.
  await navigateWithDirectoryTree(appId, '/My files');

  // Check: the My files folder should be [selected] and [active].
  let treeRow = await remoteCall.waitForElement(appId, selectedActiveRow);
  chrome.test.assertTrue(treeRow.text.includes('My files'));
  // Check: the corresponding tree item should be selected and have the "Current
  // directory" aria description.
  let treeItem =
      await remoteCall.waitForElement(appId, selectedAriaCurrentDirectory);
  chrome.test.assertEq(treeItem.attributes['entry-label'], 'My files');

  // Send ArrowUp key to change the selected folder.
  const arrowUp = ['#directory-tree', 'ArrowUp', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, arrowUp);

  // Check: no folder should be [selected] and [active].
  await remoteCall.waitForElementLost(appId, selectedActiveRow);
  // Check: no tree item should be [selected] and have the "Current directory"
  // aria description.
  await remoteCall.waitForElementLost(appId, selectedAriaCurrentDirectory);

  // Check: the My files folder should be [active].
  treeRow = await remoteCall.waitForElement(appId, '.tree-row[active]');
  chrome.test.assertTrue(treeRow.text.includes('My files'));
  // Check: the corresponding tree item should have the "Current directory" aria
  // description.
  treeItem = await remoteCall.waitForElement(
      appId, '.tree-item[aria-description="Current directory"]');
  chrome.test.assertEq(treeItem.attributes['entry-label'], 'My files');

  // Check: the Recent Media View folder should be [selected].
  treeRow = await remoteCall.waitForElement(appId, '.tree-row[selected]');
  chrome.test.assertTrue(treeRow.text.includes('Recent'));
  // Check: the corresponding tree item should be [selected].
  treeItem = await remoteCall.waitForElement(appId, '.tree-item[selected]');
  chrome.test.assertEq(treeItem.attributes['entry-label'], 'Recent');

  // Send Enter key to activate the selected folder.
  const enter = ['#directory-tree', 'Enter', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, enter);

  // Check: the Recent folder should be [selected] and [active].
  treeRow = await remoteCall.waitForElement(appId, selectedActiveRow);
  chrome.test.assertTrue(treeRow.text.includes('Recent'));
  // Check: the corresponding tree item should be selected and have the "Current
  // directory" aria description.
  treeItem =
      await remoteCall.waitForElement(appId, selectedAriaCurrentDirectory);
  chrome.test.assertEq(treeItem.attributes['entry-label'], 'Recent');
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
  await recursiveExpand(appId, '/My files/Downloads');

  // Verify the directory tree is not vertically scrolled.
  const directoryTree = '#directory-tree';
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree, ['scrollTop']);
  chrome.test.assertTrue(original.scrollTop === 0);

  // Scroll the directory tree down (vertical scroll).
  await remoteCall.callRemoteTestUtil(
      'setScrollTop', appId, [directoryTree, 100]);

  // Check: the directory tree should be vertically scrolled.
  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree, ['scrollTop']);
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
  await recursiveExpand(appId, '/My files/Downloads');

  // Verify the directory tree is not horizontally scrolled.
  const directoryTree = '#directory-tree';
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree, ['scrollLeft']);
  chrome.test.assertTrue(original.scrollLeft === 0);

  // Shrink the tree to 50px. TODO(files-ng): consider using 150px?
  const navigationList = '.dialog-navigation-list';
  await remoteCall.callRemoteTestUtil(
      'setElementStyles', appId, [navigationList, {width: '50px'}]);

  // Scroll the directory tree left (horizontal scroll).
  await remoteCall.callRemoteTestUtil(
      'setScrollLeft', appId, [directoryTree, 100]);

  // Check: the directory tree should not be horizontally scrolled.
  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree, ['scrollLeft']);
  const noScrollLeft = scrolled.scrollLeft === 0;
  chrome.test.assertTrue(noScrollLeft, 'Tree should not scroll left');
};

/**
 * Tests that clicking a directory tree recent subtype {audio,image,video}
 * tab does not vertically scroll the tree.
 */
testcase.directoryTreeRecentsSubtypeScroll = async () => {
  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Set window height to 400px so the tree has a vertical scroll bar.
  await remoteCall.callRemoteTestUtil('resizeWindow', appId, [680, 400]);
  await remoteCall.waitForWindowGeometry(appId, 680, 400);

  // Wait for the recent image tab and save its element properties.
  const recentQuery =
      '#directory-tree [root-type-icon="recent"][recent-file-type="image"]';
  const savedElement =
      await remoteCall.waitForElementStyles(appId, recentQuery, ['display']);
  chrome.test.assertTrue(savedElement.renderedTop > 0);

  // Click recent image tab and wait for its file-list content to appear.
  await remoteCall.waitAndClickElement(appId, recentQuery);
  const file = TestEntryInfo.getExpectedRows([ENTRIES.desktop]);
  await remoteCall.waitForFiles(appId, file, {ignoreLastModifiedTime: true});

  // Check: the recent image tab element.renderedTop should not change.
  const resultElement =
      await remoteCall.waitForElementStyles(appId, recentQuery, ['display']);
  const notScrolled = savedElement.renderedTop === resultElement.renderedTop;
  chrome.test.assertTrue(notScrolled, 'Tree should not vertically scroll');
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

  // Verify the directory tree is not horizontally scrolled.
  const directoryTree = '#directory-tree';
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree, ['scrollLeft']);
  chrome.test.assertTrue(original.scrollLeft === 0);

  // Shrink the tree to 150px, enough to hide the deep folder names.
  const navigationList = '.dialog-navigation-list';
  await remoteCall.callRemoteTestUtil(
      'setElementStyles', appId, [navigationList, {width: '150px'}]);

  // Expand the tree Downloads > nested-folder1 > nested-folder2 ...
  const lastFolderPath = nestedFolderTestEntries.pop().targetPath;
  await navigateWithDirectoryTree(
      appId, `/My files/Downloads/${lastFolderPath}`);

  // Check: the directory tree should be showing the last test entry.
  await remoteCall.waitForElement(
      appId, '.tree-item[entry-label="nested-folder5"]:not([hidden])');

  // Ensure the directory tree scroll event handling is complete.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('requestAnimationFrame', appId, []));

  // Check: the directory tree should not be horizontally scrolled.
  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree, ['scrollLeft']);
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

  // Redraw FilesApp with text direction RTL.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'renderWindowTextDirectionRTL', appId, []));

  // Verify the directory tree is not horizontally scrolled.
  const directoryTree = '#directory-tree';
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree, ['scrollLeft']);
  chrome.test.assertTrue(original.scrollLeft === 0);

  // Shrink the tree to 150px, enough to hide the deep folder names.
  const navigationList = '.dialog-navigation-list';
  await remoteCall.callRemoteTestUtil(
      'setElementStyles', appId, [navigationList, {width: '150px'}]);

  // Expand the tree Downloads > nested-folder1 > nested-folder2 ...
  const lastFolderPath = nestedFolderTestEntries.pop().targetPath;
  await navigateWithDirectoryTree(
      appId, `/My files/Downloads/${lastFolderPath}`);

  // Check: the directory tree should be showing the last test entry.
  await remoteCall.waitForElement(
      appId, '.tree-item[entry-label="nested-folder5"]:not([hidden])');

  // Ensure the directory tree scroll event handling is complete.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('requestAnimationFrame', appId, []));

  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree, ['scrollLeft']);

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

  const start = Date.now();

  // Expand the large-folder-0.
  await recursiveExpand(appId, '/My files/Downloads/large-folder-0');

  // Wait for all sub-folders to have the expand icon.
  const querySubFolderExpandIcons =
      ['#directory-tree [entry-label="large-folder-0"] > ' +
       '.tree-children > .tree-item > .tree-row[has-children="true"]'];
  await remoteCall.waitForElementsCount(
      appId, querySubFolderExpandIcons, numberOfSubFolders);

  // Expand a sub-folder.
  await recursiveExpand(
      appId, '/My files/Downloads/large-folder-0/sub-folder-0');

  // Wait sub-folder to have its 1k sub-sub-folders.
  const querySubSubFolderItems =
      ['#directory-tree [entry-label="sub-folder-0"] > ' +
       '.tree-children > .tree-item'];
  await remoteCall.waitForElementsCount(
      appId, querySubSubFolderItems, numberOfSubSubFolders);

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

  // Expand all sub-directories in downloads.
  await recursiveExpand(appId, '/My files/Downloads');

  // Target the non-hidden folder.
  const normalFolder = '#directory-tree [entry-label="normal-folder"]';
  const response = await remoteCall.waitForElement(appId, normalFolder);

  // Assert that the expand icon will not show up.
  chrome.test.assertTrue(response.attributes['has-children'] === 'false');
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

  // Enable show hidden files.
  await remoteCall.waitAndClickElement(appId, '#gear-button');
  await remoteCall.waitAndClickElement(
      appId,
      '#gear-menu-toggle-hidden-files' +
          ':not([checked]):not([hidden])');

  // Expand all sub-directories in Downloads.
  await recursiveExpand(appId, '/My files/Downloads');

  // Assert that the expand icon shows up.
  const normalFolder =
      '#directory-tree [entry-label="normal-folder"][has-children="true"]';
  await remoteCall.waitForElement(appId, normalFolder);
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

  // Expand the parent folder, which should check if the child folder
  // itself has children.
  await recursiveExpand(appId, '/My files/Downloads/parent-folder');

  // Check that the empty child folder has been checked for children, and was
  // found to have none. This ensures the expand icon is hidden.
  const emptyChildFolder =
      '#directory-tree [entry-label="empty-child-folder"][has-children=false]';
  await remoteCall.waitForElement(appId, emptyChildFolder);

  // Check that the non-empty child folder has been checked for children, and
  // was found to have some. This ensures the expand icon is shown.
  const nonEmptyChildFolder =
      '#directory-tree [entry-label="non-empty-child-folder"][has-children=true]';
  await remoteCall.waitForElement(appId, nonEmptyChildFolder);
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

  const SMBFS_VOLUME_QUERY = '#directory-tree [volume-type-icon="smb"]';

  // Open Files app.
  const appId = await setupAndWaitUntilReady(null, [], []);

  // Populate Smbfs with the directories.
  await addEntries(['smbfs'], entries);

  // Mount Smbfs volume.
  await sendTestMessage({name: 'mountSmbfs'});

  // Wait for the Smbfs volume to mount.
  await remoteCall.waitForElement(appId, SMBFS_VOLUME_QUERY);

  // Click to open the Smbfs volume.
  await remoteCall.waitAndClickElement(appId, [SMBFS_VOLUME_QUERY]);

  // Expand the parent folder.
  await recursiveExpand(appId, '/SMB Share/parent-folder');

  // The child folder should have the 'may-have-children' attribute and
  // should not have the 'has-children' attribute. In this state, it
  // will display an expansion icon (until it's expanded and Files app
  // *knows* it has no children.
  const childFolder = '#directory-tree [entry-label="child-folder"]';
  await remoteCall.waitForElement(
      appId, childFolder + '[may-have-children]:not([has-children])');

  // Expand the child folder, which will discover it has no children and
  // remove its expansion icon.
  const childFolderExpandIcon =
      childFolder + '> .tree-row[may-have-children] .expand-icon';
  await remoteCall.waitAndClickElement(appId, childFolderExpandIcon);

  // The child folder should now have the 'has-children' attribute and
  // it should be false.
  const childFolderExpandedSubtree =
      childFolder + '> .tree-row[has-children=false]';
  await remoteCall.waitForElement(appId, childFolderExpandedSubtree);

  // Expand the grandparent that has a middle child with a child.
  await recursiveExpand(appId, '/SMB Share/grandparent-folder');

  // The middle child should have an (eager) expand icon.
  const middleChildFolder =
      '#directory-tree [entry-label="middle-child-folder"]';
  await remoteCall.waitForElement(
      appId, middleChildFolder + '[may-have-children]:not([has-children])');

  // Expand the middle child, which will discover it does actually have
  // children.
  const middleChildFolderExpandIcon =
      middleChildFolder + '> .tree-row[may-have-children] .expand-icon';
  await remoteCall.waitAndClickElement(appId, middleChildFolderExpandIcon);

  // The middle child folder should now have the 'has-children' attribute
  // and it should be true (eg. it will retain its expand icon).
  const middleChildFolderExpandedSubtree =
      middleChildFolder + '> .tree-row[has-children=true]';
  await remoteCall.waitForElement(appId, middleChildFolderExpandedSubtree);
};
