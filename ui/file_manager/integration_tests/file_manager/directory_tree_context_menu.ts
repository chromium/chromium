// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ElementObject} from '../prod/file_manager/shared_types.js';
import {addEntries, ENTRIES, EntryType, getCaller, pending, REPEAT_UNTIL_INTERVAL, repeatUntil, RootPath, sendTestMessage, TestEntryInfo, wait} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_DRIVE_ENTRY_SET, COMPLEX_DRIVE_ENTRY_SET, COMPUTERS_ENTRY_SET, SHARED_DRIVE_ENTRY_SET} from './test_data.js';

/**
 * Sets up for directory tree context menu test. In addition to normal setup,
 * we add destination directory.
 */
async function setupForDirectoryTreeContextMenuTest() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Add destination directory.
  await addEntries(['local'], [new TestEntryInfo({
                     type: EntryType.DIRECTORY,
                     targetPath: 'destination',
                     lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
                     nameText: 'destination',
                     sizeText: '--',
                     typeText: 'Folder',
                   })]);
  return appId;
}

const ITEMS_IN_DEST_DIR_BEFORE_PASTE = TestEntryInfo.getExpectedRows([]);

const ITEMS_IN_DEST_DIR_AFTER_PASTE =
    TestEntryInfo.getExpectedRows([new TestEntryInfo({
      type: EntryType.DIRECTORY,
      targetPath: 'photos',
      lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
      nameText: 'photos',
      sizeText: '--',
      typeText: 'Folder',
    })]);

/**
 * Clicks context menu item of id in directory tree.
 *
 * @param path Path of the tree item to trigger context menu.
 * @param id The context menu id.
 */
async function clickDirectoryTreeContextMenuItem(
    appId: string, path: string, id: string) {
  const contextMenu = '#directory-tree-context-menu:not([hidden])';

  const directoryTree = await DirectoryTreePageObject.create(appId);

  // Right click photos directory.
  await directoryTree.showContextMenuForItemByPath(path);

  // Check: context menu item |id| should be shown enabled.
  await remoteCall.waitForElement(
      appId, `${contextMenu} [command="#${id}"]:not([hidden]):not([disabled])`);

  // Click the menu item specified by |id|.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [`${contextMenu} [command="#${id}"]`]);
}

/**
 * Navigates to destination directory and test paste operation to check
 * whether the paste operation is done correctly or not. This method does NOT
 * check source entry is deleted or not for cut operation.
 */
async function navigateToDestinationDirectoryAndTestPaste(appId: string) {
  // Navigates to destination directory.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/destination');

  // Confirm files before paste.
  await remoteCall.waitForFiles(
      appId, ITEMS_IN_DEST_DIR_BEFORE_PASTE, {ignoreLastModifiedTime: true});

  // Paste
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'v', true /* ctrl */, false, false]);

  // Confirm the photos directory is pasted correctly.
  await remoteCall.waitForFiles(
      appId, ITEMS_IN_DEST_DIR_AFTER_PASTE, {ignoreLastModifiedTime: true});
}

/**
 * Rename photos directory to specified name by using directory tree.
 * @param useKeyboardShortcut Set to true to use keyboard shortcut instead of
 *     mouse to trigger context menu.
 */
async function renamePhotosDirectoryTo(
    appId: string, newName: string,
    useKeyboardShortcut: boolean): Promise<void> {
  const directoryTree = await DirectoryTreePageObject.create(appId);
  if (useKeyboardShortcut) {
    await directoryTree.triggerRenameWithKeyboardByLabel('photos');
  } else {
    await clickDirectoryTreeContextMenuItem(
        appId, '/Downloads/photos', 'rename');
  }
  await directoryTree.renameItemByLabel('photos', newName);
}

/**
 * Renames directory and confirm current directory is moved to the renamed
 * directory.
 */
async function renameDirectoryFromDirectoryTreeSuccessCase(
    useKeyboardShortcut: boolean) {
  const appId = await setupForDirectoryTreeContextMenuTest();

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await renamePhotosDirectoryTo(appId, 'New photos', useKeyboardShortcut);

  // Confirm that current directory has moved to new folder.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads/New photos');
}

/**
 * Renames directory and confirms that an alert dialog is shown.
 */
async function renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(
    newName: string) {
  const appId = await setupForDirectoryTreeContextMenuTest();

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await renamePhotosDirectoryTo(appId, newName, false);
  // The folder name is not changed.
  await directoryTree.waitForChildItemByLabel('Downloads', 'photos');

  // Confirm that a dialog is shown.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');
}

/**
 * Creates directory from directory tree.
 */
async function createDirectoryFromDirectoryTree(
    useKeyboardShortcut: boolean, changeCurrentDirectory: boolean) {
  const appId = await setupForDirectoryTreeContextMenuTest();

  const directoryTree = await DirectoryTreePageObject.create(appId);
  if (changeCurrentDirectory) {
    await directoryTree.navigateToPath('/My files/Downloads/photos');
  } else {
    await directoryTree.expandTreeItemByLabel('Downloads');
  }
  if (useKeyboardShortcut) {
    await remoteCall.callRemoteTestUtil(
        'fakeKeyDown', appId, ['body', 'e', true /* ctrl */, false, false]);
  } else {
    await clickDirectoryTreeContextMenuItem(
        appId, '/Downloads/photos', 'new-folder');
  }
  await directoryTree.renameItemByLabel('New folder', 'test');
  await directoryTree.waitForItemByPath('/Downloads/photos/test');

  // Confirm that current directory is not changed at this timing.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId,
      changeCurrentDirectory ? '/My files/Downloads/photos' :
                               '/My files/Downloads');

  // Confirm that new directory is actually created by navigating to it.
  await directoryTree.navigateToPath('/My files/Downloads/photos/test');
}

/**
 * Checks all visible items in the context menu for directory tree.
 * @param breadcrumbsPath Path based on the entry labels like:
 *     /My files/Downloads/photos to item to be tested with context menu.
 * @param menuStates Mapping each command to it's enabled state.
 * @param rootsMenu True if the item uses #roots-context-menu instead of
 *     #directory-tree-context-menu
 * @param shortcutToPath For shortcuts it navigates to a different breadcrumbs
 *     path, like /My Drive/ShortcutName.
 */
async function checkContextMenu(
    appId: string, breadcrumbsPath: string,
    menuStates: Array<Array<string|boolean>>, rootsMenu?: boolean,
    shortcutToPath?: string) {
  // Navigate to the folder that will test the context menu.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  const query =
      await directoryTree.navigateToPath(breadcrumbsPath, shortcutToPath);

  // Selector for a both context menu used on directory tree, only one should
  // be visible at the time.
  const menuQuery = rootsMenu ?
      '#roots-context-menu:not([hidden]) cr-menu-item:not([hidden])' :
      '#directory-tree-context-menu:not([hidden]) cr-menu-item:not([hidden])';

  // Right click desired item in the directory tree.
  await remoteCall.waitForElement(appId, query);
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, [query]),
      'fakeMouseRightClick failed');

  // Wait for context menu to appear.
  await remoteCall.waitForElement(appId, menuQuery);

  function stateString(state?: boolean|string) {
    return state ? 'enabled' : 'disabled';
  }

  let msg;
  async function isCommandsEnabledAndOrdered() {
    // Grab all commands together and check they are in the expected order and
    // state.
    const actualItems = await remoteCall.queryElements(appId, [menuQuery]);
    let correctCommands = true;
    msg = '\nContext menu in the wrong order/state for: ' + breadcrumbsPath;
    for (let i = 0; i < Math.max(menuStates.length, actualItems.length); i++) {
      let expectedCommand = undefined;
      let expectedState = undefined;
      let actualCommand = undefined;
      let actualState = undefined;
      if (menuStates[i]) {
        expectedCommand = menuStates[i]![0];
        expectedState = menuStates[i]![1];
      }
      if (actualItems[i]) {
        actualCommand = actualItems[i]!.attributes['command'];
        actualState = actualItems[i]!.attributes['disabled'] ? false : true;
      }
      msg += '\n';
      if (expectedCommand !== actualCommand || expectedState !== actualState) {
        correctCommands = false;
      }
      msg += ` index: ${i}`;
      msg += `\n\t expected: ${expectedCommand} ${stateString(expectedState)}`;
      msg += `\n\t      got: ${actualCommand} ${stateString(actualState)}`;
    }
    return correctCommands;
  }

  // Check if commands are the way we expect, otherwise we try one more time.
  if (await isCommandsEnabledAndOrdered()) {
    return;
  }
  console.warn('Menus items did not match (trying again)...\n' + msg);
  await wait(REPEAT_UNTIL_INTERVAL);

  // Try the context menu one more time.
  await remoteCall.waitForElement(appId, query);
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, [query]),
      'fakeMouseRightClick failed');

  // Wait for context menu to appear.
  await remoteCall.waitForElement(appId, menuQuery);

  if (!await isCommandsEnabledAndOrdered()) {
    chrome.test.assertTrue(false, msg);
  }
}

/**
 * Tests copying a directory from directory tree with context menu.
 */
export async function dirCopyWithContextMenu() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await clickDirectoryTreeContextMenuItem(appId, '/Downloads/photos', 'copy');
  await navigateToDestinationDirectoryAndTestPaste(appId);
}

/**
 * Tests copying a directory from directory tree with the keyboard shortcut.
 */
export async function dirCopyWithKeyboard() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');

  // Press Ctrl+C.
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'c', true /* ctrl */, false, false]);
  await navigateToDestinationDirectoryAndTestPaste(appId);
}

/**
 * Tests copying a directory without changing the current directory.
 */
export async function dirCopyWithoutChangingCurrent() {
  const appId = await setupForDirectoryTreeContextMenuTest();

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel('Downloads');
  await clickDirectoryTreeContextMenuItem(appId, '/Downloads/photos', 'copy');
  await navigateToDestinationDirectoryAndTestPaste(appId);
}

/**
 * Tests cutting a directory with the context menu.
 */
export async function dirCutWithContextMenu() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await clickDirectoryTreeContextMenuItem(appId, '/Downloads/photos', 'cut');
  await navigateToDestinationDirectoryAndTestPaste(appId);

  // Confirm that directory tree is updated.
  await directoryTree.waitForItemLostByPath('/Downloads/photos');
}

/**
 * Tests cutting a directory with the keyboard shortcut.
 */
export async function dirCutWithKeyboard() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');

  // Press Ctrl+X.
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'x', true /* ctrl */, false, false]);
  await navigateToDestinationDirectoryAndTestPaste(appId);

  // Confirm that directory tree is updated.
  await directoryTree.waitForItemLostByPath('/Downloads/photos');
}

/**
 * Tests cutting a directory without changing the current directory.
 */
export async function dirCutWithoutChangingCurrent() {
  const appId = await setupForDirectoryTreeContextMenuTest();

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel('Downloads');
  await clickDirectoryTreeContextMenuItem(appId, '/Downloads/photos', 'cut');
  await navigateToDestinationDirectoryAndTestPaste(appId);
  await directoryTree.waitForItemLostByPath('/Downloads/photos');
}

/**
 * Tests pasting into folder with the context menu.
 */
export async function dirPasteWithContextMenu() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  const destinationPath = '/Downloads/destination';

  // Copy photos directory as a test data.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'c', true /* ctrl */, false, false]);
  await directoryTree.navigateToPath(`/My files${destinationPath}`);

  // Confirm files before paste.
  await remoteCall.waitForFiles(
      appId, ITEMS_IN_DEST_DIR_BEFORE_PASTE, {ignoreLastModifiedTime: true});

  await clickDirectoryTreeContextMenuItem(
      appId, destinationPath, 'paste-into-folder');

  // Confirm the photos directory is pasted correctly.
  await remoteCall.waitForFiles(
      appId, ITEMS_IN_DEST_DIR_AFTER_PASTE, {ignoreLastModifiedTime: true});

  // Expand the directory tree.
  await directoryTree.expandTreeItemByPath(destinationPath);

  // Confirm the copied directory is added to the directory tree.
  await directoryTree.waitForItemByPath(`${destinationPath}/photos`);
}

/**
 * Tests pasting into a folder without changing the current directory.
 */
export async function dirPasteWithoutChangingCurrent() {
  const destinationPath = '/Downloads/destination';

  const appId = await setupForDirectoryTreeContextMenuTest();
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel('Downloads');
  await directoryTree.focusTree();
  await clickDirectoryTreeContextMenuItem(appId, '/Downloads/photos', 'copy');
  await clickDirectoryTreeContextMenuItem(
      appId, destinationPath, 'paste-into-folder');
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'destination', /* hasChildren= */ true);
  await directoryTree.expandTreeItemByPath(destinationPath);

  // Confirm the copied directory is added to the directory tree.
  await directoryTree.waitForItemByPath(`${destinationPath}/photos`);
}

/**
 * Tests renaming a folder with the context menu.
 */
export async function dirRenameWithContextMenu() {
  return renameDirectoryFromDirectoryTreeSuccessCase(
      false /* do not use keyboard shortcut */);
}

/**
 * Tests that a child folder breadcrumbs is updated when renaming its parent
 * folder. crbug.com/885328.
 */
export async function dirRenameUpdateChildrenBreadcrumbs() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Add child-folder inside /photos/
  await addEntries(['local'], [new TestEntryInfo({
                     type: EntryType.DIRECTORY,
                     targetPath: 'photos/child-folder',
                     lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
                     nameText: 'child-folder',
                     sizeText: '--',
                     typeText: 'Folder',
                   })]);

  // Navigate to child folder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos/child-folder');

  // Rename parent folder.
  await clickDirectoryTreeContextMenuItem(appId, '/Downloads/photos', 'rename');
  await directoryTree.renameItemByLabel('photos', 'photos-new');

  // Confirm that current directory is now My files or /Downloads, because it
  // can't find the previously selected folder /Downloads/photos/child-folder,
  // since its path/parent has been renamed.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Navigate to child-folder using the new path.
  // |navigateWithDirectoryTree| already checks for breadcrumbs to
  // match the path.
  await directoryTree.navigateToPath(
      '/My files/Downloads/photos-new/child-folder');
}

/**
 * Tests renaming folder with the keyboard shortcut.
 */
export async function dirRenameWithKeyboard() {
  return renameDirectoryFromDirectoryTreeSuccessCase(
      true /* use keyboard shortcut */);
}

/**
 * Tests renaming folder without changing the current directory.
 */
export async function dirRenameWithoutChangingCurrent() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel('Downloads');
  await directoryTree.waitForItemByPath('/Downloads/photos');
  await renamePhotosDirectoryTo(
      appId, 'New photos', false /* Do not use keyboard shortcut. */);
  await directoryTree.waitForItemByPath('/Downloads/New photos');
}

/**
 * Tests renaming a folder to an empty string.
 */
export async function dirRenameToEmptyString() {
  const appId = await setupForDirectoryTreeContextMenuTest();

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await renamePhotosDirectoryTo(appId, '', false);
  // The folder name is not changed.
  await directoryTree.waitForChildItemByLabel('Downloads', 'photos');

  // No dialog should be shown.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container.shown');
}

/**
 * Tests renaming folder an existing name.
 */
export async function dirRenameToExisting() {
  return renameDirectoryFromDirectoryTreeAndConfirmAlertDialog('destination');
}

/**
 * Tests renaming removable volume with the keyboard.
 */
export async function dirRenameRemovableWithKeyboard() {
  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount a single partition NTFS USB volume: they can be renamed.
  await sendTestMessage({name: 'mountFakeUsb', filesystem: 'ntfs'});

  // Wait for the USB mount and click the USB volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('removable');

  // Check: the USB should be the currently focused directory tree item.
  await directoryTree.waitForFocusedItemByLabel('fake-usb');
  // Focus the directory tree.
  await directoryTree.focusTree();

  // Check: the USB volume is still the currently focused directory tree item.
  await directoryTree.waitForFocusedItemByLabel('fake-usb');

  // Rename the USB.
  await directoryTree.triggerRenameWithKeyboardByLabel('fake-usb');
  await directoryTree.renameItemByLabel('fake-usb', 'usb-was-renamed');
  await directoryTree.waitForItemByLabel('usb-was-renamed');
}

/**
 * Tests renaming removable volume with the context menu.
 */
export async function dirRenameRemovableWithContentMenu() {
  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount a single partition NTFS USB volume: they can be renamed.
  await sendTestMessage({name: 'mountFakeUsb', filesystem: 'ntfs'});

  // Wait for the USB mount and click the USB volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('removable');

  // Check: the USB should be the currently focused directory tree item.
  await directoryTree.waitForFocusedItemByLabel('fake-usb');

  // Focus the directory tree.
  await directoryTree.focusTree();

  // Check: the USB should be the currently focused directory tree item.
  await directoryTree.waitForFocusedItemByLabel('fake-usb');

  // Right-click the USB volume.
  await directoryTree.showContextMenuForItemByLabel('fake-usb');

  // Check: a context menu with a 'rename' item should appear.
  const renameItem =
      'cr-menu-item[command="#rename"]:not([hidden]):not([disabled])';
  await remoteCall.waitForElement(appId, renameItem);

  // Click the context menu 'rename' item.
  await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [renameItem]);

  // Rename the USB.
  await directoryTree.renameItemByLabel('fake-usb', 'usb-was-renamed');
  await directoryTree.waitForItemByLabel('usb-was-renamed');
}

/**
 * Tests that opening context menu in the rename input won't commit the
 * renaming.
 */
export async function dirContextMenuForRenameInput() {
  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Navigate to the photos folder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');

  // Start renaming the photos folder.
  await clickDirectoryTreeContextMenuItem(appId, '/Downloads/photos', 'rename');

  // Rename the item without committing the rename.
  await directoryTree.inputNewNameForItemByLabel('photos', 'NEW NAME');

  // Right click to show the context menu.
  await directoryTree.showContextMenuForRenameInputByLabel('photos');

  // Context menu must be visible.
  const contextMenu = '#text-context-menu:not([hidden])';
  await remoteCall.waitForElement(appId, contextMenu);

  // Dismiss the context menu.
  const escKey = [contextMenu, 'Escape', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, escKey);

  // Check: The rename input should be still be visible and with the same
  // content.
  const inputElement = await directoryTree.waitForRenameInputByLabel('photos');
  chrome.test.assertEq('NEW NAME', inputElement.value);

  // Check: The rename input should be the focused element.
  const focusedElement =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq(inputElement, focusedElement);
}

/**
 * Tests creating a folder with the context menu.
 */
export async function dirCreateWithContextMenu() {
  return createDirectoryFromDirectoryTree(
      false /* do not use keyboard shortcut */,
      true /* change current directory */);
}

/**
 * Tests creating a folder with the keyboard shortcut.
 */
export async function dirCreateWithKeyboard() {
  return createDirectoryFromDirectoryTree(
      true /* use keyboard shortcut */, true /* change current directory */);
}

/**
 * Tests creating folder without changing the current directory.
 */
export async function dirCreateWithoutChangingCurrent() {
  return createDirectoryFromDirectoryTree(
      false /* Do not use keyboard shortcut */,
      false /* Do not change current directory */);
}

/**
 * Tests the creation of new folders from the directory tree from the context
 * menu. Creates the new folders in random order to ensure directory tree
 * sorting does not break folder renaming. crbug.com/1004717
 */
export async function dirCreateMultipleFolders() {
  const caller = getCaller();

  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

  const createNewFolder = async (name: string) => {
    // Ctrl+E to create a new folder in downloads.
    await directoryTree.focusTree();
    await directoryTree.selectItemByLabel('Downloads');
    await remoteCall.fakeKeyDown(appId, 'body', 'e', true, false, false);

    // Rename folder.
    await directoryTree.renameItemByLabel('New folder', name);
  };

  const checkDownloadsSubFolders = async (expectedLabels: string[]) => {
    const directoryItems =
        await directoryTree.getChildItemsByParentLabel('Downloads');
    const directoryItemsLabels =
        directoryItems.map(child => directoryTree.getItemLabel(child));

    // Check downloads subfolders are created in sorted order.
    const equalLength = expectedLabels.length === directoryItemsLabels.length;
    for (let i = 0; i < expectedLabels.length; i++) {
      if (!equalLength || expectedLabels[i] !== directoryItemsLabels[i]) {
        return pending(
            caller,
            'Waiting for downloads subfolders to be created in sorted order');
      }
    }
    return undefined;
  };

  // The folders in sorted order would be 111, aaa. Create these
  // folders in random order. crbug.com/1004717
  const names = ['aaa', '111'];
  while (names.length) {
    const index = Math.floor(Math.random() * names.length);
    const [deletedName] = names.splice(index, 1);
    await createNewFolder(deletedName!);
  }

  // Check: the new folders should have been created in the right order.
  await repeatUntil(async () => {
    return checkDownloadsSubFolders(['111', 'aaa']);
  });
}

/**
 * Tests context menu for Recent root, currently it doesn't show context menu.
 */
export async function dirContextMenuRecent() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Focus the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.focusTree();

  // Select Recent root.
  await directoryTree.selectItemByLabel('Recent');

  // Wait it to navigate to it.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Recent');

  // Right click Recent root.
  await directoryTree.showContextMenuForItemByLabel('Recent');

  // Check that both menus are still hidden.
  await remoteCall.waitForElement(appId, '#roots-context-menu[hidden]');
  await remoteCall.waitForElement(
      appId, '#directory-tree-context-menu[hidden]');
}

/**
 * Tests context menu for a ZIP root inside it.
 */
export async function dirContextMenuZip() {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch',
  });

  const zipMenus = [
    ['#unmount', true],
    ['#share-with-linux', true],
  ];

  // Open Files app on Downloads containing a ZIP file.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchive], []);

  // Select the ZIP file.
  await remoteCall.waitUntilSelected(appId, ENTRIES.zipArchive.nameText);

  // Press the Enter key to mount the ZIP file.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check the context menu is on desired state.
  await checkContextMenu(appId, '/archive.zip', zipMenus, true /* rootMenu */);

  // checkContextMenu leaves the context menu open, so just click on the eject
  // menu item.
  await remoteCall.waitAndClickElement(
      appId, '#roots-context-menu [command="#unmount"]:not([disabled])');

  // Ensure the archive has been removed.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemLostByLabel(ENTRIES.zipArchive.nameText);
}

/**
 * Tests context menu on the Eject button of a ZIP root.
 */
export async function dirContextMenuZipEject() {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch',
  });

  // Open Files app on Downloads containing a zip file.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchive], []);

  // Select the ZIP file.
  await remoteCall.waitUntilSelected(appId, ENTRIES.zipArchive.nameText);

  // Press the Enter key to mount the ZIP file.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  const directoryTree = await DirectoryTreePageObject.create(appId);

  // Focus on the eject button and right click the eject button.
  await directoryTree.showContextMenuForEjectButtonByLabel(
      ENTRIES.zipArchive.nameText);

  // Wait for, and click the eject menu item.
  await remoteCall.waitAndClickElement(
      appId,
      '#roots-context-menu:not([hidden]) ' +
          '[command="#unmount"]:not([disabled])');

  // Ensure the archive has been removed.
  await directoryTree.waitForItemLostByLabel(ENTRIES.zipArchive.nameText);
}

/**
 * Tests context menu for Shortcut roots.
 */
export async function dirContextMenuShortcut() {
  const menus = [
    ['#rename', false],
    ['#unpin-folder', true],
    ['#share-with-linux', true],
  ];
  const entry = ENTRIES.directoryD;
  const entryName = entry.nameText;

  // Open Files app on Drive.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [entry]);

  // Create a shortcut to directory D.
  await remoteCall.createShortcut(appId, entryName);

  // Check the context menu is on desired state.
  await checkContextMenu(
      appId, `/${entryName}`, menus, true /* rootMenu */,
      `/My Drive/${entryName}`);
}

/**
 * Tests context menu for MyFiles, Downloads and sub-folder.
 */
export async function dirContextMenuMyFilesWithPaste() {
  const myFilesMenus = [
    ['#share-with-linux', true],
    ['#new-folder', true],
  ];
  const downloadsMenus = [
    ['#cut', false],
    ['#copy', true],
    ['#paste-into-folder', true],
    ['#share-with-linux', true],
    ['#move-to-trash', false],
    ['#new-folder', true],
  ];
  const photosTwoMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', true],
    ['#share-with-linux', true],
    ['#rename', true],
    ['#move-to-trash', true],
    ['#new-folder', true],
  ];

  const photosTwo = new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'photosTwo',
    lastModifiedTime: 'Jan 1, 1990, 11:59 PM',
    nameText: 'photosTwo',
    sizeText: '--',
    typeText: 'Folder',
  });

  const photosT = new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'photosT',
    mimeType: 'text/plain',
    lastModifiedTime: 'Jan 1, 1993, 11:59 PM',
    nameText: 'photosT',
    sizeText: '51 bytes',
    typeText: 'Plain text',
  });

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS,
      [ENTRIES.beautiful, ENTRIES.photos, ENTRIES.hello, photosTwo, photosT],
      []);

  {
    // Select and copy photos directory into the clipboard to test
    // paste-into-folder command.
    await remoteCall.waitUntilSelected(appId, ENTRIES.photos.nameText);
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']),
        'execCommand failed');

    const photosMenus = [
      ['#cut', true],
      ['#copy', true],
      ['#paste-into-folder', false],
      ['#share-with-linux', true],
      ['#rename', true],
      ['#move-to-trash', true],
      ['#new-folder', true],
    ];

    // Check the context menu is on desired state for MyFiles.
    await checkContextMenu(
        appId, '/My files', myFilesMenus, false /* rootMenu */);

    // Check the context menu for MyFiles>Downloads.
    await checkContextMenu(
        appId, '/My files/Downloads', downloadsMenus, false /* rootMenu */);

    // Check the context menu for MyFiles>Downloads>photos.
    await checkContextMenu(
        appId, '/My files/Downloads/photos', photosMenus, false /* rootMenu */);

    // Check the context menu for MyFiles>Downloads>photosTwo.
    // This used to be a breaking case as photos is a substring of photosTwo,
    // and we would treat photosTwo as a descendant of photos.
    // See crbug.com/1032436.
    await checkContextMenu(
        appId, '/My files/Downloads/photosTwo', photosTwoMenus,
        false /* rootMenu */);
  }

  {
    const directoryTree = await DirectoryTreePageObject.create(appId);
    await directoryTree.navigateToPath('/My files/Downloads');
    // Select and copy photosT file into the clipboard to test
    // paste-into-folder command.
    await remoteCall.waitUntilSelected(appId, 'photosT');
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']),
        'execCommand failed');

    const photosMenus = [
      ['#cut', true],
      ['#copy', true],
      ['#paste-into-folder', true],
      ['#share-with-linux', true],
      ['#rename', true],
      ['#move-to-trash', true],
      ['#new-folder', true],
    ];

    // Check the context menu is on desired state for MyFiles.
    await checkContextMenu(
        appId, '/My files', myFilesMenus, false /* rootMenu */);

    // Check the context menu for MyFiles>Downloads.
    await checkContextMenu(
        appId, '/My files/Downloads', downloadsMenus, false /* rootMenu */);

    // Check the context menu for MyFiles>Downloads>photos.
    await checkContextMenu(
        appId, '/My files/Downloads/photos', photosMenus, false /* rootMenu */);

    // Check the context menu for MyFiles>Downloads>photosTwo.
    await checkContextMenu(
        appId, '/My files/Downloads/photosTwo', photosTwoMenus,
        false /* rootMenu */);
  }
}

/**
 * Tests context menu for MyFiles, Downloads and sub-folder.
 */
export async function dirContextMenuMyFiles() {
  const myFilesMenus = [
    ['#share-with-linux', true],
    ['#new-folder', true],
  ];
  const downloadsMenus = [
    ['#cut', false],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#share-with-linux', true],
    ['#move-to-trash', false],
    ['#new-folder', true],
  ];
  const photosMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#share-with-linux', true],
    ['#rename', true],
    ['#move-to-trash', true],
    ['#new-folder', true],
  ];

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.photos], []);

  // Check the context menu is on desired state for MyFiles.
  await checkContextMenu(
      appId, '/My files', myFilesMenus, false /* rootMenu */);

  // Check the context menu for MyFiles>Downloads.
  await checkContextMenu(
      appId, '/My files/Downloads', downloadsMenus, false /* rootMenu */);

  // Check the context menu for MyFiles>Downloads>photos.
  await checkContextMenu(
      appId, '/My files/Downloads/photos', photosMenus, false /* rootMenu */);

  // Right click Linux files (FakeEntry).
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.showContextMenuForItemByLabel('Linux files');

  // Wait a few milliseconds to give menu a chance to display.
  await wait(REPEAT_UNTIL_INTERVAL);

  // Fetch all visible cr-menu's.
  const elements =
      await remoteCall.queryElements(appId, ['cr-menu:not([hidden])']);

  // Check: No context menus should be visible for FakeEntry.
  chrome.test.assertEq(0, elements.length);
}

/**
 * Tests context menu for Crostini real root and a folder inside it.
 */
export async function dirContextMenuCrostini() {
  const linuxMenus = [
    ['#new-folder', true],
  ];
  const folderMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#rename', true],
    ['#delete', true],
    ['#new-folder', true],
  ];

  // Add a crostini folder.
  await addEntries(['crostini'], [ENTRIES.photos]);

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Select Crostini, because the first right click doesn't show any context
  // menu, just actually mounts crostini converting the tree item from fake to
  // real root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectPlaceholderItemByType('crostini');

  // Wait for the real root to appear.
  await directoryTree.waitForItemByType('crostini');

  // Check the context menu for Linux files.
  await checkContextMenu(
      appId, '/My files/Linux files', linuxMenus, false /* rootMenu */);

  // Check the context menu for a folder in Linux files.
  await checkContextMenu(
      appId, '/My files/Linux files/photos', folderMenus, false /* rootMenu */);
}

/**
 * Tests context menu for ARC++/Play files root and a folder inside it.
 */
export async function dirContextMenuPlayFiles() {
  const playFilesMenus = [
    ['#new-folder', false],
  ];
  const folderMenus = [
    ['#cut', false],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#share-with-linux', true],
    ['#delete', false],
    ['#new-folder', true],
  ];

  // Add an Android folder.
  await addEntries(['android_files'], [ENTRIES.directoryDocuments]);

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check the context menu for Play files.
  await checkContextMenu(
      appId, '/My files/Play files', playFilesMenus, false /* rootMenu */);

  // Check the context menu for a folder in Play files.
  await checkContextMenu(
      appId, '/My files/Play files/Documents', folderMenus,
      false /* rootMenu */);
}

/**
 * Tests context menu for USB root (single and multiple partitions).
 */
export async function dirContextMenuUsbs() {
  const ext4UsbMenus = [
    ['#unmount', true],
    ['#format', true],
    ['#rename', false],
    ['#share-with-linux', true],
  ];
  const ntfsUsbMenus = [
    ['#unmount', true],
    ['#format', true],
    ['#rename', true],
    ['#share-with-linux', true],
  ];
  const partitionsRootMenus = [
    ['#unmount', true],
    ['#format', false],
  ];
  const partition1Menus = [
    ['#share-with-linux', true],
    ['#format', true],
    ['#rename', false],
    ['#new-folder', true],
  ];
  const folderMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#share-with-linux', true],
    ['#rename', true],
    ['#delete', true],
    ['#new-folder', true],
  ];
  const ext4DeviceMenus = [
    ['#unmount', true],
    ['#erase-device', true],
  ];
  const ext4PartitionMenus = [
    ['#share-with-linux', true],
    ['#format', true],
    ['#rename', false],
    ['#new-folder', true],
  ];
  const ntfsDeviceMenus = [
    ['#unmount', true],
    ['#erase-device', true],
  ];
  const ntfsPartitionMenus = [
    ['#share-with-linux', true],
    ['#format', true],
    ['#rename', true],
    ['#new-folder', true],
  ];
  const deviceMenus = [
    ['#unmount', true],
    ['#erase-device', true],
  ];

  // Mount removable volumes.
  await sendTestMessage({name: 'mountUsbWithPartitions'});
  await sendTestMessage({name: 'mountFakeUsb', filesystem: 'ext4'});

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  if (await remoteCall.isSinglePartitionFormat(appId)) {
    // Check the context menu for single partition drive.
    await checkContextMenu(
        appId, '/FAKEUSB', ext4DeviceMenus, true /* rootMenu */);

    // Check the context menu for single partition ext4 USB.
    await checkContextMenu(
        appId, '/FAKEUSB/fake-usb', ext4PartitionMenus, false /* rootMenu */);

    // Check the context menu for a folder inside a single USB partition.
    await checkContextMenu(
        appId, '/FAKEUSB/fake-usb/A', folderMenus, false /* rootMenu */);

    // Check the context menu for multiple partitions USB (root).
    await checkContextMenu(
        appId, '/Drive Label', deviceMenus, true /* rootMenu */);

    // Check the context menu for multiple partitions USB (actual partition).
    await checkContextMenu(
        appId, '/Drive Label/partition-1', partition1Menus,
        false /* rootMenu */);

    // Check the context menu for a folder inside a partition1.
    await checkContextMenu(
        appId, '/Drive Label/partition-1/A', folderMenus, false /* rootMenu */);

    // Remount the single partition ext4 USB as NTFS
    await sendTestMessage({name: 'unmountUsb'});
    await sendTestMessage({name: 'mountFakeUsb', filesystem: 'ntfs'});

    // Check the context menu for a single partition NTFS USB.
    await checkContextMenu(
        appId, '/FAKEUSB', ntfsDeviceMenus, true /* rootMenu */);

    // Check the context menu for a single partition NTFS USB.
    await checkContextMenu(
        appId, '/FAKEUSB/fake-usb', ntfsPartitionMenus, false /* rootMenu */);
  } else {
    // Check the context menu for single partition ext4 USB.
    await checkContextMenu(
        appId, '/fake-usb', ext4UsbMenus, true /* rootMenu */);

    // Check the context menu for a folder inside a single USB partition.
    await checkContextMenu(
        appId, '/fake-usb/A', folderMenus, false /* rootMenu */);

    // Check the context menu for multiple partitions USB (root).
    await checkContextMenu(
        appId, '/Drive Label', partitionsRootMenus, true /* rootMenu */);

    // Check the context menu for multiple partitions USB (actual partition).
    await checkContextMenu(
        appId, '/Drive Label/partition-1', partition1Menus,
        false /* rootMenu */);

    // Check the context menu for a folder inside a partition1.
    await checkContextMenu(
        appId, '/Drive Label/partition-1/A', folderMenus, false /* rootMenu */);

    // Remount the single partition ext4 USB as NTFS
    await sendTestMessage({name: 'unmountUsb'});
    await sendTestMessage({name: 'mountFakeUsb', filesystem: 'ntfs'});

    // Check the context menu for a single partition NTFS USB.
    await checkContextMenu(
        appId, '/fake-usb', ntfsUsbMenus, true /* rootMenu */);
  }
}

/**
 * Tests context menu for USB root with DCIM folder.
 */
export async function dirContextMenuUsbDcim() {
  const usbMenus = [
    ['#unmount', true],
    ['#format', true],
    ['#rename', false],
    ['#share-with-linux', true],
  ];
  const dcimFolderMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#share-with-linux', true],
    ['#rename', true],
    ['#delete', true],
    ['#new-folder', true],
  ];
  const deviceUsbMenus = [
    ['#share-with-linux', true],
    ['#format', true],
    ['#rename', false],
    ['#new-folder', true],
  ];

  // Mount removable volumes.
  await sendTestMessage({name: 'mountFakeUsbDcim'});

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  if (await remoteCall.isSinglePartitionFormat(appId)) {
    // Check the context menu for single partition USB.
    await checkContextMenu(
        appId, '/FAKEUSB/fake-usb', deviceUsbMenus, false /* rootMenu */);

    // Check the context menu for the DCIM folder inside USB.
    await checkContextMenu(
        appId, '/FAKEUSB/fake-usb/DCIM', dcimFolderMenus, false /* rootMenu */);
  } else {
    // Check the context menu for single partition USB.
    await checkContextMenu(appId, '/fake-usb', usbMenus, true /* rootMenu */);

    // Check the context menu for the DCIM folder inside USB.
    await checkContextMenu(
        appId, '/fake-usb/DCIM', dcimFolderMenus, false /* rootMenu */);
  }
}

/*
 * Tests context menu for Mtp root and a folder inside it.
 */
export async function dirContextMenuMtp() {
  const folderMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#rename', true],
    ['#delete', true],
    ['#new-folder', true],
  ];

  // Mount fake MTP volume.
  await sendTestMessage({name: 'mountFakeMtp'});

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Select Recent root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'fake-mtp', /* hasChildren= */ true);
  await directoryTree.selectItemByLabel('fake-mtp');

  // Right click on MTP root.
  await directoryTree.showContextMenuForItemByLabel('fake-mtp');

  // Check that both menus are still hidden, since there is not context menu
  // for MTP root.
  await remoteCall.waitForElement(appId, '#roots-context-menu[hidden]');
  await remoteCall.waitForElement(
      appId, '#directory-tree-context-menu[hidden]');

  // Check the context menu for a folder inside a MTP.
  await checkContextMenu(
      appId, '/fake-mtp/A', folderMenus, false /* rootMenu */);
}

/**
 * Tests context menu for FSP root and a folder inside it.
 */
export async function dirContextMenuFsp() {
  const fspMenus = [
    ['#unmount', true],
  ];
  const folderMenus = [
    ['#cut', false],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#rename', false],
    ['#delete', false],
    ['#new-folder', false],
  ];

  // Install a FSP.
  const manifest = 'manifest_source_file.json';
  await sendTestMessage({name: 'launchProviderExtension', manifest: manifest});

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check the context menu for FSP root.
  await checkContextMenu(appId, '/Test (1)', fspMenus, true /* rootMenu */);

  // Check the context menu for a folder inside a FSP.
  await checkContextMenu(
      appId, '/Test (1)/folder', folderMenus, false /* rootMenu */);
}

/**
 * Tests context menu for DocumentsProvider root and a folder inside it.
 */
export async function dirContextMenuDocumentsProvider() {
  const folderMenus = [
    ['#cut', false],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#rename', false],
    ['#delete', false],
    ['#new-folder', false],
  ];

  // Add a DocumentsProvider folder.
  await addEntries(['documents_provider'], [ENTRIES.readOnlyFolder]);

  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Wait for DocumentsProvider to appear.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToHaveChildrenByLabel(
      'DocumentsProvider', /* hasChildren= */ true);

  // Select DocumentsProvider root.
  await directoryTree.selectItemByLabel('DocumentsProvider');

  // Wait it to navigate to it.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/DocumentsProvider');

  // Right click DocumentsProvider root.
  await directoryTree.showContextMenuForItemByLabel('DocumentsProvider');

  // Check that both menus are still hidden, because DocumentsProvider root
  // doesn't show any context menu.
  await remoteCall.waitForElement(appId, '#roots-context-menu[hidden]');
  await remoteCall.waitForElement(
      appId, '#directory-tree-context-menu[hidden]');

  // Check the context menu for a folder inside a DocumentsProvider.
  await checkContextMenu(
      appId, '/DocumentsProvider/Read-Only Folder', folderMenus,
      false /* rootMenu */);
}

/**
 * Tests context menu for My Drive, read-only and read-write folder inside it.
 */
export async function dirContextMenuMyDrive() {
  const myDriveMenus = [
    ['#share-with-linux', true],
    ['#new-folder', true],
  ];
  const readOnlyFolderMenus = [
    ['#cut', false],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#share-with-linux', true],
    ['#rename', false],
    ['#pin-folder', true],
    ['#delete', false],
    ['#new-folder', false],
  ];
  const readWriteFolderMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', true],
    ['#share-with-linux', true],
    ['#rename', true],
    ['#pin-folder', true],
    ['#delete', true],
    ['#new-folder', true],
  ];

  // Open Files App on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], COMPLEX_DRIVE_ENTRY_SET);

  // Select and copy hello.txt into the clipboard to test paste-into-folder
  // command.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']),
      'execCommand failed');

  // Check that Google Drive is expanded.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToExpandByLabel('Google Drive');

  // Check the context menu for My Drive root.
  await checkContextMenu(
      appId, '/My Drive', myDriveMenus, false /* rootMenu */);

  // Check the context menu for read-only folder.
  await checkContextMenu(
      appId, '/My Drive/Read-Only Folder', readOnlyFolderMenus,
      false /* rootMenu */);

  // Check the context menu for read+write folder.
  await checkContextMenu(
      appId, '/My Drive/photos', readWriteFolderMenus, false /* rootMenu */);
}

/**
 * Tests context menu for Shared drives grand-root, a read+write shared drive
 * root, a folder inside it, a read-only shared drive and a folder inside
 * it.
 */
export async function dirContextMenuSharedDrive() {
  const sharedDriveGrandRootMenus = [
    ['#share-with-linux', true],
  ];
  const readWriteSharedDriveRootMenus = [
    ['#cut', false],
    ['#copy', false],
    ['#paste-into-folder', true],
    ['#share-with-linux', true],
    ['#rename', false],
    ['#delete', true],
    ['#new-folder', true],
  ];
  const readWriteFolderMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', true],
    ['#share-with-linux', true],
    ['#rename', true],
    ['#pin-folder', true],
    ['#delete', true],
    ['#new-folder', true],
  ];
  const readOnlySharedDriveRootMenus = [
    ['#cut', false],
    ['#copy', false],
    ['#paste-into-folder', false],
    ['#share-with-linux', true],
    ['#rename', false],
    ['#delete', false],
    ['#new-folder', false],
  ];
  const readOnlyFolderMenus = [
    ['#cut', false],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#share-with-linux', true],
    ['#rename', false],
    ['#pin-folder', true],
    ['#delete', false],
    ['#new-folder', false],
  ];

  // Open Files App on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], SHARED_DRIVE_ENTRY_SET);

  // Select and copy hello.txt into the clipboard to test paste-into-folder
  // command.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']),
      'execCommand failed');

  // Check that Google Drive is expanded.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToExpandByLabel('Google Drive');

  // Check the context menu for Shared drives grand root.
  await checkContextMenu(
      appId, '/Shared drives', sharedDriveGrandRootMenus, false /* rootMenu */);

  // Check the context menu for a read+write shared drive root.
  await checkContextMenu(
      appId, '/Shared drives/Team Drive A', readWriteSharedDriveRootMenus,
      false /* rootMenu */);

  // Check the context menu for read+write folder.
  await checkContextMenu(
      appId, '/Shared drives/Team Drive A/teamDriveADirectory',
      readWriteFolderMenus, false /* rootMenu */);

  // Check the context menu for a read-only shared drive root.
  await checkContextMenu(
      appId, '/Shared drives/Team Drive B', readOnlySharedDriveRootMenus,
      false /* rootMenu */);

  // Check the context menu for read+write folder.
  await checkContextMenu(
      appId, '/Shared drives/Team Drive B/teamDriveBDirectory',
      readOnlyFolderMenus, false /* rootMenu */);
}

/**
 * Tests context menu for Google Drive/Shared with me root, currently it
 * doesn't show context menu.
 */
export async function dirContextMenuSharedWithMe() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);

  // Focus the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.focusTree();

  // Select Shared with me root.
  await directoryTree.selectItemByLabel('Shared with me');

  // Wait it to navigate to it.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Right click Shared with me root.
  await directoryTree.showContextMenuForItemByLabel('Shared with me');

  // Check that both menus are still hidden.
  await remoteCall.waitForElement(appId, '#roots-context-menu[hidden]');
  await remoteCall.waitForElement(
      appId, '#directory-tree-context-menu[hidden]');
}

/**
 * Tests context menu for Google Drive/Offline root, currently it doesn't show
 * context menu.
 */
export async function dirContextMenuOffline() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);

  // Focus the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.focusTree();

  // Select Shared with me root.
  await directoryTree.selectItemByLabel('Offline');

  // Wait it to navigate to it.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Offline');

  // Right click Shared with me root.
  await directoryTree.showContextMenuForItemByLabel('Offline');

  // Check that both menus are still hidden.
  await remoteCall.waitForElement(appId, '#roots-context-menu[hidden]');
  await remoteCall.waitForElement(
      appId, '#directory-tree-context-menu[hidden]');
}

/**
 * Tests context menu for Google Drive/Computer grand-root, a computer root, a
 * folder inside it.
 */
export async function dirContextMenuComputers() {
  const computersGrandRootMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#rename', false],
    ['#delete', false],
    ['#new-folder', false],
  ];
  const computerRootMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#rename', false],
    ['#delete', false],
    ['#new-folder', false],
  ];
  const folderMenus = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', true],
    ['#share-with-linux', true],
    ['#rename', false],
    ['#pin-folder', true],
    ['#delete', true],
    ['#new-folder', true],
  ];

  // Open Files App on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], COMPUTERS_ENTRY_SET);

  // Select and copy hello.txt into the clipboard to test paste-into-folder
  // command.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']),
      'execCommand failed');

  // Check that Google Drive is expanded.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToExpandByLabel('Google Drive');

  // Check the context menu for Computers grand root.
  await checkContextMenu(
      appId, '/Computers', computersGrandRootMenus, false /* rootMenu */);

  // Check the context menu for a computer root.
  await checkContextMenu(
      appId, '/Computers/Computer A', computerRootMenus, false /* rootMenu */);

  // Check the context menu for a folder inside a computer.
  await checkContextMenu(
      appId, '/Computers/Computer A/A', folderMenus, false /* rootMenu */);
}

/**
 * Tests context menu for Trash root.
 */
export async function dirContextMenuTrash() {
  const trashMenu = [
    ['#empty-trash', true],
  ];

  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Check the context menu for Trash.
  await checkContextMenu(appId, '/Trash', trashMenu, /*rootMenu=*/ false);
}

/**
 * Tests that context menu in directory tree gets the focus, so ChromeVox can
 * announce it.
 */
export async function dirContextMenuFocus() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Wait for /My files/Downloads to appear in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Downloads');

  // Right-click the /My files/Downloads tree row.
  await directoryTree.showContextMenuForItemByLabel('Downloads');

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(
      appId, '#directory-tree-context-menu:not([hidden])');

  // Wait for the menu item to get focus.
  await remoteCall.waitForElement(
      appId, '#directory-tree-context-menu cr-menu-item:focus');

  // Check currently focused element.
  const focusedElement =
      await remoteCall.callRemoteTestUtil<ElementObject|null>(
          'getActiveElement', appId, []);
  chrome.test.assertEq('menuitem', focusedElement?.attributes['role']);
}

/**
 * Test that the directory tree context menu can be opened by keyboard
 * navigation.
 */
export async function dirContextMenuKeyboardNavigation() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Navigate to /My files/Downloads which will focus the Downloads tree item.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads');

  // Send a contextmenu event to the directory tree. Downloads is initially
  // focused so the subitem context menu will appear.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, [directoryTree.rootSelector, 'contextmenu']));

  // Wait for context menu to appear.
  const contextMenuQuery =
      '#directory-tree-context-menu:not([hidden]) cr-menu-item:not([hidden])';
  await remoteCall.waitForElement(appId, contextMenuQuery);

  // Dismiss the context menu.
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, [contextMenuQuery, 'Escape', false, false, false]);
  await remoteCall.waitForElementLost(appId, contextMenuQuery);

  // Move the focus up on the directory tree to "My files" via keyboard
  // navigation.
  await directoryTree.focusPreviousItem();

  // Send a contextmenu event to the directory tree.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, [directoryTree.rootSelector, 'contextmenu']));

  // Wait for the "New folder" to appear in the context menu and then press it.
  // This is to verify that the folder created is done in the correct location
  // (i.e. "My files").
  const newFolderQuery = contextMenuQuery + '[command="#new-folder"]';
  await remoteCall.waitForElement(appId, newFolderQuery);
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [newFolderQuery]);

  // Ensure it's possible to navigate to the newly created folder.
  await directoryTree.navigateToPath('/My files/New folder');
}
