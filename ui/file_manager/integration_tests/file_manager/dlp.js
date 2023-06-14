// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../dialog_type.js';
import {addEntries, ENTRIES, EntryType, RootPath, sendBrowserTestCommand, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, openAndWaitForClosingDialog, remoteCall, setupAndWaitUntilReady} from './background.js';
import {FakeTask} from './tasks.js';
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

// Filters used for the following save-as and file-open tests.
// Rows in `My files`
const downloadsRow = ['Downloads', '--', 'Folder'];
const playFilesRow = ['Play files', '--', 'Folder'];
const linuxFilesRow = ['Linux files', '--', 'Folder'];
// Dialog buttons
const okButton = '.button-panel button.ok:enabled';
const disabledOkButton = '.button-panel button.ok:disabled';
const cancelButton = '.button-panel button.cancel';

/**
 * Tests the save dialogs properly show DLP blocked Play files, before
 * and after being mounted, both in the navigation list and in
 * the details list.
 */
testcase.saveAsDlpRestrictedAndroid = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'arc'});

  const closer = async (dialog) => {
    // Select My Files folder and wait for file list to display Downloads, Play
    // files, and Linux files.
    await navigateWithDirectoryTree(dialog, '/My files');

    await remoteCall.waitForFiles(
        dialog, [downloadsRow, playFilesRow, linuxFilesRow],
        {ignoreFileSize: true, ignoreLastModifiedTime: true});

    // Only one directory, Android files, should be disabled, both as the tree
    // item and the directory in the main list.
    const guestName = 'Play files';
    const disabledDirectory = `.directory[disabled][file-name="${guestName}"]`;
    const disabledRealTreeItem = '#directory-tree .tree-item[disabled] ' +
        '.icon[volume-type-icon="android_files"]';
    const disabledFakeTreeItem = '#directory-tree .tree-item[disabled] ' +
        '[root-type-icon=android_files]';
    await remoteCall.waitForElement(dialog, disabledDirectory);
    await remoteCall.waitForElement(dialog, disabledRealTreeItem);

    // Verify that the button is enabled when a non-blocked volume is selected.
    await remoteCall.waitUntilSelected(dialog, 'Downloads');
    await remoteCall.waitForElement(dialog, okButton);

    // Verify that the button is disabled when a blocked volume is selected.
    await remoteCall.waitUntilSelected(dialog, guestName);
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Unmount Play files and mount ARCVM.
    await sendTestMessage({name: 'unmountPlayFiles'});
    const guestId = await sendTestMessage({
      name: 'registerMountableGuest',
      displayName: guestName,
      canMount: true,
      vmType: 'arcvm',
    });

    // Wait for the placeholder "Play files" to appear, the directory tree item
    // should be disabled, but the file row shouldn't be disabled.
    await remoteCall.waitAndClickElement(dialog, disabledFakeTreeItem);
    await remoteCall.waitForFiles(
        dialog, [downloadsRow, playFilesRow, linuxFilesRow],
        {ignoreFileSize: true, ignoreLastModifiedTime: true});

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [], closer));
};

/**
 * Tests the save dialogs properly show DLP blocked guest OS volumes, before
 * and after being mounted: it should be marked as disabled in the navigation
 * list both before and after mounting, but in the file list it will only be
 * disabled after mounting.
 */
testcase.saveAsDlpRestrictedVm = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'pluginVm'});

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
  await sendTestMessage({name: 'setBlockedComponent', component: 'crostini'});

  // Add entries to Downloads.
  await addEntries(['local'], [ENTRIES.hello]);

  const closer = async (dialog) => {
    // Verify that the button is enabled when a file is selected.
    await remoteCall.waitUntilSelected(dialog, ENTRIES.hello.targetPath);
    await remoteCall.waitForElement(dialog, okButton);

    // Select My Files folder and wait for file list to display Downloads, Play
    // files, and Linux files.
    await navigateWithDirectoryTree(dialog, '/My files');
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
 * Tests the save dialogs properly show blocked USB volumes.
 */
testcase.saveAsDlpRestrictedUsb = async () => {
  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'usb'});

  const closer = async (dialog) => {
    const disabledRealTreeItem = '#directory-tree .tree-item[disabled] ' +
        '[volume-type-icon="removable"]';
    // It should be disabled in the navigation list, but the eject button should
    // be enabled.
    await remoteCall.waitForElementsCount(dialog, [disabledRealTreeItem], 1);
    await remoteCall.waitForElementsCount(
        dialog, ['.root-eject:not([disabled])'], 1);

    // Unmount.
    await sendTestMessage({name: 'unmountUsb'});
    await remoteCall.waitForElementsCount(dialog, [disabledRealTreeItem], 0);

    // Mount again - should still be disabled.
    await sendTestMessage({name: 'mountFakeUsbEmpty'});
    await remoteCall.waitForElementsCount(dialog, [disabledRealTreeItem], 1);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [], closer));
};

/**
 * Tests the save dialogs properly show blocked Google drive volume.
 */
testcase.saveAsDlpRestrictedDrive = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'drive'});

  const closer = async (dialog) => {
    const disabledRealTreeItem = '#directory-tree ' +
        '.tree-item.drive-volume[disabled][has-children=false]';
    const expandIcon = disabledRealTreeItem + ' > .tree-row .expand-icon';
    // It should be disabled in the navigation list, and the expand icon
    // shouldn't be visible.
    await remoteCall.waitForElementsCount(dialog, [disabledRealTreeItem], 1);
    const element = await remoteCall.waitForElementStyles(
        dialog, expandIcon, ['visibility']);
    chrome.test.assertEq('hidden', element.styles['visibility']);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [], closer));
};

/**
 * Tests that save dialogs are opened in a requested volume/directory,
 * when it's not blocked by DLP.
 * This test is an addition to the `saveAsDlpRestrictedRedirectsToMyFiles` test
 * case, which assert that if the directory is blocked, the dialog will not be
 * opened in the requested path.
 */
testcase.saveAsNonDlpRestricted = async () => {
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
  // Add entries to Downloads and Play files.
  await addEntries(['local'], [ENTRIES.hello]);
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET);

  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'arc'});

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
  await remoteCall.waitAndClickElement(dialog, okButton);
};

/**
 * Tests that DLP disabled file tasks are shown as disabled in the menu.
 */
testcase.fileTasksDlpRestricted = async () => {
  const entry = ENTRIES.hello;
  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);
  // Override file tasks so that some are DLP disabled.
  const fakeTasks = [
    new FakeTask(
        true, {appId: 'dummyId1', taskType: 'file', actionId: 'open-with'},
        'DummyTask1', false, true),
    new FakeTask(
        false, {appId: 'dummyId2', taskType: 'file', actionId: 'open-with'},
        'DummyTask2', false, false),
    new FakeTask(
        false, {appId: 'dummyId3', taskType: 'file', actionId: 'open-with'},
        'DummyTask3', false, true),
  ];
  await remoteCall.callRemoteTestUtil('overrideTasks', appId, [fakeTasks]);

  // Open the context menu.
  await remoteCall.showContextMenuFor(appId, entry.nameText);

  // Verify that the default task item is visible but disabled.
  await remoteCall.waitForElement(
      appId,
      ['#file-context-menu:not([hidden]) ' +
       '[command="#default-task"][disabled]:not([hidden])']);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Display the tasks menu.
  await remoteCall.expandOpenDropdown(appId);

  // Wait for the dropdown menu to show.
  await remoteCall.waitForElement(
      appId, '#tasks-menu:not([hidden]) cr-menu-item');

  // Verify that the first and third tasks are disabled, and the second one is
  // not.
  await remoteCall.waitForElement(
      appId, ['#tasks-menu:not([hidden]) cr-menu-item[disabled]:nth-child(1)']);
  await remoteCall.waitForElement(
      appId,
      ['#tasks-menu:not([hidden]) cr-menu-item:not([disabled]):nth-child(2)']);
  await remoteCall.waitForElement(
      appId, ['#tasks-menu:not([hidden]) cr-menu-item[disabled]:nth-child(3)']);
};


/**
 * Tests that extraction works when the scoped file access delegate exists and
 * correct output files are generated.
 */
testcase.zipExtractRestrictedArchiveCheckContent = async () => {
  const entry = ENTRIES.zipArchive;

  // Add entries to Downloads and setup the fake source URLs.
  await addEntries(['local'], [entry]);
  await sendTestMessage({
    name: 'setGetFilesSourcesMock',
    fileNames: [entry.nameText],
    sourceUrls: ['https://blocked.com'],
  });

  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleBlocked'});

  // Setup the scoped file access delegate.
  await sendTestMessage({name: 'setupScopedFileAccessDelegateAllowed'});

  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Wait for the DLP managed icon to be shown.
  await remoteCall.waitForElementsCount(
      appId, ['#file-list .dlp-managed-icon'], 1);

  const targetDirectoryName = entry.nameText.split('.')[0];

  // Make sure the test extension handles the new window creation properly.
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [targetDirectoryName],
    openType: 'launch',
  });

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click the selected file.
  await remoteCall.waitAndRightClick(appId, '.table-row[selected]');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Click the 'Extract all' menu command.
  await remoteCall.waitAndClickElement(
      appId, '[command="#extract-all"]:not([hidden])');

  const directoryQuery = '#file-list [file-name="' + targetDirectoryName + '"]';
  // Check: the extract directory should appear.
  await remoteCall.waitForElement(appId, directoryQuery);

  // Double click the created directory to open it.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseDoubleClick', appId, [directoryQuery]),
      'fakeMouseDoubleClick failed');

  // Check: File content in the ZIP should appear.
  await remoteCall.waitForFiles(
      appId,
      [
        ['folder', '--', 'Folder'],
        ['text.txt', '--', 'Plain text'],
        ['image.png', '--', 'PNG image'],
      ],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});
};
