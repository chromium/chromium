// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [file]),
      'selectFile failed');
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
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [path]));

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
 *     in the context menu, false if not.
 */
async function checkContextMenu(commandId, path, expectedEnabledState) {
  // Open Files App on Drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], COMPLEX_DRIVE_ENTRY_SET);

  // Optionally copy hello.txt into the clipboard if needed.
  await maybeCopyToClipboard(appId, commandId);

  // Select the file |path|.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [path]));

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
  for (let element of elements) {
    chrome.test.assertEq('#text-context-menu', element.attributes.contextmenu);
  }

  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['#search-box cr-input', 'test.pdf']);

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
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * context menu is opened from the file list inside the folder called
 * |folderName|. The folder is opened and the white area inside the folder is
 * selected. |folderName| must be inside the Google Drive root.
 * TODO(sashab): Allow specifying a generic path to any folder in the tree.
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

  // Focus the file list.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'focus', appId, ['#file-list:not([hidden])']));

  // Select 'My Drive'.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'selectFolderInTree', appId, ['My Drive']));

  // Wait for My Drive to load.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My Drive');

  // Expand 'My Drive'.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'expandSelectedFolderInTree', appId, []));

  // Select the folder.
  await remoteCall.callRemoteTestUtil(
      'selectFolderInTree', appId, [folderName]);

  // Wait the folder to load.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My Drive/' + folderName);

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
  for (let [cmd, enabled] of Object.entries(commandStates)) {
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
  await remoteCall.waitAndClickElement(appId, '#breadcrumb-path-0');

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
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [itemName]));

  // Wait for the file to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Right-click the selected file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check the enabled commands.
  for (const commandId of enabledCmds) {
    let query = `#file-context-menu:not([hidden]) [command="#${
        commandId}"]:not([disabled])`;
    await remoteCall.waitForElement(appId, query);
  }

  // Check the disabled commands.
  for (const commandId of disabledCmds) {
    let query =
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

  // Open Files app.
  const appId = await openNewWindow(RootPath.DOWNLOADS);

  // Add files to the DocumentsProvider volume.
  await addEntries(
      ['documents_provider'], COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET);

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
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [path]));

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
 * Tests that context menu in file list gets the focus, so ChromeVox can
 * announce it.
 */
testcase.checkContextMenuFocus = async () => {
  // Open Files App on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Select the file |path|.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'selectFile', appId, ['hello.txt']));

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
  chrome.test.assertEq('menuitem', focusedElement.attributes.role);
};
