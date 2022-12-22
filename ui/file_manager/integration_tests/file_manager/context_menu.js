// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, openNewWindow, remoteCall, setupAndWaitUntilReady} from './background.js';
import {FakeTask} from './tasks.js';
import {COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET, COMPLEX_DRIVE_ENTRY_SET, RECENT_ENTRY_SET} from './test_data.js';

/**
 * Tests that check the context menu displays the right options (enabled and
 * disabled) for different types of files.
 *
 * The names passed to the tests are file names to select. They are generated
 * from COMPLEX_DRIVE_ENTRY_SET (see setupAndWaitUntilReady).
 */

/**
 * Copy a text file to clipboard if the test requires it.
 *
 * @param {string} appId ID of the app window.
 * @param {string} commandId ID of the command in the context menu to check.
 */
async function maybeCopyToClipboard(appId, commandId, file = 'hello.txt') {
  if (!/^paste/.test(commandId)) {
    return;
  }
  await remoteCall.waitUntilSelected(appId, file);
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']),
      'execCommand failed');
}

/**
 * Selects a file in the file list.
 *
 * @param {string} appId ID of the app window.
 * @param {string} path Path to the file to be selected.
 */
async function selectFile(appId, path) {
  // Select the file |path|.
  await remoteCall.waitUntilSelected(appId, path);

  // Wait for the file to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');
}

/**
 * Right clicks the currently selected file in the file list and waits for its
 * context menu to appear.
 *
 * @param {string} appId ID of the app window.
 */
async function rightClickSelectedFile(appId) {
  // Right-click the selected file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the file context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');
}

/**
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * entry at |path| is selected.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} path Path to the file to open the context menu for.
 * @param {boolean} expectedEnabledState True if the command should be enabled
 *     in the context menu, false if not. Only checked if the command isn't
 * hidden.
 * @param {boolean} expectedHiddenState True if the command should be hidden
 *     in the context menu, false if not. Defaults to false.
 */
async function checkContextMenu(
    commandId, path, expectedEnabledState, expectedHiddenState = false) {
  // Open Files App on Drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], COMPLEX_DRIVE_ENTRY_SET);

  // Optionally copy hello.txt into the clipboard if needed.
  await maybeCopyToClipboard(appId, commandId);

  // Select the file |path|.
  await remoteCall.waitUntilSelected(appId, path);

  // Wait for the file to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Right-click the selected file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Wait for the command option to appear.
  let query = '#file-context-menu:not([hidden])';
  if (expectedHiddenState) {
    query += ` [command="#${commandId}"][hidden]`;
  } else {
    if (expectedEnabledState) {
      query += ` [command="#${commandId}"]:not([hidden]):not([disabled])`;
    } else {
      query += ` [command="#${commandId}"][disabled]:not([hidden])`;
    }
  }

  await remoteCall.waitForElement(appId, query);
}

/**
 * Tests that the Delete menu item is enabled if a read-write entry is selected.
 */
testcase.checkDeleteEnabledForReadWriteFile = () => {
  return checkContextMenu('delete', 'hello.txt', true);
};

/**
 * Tests that the Delete menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyDocument = () => {
  return checkContextMenu('delete', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only file is selected.
 */
testcase.checkDeleteDisabledForReadOnlyFile = () => {
  return checkContextMenu('delete', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyFolder = () => {
  return checkContextMenu('delete', 'Read-Only Folder', false);
};

/**
 * Tests that the Rename menu item is enabled if a read-write entry is selected.
 */
testcase.checkRenameEnabledForReadWriteFile = () => {
  return checkContextMenu('rename', 'hello.txt', true);
};

/**
 * Tests that the Rename menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyDocument = () => {
  return checkContextMenu('rename', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only file is selected.
 */
testcase.checkRenameDisabledForReadOnlyFile = () => {
  return checkContextMenu('rename', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyFolder = () => {
  return checkContextMenu('rename', 'Read-Only Folder', false);
};

/**
 * Tests that the Share menu item is enabled if a read-write entry is selected.
 */
testcase.checkShareEnabledForReadWriteFile = () => {
  return checkContextMenu('share', 'hello.txt', true);
};

/**
 * Tests that the Share menu item is enabled if a read-only document is
 * selected.
 */
testcase.checkShareEnabledForReadOnlyDocument = () => {
  return checkContextMenu('share', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Share menu item is disabled if a strict read-only document is
 * selected.
 */
testcase.checkShareDisabledForStrictReadOnlyDocument = () => {
  return checkContextMenu('share', 'Read-Only (Strict) Doc.gdoc', false);
};

/**
 * Tests that the Share menu item is enabled if a read-only file is selected.
 */
testcase.checkShareEnabledForReadOnlyFile = () => {
  return checkContextMenu('share', 'Read-Only File.jpg', true);
};

/**
 * Tests that the Share menu item is enabled if a read-only folder is
 * selected.
 */
testcase.checkShareEnabledForReadOnlyFolder = () => {
  return checkContextMenu('share', 'Read-Only Folder', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-write entry is selected.
 */
testcase.checkCopyEnabledForReadWriteFile = () => {
  return checkContextMenu('copy', 'hello.txt', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only document is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyDocument = () => {
  return checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is disabled if a strict (no-copy) read-only
 * document is selected.
 */
testcase.checkCopyDisabledForStrictReadOnlyDocument = () => {
  return checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only file is selected.
 */
testcase.checkCopyEnabledForReadOnlyFile = () => {
  return checkContextMenu('copy', 'Read-Only File.jpg', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only folder is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyFolder = () => {
  return checkContextMenu('copy', 'Read-Only Folder', true);
};

/**
 * Tests that the Cut menu item is enabled if a read-write entry is selected.
 */
testcase.checkCutEnabledForReadWriteFile = () => {
  return checkContextMenu('cut', 'hello.txt', true);
};

/**
 * Tests that the Cut menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyDocument = () => {
  return checkContextMenu('cut', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Cut menu item is disabled if a read-only file is selected.
 */
testcase.checkCutDisabledForReadOnlyFile = () => {
  return checkContextMenu('cut', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Restriction details menu item is hidden if DLP is disabled.
 */
testcase.checkDlpRestrictionDetailsDisabledForNonDlpFiles = () => {
  return checkContextMenu(
      'dlp-restriction-details', 'hello.txt', /*expectedEnabledState=*/ false,
      /*expectedHiddenState=*/ true);
};

/**
 * Tests that the Cut menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyFolder = () => {
  return checkContextMenu('cut', 'Read-Only Folder', false);
};

/**
 * Tests that the Paste into Folder menu item is enabled if a read-write folder
 * is selected.
 */
testcase.checkPasteIntoFolderEnabledForReadWriteFolder = () => {
  return checkContextMenu('paste-into-folder', 'photos', true);
};

/**
 * Tests that the Paste into Folder menu item is disabled if a read-only folder
 * is selected.
 */
testcase.checkPasteIntoFolderDisabledForReadOnlyFolder = () => {
  return checkContextMenu('paste-into-folder', 'Read-Only Folder', false);
};

/**
 * Tests that the "Install with Linux" file context menu item is hidden for a
 * Debian file if Crostini root access is disabled.
 */
testcase.checkInstallWithLinuxDisabledForDebianFile = async () => {
  const optionHidden = '#file-context-menu:not([hidden]) ' +
      '[command="#default-task"][hidden]';

  // Open FilesApp on Downloads with deb file.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.debPackage], []);

  // Disallow root access.
  await sendTestMessage({name: 'setCrostiniRootAccessAllowed', enabled: false});

  // Select and right click the deb file to show its context menu.
  await selectFile(appId, 'package.deb');
  await rightClickSelectedFile(appId);

  // Check: the "Install with Linux" context menu item should be hidden.
  await remoteCall.waitForElement(appId, optionHidden);
};

/**
 * Tests that the "Install with Linux" file context menu item is shown for a
 * Debian file if Crostini root access is enabled.
 */
testcase.checkInstallWithLinuxEnabledForDebianFile = async () => {
  const optionShown = '#file-context-menu:not([hidden]) ' +
      '[command="#default-task"]:not([hidden])';

  // Open FilesApp on Downloads with deb file.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.debPackage], []);

  // Select and right click the deb file to show its context menu.
  await selectFile(appId, 'package.deb');
  await rightClickSelectedFile(appId);

  // Check: the "Install with Linux" context menu item should be shown.
  await remoteCall.waitForElement(appId, optionShown);
};

/**
 * Tests that the "Replace your Linux apps and files" file context menu item is
 * hidden for a *.tini file if Crostini backup is disabled.
 */
testcase.checkImportCrostiniImageDisabled = async () => {
  const optionHidden = '#file-context-menu:not([hidden]) ' +
      '[command="#default-task"][hidden]';

  // Open FilesApp on Downloads with test.tini file.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.tiniFile], []);

  // Disable Crostini backup.
  await sendTestMessage(
      {name: 'setCrostiniExportImportAllowed', enabled: false});

  // Select and right click the tini file to show its context menu.
  await selectFile(appId, 'test.tini');
  await rightClickSelectedFile(appId);

  // Check: the context menu item should be hidden.
  await remoteCall.waitForElement(appId, optionHidden);
};

/**
 * Tests that the "Replace your Linux apps and files" file context menu item is
 * shown for a *.tini file if Crostini backup is enabled.
 */
testcase.checkImportCrostiniImageEnabled = async () => {
  const optionShown = '#file-context-menu:not([hidden]) ' +
      '[command="#default-task"]:not([hidden])';

  // Open FilesApp on Downloads with test.tini file.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.tiniFile], []);

  // Select and right click the tini file to show its context menu.
  await selectFile(appId, 'test.tini');
  await rightClickSelectedFile(appId);

  // Check: the context menu item should be shown.
  await remoteCall.waitForElement(appId, optionShown);
};

/**
 * Tests that text selection context menus are disabled in tablet mode.
 */
testcase.checkContextMenusForInputElements = async () => {
  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Click on the search button to display the search box.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Query all input elements.
  const elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId,
      ['input[type=text], input[type=search], textarea, cr-input']);

  // Focus the search box.
  chrome.test.assertEq(2, elements.length);
  for (const element of elements) {
    chrome.test.assertEq(
        '#text-context-menu', element.attributes['contextmenu']);
  }

  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.inputText(appId, '#search-box cr-input', 'test.pdf');

  // Notify the element of the input.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']));

  // Do the touch.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeTouchClick', appId, ['#search-box cr-input']));

  // Context menu must be hidden if touch induced.
  await remoteCall.waitForElement(appId, '#text-context-menu[hidden]');

  // Do the right click.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['#search-box cr-input']));

  // Context menu must be visible if mouse induced.
  await remoteCall.waitForElement(appId, '#text-context-menu:not([hidden])');
};

/**
 * Tests that opening context menu in the rename input won't commit the
 * renaming.
 */
testcase.checkContextMenuForRenameInput = async () => {
  const textInput = '#file-list .table-row[renaming] input.rename';
  const contextMenu = '#text-context-menu:not([hidden])';

  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');

  // Press Ctrl+Enter key to rename the file.
  const key = ['#file-list', 'Enter', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Check: the renaming text input should be shown in the file list.
  await remoteCall.waitForElement(appId, textInput);

  // Type new file name.
  await remoteCall.inputText(appId, textInput, 'NEW NAME');

  // Right click to show the context menu.
  await remoteCall.waitAndRightClick(appId, textInput);

  // Context menu must be visible.
  await remoteCall.waitForElement(appId, contextMenu);

  // Dismiss the context menu.
  const escKey = [contextMenu, 'Escape', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, escKey);

  // Check: The rename input should be still be visible and with the same
  // content.
  const inputElement = await remoteCall.waitForElement(appId, textInput);
  chrome.test.assertEq('NEW NAME', inputElement.value);

  // Check: The rename input should be the focused element.
  const focusedElement =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq(inputElement, focusedElement);
};

/**
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * context menu is opened from the file list inside the folder called
 * |folderName|. The folder is opened and the white area inside the folder is
 * selected. |folderName| must be inside the Google Drive root.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} folderName Path to the file to open the context menu for.
 * @param {boolean} expectedEnabledState True if the command should be enabled
 *     in the context menu, false if not.
 */
async function checkContextMenuInDriveFolder(
    commandId, folderName, expectedEnabledState) {
  // Open Files App on Drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], COMPLEX_DRIVE_ENTRY_SET);

  // Optionally copy hello.txt into the clipboard if needed.
  await maybeCopyToClipboard(appId, commandId);

  // Navigate to folder.
  await navigateWithDirectoryTree(appId, '/My Drive/' + folderName);

  // Right-click inside the file list.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['#file-list']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Wait for the command option to appear.
  let query = '#file-context-menu:not([hidden])';
  if (expectedEnabledState) {
    query += ` [command="#${commandId}"]:not([hidden]):not([disabled])`;
  } else {
    query += ` [command="#${commandId}"][disabled]:not([hidden])`;
  }
  await remoteCall.waitForElement(appId, query);
}

/**
 * Tests that the New Folder menu item is enabled inside a folder that has
 * read-write permissions.
 */
testcase.checkNewFolderEnabledInsideReadWriteFolder = () => {
  return checkContextMenuInDriveFolder('new-folder', 'photos', true);
};

/**
 * Tests that the New Folder menu item is enabled inside a folder that has
 * read-write permissions.
 */
testcase.checkNewFolderDisabledInsideReadOnlyFolder = () => {
  return checkContextMenuInDriveFolder('new-folder', 'Read-Only Folder', false);
};

/**
 * Tests that the Paste menu item is enabled inside a folder that has read-write
 * permissions.
 */
testcase.checkPasteEnabledInsideReadWriteFolder = () => {
  return checkContextMenuInDriveFolder('paste', 'photos', true);
};

/**
 * Tests that the Paste menu item is disabled inside a folder that has read-only
 * permissions.
 */
testcase.checkPasteDisabledInsideReadOnlyFolder = () => {
  return checkContextMenuInDriveFolder('paste', 'Read-Only Folder', false);
};

/**
 * Checks that mutating context menu items are not present for a root within
 * My files.
 * @param {string} itemName Name of item inside MyFiles that should be checked.
 * @param {!Object<string, boolean>} commandStates Commands that should be
 *     enabled for the checked item.
 */
async function checkMyFilesRootItemContextMenu(itemName, commandStates) {
  const validCmds = {
    'copy': true,
    'cut': true,
    'delete': true,
    'rename': true,
    'zip-selection': true,
  };

  const enabledCmds = [];
  const disabledCmds = [];
  for (const [cmd, enabled] of Object.entries(commandStates)) {
    chrome.test.assertTrue(cmd in validCmds, cmd + ' is not a valid command.');
    if (enabled) {
      enabledCmds.push(cmd);
    } else {
      disabledCmds.push(cmd);
    }
  }

  // Open FilesApp on Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Navigate to My files.
  await navigateWithDirectoryTree(appId, '/My files');

  // Wait for the navigation to complete.
  const expectedRows = [
    ['Downloads', '--', 'Folder'],
    ['Play files', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows,
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Select the item.
  await remoteCall.waitUntilSelected(appId, itemName);

  // Wait for the file to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Right-click the selected file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check the enabled commands.
  for (const commandId of enabledCmds) {
    const query = `#file-context-menu:not([hidden]) [command="#${
        commandId}"]:not([disabled])`;
    await remoteCall.waitForElement(appId, query);
  }

  // Check the disabled commands.
  for (const commandId of disabledCmds) {
    const query =
        `#file-context-menu:not([hidden]) [command="#${commandId}"][disabled]`;
    await remoteCall.waitForElement(appId, query);
  }

  // Check that the delete button isn't visible.
  const deleteButton = await remoteCall.waitForElement(appId, '#delete-button');
  chrome.test.assertTrue(deleteButton.hidden, 'delete button should be hidden');
}

/**
 * Check that mutating context menu items are not shown for Downloads within My
 * files.
 */
testcase.checkDownloadsContextMenu = () => {
  const commands = {
    copy: true,
    cut: false,
    delete: false,
    rename: false,
    'zip-selection': true,
  };
  return checkMyFilesRootItemContextMenu('Downloads', commands);
};

/**
 * Check that mutating context menu items are not shown for Play files within My
 * files.
 */
testcase.checkPlayFilesContextMenu = () => {
  const commands = {
    copy: false,
    cut: false,
    delete: false,
    rename: false,
    'zip-selection': false,
  };
  return checkMyFilesRootItemContextMenu('Play files', commands);
};

/**
 * Check that mutating context menu items are not shown for Linux files within
 * My files.
 */
testcase.checkLinuxFilesContextMenu = () => {
  const commands = {
    copy: false,
    cut: false,
    delete: false,
    rename: false,
    'zip-selection': false,
  };
  return checkMyFilesRootItemContextMenu('Linux files', commands);
};

/**
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * entry at |path| is selected.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} path Path to the file to open the context menu for.
 * @param {boolean} expectedEnabledState True if the command should be enabled
 *     in the context menu, false if not.
 */
async function checkDocumentsProviderContextMenu(
    commandId, path, expectedEnabledState) {
  const documentsProviderVolumeQuery =
      '[has-children="true"] [volume-type-icon="documents_provider"]';

  // Add files to the DocumentsProvider volume.
  await addEntries(
      ['documents_provider'], COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET);

  // Open Files app.
  const appId = await openNewWindow(RootPath.DOWNLOADS);

  // Wait for the DocumentsProvider volume to mount.
  await remoteCall.waitForElement(appId, documentsProviderVolumeQuery);

  // Click to open the DocumentsProvider volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [documentsProviderVolumeQuery]),
      'fakeMouseClick failed');

  // Check: the DocumentsProvider files should appear in the file list.
  const files =
      TestEntryInfo.getExpectedRows(COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Select the file |path|.
  await remoteCall.waitUntilSelected(appId, path);

  // Wait for the file to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Right-click the selected file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Wait for the command option to appear.
  let query = '#file-context-menu:not([hidden])';
  if (expectedEnabledState) {
    query += ` [command="#${commandId}"]:not([hidden]):not([disabled])`;
  } else {
    query += ` [command="#${commandId}"][disabled]:not([hidden])`;
  }
  await remoteCall.waitForElement(appId, query);
}

/**
 * Tests that the Delete menu item is disabled if the DocumentsProvider file is
 * not deletable.
 */
testcase.checkDeleteDisabledInDocProvider = () => {
  return checkDocumentsProviderContextMenu(
      'delete', 'Renamable File.txt', false);
};

/**
 * Tests that the Delete menu item is enabled if the DocumentsProvider file is
 * deletable.
 */
testcase.checkDeleteEnabledInDocProvider = () => {
  return checkDocumentsProviderContextMenu(
      'delete', 'Deletable File.txt', true);
};

/**
 * Tests that the Rename menu item is disabled if the DocumentsProvider file is
 * not renamable.
 */
testcase.checkRenameDisabledInDocProvider = () => {
  return checkDocumentsProviderContextMenu(
      'rename', 'Deletable File.txt', false);
};

/**
 * Tests that the Rename menu item is enabled if the DocumentsProvider file is
 * renamable.
 */
testcase.checkRenameEnabledInDocProvider = () => {
  return checkDocumentsProviderContextMenu(
      'rename', 'Renamable File.txt', true);
};

/**
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * entry at |path| is selected.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} fileName Name of the file to open the context menu for.
 * @param {boolean} expectedEnabledState True if the command should be enabled
 *     in the context menu, false if not.
 * @param {boolean=} opt_selectMultiple True if multiple file should be selected
 *     before the context menu is shown.
 */
async function checkRecentsContextMenu(
    commandId, fileName, expectedEnabledState, opt_selectMultiple) {
  // Populate both downloads and drive with disjoint sets of files.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.hello, ENTRIES.photos],
      [ENTRIES.desktop, ENTRIES.world, ENTRIES.testDocument]);

  // Navigate to Recents.
  await navigateWithDirectoryTree(appId, '/Recent');

  // Wait for the navigation to complete.
  const expectedRows = TestEntryInfo.getExpectedRows(RECENT_ENTRY_SET);
  await remoteCall.waitForFiles(appId, expectedRows);

  if (opt_selectMultiple) {
    // Select all the files.
    const ctrlA = ['#file-list', 'a', true, false, false];
    await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA);

    // Check: the file-list should be selected.
    await remoteCall.waitForElement(appId, '#file-list li[selected]');
  } else {
    // Select the item.
    await remoteCall.waitUntilSelected(appId, fileName);

    // Wait for the file to be selected.
    await remoteCall.waitForElement(appId, '.table-row[selected]');
  }

  // Right-click the selected file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Wait for the command option to appear.
  let query = '#file-context-menu:not([hidden])';
  if (expectedEnabledState) {
    query += ` [command="#${commandId}"]:not([hidden]):not([disabled])`;
  } else {
    query += ` [command="#${commandId}"][disabled]`;
  }
  await remoteCall.waitForElement(appId, query);
}

/**
 * Tests that the Delete menu item is disabled for files in Recents.
 */
testcase.checkDeleteEnabledInRecents = () => {
  return checkRecentsContextMenu('delete', 'My Desktop Background.png', true);
};

/**
 * Tests that the "Go to file location" menu item is enabled for files in
 * Recents.
 */
testcase.checkGoToFileLocationEnabledInRecents = () => {
  return checkRecentsContextMenu(
      'go-to-file-location', 'My Desktop Background.png', true);
};

/**
 * Tests that the "Go to file location" menu item is disabled when multiple
 * files are selected in Recents.
 */
testcase.checkGoToFileLocationDisabledInMultipleSelection = () => {
  return checkRecentsContextMenu(
      'go-to-file-location', 'My Desktop Background.png', false, true);
};

/**
 * Tests that context menu in file list gets the focus, so ChromeVox can
 * announce it.
 */
testcase.checkContextMenuFocus = async () => {
  // Open Files App on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Select the file |path|.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');

  // Wait for the file to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Right-click the selected file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Wait for the menu item to get focus.
  await remoteCall.waitForElement(
      appId, '#file-context-menu cr-menu-item:focus');

  // Check currently focused element.
  const focusedElement =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq('menuitem', focusedElement.attributes['role']);
};

testcase.checkDefaultTask = async () => {
  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos, ENTRIES.hello], []);

  // Force a task for the `hello` file.
  const fakeTask = new FakeTask(
      /* isDefault */ true,
      {appId: 'dummyId', taskType: 'app', actionId: 'open-with'}, 'DummyTask');
  await remoteCall.callRemoteTestUtil('overrideTasks', appId, [[fakeTask]]);

  // Display the context menu.
  await remoteCall.showContextMenuFor(appId, ENTRIES.hello.nameText);

  // Get the context menu.
  const menu = await remoteCall.getMenu(appId, 'context-menu');

  // Check the default task item is displayed for the DummyTask.
  const defaultTaskItem =
      menu['items'].find(el => el.attributes.id === 'default-task-menu-item');
  chrome.test.assertTrue(!!defaultTaskItem);
  chrome.test.assertFalse(defaultTaskItem.hidden);
  chrome.test.assertEq(defaultTaskItem.text, 'DummyTask');

  // Dismiss the context menu.
  await remoteCall.dismissMenu(appId);

  // Force empty tasks for the folder `photos`.
  await remoteCall.callRemoteTestUtil('overrideTasks', appId, [[]]);

  // Display the context menu.
  await remoteCall.showContextMenuFor(appId, ENTRIES.photos.nameText);

  // Get the context menu.
  const folderMenu = await remoteCall.getMenu(appId, 'context-menu');

  // Check the default task item is hidden.
  const folderDefaultTaskItem = folderMenu['items'].find(
      el => el.attributes.id === 'default-task-menu-item');
  chrome.test.assertTrue(!!folderDefaultTaskItem);
  chrome.test.assertTrue(folderDefaultTaskItem.hidden);
};

testcase.checkPolicyAssignedDefaultHasManagedIcon = async () => {
  // Open FilesApp on Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Force a task for the `hello` file.
  const fakeDefaultTask = new FakeTask(
      /* isDefault */ true,
      {appId: 'dummyId1', taskType: 'app', actionId: 'open-with'},
      'DummyDefaultTask');
  const fakeSecondaryTask = new FakeTask(
      /* isDefault */ false,
      {appId: 'dummyId2', taskType: 'app', actionId: 'open-with'},
      'DummySecondaryTask');

  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId,
      [[fakeDefaultTask, fakeSecondaryTask], /*isPolicyDefault=*/ true]);

  // Display the context menu.
  await remoteCall.showContextMenuFor(appId, ENTRIES.hello.nameText);

  // Get the context menu.
  const contextMenu = await remoteCall.getMenu(appId, 'context-menu');

  // Check the default task item is visible and has is-default/is-managed
  // properties set.
  const contextMenuDefaultTaskItem = contextMenu['items'].find(
      el => el.attributes.id === 'default-task-menu-item');
  chrome.test.assertTrue(!!contextMenuDefaultTaskItem);
  chrome.test.assertFalse(contextMenuDefaultTaskItem.hidden);
  chrome.test.assertEq(contextMenuDefaultTaskItem.text, 'DummyDefaultTask');
  chrome.test.assertTrue('is-default' in contextMenuDefaultTaskItem.attributes);
  chrome.test.assertTrue('is-managed' in contextMenuDefaultTaskItem.attributes);

  // Dismiss the context menu.
  await remoteCall.dismissMenu(appId);

  // Display the tasks menu.
  await remoteCall.expandOpenDropdown(appId);

  // Get the tasks menu.
  const tasksMenu = await remoteCall.getMenu(appId, 'tasks');

  // Check the default task item is visible and has is-default/is-managed
  // properties set.
  const tasksMenuDefaultTaskItem = tasksMenu['items'][0];
  chrome.test.assertTrue(!!tasksMenuDefaultTaskItem);
  chrome.test.assertFalse(tasksMenuDefaultTaskItem.hidden);
  chrome.test.assertTrue(
      tasksMenuDefaultTaskItem.text.includes('DummyDefaultTask'));
  chrome.test.assertTrue('is-default' in tasksMenuDefaultTaskItem.attributes);
  chrome.test.assertTrue('is-managed' in tasksMenuDefaultTaskItem.attributes);

  // Check that the remaining items do not have is-default/is-managed
  // properties, and that `Change Default` is not shown.
  const tasksMenuNonDefaultTaskItems = tasksMenu['items'].slice(1);
  for (const nonDefaultTaskItem of tasksMenuNonDefaultTaskItems) {
    chrome.test.assertFalse('is-default' in nonDefaultTaskItem.attributes);
    chrome.test.assertFalse('is-managed' in nonDefaultTaskItem.attributes);
    chrome.test.assertFalse(nonDefaultTaskItem.attributes['class'].includes('change-default'));
  }
};
