// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../dialog_type.js';
import {addEntries, ENTRIES, EntryType, RootPath, sendBrowserTestCommand, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, openAndWaitForClosingDialog, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Tests that DLP block toast is shown when a restricted file is cut.
 */
testcase.transferShowDlpToast = async () => {
  const entry = ENTRIES.hello;

  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Setup the restrictions.
  await sendTestMessage({
    name: 'setBlockedFilesTransfer',
    fileNames: [entry.nameText],
  });

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const usbVolumeQuery = '#directory-tree [volume-type-icon="removable"]';
  await remoteCall.waitForElement(appId, usbVolumeQuery);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Cut the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['cut']));

  // Select USB volume.
  await navigateWithDirectoryTree(appId, '/fake-usb');

  // Paste the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check: a toast should be displayed because cut is disallowed.
  await remoteCall.waitForElement(appId, '#toast');

  // Navigate back to Downloads.
  await navigateWithDirectoryTree(appId, '/My files/Downloads');

  // The file should be there because the transfer was restricted.
  await remoteCall.waitUntilSelected(appId, entry.nameText);
};

/**
 * Tests that if the file is restricted by DLP, a managed icon is shown in the
 * detail list and a tooltip is displayed when hovering over that icon.
 */
testcase.dlpShowManagedIcon = async () => {
  // Add entries to Downloads and setup the fake source URLs.
  await addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
  await sendTestMessage({
    name: 'setGetFilesSourcesMock',
    fileNames: BASIC_LOCAL_ENTRY_SET.map(e => e.nameText),
    sourceUrls: [
      'https://blocked.com',
      'https://allowed.com',
      'https://blocked.com',
      'https://warned.com',
      'https://not-set.com',
    ],
  });

  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleRestrictions'});

  // Open Files app.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const dlpManagedIcon = '#file-list .dlp-managed-icon';

  // Check: only three of the five files should have the 'dlp-managed-icon'
  // class, which means that the icon is displayed.
  await remoteCall.waitForElementsCount(appId, [dlpManagedIcon], 3);

  // Hover over an icon: a tooltip should appear.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [dlpManagedIcon]));

  // Check: the DLP managed icon tooltip should be visible. The full text
  // contains a placeholder for the link so here we only check the first part.
  const labelTextPrefix = 'This file is confidential and subject ' +
      'to restrictions by administrator policy.';
  const label = await remoteCall.waitForElement(
      appId, ['files-tooltip[visible=true]', '#label']);
  chrome.test.assertTrue(label.text.startsWith(labelTextPrefix));
};

/**
 * Tests that if the file is restricted by DLP, the Restriction details context
 * menu item appears and is enabled.
 */
testcase.dlpContextMenuRestrictionDetails = async () => {
  // Add entries to Downloads and setup the fake source URLs.
  const entry = ENTRIES.hello;
  await addEntries(['local'], [entry]);
  await sendTestMessage({
    name: 'setGetFilesSourcesMock',
    fileNames: [entry.nameText],
    sourceUrls: ['https://blocked.com'],
  });

  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleBlocked'});

  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Wait for the DLP managed icon to be shown - this also means metadata has
  // been cached and can be used to show the context menu command.
  await remoteCall.waitForElementsCount(
      appId, ['#file-list .dlp-managed-icon'], 1);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click on the file.
  await remoteCall.waitAndRightClick(appId, ['.table-row[selected]']);

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Wait for the context menu command option to appear.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden])' +
          ' [command="#dlp-restriction-details"]' +
          ':not([hidden]):not([disabled])');
};

/**
 * Tests the save dialogs properly show DLP blocked volumes/directories. If a
 * volume is blocked by DLP, it should be marked as disabled both in the
 * navigation list and in the details list. If such a volume is selected, the
 * "Open" dialog button should be disabled, i.e. changing the directory is
 * prevented.
 */
testcase.saveAsDlpRestrictedDirectory = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedArc'});

  const okButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';

  // Add entries to Downloads.
  await addEntries(['local'], [ENTRIES.hello]);

  const closer = async (dialog) => {
    // Verify that the button is enabled when a file is selected.
    await remoteCall.waitUntilSelected(dialog, ENTRIES.hello.targetPath);
    await remoteCall.waitForElement(dialog, okButton);

    // Select My Files folder and wait for file list to display Downloads, Play
    // files, and Linux files.
    await navigateWithDirectoryTree(dialog, '/My files');
    const downloadsRow = ['Downloads', '--', 'Folder'];
    const playFilesRow = ['Play files', '--', 'Folder'];
    const linuxFilesRow = ['Linux files', '--', 'Folder'];
    await remoteCall.waitForFiles(
        dialog, [downloadsRow, playFilesRow, linuxFilesRow],
        {ignoreFileSize: true, ignoreLastModifiedTime: true});

    // Only one directory, Android files, should be disabled, both as the tree
    // item and the directory in the main list.
    const playFilesInFileList = '.directory[disabled][file-name="Play files"]';
    const playFilesInDirectoryTree = '#directory-tree .tree-item[disabled] ' +
        '.icon[volume-type-icon="android_files"]';
    await remoteCall.waitForElementsCount(dialog, [playFilesInFileList], 1);
    await remoteCall.waitForElementsCount(
        dialog, [playFilesInDirectoryTree], 1);

    // Verify that the button is enabled when a non-blocked volume is selected.
    await remoteCall.waitUntilSelected(dialog, 'Downloads');
    await remoteCall.waitForElement(dialog, okButton);

    // Verify that the button is disabled when a blocked volume is selected.
    await remoteCall.waitUntilSelected(dialog, 'Play files');
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [ENTRIES.hello], closer));
};

/**
 * Tests the save dialogs properly show DLP blocked guest OS volumes, before
 * and after being mounted: it should be marked as disabled in the navigation
 * list both before and after mounting, but in the file list it will only be
 * disabled after mounting.
 */
testcase.saveAsDlpRestrictedVm = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedPluginVM'});

  const okButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';

  const guestName = 'JennyAnyDots';
  const guestId = await sendTestMessage({
    name: 'registerMountableGuest',
    displayName: guestName,
    canMount: true,
    vmType: 'bruschetta',
  });

  const closer = async (dialog) => {
    // Select My Files folder and wait for file list.
    await navigateWithDirectoryTree(dialog, '/My files');
    const downloadsRow = ['Downloads', '--', 'Folder'];
    const playFilesRow = ['Play files', '--', 'Folder'];
    const linuxFilesRow = ['Linux files', '--', 'Folder'];
    const guestFilesRow = [guestName, '--', 'Folder'];
    await remoteCall.waitForFiles(
        dialog, [downloadsRow, playFilesRow, linuxFilesRow, guestFilesRow],
        {ignoreFileSize: true, ignoreLastModifiedTime: true});

    const directory = `.directory:not([disabled])[file-name="${guestName}"]`;
    const disabledDirectory = `.directory[disabled][file-name="${guestName}"]`;
    const disabledFakeTreeItem = '#directory-tree .tree-item[disabled] ' +
        '[root-type-icon=bruschetta]';
    const disabledRealTreeItem = `#directory-tree .tree-item[disabled] ` +
        `[volume-type-icon=bruschetta]`;

    // Before mounting, the guest should be disabled in the navigation list, but
    // not in the file list.
    await remoteCall.waitForElementsCount(dialog, [disabledFakeTreeItem], 1);
    await remoteCall.waitForElementsCount(dialog, [directory], 1);

    // Mount the guest by selecting it in the file list.
    await remoteCall.waitUntilSelected(dialog, guestName);
    await remoteCall.waitAndClickElement(dialog, [okButton]);

    // Verify that the guest is mounted and disabled, now both in the navigation
    // and the file list, as well as that the OK button is disabled while we're
    // still in the guest directory.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        dialog, `/My files/${guestName}`);
    await remoteCall.waitForElement(dialog, disabledOkButton);
    await navigateWithDirectoryTree(dialog, '/My files');
    await remoteCall.waitForElementsCount(dialog, [disabledRealTreeItem], 1);
    await remoteCall.waitForElementsCount(dialog, [disabledDirectory], 1);
    await remoteCall.waitUntilSelected(dialog, guestName);
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Unmount the volume.
    await sendTestMessage({
      name: 'unmountGuest',
      guestId: guestId,
    });

    // Verify that volume is replaced by the fake and is still disabled.
    await remoteCall.waitForElementsCount(dialog, [disabledFakeTreeItem], 1);
    await remoteCall.waitForElementsCount(
        dialog, [`#directory-tree [volume-type-icon=bruschetta]`], 0);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [], closer));
};

/**
 * Tests the save dialogs properly show DLP blocked Linux files, before
 * and after being mounted: it should be marked as disabled in the navigation
 * list both before and after mounting, but in the file list it will only be
 * disabled after mounting.
 */
testcase.saveAsDlpRestrictedCrostini = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedCrostini'});

  const okButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';

  // Add entries to Downloads.
  await addEntries(['local'], [ENTRIES.hello]);

  const closer = async (dialog) => {
    // Verify that the button is enabled when a file is selected.
    await remoteCall.waitUntilSelected(dialog, ENTRIES.hello.targetPath);
    await remoteCall.waitForElement(dialog, okButton);

    // Select My Files folder and wait for file list to display Downloads, Play
    // files, and Linux files.
    await navigateWithDirectoryTree(dialog, '/My files');
    const downloadsRow = ['Downloads', '--', 'Folder'];
    const playFilesRow = ['Play files', '--', 'Folder'];
    const linuxFilesRow = ['Linux files', '--', 'Folder'];
    await remoteCall.waitForFiles(
        dialog, [downloadsRow, playFilesRow, linuxFilesRow],
        {ignoreFileSize: true, ignoreLastModifiedTime: true});

    const directory = '.directory:not([disabled])[file-name="Linux files"]';
    const disabledDirectory = '.directory[disabled][file-name="Linux files"]';
    const disabledFakeTreeItem = '#directory-tree .tree-item[disabled] ' +
        '.icon[root-type-icon="crostini"]';
    const disabledLinuxTreeItem = '#directory-tree .tree-item[disabled] ' +
        '.icon[volume-type-icon="crostini"]';
    // Before mounting, Linux files should be disabled in the navigation list,
    // but not in the file list.
    await remoteCall.waitForElementsCount(dialog, [directory], 1);
    await remoteCall.waitForElementsCount(dialog, [disabledFakeTreeItem], 1);

    // Mount Crostini by selecting it in the file list. We cannot select/mount
    // it from the navigation list since it's already disabled there.
    await remoteCall.waitUntilSelected(dialog, 'Linux files');
    await remoteCall.waitAndClickElement(dialog, [okButton]);
    // Verify that Crostini is mounted and disabled, now both in the navigation
    // and the file list, as well as that the OK button is disabled while we're
    // still in the Linux files directory.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(dialog, '/Linux files');
    await remoteCall.waitForElement(dialog, disabledOkButton);
    await navigateWithDirectoryTree(dialog, '/My files');
    await remoteCall.waitForElementsCount(dialog, [disabledLinuxTreeItem], 1);
    await remoteCall.waitForElementsCount(dialog, [disabledDirectory], 1);
    await remoteCall.waitUntilSelected(dialog, 'Linux files');
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [ENTRIES.hello], closer));
};

/**
 * Tests that save dialogs are opened in a requested volume/directory,
 * when it's not blocked by DLP.
 * This test is an addition to the `saveAsDlpRestrictedRedirectsToMyFiles` test
 * case, which assert that if the directory is blocked, the dialog will not be
 * opened in the requested path.
 */
testcase.saveAsNonDlpRestricted = async () => {
  const cancelButton = '.button-panel button.cancel';

  // Add entries to Play files.
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET);

  const allowedCloser = async (dialog) => {
    // Double check: current directory should be Play files.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        dialog, '/My files/Play files');

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  // Open a save dialog in Play Files.
  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'android_files', BASIC_ANDROID_ENTRY_SET,
          allowedCloser));
};

/**
 * Tests that save dialogs are never opened in a DLP blocked volume/directory,
 * but rather in the default display root.
 */
testcase.saveAsDlpRestrictedRedirectsToMyFiles = async () => {
  const cancelButton = '.button-panel button.cancel';

  // Add entries to Downloads and Play files.
  await addEntries(['local'], [ENTRIES.hello]);
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET);

  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedArc'});

  const blockedCloser = async (dialog) => {
    // Double check: current directory should be the default root, not Play
    // files.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        dialog, '/My files/Downloads');

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  // Try to open a save dialog in Play Files. Since ARC is blocked by DLP, the
  // dialog should open in the default root instead.
  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'android_files', [ENTRIES.hello], blockedCloser));
};

/**
 * Tests the open file dialogs properly show DLP blocked files. If a file cannot
 * be opened by the caller of the dialog, it should be marked as disabled in the
 * details list. If such a file is selected, the "Open" dialog button should be
 * disabled.
 */
testcase.openDlpRestrictedFile = async () => {
  // Add entries to Downloads and setup the fake source URLs.
  await addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
  await sendTestMessage({
    name: 'setGetFilesSourcesMock',
    fileNames: BASIC_LOCAL_ENTRY_SET.map(e => e.nameText),
    sourceUrls: [
      'https://blocked.com',
      'https://allowed.com',
      'https://blocked.com',
      'https://warned.com',
      'https://not-set.com',
    ],
  });

  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleRestrictions'});
  await sendTestMessage({name: 'setIsRestrictedDestinationRestriction'});

  const okButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';

  const closer = async (dialog) => {
    // Wait for the file list to appear.
    await remoteCall.waitForElement(dialog, '#file-list');
    // Wait for the DLP managed icon to be shown - this means that metadata has
    // been fetched, including the disabled status. Three are managed, but only
    // two disabled.
    await remoteCall.waitForElementsCount(
        dialog, ['#file-list .dlp-managed-icon'], 3);

    await remoteCall.waitForElementsCount(
        dialog, ['#file-list .file[disabled]'], 2);

    // Verify that the button is enabled when a non-blocked (warning level) file
    // is selected.
    await remoteCall.waitUntilSelected(dialog, ENTRIES.beautiful.nameText);
    await remoteCall.waitForElement(dialog, okButton);

    // Verify that the button is disabled when a blocked file is selected.
    await remoteCall.waitUntilSelected(dialog, ENTRIES.hello.nameText);
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'openFile'}, 'downloads', BASIC_LOCAL_ENTRY_SET, closer));
};

/**
 * Tests that the file picker disables DLP blocked files and doesn't allow
 * opening them, while it allows selecting and opening folders.
 */
testcase.openFolderDlpRestricted = async () => {
  // Make sure the file picker will open to Downloads.
  sendBrowserTestCommand({name: 'setLastDownloadDir'}, () => {});

  const directoryAjpeg = new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: `${ENTRIES.directoryA.nameText}/deep.jpg`,
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'deep.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  });
  const entries = [ENTRIES.directoryA, directoryAjpeg];

  // Add entries to Downloads and setup the fake source URLs.
  await addEntries(['local'], entries);
  await sendTestMessage({
    name: 'setGetFilesSourcesMock',
    fileNames: [directoryAjpeg.targetPath],
    sourceUrls: [
      'https://blocked.com',
    ],
  });

  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleRestrictions'});
  await sendTestMessage({name: 'setIsRestrictedDestinationRestriction'});

  const enabledOkButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';

  const closer = async (dialog) => {
    // Wait for directoryA to appear.
    await remoteCall.waitForElement(
        dialog, `#file-list [file-name="${ENTRIES.directoryA.nameText}"]`);

    chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
        'fakeMouseDoubleClick', dialog,
        [`#file-list [file-name="${ENTRIES.directoryA.nameText}"]`]));

    // Wait for the image file to appear.
    await remoteCall.waitForElement(
        dialog, `#file-list [file-name="${directoryAjpeg.nameText}"]`);

    // Verify that the DLP managed icon for the image file is shown.
    await remoteCall.waitForElementsCount(
        dialog, ['#file-list .dlp-managed-icon'], 1);

    // Verify that the image file is disabled.
    await remoteCall.waitForElementsCount(
        dialog, ['#file-list .file[disabled]'], 1);

    // Verify that the button is disabled when the image file is selected.
    await remoteCall.waitUntilSelected(dialog, directoryAjpeg.nameText);
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'openFile'}, 'downloads', [ENTRIES.directoryA], closer));

  // Open Files app on Downloads as a folder picker.
  const dialog = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, entries, [], {type: DialogType.SELECT_UPLOAD_FOLDER});

  // Verify that directoryA is not disabled.
  await remoteCall.waitForElementsCount(
      dialog, ['#file-list .file[disabled]'], 0);

  // Select directoryA with the dialog.
  await remoteCall.waitAndClickElement(
      dialog, `#file-list [file-name="${ENTRIES.directoryA.nameText}"]`);

  // Verify that directoryA selection is enabled while it contains a blocked
  // file.
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.directoryA.targetPath],
    openType: 'open',
  });
  await remoteCall.waitAndClickElement(dialog, enabledOkButton);
};