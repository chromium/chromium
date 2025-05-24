// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, EntryType, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_LOCAL_ENTRY_SET, COMPUTERS_ENTRY_SET, SHARED_DRIVE_ENTRY_SET} from './test_data.js';

/**
 * Tests that when the current folder is changed, the 'selected' attribute
 * appears in the selected folder .tree-row and the "Current directory" aria
 * description is present on the corresponding tree item.
 */
export async function directoryTreeActiveDirectory() {
  // Open FilesApp on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  const directoryTree = await DirectoryTreePageObject.create(appId);

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
}

/**
 * Tests that when the selected folder in the directory tree changes, the
 * selected folder and the tree item that has the "Current directory" aria
 * description do not change. Also tests that when the focused folder is
 * activated (via the Enter key for example), both the selected folder and the
 * tree item with a "Current directory" aria description are updated.
 */
export async function directoryTreeSelectedDirectory() {
  // Open FilesApp on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  const directoryTree = await DirectoryTreePageObject.create(appId);

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
}

/**
 * Tests that the directory tree can be vertically scrolled.
 */
export async function directoryTreeVerticalScroll() {
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
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, folders, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);
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
}

/**
 * Tests that the directory tree does not horizontally scroll.
 */
export async function directoryTreeHorizontalScroll() {
  // Open FilesApp on Downloads and expand the tree view of Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.recursiveExpand('/My files/Downloads');

  // Verify the directory tree is not horizontally scrolled.
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);
  chrome.test.assertTrue(original.scrollLeft === 0);

  // Shrink the tree to 50px.
  await remoteCall.callRemoteTestUtil(
      'setElementStyles', appId,
      [directoryTree.containerSelector, {width: '50px'}]);

  // Scroll the directory tree left (horizontal scroll).
  await remoteCall.callRemoteTestUtil(
      'setScrollLeft', appId, [directoryTree.rootSelector, 100]);

  // Check: the directory tree should not be horizontally scrolled.
  const caller = getCaller();
  await repeatUntil(async () => {
    const scrolled = await remoteCall.waitForElementStyles(
        appId, directoryTree.rootSelector, ['scrollLeft']);
    if (scrolled.scrollLeft !== 0) {
      return pending(caller, 'Tree should not scroll left');
    }
    return undefined;
  });
}

/**
 * Tests that the directory tree does not horizontally scroll when expanding
 * nested folder items.
 */
export async function directoryTreeExpandHorizontalScroll() {
  /**
   * Creates a folder test entry from a folder |path|.
   * @param path The folder path.
   */
  function createFolderTestEntry(path: string): TestEntryInfo {
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
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, nestedFolderTestEntries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

  // Verify the directory tree is not horizontally scrolled.
  const original = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);
  chrome.test.assertTrue(original.scrollLeft === 0);

  // Shrink the tree to 150px, enough to hide the deep folder names.
  await remoteCall.callRemoteTestUtil(
      'setElementStyles', appId,
      [directoryTree.containerSelector, {width: '150px'}]);

  // Expand the tree Downloads > nested-folder1 > nested-folder2 ...
  const lastFolderPath = nestedFolderTestEntries.pop()!.targetPath;
  await directoryTree.navigateToPath(`/My files/Downloads/${lastFolderPath}`);

  // Check: the directory tree should be showing the last test entry.
  const folder5Item = await directoryTree.waitForItemByLabel('nested-folder5');
  chrome.test.assertFalse(!!folder5Item.attributes['hidden']);

  // Ensure the directory tree scroll event handling is complete.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('requestAnimationFrame', appId, []));

  // Check: the directory tree should not be horizontally scrolled.
  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);
  const notScrolled = scrolled.scrollLeft === 0;
  chrome.test.assertTrue(notScrolled, 'Tree should not scroll left');
}

/**
 * Creates a folder test entry from a folder |path|.
 * @param path The folder path.
 */
function createFolderTestEntry(path: string): TestEntryInfo {
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
export async function directoryTreeExpandHorizontalScrollRTL() {
  // Build an array of nested folder test entries.
  const nestedFolderTestEntries = [];
  for (let path = 'nested-folder1', i = 0; i < 6; ++i) {
    nestedFolderTestEntries.push(createFolderTestEntry(path));
    path += `/nested-folder${i + 1}`;
  }

  // Open FilesApp on Downloads containing the folder test entries.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, nestedFolderTestEntries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

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
  const lastFolderPath = nestedFolderTestEntries.pop()!.targetPath;
  await directoryTree.navigateToPath(`/My files/Downloads/${lastFolderPath}`);

  // Check: the directory tree should be showing the last test entry.
  const folder5Item = await directoryTree.waitForItemByLabel('nested-folder5');
  chrome.test.assertFalse(!!folder5Item.attributes['hidden']);

  // Ensure the directory tree scroll event handling is complete.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('requestAnimationFrame', appId, []));

  const scrolled = await remoteCall.waitForElementStyles(
      appId, directoryTree.rootSelector, ['scrollLeft']);

  // Check: the directory tree should not be horizontally scrolled.
  const notScrolled = scrolled.scrollLeft === 0;
  chrome.test.assertTrue(notScrolled, 'Tree should not scroll right');
}

/**
 * Adds folders with the name prefix /path/to/sub-folders, it appends "-$X"
 * suffix for each folder.
 *
 * NOTE: It assumes the parent folders exist.
 *
 * @param number Number of sub-folders to be created.
 * @param namePrefix Prefix name to be used in the folder.
 */
function addSubFolders(number: number, namePrefix: string): TestEntryInfo[] {
  const result = new Array<TestEntryInfo>(number);
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
export async function directoryTreeExpandFolder() {
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
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

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
  console.info(`[measurement] Test time: ${testTime}ms`);
}

/**
 * Tests to ensure expand icon does not show up if show hidden files is off
 * and a directory only contains hidden directories.
 */
export async function
directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOff() {
  // Populate a normal folder entry with a hidden folder inside.
  const entries = [
    createFolderTestEntry('normal-folder'),
    createFolderTestEntry('normal-folder/.hidden-folder'),
  ];

  // Opens FilesApp on downloads.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

  // Expand all sub-directories in downloads.
  await directoryTree.recursiveExpand('/My files/Downloads');

  // Assert that the expand icon will not show up.
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'normal-folder', /* hasChildren= */ false);
}

/**
 * Tests to ensure expand icon shows up if show hidden files
 * is on and a directory only contains hidden directories.
 */
export async function
directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOn() {
  // Populate a normal folder entry with a hidden folder inside.
  const entries = [
    createFolderTestEntry('normal-folder'),
    createFolderTestEntry('normal-folder/.hidden-folder'),
  ];

  // Opens FilesApp on downloads.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

  // Enable show hidden files.
  await remoteCall.waitAndClickElement(appId, '#gear-button');
  await remoteCall.waitAndClickElement(
      appId,
      '#gear-menu-toggle-hidden-files' +
          ':not([checked]):not([hidden])');

  // Expand all sub-directories in Downloads.
  await directoryTree.recursiveExpand('/My files/Downloads');

  // Assert that the expand icon shows up.
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'normal-folder', /* hasChildren= */ true);
}

/**
 * Tests that the "expand icon" on directory tree items is correctly
 * shown for directories on the "My Files" volume. Volumes such as
 * this, which do not delay expansion, eagerly traverse into children
 * directories of the current directory to see if the children have
 * children: if they do, an expand icon is shown, if not, it is hidden.
 */
export async function directoryTreeExpandFolderOnNonDelayExpansionVolume() {
  // Create a parent folder with a child folder inside it.
  const entries = [
    createFolderTestEntry('parent-folder'),
    createFolderTestEntry('parent-folder/empty-child-folder'),
    createFolderTestEntry('parent-folder/non-empty-child-folder'),
    createFolderTestEntry('parent-folder/non-empty-child-folder/child'),
  ];

  // Opens FilesApp on downloads with the folders above.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

  // Expand the parent folder, which should check if the child folder
  // itself has children.
  await directoryTree.recursiveExpand('/My files/Downloads/parent-folder');

  // Check that the empty child folder has been checked for children, and was
  // found to have none. This ensures the expand icon is hidden.
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'empty-child-folder', /* hasChildren= */ false);

  // Check that the non-empty child folder has been checked for children, and
  // was found to have some. This ensures the expand icon is shown.
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'non-empty-child-folder', /* hasChildren= */ true);
}

/**
 * Tests that the "expand icon" on directory tree items is correctly
 * shown for directories on an SMB volume. Volumes such as this, which
 * do delay expansion, don't eagerly traverse into children directories
 * of the current directory to see if the children have children, but
 * instead give every child directory a 'tentative' expand icon. When
 * that icon is clicked, the child directory is read and the icon will
 * only remain if the child really has children.
 */
export async function directoryTreeExpandFolderOnDelayExpansionVolume() {
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
  const appId = await remoteCall.setupAndWaitUntilReady(null, [], []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

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
  await directoryTree.waitForItemToMayHaveChildrenByLabel('child-folder');

  // Expand the child folder, which will discover it has no children and
  // remove its expansion icon.
  await directoryTree.expandTreeItemByLabel(
      'child-folder', /* allowEmpty= */ true);

  // The child folder should now have the 'has-children' attribute and
  // it should be false.
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'child-folder', /* hasChildren= */ false);

  // Expand the grandparent that has a middle child with a child.
  await directoryTree.recursiveExpand('/SMB Share/grandparent-folder');

  // The middle child should have an (eager) expand icon.
  await directoryTree.waitForItemToMayHaveChildrenByLabel(
      'middle-child-folder');

  // Expand the middle child, which will discover it does actually have
  // children.
  await directoryTree.expandTreeItemByLabel('middle-child-folder');

  // The middle child folder should now have the 'has-children' attribute
  // and it should be true (eg. it will retain its expand icon).
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'middle-child-folder', /* hasChildren= */ true);
}

/**
 * When drag a file and hover over the directory tree item, the item should be
 * expanded and selected, current directory should also change to that folder.
 */
export async function directoryTreeExpandAndSelectedOnDragMove() {
  // Create a file and a folder.
  const entries = [
    // file to drag.
    ENTRIES.hello,
    // folder to drag over and drop.
    createFolderTestEntry('aaa'),
  ];

  // Open Files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel('Downloads');

  // File to drag.
  const source =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;

  // Drag file to the Downloads folder without drop.
  const finishDragToDownloads = await directoryTree.dragFilesToItemByLabel(
      source, 'Downloads', /* skipDrop= */ true);

  // Downloads folder should be expanded.
  await directoryTree.waitForItemToExpandByLabel('Downloads');

  // Send dragleave on the Downloads folder and move the drag to aaa folder.
  await finishDragToDownloads(
      directoryTree.itemSelectorByLabel('Downloads'), /* dragleave= */ true);
  await directoryTree.dragFilesToItemByLabel(
      source, 'aaa', /* skipDrop= */ true);

  // aaa folder should be expanded and selected.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads/aaa');
  await directoryTree.waitForSelectedItemByLabel('aaa');
}


/**
 * When My Drive is active, clicking Google Drive shouldn't change the current
 * directory.
 */
export async function directoryTreeClickDriveRootWhenMyDriveIsActive() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel('My Drive');

  // Select Google Drive.
  await directoryTree.selectItemByLabel('Google Drive');

  // My Drive should still be selected.
  await directoryTree.waitForSelectedItemByLabel('My Drive');
  // Current directory is still My Drive, not Google Drive.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My Drive');
}

/**
 * When the last sub folder is deleted, the expand icon should disappear
 * for its parent folder, it will reappear after adding a new sub folder back.
 */
export async function directoryTreeHideExpandIconWhenLastSubFolderIsRemoved() {
  // Create a parent folder with a child folder inside it.
  const entries = [
    createFolderTestEntry('parent-folder'),
    createFolderTestEntry('parent-folder/child-folder'),
  ];

  // Open Files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel('Downloads');

  // Expand Downloads and parent-folder.
  await directoryTree.expandTreeItemByLabel('Downloads');
  await directoryTree.waitForItemByLabel('parent-folder');
  await directoryTree.expandTreeItemByLabel('parent-folder');
  // Expand icon should show for parent-folder.
  await directoryTree.waitForItemExpandIconToShowByLabel('parent-folder');

  // Select to navigate to parent-folder.
  await directoryTree.selectItemByLabel('parent-folder');

  // Delete the child folder from the file list.
  await remoteCall.waitUntilSelected(appId, 'child-folder');
  await remoteCall.clickTrashButton(appId);
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="child-folder"]');

  // Expand icon should disappear for parent-folder.
  await directoryTree.waitForItemExpandIconToHideByLabel('parent-folder');

  // Add a new folder by Ctrl + E under parent-folder.
  await remoteCall.fakeKeyDown(appId, 'body', 'e', true, false, false);
  await remoteCall.waitForElement(appId, '#file-list [file-name="New folder"]');

  // Expand icon should appear again.
  await directoryTree.waitForItemExpandIconToShowByLabel('parent-folder');
}

/**
 * When Google Drive is being disconnected and reconnected, the children order
 * of "Google Drive" directory tree item should be kept.
 */
export async function directoryTreeKeepDriveOrderAfterReconnected() {
  const expectedChildrenLabels = [
    'My Drive',
    'Shared drives',
    'Computers',
    'Shared with me',
    'Offline',
  ];

  // Open FilesApp on Drive with Computers and TeamDrives entries.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [...COMPUTERS_ENTRY_SET, ...SHARED_DRIVE_ENTRY_SET]);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel('My Drive');

  // Check children order.
  const childrenBefore =
      await directoryTree.getChildItemsByParentLabel('Google Drive');
  const labelsBefore =
      childrenBefore.map(childItem => directoryTree.getItemLabel(childItem));
  chrome.test.assertEq(labelsBefore, expectedChildrenLabels);

  // Disable Drive.
  await sendTestMessage({name: 'setDriveEnabled', enabled: false});

  // Drive will be gone and My Files will be selected.
  await directoryTree.waitForItemLostByLabel('Google Drive');
  await directoryTree.waitForSelectedItemByLabel('My files');

  // Mount drive and re-add Computers/Team Drives and then re-enable it,
  // otherwise the Computers/Team Drives won't be there after remounting.
  await sendTestMessage({name: 'mountDrive'});
  await addEntries(
      ['drive'], [...COMPUTERS_ENTRY_SET, ...SHARED_DRIVE_ENTRY_SET]);
  await sendTestMessage({name: 'setDriveEnabled', enabled: true});
  // Drive will be back.
  await directoryTree.waitForItemByLabel('Google Drive');

  // Expand it and check the children.
  await directoryTree.expandTreeItemByLabel('Google Drive');
  const caller = getCaller();
  await repeatUntil(async () => {
    const childrenAfter =
        await directoryTree.getChildItemsByParentLabel('Google Drive');
    const labelsAfter =
        childrenAfter.map(childItem => directoryTree.getItemLabel(childItem));
    const sameLength = labelsAfter.length === labelsBefore.length;
    if (!sameLength) {
      return pending(
          caller, 'Expect Google Drive to have %d children, but got %d',
          labelsBefore.length, labelsAfter.length);
    }
    for (let i = 0; i < labelsAfter.length; i++) {
      if (labelsBefore[i] !== labelsAfter[i]) {
        return pending(
            caller, 'Expect Google Drive children to be %j, but got %j',
            labelsBefore, labelsAfter);
      }
    }
    return undefined;
  });
}
