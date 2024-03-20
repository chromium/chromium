// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../prod/file_manager/shared_types.js';
import {addEntries, ENTRIES, EntryType, RootPath, sendBrowserTestCommand, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, FakeTask} from './test_data.js';

/**
 * Copies or moves a file from Downloads to the provided location.
 * @param appId ID of the Files app window.
 * @param file Test entry info to be copied/cut.
 * @param destination Name of the destination folder.
 * @param isCopy Whether it should copy or move the file.
 * @return Promise fulfilled on success.
 */
async function copyOrMove(
    appId: string, file: TestEntryInfo, destination: string,
    isCopy: boolean): Promise<void> {
  if (!file || !file.nameText || !destination) {
    chrome.test.assertTrue(false, 'copyOrMove invalid parameters');
  }

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads');
  await remoteCall.waitForFiles(appId, [file.getExpectedRow()]);
  await remoteCall.waitUntilSelected(appId, file.nameText);

  const command = isCopy ? 'copy' : 'cut';
  await remoteCall.callRemoteTestUtil('execCommand', appId, [command]);

  await directoryTree.navigateToPath(destination);

  await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']);
}

/**
 * List of panel types.
 * Keep this in sync with PanelItem panel types.
 */
enum PanelType {
  DEFAULT = -1,
  PROGRESS = 0,
  SUMMARY = 1,
  DONE = 2,
  ERROR = 3,
  INFO = 4,
  FORMAT_PROGRESS = 5,
  SYNC_PROGRESS = 6,
}

/**
 * List of checked panel status indicator types.
 */
enum StatusIndicator {
  WARNING = 'warning',
  FAILURE = 'failure',
}

/**
 * Returns the first panel item with the provided panel type.
 * @param appId ID of the Files app window.
 */
async function getPanelItem(appId: string, panelType: PanelType) {
  const panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', `xf-panel-item[panel-type="${panelType}"]`]);
  return panel;
}

/**
 * Checks that the panel item with provided parameters exists.
 * @param appId ID of the Files app window.
 * @param panelType Expected panel type.
 * @param primaryText Expected primary text.
 * @param secondaryText Expected secondary text. Can be null.
 * @param status Expected status indicator (failure or warning).
 * @return Promise fulfilled on success.
 */
async function verifyPanelItem(
    appId: string, panelType: PanelType, primaryText: string,
    secondaryText: null|string, status: StatusIndicator): Promise<void> {
  const panel = await getPanelItem(appId, panelType);

  chrome.test.assertEq(primaryText, panel.attributes['primary-text']);
  chrome.test.assertEq(secondaryText, panel.attributes['secondary-text']);

  chrome.test.assertEq('status', panel.attributes['indicator']);
  chrome.test.assertEq(status, panel.attributes['status']);
}

/**
 * Checks that the panel item's primary and secondary buttons have expected type
 * and text, and then clicks the button defined by selectedButton.
 * @param appId ID of the Files app window.
 * @param secondaryButtonCategory Expected secondary button category (dismiss or
 *     cancel).
 * @param selectedButton The button to click (primary or secondary).
 */
async function verifyPanelButtonsAndClick(
    appId: string, secondaryButtonCategory: string, selectedButton: string) {
  const primaryButton = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item', 'xf-button#primary-action']);
  chrome.test.assertEq(
      'extra-button', primaryButton.attributes['data-category']);

  const secondaryButton = await remoteCall.waitForElement(
      appId,
      ['#progress-panel', 'xf-panel-item', 'xf-button#secondary-action']);
  chrome.test.assertEq(
      secondaryButtonCategory, secondaryButton.attributes['data-category']);

  await remoteCall.waitAndClickElement(appId, [
    '#progress-panel',
    'xf-panel-item',
    `xf-button#${selectedButton}-action`,
  ]);
}

/**
 * Expands the summary panel if it's collapsed, no-op if already expanded.
 * @param appId ID of the Files app window.
 * */
async function maybeExpandSummary(appId: string) {
  const summaryPanel = await getPanelItem(appId, PanelType.SUMMARY);
  if (summaryPanel.attributes['data-category'] === 'expanded') {
    return;
  }

  await remoteCall.waitAndClickElement(appId, [
    '#progress-panel',
    `xf-panel-item[panel-type="${PanelType.SUMMARY}"]`,
    'xf-button#primary-action',
  ]);

  await remoteCall.waitForElement(appId, [
    '#progress-panel',
    `xf-panel-item[panel-type="${
        PanelType.SUMMARY}"][data-category="expanded"]`,
  ]);
}

/**
 * Tests that DLP block toast is shown when a restricted file is cut.
 */
export async function transferShowDlpToast() {
  const entry = ENTRIES.hello;

  // Open Files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Setup the restrictions.
  await sendTestMessage({
    name: 'setBlockedFilesTransfer',
    fileNames: [entry.nameText],
  });

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('removable');

  // Cut and paste the file.
  await copyOrMove(appId, entry, '/fake-usb', /*isCopy=*/ false);

  // Check: a toast should be displayed because cut is disallowed.
  await remoteCall.waitForElement(appId, '#toast');

  // Navigate back to Downloads.
  await directoryTree.navigateToPath('/My files/Downloads');

  // The file should be there because the transfer was restricted.
  await remoteCall.waitUntilSelected(appId, entry.nameText);
}

/**
 * Tests that if the file is restricted by DLP, a managed icon is shown in the
 * detail list and a tooltip is displayed when hovering over that icon.
 */
export async function dlpShowManagedIcon() {
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
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const dlpManagedIcon = '#file-list .dlp-managed-icon.is-dlp-restricted';

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
  chrome.test.assertTrue((label.text ?? '').startsWith(labelTextPrefix));
}

/**
 * Tests that if the file is restricted by DLP, the Restriction details context
 * menu item appears and is enabled.
 */
export async function dlpContextMenuRestrictionDetails() {
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
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Wait for the DLP managed icon to be shown - this also means metadata has
  // been cached and can be used to show the context menu command.
  await remoteCall.waitForElementsCount(
      appId, ['#file-list .dlp-managed-icon.is-dlp-restricted'], 1);

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
}

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
 * Tests the save dialogs properly show DLP blocked Play files, before and after
 * being mounted, both in the navigation list and in the details list.
 */
export async function saveAsDlpRestrictedAndroid() {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'arc'});

  const closer = async (dialog: string) => {
    // Select My Files folder and wait for file list to display Downloads, Play
    // files, and Linux files.
    const directoryTree = await DirectoryTreePageObject.create(dialog);
    await directoryTree.navigateToPath('/My files');

    await remoteCall.waitForFiles(
        dialog, [downloadsRow, playFilesRow, linuxFilesRow],
        {ignoreFileSize: true, ignoreLastModifiedTime: true});

    // Only one directory, Android files, should be disabled, both as the tree
    // item and the directory in the main list.
    const guestName = 'Play files';
    const disabledDirectory = `.directory[disabled][file-name="${guestName}"]`;
    await remoteCall.waitForElement(dialog, disabledDirectory);
    const realTreeItem = await directoryTree.waitForItemByType('android_files');
    directoryTree.assertItemDisabled(realTreeItem);

    // Verify that the button is enabled when a non-blocked volume is selected.
    await remoteCall.waitUntilSelected(dialog, 'Downloads');
    await remoteCall.waitForElement(dialog, okButton);

    // Verify that the button is disabled when a blocked volume is selected.
    await remoteCall.waitUntilSelected(dialog, guestName);
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Unmount Play files and mount ARCVM.
    await sendTestMessage({name: 'unmountPlayFiles'});
    await sendTestMessage({
      name: 'registerMountableGuest',
      displayName: guestName,
      canMount: true,
      vmType: 'arcvm',
    });

    // Wait for the placeholder "Play files" to appear, the directory tree item
    // should be disabled, but the file row shouldn't be disabled.
    const fakeTreeItem =
        await directoryTree.waitForPlaceholderItemByType('android_files');
    directoryTree.assertItemDisabled(fakeTreeItem);
    await directoryTree.selectPlaceholderItemByType('android_files');
    await remoteCall.waitForFiles(
        dialog, [downloadsRow, playFilesRow, linuxFilesRow],
        {ignoreFileSize: true, ignoreLastModifiedTime: true});

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [], closer));
}

/**
 * Tests the save dialogs properly show DLP blocked guest OS volumes, before and
 * after being mounted: it should be marked as disabled in the navigation list
 * both before and after mounting, but in the file list it will only be disabled
 * after mounting.
 */
export async function saveAsDlpRestrictedVm() {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'pluginVm'});

  const guestName = 'JennyAnyDots';
  const guestId = await sendTestMessage({
    name: 'registerMountableGuest',
    displayName: guestName,
    canMount: true,
    vmType: 'bruschetta',
  });

  const closer = async (dialog: string) => {
    // Select My Files folder and wait for file list.
    const directoryTree = await DirectoryTreePageObject.create(dialog);
    await directoryTree.navigateToPath('/My files');
    const guestFilesRow = [guestName, '--', 'Folder'];
    await remoteCall.waitForFiles(
        dialog, [downloadsRow, playFilesRow, linuxFilesRow, guestFilesRow],
        {ignoreFileSize: true, ignoreLastModifiedTime: true});

    const directory = `.directory:not([disabled])[file-name="${guestName}"]`;
    const disabledDirectory = `.directory[disabled][file-name="${guestName}"]`;

    // Before mounting, the guest should be disabled in the navigation list, but
    // not in the file list.
    let fakeTreeItem =
        await directoryTree.waitForPlaceholderItemByType('bruschetta');
    directoryTree.assertItemDisabled(fakeTreeItem);
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
    await directoryTree.navigateToPath('/My files');
    const realTreeItem = await directoryTree.waitForItemByType('bruschetta');
    directoryTree.assertItemDisabled(realTreeItem);
    await remoteCall.waitForElementsCount(dialog, [disabledDirectory], 1);
    await remoteCall.waitUntilSelected(dialog, guestName);
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Unmount the volume.
    await sendTestMessage({
      name: 'unmountGuest',
      guestId: guestId,
    });

    // Verify that volume is replaced by the fake and is still disabled.
    fakeTreeItem =
        await directoryTree.waitForPlaceholderItemByType('bruschetta');
    directoryTree.assertItemDisabled(fakeTreeItem);
    await directoryTree.waitForItemLostByType('bruschetta');

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [], closer));
}

/**
 * Tests the save dialogs properly show DLP blocked Linux files, before and
 * after being mounted: it should be marked as disabled in the navigation list
 * both before and after mounting, but in the file list it will only be disabled
 * after mounting.
 */
export async function saveAsDlpRestrictedCrostini() {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'crostini'});

  // Add entries to Downloads.
  await addEntries(['local'], [ENTRIES.hello]);

  const closer = async (dialog: string) => {
    // Verify that the button is enabled when a file is selected.
    await remoteCall.waitUntilSelected(dialog, ENTRIES.hello.targetPath);
    await remoteCall.waitForElement(dialog, okButton);

    // Select My Files folder and wait for file list to display Downloads, Play
    // files, and Linux files.
    const directoryTree = await DirectoryTreePageObject.create(dialog);
    await directoryTree.navigateToPath('/My files');
    await remoteCall.waitForFiles(
        dialog, [downloadsRow, playFilesRow, linuxFilesRow],
        {ignoreFileSize: true, ignoreLastModifiedTime: true});

    const directory = '.directory:not([disabled])[file-name="Linux files"]';
    const disabledDirectory = '.directory[disabled][file-name="Linux files"]';
    // Before mounting, Linux files should be disabled in the navigation list,
    // but not in the file list.
    await remoteCall.waitForElementsCount(dialog, [directory], 1);
    const fakeTreeItem =
        await directoryTree.waitForPlaceholderItemByType('crostini');
    directoryTree.assertItemDisabled(fakeTreeItem);

    // Mount Crostini by selecting it in the file list. We cannot select/mount
    // it from the navigation list since it's already disabled there.
    await remoteCall.waitUntilSelected(dialog, 'Linux files');
    await remoteCall.waitAndClickElement(dialog, [okButton]);
    // Verify that Crostini is mounted and disabled, now both in the navigation
    // and the file list, as well as that the OK button is disabled while we're
    // still in the Linux files directory.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(dialog, '/Linux files');
    await remoteCall.waitForElement(dialog, disabledOkButton);
    await directoryTree.navigateToPath('/My files');
    const realTreeItem = await directoryTree.waitForItemByType('crostini');
    directoryTree.assertItemDisabled(realTreeItem);
    await remoteCall.waitForElementsCount(dialog, [disabledDirectory], 1);
    await remoteCall.waitUntilSelected(dialog, 'Linux files');
    await remoteCall.waitForElement(dialog, disabledOkButton);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [ENTRIES.hello], closer));
}

/**
 * Tests the save dialogs properly show blocked USB volumes.
 */
export async function saveAsDlpRestrictedUsb() {
  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'usb'});

  const closer = async (dialog: string) => {
    const directoryTree = await DirectoryTreePageObject.create(dialog);
    // It should be disabled in the navigation list, but the eject button should
    // be enabled.
    let realTreeItem = await directoryTree.waitForItemByType('removable');
    directoryTree.assertItemDisabled(realTreeItem);
    const ejectButton =
        await directoryTree.waitForItemEjectButtonByType('removable');
    chrome.test.assertEq(undefined, ejectButton.attributes['disabled']);

    // Unmount.
    await sendTestMessage({name: 'unmountUsb'});
    await directoryTree.waitForItemLostByType('removable');

    // Mount again - should still be disabled.
    await sendTestMessage({name: 'mountFakeUsbEmpty'});
    realTreeItem = await directoryTree.waitForItemByType('removable');
    directoryTree.assertItemDisabled(realTreeItem);

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [], closer));
}

/**
 * Tests the save dialogs properly show blocked Google drive volume.
 */
export async function saveAsDlpRestrictedDrive() {
  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'drive'});

  const closer = async (dialog: string) => {
    const directoryTree = await DirectoryTreePageObject.create(dialog);
    // It should be disabled in the navigation list, and the expand icon
    // shouldn't be visible.
    const treeItem = await directoryTree.waitForItemToHaveChildrenByLabel(
        'Google Drive', /* hasChildren= */ false);
    directoryTree.assertItemDisabled(treeItem);
    await directoryTree.waitForItemExpandIconToHideByLabel('Google Drive');

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  chrome.test.assertEq(
      undefined,
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'downloads', [], closer));
}

/**
 * Tests that save dialogs are opened in a requested volume/directory, when it's
 * not blocked by DLP. This test is an addition to the
 * `saveAsDlpRestrictedRedirectsToMyFiles` test case, which assert that if the
 * directory is blocked, the dialog will not be opened in the requested path.
 */
export async function saveAsNonDlpRestricted() {
  // Add entries to Play files.
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET);

  const allowedCloser = async (dialog: string) => {
    // Double check: current directory should be Play files.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        dialog, '/My files/Play files');

    // Click the close button to dismiss the dialog.
    await remoteCall.waitAndClickElement(dialog, [cancelButton]);
  };

  // Open a save dialog in Play Files.
  chrome.test.assertEq(
      undefined,
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'android_files', BASIC_ANDROID_ENTRY_SET,
          allowedCloser));
}

/**
 * Tests that save dialogs are never opened in a DLP blocked volume/directory,
 * but rather in the default display root.
 */
export async function saveAsDlpRestrictedRedirectsToMyFiles() {
  // Add entries to Downloads and Play files.
  await addEntries(['local'], [ENTRIES.hello]);
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET);

  // Setup the restrictions.
  await sendTestMessage({name: 'setBlockedComponent', component: 'arc'});

  const blockedCloser = async (dialog: string) => {
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
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'saveFile'}, 'android_files', [ENTRIES.hello], blockedCloser));
}

/**
 * Tests the open file dialogs properly show DLP blocked files. If a file cannot
 * be opened by the caller of the dialog, it should be marked as disabled in the
 * details list. If such a file is selected, the "Open" dialog button should be
 * disabled.
 */
export async function openDlpRestrictedFile() {
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

  const closer = async (dialog: string) => {
    // Wait for the file list to appear.
    await remoteCall.waitForElement(dialog, '#file-list');
    // Wait for the DLP managed icon to be shown - this means that metadata has
    // been fetched, including the disabled status. Three are managed, but only
    // two disabled.
    await remoteCall.waitForElementsCount(
        dialog, ['#file-list .dlp-managed-icon.is-dlp-restricted'], 3);

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
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'openFile'}, 'downloads', BASIC_LOCAL_ENTRY_SET, closer));
}

/**
 * Tests that the file picker disables DLP blocked files and doesn't allow
 * opening them, while it allows selecting and opening folders.
 */
export async function openFolderDlpRestricted() {
  // Make sure the file picker will open to Downloads.
  sendBrowserTestCommand({name: 'setLastDownloadDir'});

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

  const closer = async (dialog: string) => {
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
        dialog, ['#file-list .dlp-managed-icon.is-dlp-restricted'], 1);

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
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'openFile'}, 'downloads', [ENTRIES.directoryA], closer));

  // Open Files app on Downloads as a folder picker.
  const dialog = await remoteCall.setupAndWaitUntilReady(
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
}

/**
 * Tests that DLP disabled file tasks are shown as disabled in the menu.
 */
export async function fileTasksDlpRestricted() {
  const entry = ENTRIES.hello;
  // Open Files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);
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
}


/**
 * Tests that extraction works when the scoped file access delegate exists and
 * correct output files are generated.
 */
export async function zipExtractRestrictedArchiveCheckContent() {
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
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Wait for the DLP managed icon to be shown.
  await remoteCall.waitForElementsCount(
      appId, ['#file-list .dlp-managed-icon.is-dlp-restricted'], 1);

  const targetDirectoryName = entry.nameText.split('.')[0];

  // Expect newly extracted files to be added to the DLP daemon.
  await sendTestMessage({
    name: 'expectFilesAdditionToDaemon',
    fileNames:
        [targetDirectoryName + '/image.png', targetDirectoryName + '/text.txt'],
    sourceUrls: ['https://blocked.com', 'https://blocked.com'],
  });

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click the selected file.
  await remoteCall.waitAndRightClick(appId, '.table-row[selected]');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Resolves when the new app window opens.
  const waitForWindowPromise = remoteCall.waitForWindow();

  // Click the 'Extract all' menu command.
  await remoteCall.waitAndClickElement(
      appId, '[command="#extract-all"]:not([hidden])');

  const directoryQuery = '#file-list [file-name="' + targetDirectoryName + '"]';
  // Check: the extract directory should appear.
  await remoteCall.waitForElement(appId, directoryQuery);

  // Check: The new window has navigated to the unzipped folder.
  const newAppId = await waitForWindowPromise;
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      newAppId, '/My files/Downloads/' + targetDirectoryName);

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
}

/**
 * Tests that a copy or move IO task that completed with error due to block
 * restriction properly updates the task state and shows a correct panel item.
 */
export async function blockShowsPanelItem() {
  // Add entry to Downloads.
  const entry = ENTRIES.hello;
  await addEntries(['local'], [entry]);

  // Open Files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Setup the restrictions.
  await sendTestMessage({
    name: 'setBlockedFilesTransfer',
    fileNames: [entry.nameText],
  });

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('removable');

  // Copy and paste the file to USB.
  await copyOrMove(appId, entry, '/fake-usb', /*isCopy=*/ true);

  // Check that the error panel is open with correct primary and secondary text,
  // and has the expected button types.
  await verifyPanelItem(
      appId, PanelType.ERROR, 'File blocked from copying',
      `${entry.nameText} was blocked because of policy`,
      StatusIndicator.FAILURE);
  await verifyPanelButtonsAndClick(appId, 'dismiss', 'secondary');

  // Cut and paste the file to USB.
  await copyOrMove(appId, entry, '/fake-usb', /*isCopy=*/ false);

  // Check that the error panel is open with correct primary and secondary text,
  // and has the expected button types.
  await verifyPanelItem(
      appId, PanelType.ERROR, 'File blocked from moving',
      `${entry.nameText} was blocked because of policy`,
      StatusIndicator.FAILURE);
  await verifyPanelButtonsAndClick(appId, 'dismiss', 'primary');
}

/**
 * Tests that a copy or move IO task that is paused due to warn restriction
 * properly updates the task state and shows a correct panel item.
 */
export async function warnShowsPanelItem() {
  // Add entry to Downloads.
  const entry = ENTRIES.hello;
  await addEntries(['local'], [entry]);

  // Open Files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Set the mock to pause the first task.
  await sendTestMessage({
    name: 'setCheckFilesTransferMockToPause',
    taskId: 1,
    fileNames: [entry.nameText],
    action: 'copy',
  });

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('removable');

  // Copy and paste the file to USB.
  await copyOrMove(appId, entry, '/fake-usb', /*isCopy=*/ true);

  // Check that the warning panel is open with correct primary and secondary
  // text, and has the expected button types.
  await verifyPanelItem(
      appId, PanelType.INFO, 'Review is required before copying',
      `${entry.nameText} may contain sensitive content`,
      StatusIndicator.WARNING);
  await verifyPanelButtonsAndClick(appId, 'cancel', 'secondary');

  // Set the first mock to pause the task.
  await sendTestMessage({
    name: 'setCheckFilesTransferMockToPause',
    taskId: 2,
    fileNames: [entry.nameText],
    action: 'move',
  });

  // Cut and paste the file to USB.
  await copyOrMove(appId, entry, '/fake-usb', /*isCopy=*/ false);

  // Check that the warning panel is open with correct primary and secondary
  // text, and has the expected button types.
  await verifyPanelItem(
      appId, PanelType.INFO, 'Review is required before moving',
      `${entry.nameText} may contain sensitive content`,
      StatusIndicator.WARNING);
  await verifyPanelButtonsAndClick(appId, 'cancel', 'primary');
}

/**
 * Test for http://b/299583281.
 * Tests that after DLP warning times out, the copy or move IO task
 * properly updates the task state and shows a correct panel item.
 */
export async function warnTimeoutShowsPanelItem() {
  // Add entry to Downloads.
  const entry = ENTRIES.hello;
  await addEntries(['local'], [entry]);

  // Open Files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Set the mock to pause the first task.
  await sendTestMessage({
    name: 'setCheckFilesTransferMockToPause',
    taskId: 1,
    fileNames: [entry.nameText],
    action: 'copy',
  });

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('removable');

  // Copy and paste the file to USB.
  await copyOrMove(appId, entry, '/fake-usb', /*isCopy=*/ true);

  // Check that the warning panel is open with correct primary and secondary
  // text, and has the expected button types.
  await verifyPanelItem(
      appId, PanelType.INFO, 'Review is required before copying',
      `${entry.nameText} may contain sensitive content`,
      StatusIndicator.WARNING);

  // Fast forward to time out the warning.
  await sendTestMessage({name: 'timeoutWarning'});

  // Check that the warning panel is open with correct primary and secondary
  // text, and has the expected button types.
  await verifyPanelItem(
      appId, PanelType.ERROR, 'Copying timed out',
      'Try copying your files again', StatusIndicator.FAILURE);
  await verifyPanelButtonsAndClick(appId, 'dismiss', 'secondary');
}

/**
 * Tests that the summary panel shows the correct title when it contains a mix
 * of warning (paused copy or move IO task) and error (blocked copy or move IO
 * task) panels, or multiple warnings, but is not shown if only one panel is
 * visible.
 */
export async function mixedSummaryDisplayPanel() {
  // Add entry to Downloads.
  const entry = ENTRIES.hello;
  await addEntries(['local'], [entry]);

  // Open Files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Block the second task.
  await sendTestMessage({
    name: 'setBlockedFilesTransfer',
    fileNames: [entry.nameText],
  });

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('removable');

  // Copy and paste the file to USB.
  await copyOrMove(appId, entry, '/fake-usb', /*isCopy=*/ true);

  // Check that only 1 error panel is opened.
  await remoteCall.waitForElementsCount(
      appId, ['#progress-panel', `xf-panel-item`], 1);
  await remoteCall.waitForElementsCount(
      appId,
      ['#progress-panel', `xf-panel-item[panel-type="${PanelType.ERROR}"]`], 1);

  // Set the mock to pause the second task.
  await sendTestMessage({
    name: 'setCheckFilesTransferMockToPause',
    taskId: 2,
    fileNames: [entry.nameText],
    action: 'copy',
  });

  // Copy the file to USB.
  await copyOrMove(appId, entry, '/fake-usb', /*isCopy=*/ true);

  // Check that the summary panel is open with correct title and the two sub
  // panels (3 in total).
  await remoteCall.waitForElementsCount(
      appId, ['#progress-panel', 'xf-panel-item'], 3);
  await verifyPanelItem(
      appId, PanelType.SUMMARY, '1 error. 1 warning.', null,
      StatusIndicator.FAILURE);
  // Expand the summary panel if needed, in order to click on the individual
  // ones.
  await maybeExpandSummary(appId);

  // Dismiss the error panel.
  await remoteCall.waitAndClickElement(appId, [
    '#progress-panel',
    `xf-panel-item[panel-type="${PanelType.ERROR}"]`,
    'xf-button#secondary-action',
  ]);

  // Check that only 1 warning panel remains.
  await remoteCall.waitForElementsCount(
      appId, ['#progress-panel', `xf-panel-item`], 1);
  await remoteCall.waitForElementsCount(
      appId,
      ['#progress-panel', `xf-panel-item[panel-type="${PanelType.INFO}"]`], 1);

  // Set the mock to pause the third task.
  await sendTestMessage({
    name: 'setCheckFilesTransferMockToPause',
    taskId: 3,
    fileNames: [entry.nameText],
    action: 'copy',
  });

  // Copy the file to USB.
  await copyOrMove(appId, entry, '/fake-usb', /*isCopy=*/ true);

  // Check that the summary panel is open with correct title and the two sub
  // panels (3 in total).
  await remoteCall.waitForElementsCount(
      appId, ['#progress-panel', 'xf-panel-item'], 3);
  await remoteCall.waitForElementsCount(
      appId,
      ['#progress-panel', `xf-panel-item[panel-type="${PanelType.INFO}"]`], 2);
  await verifyPanelItem(
      appId, PanelType.SUMMARY, '2 warnings.', null, StatusIndicator.WARNING);
}
