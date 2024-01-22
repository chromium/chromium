// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {createShortcut, openNewWindow, remoteCall, setupAndWaitUntilReady} from './background.js';
import {TREEITEM_DRIVE} from './create_new_folder.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

/**
 * Directory tree path constants.
 */
const TREEITEM_A = `/${TREEITEM_DRIVE}/A`;
const TREEITEM_B = `${TREEITEM_A}/B`;
const TREEITEM_C = `${TREEITEM_B}/C`;

const TREEITEM_D = `/${TREEITEM_DRIVE}/D`;
const TREEITEM_E = `${TREEITEM_D}/E`;

/**
 * Entry set used for the folder shortcut tests.
 * @type {!Array<TestEntryInfo>}
 */
const FOLDER_ENTRY_SET = [
  // @ts-ignore: error TS4111: Property 'directoryA' comes from an index
  // signature, so it must be accessed with ['directoryA'].
  ENTRIES.directoryA,
  // @ts-ignore: error TS4111: Property 'directoryB' comes from an index
  // signature, so it must be accessed with ['directoryB'].
  ENTRIES.directoryB,
  // @ts-ignore: error TS4111: Property 'directoryC' comes from an index
  // signature, so it must be accessed with ['directoryC'].
  ENTRIES.directoryC,
  // @ts-ignore: error TS4111: Property 'directoryD' comes from an index
  // signature, so it must be accessed with ['directoryD'].
  ENTRIES.directoryD,
  // @ts-ignore: error TS4111: Property 'directoryE' comes from an index
  // signature, so it must be accessed with ['directoryE'].
  ENTRIES.directoryE,
  // @ts-ignore: error TS4111: Property 'directoryF' comes from an index
  // signature, so it must be accessed with ['directoryF'].
  ENTRIES.directoryF,
];

/**
 * Constants for each folder.
 * @type {Object}
 */
const DIRECTORY = {
  Drive: {
    contents: [
      // @ts-ignore: error TS4111: Property 'directoryA' comes from an index
      // signature, so it must be accessed with ['directoryA'].
      ENTRIES.directoryA.getExpectedRow(),
      // @ts-ignore: error TS4111: Property 'directoryD' comes from an index
      // signature, so it must be accessed with ['directoryD'].
      ENTRIES.directoryD.getExpectedRow(),
    ],
    name: 'My Drive',
    treeItem: TREEITEM_DRIVE,
  },
  A: {
    // @ts-ignore: error TS4111: Property 'directoryB' comes from an index
    // signature, so it must be accessed with ['directoryB'].
    contents: [ENTRIES.directoryB.getExpectedRow()],
    name: 'A',
    treeItem: TREEITEM_A,
  },
  B: {
    // @ts-ignore: error TS4111: Property 'directoryC' comes from an index
    // signature, so it must be accessed with ['directoryC'].
    contents: [ENTRIES.directoryC.getExpectedRow()],
    name: 'B',
    treeItem: TREEITEM_B,
  },
  C: {
    contents: [],
    name: 'C',
    treeItem: TREEITEM_C,
  },
  D: {
    // @ts-ignore: error TS4111: Property 'directoryE' comes from an index
    // signature, so it must be accessed with ['directoryE'].
    contents: [ENTRIES.directoryE.getExpectedRow()],
    name: 'D',
    treeItem: TREEITEM_D,
  },
  E: {
    // @ts-ignore: error TS4111: Property 'directoryF' comes from an index
    // signature, so it must be accessed with ['directoryF'].
    contents: [ENTRIES.directoryF.getExpectedRow()],
    name: 'E',
    treeItem: TREEITEM_E,
  },
};

/**
 * Expands whole directory tree under DIRECTORY.Drive.
 *
 * @param {string} appId Files app windowId.
 * @return {Promise<void>} Promise fulfilled on success.
 */
async function expandDirectoryTree(appId) {
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  // @ts-ignore: error TS2339: Property 'Drive' does not exist on type 'Object'.
  await directoryTree.recursiveExpand(DIRECTORY.Drive.treeItem);
  // @ts-ignore: error TS2339: Property 'A' does not exist on type 'Object'.
  await directoryTree.recursiveExpand(DIRECTORY.A.treeItem);
  // @ts-ignore: error TS2339: Property 'B' does not exist on type 'Object'.
  await directoryTree.recursiveExpand(DIRECTORY.B.treeItem);
  // @ts-ignore: error TS2339: Property 'D' does not exist on type 'Object'.
  await directoryTree.recursiveExpand(DIRECTORY.D.treeItem);
}

/**
 * Navigate to |directory| (makes |directory| the current directory).
 *
 * @param {string} appId Files app windowId.
 * @param {Object} directory Directory to navigate to.
 * @return {Promise<void>} Promise fulfilled on success.
 */
async function navigateToDirectory(appId, directory) {
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  // @ts-ignore: error TS2339: Property 'treeItem' does not exist on type
  // 'Object'.
  await directoryTree.navigateToPath(directory.treeItem);

  // @ts-ignore: error TS2339: Property 'contents' does not exist on type
  // 'Object'.
  await remoteCall.waitForFiles(appId, directory.contents);
}

/**
 * Removes the folder shortcut to |directory|. Note the current directory must
 * be a parent of the given |directory|.
 *
 * @param {string} appId Files app windowId.
 * @param {Object} directory Directory of shortcut to be removed.
 * @return {Promise<void>} Promise fulfilled on success.
 */
async function removeShortcut(appId, directory) {
  const caller = getCaller();
  const removeShortcutMenuItem =
      '[command="#unpin-folder"]:not([hidden]):not([disabled])';
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Right-click for context menu with retry.
  await repeatUntil(async () => {
    // Right click.
    // @ts-ignore: error TS2339: Property 'name' does not exist on type
    // 'Object'.
    await directoryTree.showContextMenuForShortcutItemByLabel(directory.name);

    // Wait context menu to show.
    await remoteCall.waitForElement(appId, '#roots-context-menu:not([hidden])');

    // Check menu item is visible and enabled.
    const menuItem = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, [removeShortcutMenuItem]);
    if (menuItem.length > 0) {
      return true;
    }

    return pending(
        caller,
        `Waiting "remove shortcut" menu item to be available on ${appId}.`);
  });

  // Click the remove shortcut menu item.
  await remoteCall.waitAndClickElement(
      appId,
      ['#roots-context-menu [command="#unpin-folder"]:' +
       'not([hidden])']);

  // @ts-ignore: error TS2339: Property 'name' does not exist on type 'Object'.
  await directoryTree.waitForShortcutItemLostByLabel(directory.name);
}

/**
 * Waits until the current directory becomes |currentDir| and current
 * selection becomes the shortcut to |shortcutDir|.
 *
 * @param {string} appId Files app windowId.
 * @param {Object} currentDir Directory which should be a current directory.
 * @param {Object} shortcutDir Directory whose shortcut should be selected.
 * @return {Promise<void>} Promise fulfilled on success.
 */
async function expectSelection(appId, currentDir, shortcutDir) {
  // @ts-ignore: error TS2339: Property 'contents' does not exist on type
  // 'Object'.
  await remoteCall.waitForFiles(appId, currentDir.contents);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  // @ts-ignore: error TS2339: Property 'name' does not exist on type 'Object'.
  await directoryTree.waitForFocusedShortcutItemByLabel(shortcutDir.name);
}

/**
 * Clicks folder shortcut to |directory|.
 *
 * @param {string} appId Files app windowId.
 * @param {Object} directory Directory whose shortcut will be clicked.
 * @return {Promise<void>} Promise fulfilled on success.
 */
async function clickShortcut(appId, directory) {
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  // @ts-ignore: error TS2339: Property 'name' does not exist on type 'Object'.
  await directoryTree.selectShortcutItemByLabel(directory.name);
}

/**
 * Creates some shortcuts and traverse them and some other directories.
 */
// @ts-ignore: error TS4111: Property 'traverseFolderShortcuts' comes from an
// index signature, so it must be accessed with ['traverseFolderShortcuts'].
testcase.traverseFolderShortcuts = async () => {
  // Open Files app on Drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], FOLDER_ENTRY_SET);

  // Expand the directory tree.
  await expandDirectoryTree(appId);

  // Create a shortcut to directory D.
  // @ts-ignore: error TS2339: Property 'D' does not exist on type 'Object'.
  await createShortcut(appId, DIRECTORY.D.name);

  // Navigate to directory B.
  // @ts-ignore: error TS2339: Property 'B' does not exist on type 'Object'.
  await navigateToDirectory(appId, DIRECTORY.B);

  // Create a shortcut to directory C.
  // @ts-ignore: error TS2339: Property 'C' does not exist on type 'Object'.
  await createShortcut(appId, DIRECTORY.C.name);

  // Click the Drive root (My Drive).
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.selectItemByLabel('My Drive');

  // Check: current directory and selection should be the Drive root.
  await directoryTree.waitForSelectedItemByLabel('My Drive');

  // Send Ctrl+3 key to file-list to select 3rd volume in the
  // directory tree. This corresponds to the second shortcut (to 'D')
  // as shortcuts are ordered alphabetically. Volumes 1 is the
  // Recent View.
  const key = ['#file-list', '3', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Make sure direcotry change is finished before focusing on the tree.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My Drive/D');

  // The focus is on file list now after Ctrl+3, in order to use `:focus`
  // selector in the below `expectSelection` we need to focus the tree first.
  await directoryTree.focusTree();

  // Check: current directory and selection should be D.
  // @ts-ignore: error TS2339: Property 'D' does not exist on type 'Object'.
  await expectSelection(appId, DIRECTORY.D, DIRECTORY.D);

  // Send UpArrow key to directory tree to select the shortcut
  // above D.
  await directoryTree.focusPreviousItem();

  // Check: current directory should be D, with shortcut C selected.
  // @ts-ignore: error TS2339: Property 'C' does not exist on type 'Object'.
  await expectSelection(appId, DIRECTORY.D, DIRECTORY.C);

  // Send Enter key to the directory tree to change to directory C.
  await directoryTree.selectFocusedItem();

  // Check: current directory and selection should be C.
  // @ts-ignore: error TS2339: Property 'C' does not exist on type 'Object'.
  await expectSelection(appId, DIRECTORY.C, DIRECTORY.C);
};

/**
 * Adds and removes shortcuts from other window and check if the active
 * directories and selected navigation items are correct.
 */
// @ts-ignore: error TS4111: Property 'addRemoveFolderShortcuts' comes from an
// index signature, so it must be accessed with ['addRemoveFolderShortcuts'].
testcase.addRemoveFolderShortcuts = async () => {
  async function openFilesAppOnDrive() {
    const appId = await openNewWindow(RootPath.DRIVE);
    await remoteCall.waitForElement(appId, '#file-list');
    // @ts-ignore: error TS2339: Property 'Drive' does not exist on type
    // 'Object'.
    await remoteCall.waitForFiles(appId, DIRECTORY.Drive.contents);
    return appId;
  }

  // Add entries to Drive.
  await addEntries(['drive'], FOLDER_ENTRY_SET);

  // Open one Files app window on Drive.
  const appId1 = await openFilesAppOnDrive();

  // Open another Files app window on Drive.
  const appId2 = await openFilesAppOnDrive();

  // appId2 window is focused now because that's the last opened window. We are
  // asserting on the appId1 window below, in order to use ":focus" selector in
  // `expectSelection`, we need to make sure the appId1 window is focused first.
  await sendTestMessage({appId: appId1, name: 'focusWindow'});

  // Create a shortcut to D.
  // @ts-ignore: error TS2339: Property 'D' does not exist on type 'Object'.
  await createShortcut(appId1, DIRECTORY.D.name);

  // Click the shortcut to D.
  // @ts-ignore: error TS2339: Property 'D' does not exist on type 'Object'.
  await clickShortcut(appId1, DIRECTORY.D);

  // Check: current directory and selection should be D.
  // @ts-ignore: error TS2339: Property 'D' does not exist on type 'Object'.
  await expectSelection(appId1, DIRECTORY.D, DIRECTORY.D);

  // Create a shortcut to A from the other window.
  // @ts-ignore: error TS2339: Property 'A' does not exist on type 'Object'.
  await createShortcut(appId2, DIRECTORY.A.name);

  // Check: current directory and selection should still be D.
  // @ts-ignore: error TS2339: Property 'D' does not exist on type 'Object'.
  await expectSelection(appId1, DIRECTORY.D, DIRECTORY.D);

  // Remove shortcut to D from the other window.
  // @ts-ignore: error TS2339: Property 'D' does not exist on type 'Object'.
  await removeShortcut(appId2, DIRECTORY.D);

  // Check: directory D in the directory tree should be focused.
  const directoryTree =
      await DirectoryTreePageObject.create(appId1, remoteCall);
  await directoryTree.waitForFocusedItemByLabel('D');
};
