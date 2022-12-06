// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, openAndWaitForClosingDialog, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';


/**
 * Tests DLP block toast is shown when a restricted file is copied.
 */
testcase.transferShowDlpToast = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedDestinationRestriction'});

  const entry = ENTRIES.hello;

  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const usbVolumeQuery = '#directory-tree [volume-type-icon="removable"]';
  await remoteCall.waitForElement(appId, usbVolumeQuery);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Select USB volume.
  await navigateWithDirectoryTree(appId, '/fake-usb');

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check that a toast is displayed because copy is disallowed.
  await remoteCall.waitForElement(appId, '#toast');
};

/**
 * Tests that if the file is restricted by DLP, a managed icon is shown in the
 * detail list and a tooltip is displayed when hovering over that icon.
 */
testcase.dlpShowManagedIcon = async () => {
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
  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleBlocked'});

  const entry = ENTRIES.hello;

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
    await remoteCall.waitForElement(dialog, cancelButton);
    const event = [cancelButton, 'click'];
    await remoteCall.callRemoteTestUtil('fakeEvent', dialog, event);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [ENTRIES.hello], closer));
};

/**
 * Tests the save dialogs properly show DLP blocked volumes/directories, before
 * and after they are mounted. If a volume is blocked by DLP, it should be
 * marked as disabled in the navigation list both before and after mounting, but
 * in the file list it will only be disabled after mounting.
 */
testcase.saveAsDlpRestrictedMountableDirectory = async () => {
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

    const linuxFilesInFileList =
        '.directory:not([disabled])[file-name="Linux files"]';
    const disabledLinuxFilesInFileList =
        '.directory[disabled][file-name="Linux files"]';
    const disabledFakeLinuxTreeItem = '#directory-tree .tree-item[disabled] ' +
        '.icon[root-type-icon="crostini"]';
    const disabledRealLinuxTreeItem = '#directory-tree .tree-item[disabled] ' +
        '.icon[volume-type-icon="crostini"]';
    // Before Crostini is mounted, Linux files should be disabled in the
    // navigation list, but not in the file list.
    await remoteCall.waitForElementsCount(dialog, [linuxFilesInFileList], 1);
    await remoteCall.waitForElementsCount(
        dialog, [disabledFakeLinuxTreeItem], 1);

    // Select Linux files from the file list and mount Crostini. We cannot
    // select/mount it from the navigation list since it's already disabled
    // there.
    await remoteCall.waitUntilSelected(dialog, 'Linux files');
    await remoteCall.waitForElement(dialog, okButton);
    await remoteCall.callRemoteTestUtil(
        'fakeEvent', dialog, [okButton, 'click']);
    // Verify that Crostini is mounted and disabled, now both in the navigation
    // and the file list, as well as that the OK button is disabled while we're
    // still in the Linux files directory.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(dialog, '/Linux files');
    await remoteCall.waitForElement(dialog, disabledOkButton);
    await navigateWithDirectoryTree(dialog, '/My files');
    await remoteCall.waitForElementsCount(
        dialog, [disabledRealLinuxTreeItem], 1);
    await remoteCall.waitForElementsCount(
        dialog, [disabledLinuxFilesInFileList], 1);
    await remoteCall.waitUntilSelected(dialog, 'Linux files');
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitForElement(dialog, cancelButton);
    const event = [cancelButton, 'click'];
    await remoteCall.callRemoteTestUtil('fakeEvent', dialog, event);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [ENTRIES.hello], closer));
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

  const allowedCloser = async (dialog) => {
    // Double check: current directory should be Play files.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        dialog, '/My files/Play files');

    // Click the close button to dismiss the dialog.
    const event = [cancelButton, 'click'];
    await remoteCall.callRemoteTestUtil('fakeEvent', dialog, event);
  };

  // Open a save dialog in Play Files.
  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'android_files', BASIC_ANDROID_ENTRY_SET,
          allowedCloser));

  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedArc'});

  const blockedCloser = async (dialog) => {
    // Double check: current directory should be the default root, not Play
    // files.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        dialog, '/My files/Downloads');

    // Click the close button to dismiss the dialog.
    const event = [cancelButton, 'click'];
    await remoteCall.callRemoteTestUtil('fakeEvent', dialog, event);
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
 * details list.
 */
testcase.openDlpRestrictedFile = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleBlocked'});
  await sendTestMessage({name: 'setIsRestrictedDestinationRestriction'});

  // Add entries to Downloads.
  await addEntries(['local'], [ENTRIES.hello]);

  const cancelButton = '.button-panel button.cancel';

  const closer = async (dialog) => {
    // Wait for the file list to appear.
    await remoteCall.waitForElement(dialog, '#file-list');
    await remoteCall.waitForFiles(
        dialog, TestEntryInfo.getExpectedRows([ENTRIES.hello]));
    // Wait for the DLP managed icon to be shown - this means that metadata has
    // been fetched and we can check the disabled status as well.
    await remoteCall.waitForElementsCount(
        dialog, ['#file-list .dlp-managed-icon'], 1);
    await remoteCall.waitForElementsCount(
        dialog, ['#file-list .file[disabled]'], 1);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitForElement(dialog, cancelButton);
    const event = [cancelButton, 'click'];
    await remoteCall.callRemoteTestUtil('fakeEvent', dialog, event);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'openFile'}, 'downloads', [ENTRIES.hello], closer));
};
