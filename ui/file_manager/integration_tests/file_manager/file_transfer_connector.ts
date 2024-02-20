// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, EntryType, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

interface TransferLocationOptions {
  volumeName: string;
  breadcrumbsPath: string;
  enterpriseConnectorsVolumeIdentifier: string;
}
/**
 * Info for the source or destination of a transfer.
 */
class TransferLocationInfo {
  /**
   * The volume type (e.g. downloads, drive, drive_recent,
   * drive_shared_with_me, drive_offline) or team drive name.
   */
  volumeName: string;
  breadcrumbsPath: string;
  /**
   * Identifies the volume and can be used for the
   * OnFileTransferEnterpriseConnector policy. This should match the allowed
   * policy values in policy_templates.json or
   * source_destination_matcher_ash.cc.
   */
  enterpriseConnectorsVolumeIdentifier: string;

  constructor(opts: TransferLocationOptions) {
    this.volumeName = opts.volumeName;
    this.breadcrumbsPath = opts.breadcrumbsPath;
    this.enterpriseConnectorsVolumeIdentifier =
        opts.enterpriseConnectorsVolumeIdentifier;
  }
}

interface TransferInfoOptions {
  source: TransferLocationInfo;
  destination: TransferLocationInfo;
  isMove?: boolean;
  proceedOnWarning?: boolean;
}

/**
 * Info for the transfer operation.
 */
class TransferInfo {
  source: TransferLocationInfo;
  destination: TransferLocationInfo;
  /**
   * True if this transfer is for a move operation, false for a copy
   * operation.
   */
  isMove: boolean;
  /**
   * Whether to proceed a potential warning or cancel the transfer.
   */
  proceedOnWarning: boolean;

  constructor(opts: TransferInfoOptions) {
    this.source = opts.source;
    this.destination = opts.destination;
    this.isMove = opts.isMove || false;
    this.proceedOnWarning = opts.proceedOnWarning || false;
  }
}

/**
 * Flat connector entry test set that does not include any directories.
 *
 * If a file should be blocked, name it "*blocked*".
 * If a file is allowed, name it "*allowed*".
 */
const CONNECTOR_ENTRIES_FLAT = [
  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'a_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'a_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'b_blocked.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'b_blocked.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'c_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'c_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),
];

/**
 * Flat connector entry test set that does not include any directories.
 *
 * If a file should be blocked, name it "*blocked*".
 * If a file should be warned, name it "*warned*".
 * If a file is allowed, name it "*allowed*".
 */
const CONNECTOR_ENTRIES_FLAT_WARNED = [
  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'a_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'a_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'b_blocked.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'b_blocked.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'c_warned.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'c_warned.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'd_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'd_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),
];

/**
 * Test set to test deep scanninng, contains nested directories.
 *
 * If a file should be blocked, name it "*blocked*".
 * If a file is allowed, name it "*allowed*".
 * If a directory only contains allowed files, name it "*allowed*".
 */
const CONNECTOR_ENTRIES_DEEP = [
  new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'A',
    sizeText: '--',
    typeText: 'Folder',
  }),

  new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/B_allowed',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'B_allowed',
    sizeText: '--',
    typeText: 'Folder',
  }),

  new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/C',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'C',
    sizeText: '--',
    typeText: 'Folder',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/B_allowed/g_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'g_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/C/i_blocked.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'i_blocked.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/C/j_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'j_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/C/k_blocked.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'k_blocked.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),
];

/**
 * Test set to test deep scanninng, contains nested directories.
 *
 * If a file should be blocked, name it "*blocked*".
 * If a file is allowed, name it "*allowed*".
 * If a directory only contains allowed files, name it "*allowed*".
 */
const CONNECTOR_ENTRIES_DEEP_WARNED = [
  new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'A',
    sizeText: '--',
    typeText: 'Folder',
  }),

  new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/B_allowed',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'B_allowed',
    sizeText: '--',
    typeText: 'Folder',
  }),

  new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/C',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'C',
    sizeText: '--',
    typeText: 'Folder',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/B_allowed/g_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'g_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/B_allowed/h_warned.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'h_warned.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/C/i_blocked.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'i_blocked.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/C/j_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'j_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/C/k_blocked.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'k_blocked.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/C/l_warned.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'l_warned.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),
];

/**
 * A list of transfer locations, for use with transferBetweenVolumes.
 * volumeName has to match an entry of
 * AddEntriesMessage::MapStringToTargetVolume().
 */
const TRANSFER_LOCATIONS = {
  drive: new TransferLocationInfo({
    breadcrumbsPath: '/My Drive',
    volumeName: 'drive',
    enterpriseConnectorsVolumeIdentifier: 'GOOGLE_DRIVE',
  }),

  downloads: new TransferLocationInfo({
    breadcrumbsPath: '/My files/Downloads',
    volumeName: 'local',
    enterpriseConnectorsVolumeIdentifier: 'MY_FILES',
  }),

  crostini: new TransferLocationInfo({
    breadcrumbsPath: '/My files/Linux files',
    volumeName: 'crostini',
    enterpriseConnectorsVolumeIdentifier: 'CROSTINI',
  }),

  usb: new TransferLocationInfo({
    breadcrumbsPath: '/fake-usb',
    volumeName: 'usb',
    enterpriseConnectorsVolumeIdentifier: 'REMOVABLE',
  }),

  smbfs: new TransferLocationInfo({
    breadcrumbsPath: '/SMB Share',
    volumeName: 'smbfs',
    enterpriseConnectorsVolumeIdentifier: 'SMB',
  }),

  mtp: new TransferLocationInfo({
    breadcrumbsPath: '/fake-mtp',
    volumeName: 'mtp',
    enterpriseConnectorsVolumeIdentifier: 'DEVICE_MEDIA_STORAGE',
  }),

  android_files: new TransferLocationInfo({
    breadcrumbsPath: '/My files/Play files',
    volumeName: 'android_files',
    enterpriseConnectorsVolumeIdentifier: 'ARC',
  }),
};
Object.freeze(TRANSFER_LOCATIONS);

// TODO(crbug.com/1361898): Remove these ones proper error details are
// displayed.
const OLD_COPY_FAIL_MESSAGE =
    'Copy operation failed. The file could not be accessed ' +
    'for security reasons.';
const OLD_MOVE_FAIL_DIRECTORY_MESSAGE =
    `Can't move file. The file could not be modified.`;
const OLD_MOVE_FAIL_FILE_MESSAGE =
    `Can't move file. The file could not be accessed ` +
    'for security reasons.';

const NEW_COPY_FAIL_MESSAGE = 'File blocked from copying';
const NEW_MOVE_FAIL_MESSAGE = `File blocked from moving`;
const TWO_FILES_COPY_FAIL_MESSAGE = '2 files blocked from copying';
const TWO_FILES_MOVE_FAIL_MESSAGE = '2 files blocked from moving';
const SINGLE_FILE_WARN_MESSAGE = 'c_warned.jpg may contain sensitive content';
const TWO_FILES_WARN_MESSAGE = '2 files may contain sensitive content';

const COPY_OUT_OF_SPACE_ERROR_MESSAGE =
    'Copy operation failed. There is not enough space.';

/**
 * Opens a Files app's main window and creates the source and destination
 * entries.
 * @param transferInfo Options for the transfer.
 * @return Promise to be fulfilled with the window ID.
 */
async function setupForFileTransferConnector(
    transferInfo: TransferInfo, srcContents: TestEntryInfo[],
    dstContents: TestEntryInfo[]): Promise<string> {
  const sourceEntriesPromise =
      addEntries([transferInfo.source.volumeName], srcContents);
  const destEntriesPromise =
      addEntries([transferInfo.destination.volumeName], dstContents);

  const appId = await remoteCall.openNewWindow(RootPath.DOWNLOADS, {});
  await remoteCall.waitForElement(appId, '#detail-table');

  // Wait until the elements are loaded in the table.
  await Promise.all([
    remoteCall.waitForFileListChange(appId, 0),
    sourceEntriesPromise,
    destEntriesPromise,
  ]);
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
  return appId;
}

/**
 * Returns all entries that are children of the passed directory.
 * @param entries The entries.
 * @param directory The directory path. Contains the path of the current
 *     directory, e.g., ["A", "B"] for A/B/.
 */
function getCurrentEntries(entries: TestEntryInfo[], directory: string[]) {
  return entries.filter(entry => {
    const parent = entry.targetPath.split('/').slice(0, -1);
    return parent.length === directory.length &&
        parent.every((value, index) => value === directory[index]);
  });
}

/**
 * Verifies the recursive contents of the current path by checking the file list
 * of the current path and its ancestors.
 * @param appId App window Id.
 * @param expectedEntries Expected contents of file list.
 * @param rootDirectory The path to the root directory for the check.
 * @param currentSubDirectory The current directory path split at '/', e.g.,
 *     ["A", "B"] for A/B/.
 */
async function verifyDirectoryRecursively(
    appId: string, expectedEntries: TestEntryInfo[], rootDirectory: string,
    currentSubDirectory: string[] = []) {
  // 1. Check current directory.
  const currentEntries =
      getCurrentEntries(expectedEntries, currentSubDirectory);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(currentEntries),
      {ignoreLastModifiedTime: true});

  // 2. For each subdirectory: enter subdirectory and call recursion.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  for (const entry of currentEntries.filter(
           entry => entry.type === EntryType.DIRECTORY)) {
    currentSubDirectory.push(entry.nameText);
    await directoryTree.navigateToPath(
        rootDirectory + '/' + currentSubDirectory.join('/'));
    await verifyDirectoryRecursively(
        appId, expectedEntries, rootDirectory, currentSubDirectory);
    currentSubDirectory.pop();
  }

  // 3. After the recursion ends, navigate back to the root directory.
  if (currentSubDirectory.length === 0) {
    // Go back to the root directory.
    await directoryTree.navigateToPath(rootDirectory);
  }
}

/**
 * Function to toggle display of all play files.
 * Before this function is called, the play file folder has to be opened.
 * @param appId App window Id.
 */
async function showAllPlayFiles(appId: string) {
  const toggleMenuItemSelector = '#gear-menu-toggle-hidden-android-folders';

  // Open the gear menu by clicking the gear button.
  await remoteCall.waitAndClickElement(appId, '#gear-button:not([hidden])');

  // Wait for the gear-menu to appear and click the menu item.
  await remoteCall.waitAndClickElement(
      appId,
      `#gear-menu:not([hidden]) ${
          toggleMenuItemSelector}:not([disabled]):not([checked])`);

  // Wait for item to be checked.
  await remoteCall.waitForElement(appId, toggleMenuItemSelector + '[checked]');
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
 * Test function to copy from the specified source to the specified destination.
 * @param transferInfo Options for the transfer.
 * @param entryTestSet The set of file and directory entries to be used for the
 *     test.
 * @param expectedFinalMsg The final message to expect at the progress center.
 * @param expectedWarnMsg The warning message to expect at the progress center.
 */
async function transferBetweenVolumes(
    transferInfo: TransferInfo, entryTestSet: TestEntryInfo[],
    expectedFinalMsg: string, expectedWarnMsg: string = '') {
  await setupVolumes(transferInfo);

  // Setup policy.
  await sendTestMessage({
    name: 'setupFileTransferPolicy',
    source: transferInfo.source.enterpriseConnectorsVolumeIdentifier,
    destination: transferInfo.destination.enterpriseConnectorsVolumeIdentifier,
  });

  // Setup reporting expectations.
  await sendTestMessage({
    name: 'expectFileTransferReports',
    source_volume: transferInfo.source.enterpriseConnectorsVolumeIdentifier,
    destination_volume:
        transferInfo.destination.enterpriseConnectorsVolumeIdentifier,
    entry_paths: entryTestSet.filter(entry => entry.type === EntryType.FILE)
                     .map(entry => entry.targetPath),
  });

  // Setup the scanning closure to be able to wait for the scanning to be
  // complete.
  await sendTestMessage({
    name: 'setupScanningRunLoop',
    number_of_expected_delegates:
        entryTestSet.filter(entry => !entry.targetPath.includes('/')).length,
  });

  const appId = await openFilesAppAndInitTransfer(transferInfo, entryTestSet);

  const reportOnly =
      await sendTestMessage({name: 'isReportOnlyFileTransferConnector'}) ===
      'true';
  if (reportOnly) {
    await verifyAfterPasteReportOnly(appId, transferInfo, entryTestSet);
  } else {
    await verifyAfterPasteBlocking(
        appId, transferInfo, entryTestSet, expectedFinalMsg, expectedWarnMsg);
  }
}

/**
 * Test function to copy from the specified source to the specified destination.
 * @param transferInfo Options for the transfer.
 * @param entryTestSet The set of file and directory entries to be used for the
 *     test.
 * @param expectedFinalMsg The final message to expect at the progress center.
 */
async function transferBetweenVolumesNoSpace(
    transferInfo: TransferInfo, entryTestSet: TestEntryInfo[],
    expectedFinalMsg: string) {
  // Ensure reportOnly is set, as the no-space behavior is special for
  // report-only mode.
  const reportOnly =
      await sendTestMessage({name: 'isReportOnlyFileTransferConnector'}) ===
      'true';
  chrome.test.assertTrue(reportOnly);


  await setupVolumes(transferInfo);

  // Setup policy.
  await sendTestMessage({
    name: 'setupFileTransferPolicy',
    source: transferInfo.source.enterpriseConnectorsVolumeIdentifier,
    destination: transferInfo.destination.enterpriseConnectorsVolumeIdentifier,
  });

  await sendTestMessage({
    name: 'mockIOTaskDestinationNoSpace',
  });

  // Setup the scanning closure to be able to wait for the scanning to be
  // complete. There should only be one delegate, as all other delegates aren't
  // initiated when an out of space error occurs.
  await sendTestMessage({
    name: 'setupScanningRunLoop',
    number_of_expected_delegates: 1,
  });

  const appId = await openFilesAppAndInitTransfer(transferInfo, entryTestSet);

  await verifyAfterPasteReportOnlyNoSpace(
      appId, transferInfo, expectedFinalMsg, entryTestSet);
}

/**
 * Mounts required volumes.
 * @param transferInfo Options for the transfer.
 */
async function setupVolumes(transferInfo: TransferInfo) {
  if (transferInfo.source.volumeName === 'usb' ||
      transferInfo.destination.volumeName === 'usb') {
    await sendTestMessage({name: 'mountFakeUsbEmpty'});
  }
  if (transferInfo.source.volumeName === 'smbfs' ||
      transferInfo.destination.volumeName === 'smbfs') {
    await sendTestMessage({name: 'mountSmbfs'});
  }
  if (transferInfo.source.volumeName === 'mtp' ||
      transferInfo.destination.volumeName === 'mtp') {
    await sendTestMessage({name: 'mountFakeMtpEmpty'});
  }
}

/**
 * Opens Files app and initiates source with entryTestSet and destination
 * with [ENTRIES.hello].
 * The destination is populated to prevent flakes (we can wait for the `hello`
 * file to appear).
 * @param transferInfo Options for the transfer.
 * @param entryTestSet The set of file and directory entries to be used for the
 *     test.
 */
async function openFilesAppAndInitTransfer(
    transferInfo: TransferInfo, entryTestSet: TestEntryInfo[]) {
  const dstContents = TestEntryInfo.getExpectedRows([ENTRIES.hello]);

  const appId = await setupForFileTransferConnector(
      transferInfo, entryTestSet, [ENTRIES.hello]);

  // Select the source folder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath(transferInfo.source.breadcrumbsPath);

  if (transferInfo.source.volumeName === 'android_files') {
    await showAllPlayFiles(appId);
  }

  // Wait for the expected files to appear in the file list.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(getCurrentEntries(entryTestSet, [])),
      {ignoreLastModifiedTime: true});

  // Focus the file list.
  await remoteCall.focus(appId, ['#file-list:not([hidden])']);

  // Select all files.
  const ctrlA = ['#file-list', 'a', true, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...ctrlA);
  // Check: the file-list should be selected.
  await remoteCall.waitForElement(appId, '#file-list li[selected]');

  // Copy the files. Similar to (ctrl + c) or (ctrl + x).
  // The actual copy only starts with the paste (ctrl + v).
  const transferCommand = transferInfo.isMove ? 'cut' : 'copy';
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'execCommand', appId, [transferCommand]));

  // Select the destination folder.
  await directoryTree.navigateToPath(transferInfo.destination.breadcrumbsPath);

  // Wait for the initially expected files to appear in the file list.
  // This is before the actual copy!
  await remoteCall.waitForFiles(
      appId, dstContents, {ignoreFileSize: true, ignoreLastModifiedTime: true});
  // Paste the file. Similar to (ctrl + v).
  // This will execute the actual paste.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));
  return appId;
}

/**
 * Verify what happens after a paste when scanning can block files.
 *
 * @param appId The app id of the files app window.
 * @param transferInfo Options for the transfer.
 * @param entryTestSet The set of file and directory entries to be used for the
 *     test.
 * @param expectedFinalMsg The final message to expect at the progress center.
 * @param expectedWarnMsg The warning message to expect at the progress center.
 */
async function verifyAfterPasteBlocking(
    appId: string, transferInfo: TransferInfo, entryTestSet: TestEntryInfo[],
    expectedFinalMsg: string, expectedWarnMsg: string) {
  // Check that a scanning label is shown.
  await remoteCall.waitForFeedbackPanelItem(
      appId,
      transferInfo.isMove ? new RegExp('^Moving.*$') :
                            new RegExp('^Copying.*$'),
      new RegExp('^Checking.*$'));

  // After the scanning label is shown, we resume the transfer.
  // Issue the responses, s.t., the transfer can continue.
  await sendTestMessage({name: 'issueFileTransferResponses'});

  const usesNewFileTransferConnectorUI =
      await sendTestMessage({name: 'usesNewFileTransferConnectorUI'}) ===
      'true';

  const expectedNumberOfWarnedFilesByConnectors = Number(await sendTestMessage(
      {name: 'getExpectedNumberOfWarnedFilesByConnectors'}));

  const bypassRequireJustification =
      await sendTestMessage({name: 'doesBypassRequireJustification'}) ===
      'true';

  const directoryTree = await DirectoryTreePageObject.create(appId);

  if (usesNewFileTransferConnectorUI &&
      expectedNumberOfWarnedFilesByConnectors > 0) {
    // Check that the warning appears in the feedback panel.
    await remoteCall.waitForFeedbackPanelItem(
        appId,
        transferInfo.isMove ? new RegExp('^Review is required before moving$') :
                              new RegExp('^Review is required before copying$'),
        new RegExp(`^${expectedWarnMsg}$`));

    if (transferInfo.proceedOnWarning) {
      // Expect warning proceeded messages.
      await sendTestMessage({
        name: 'expectFileTransferReports',
        source_volume: transferInfo.source.enterpriseConnectorsVolumeIdentifier,
        destination_volume:
            transferInfo.destination.enterpriseConnectorsVolumeIdentifier,
        entry_paths: entryTestSet.filter(entry => entry.type === EntryType.FILE)
                         .map(entry => entry.targetPath),
        expect_proceed_warning_reports: true,
      });

      // Proceed the warning (single file warning without user justification
      // required) / open the warning dialog (multiple file warning or user
      // justification required).
      await verifyPanelButtonsAndClick(appId, 'cancel', 'primary');

      if (expectedNumberOfWarnedFilesByConnectors > 1 ||
          bypassRequireJustification) {
        await sendTestMessage({
          name: 'verifyFileTransferWarningDialogAndProceed',
          app_id: appId,
        });
      }
    } else {
      // Cancel the warning by pressing on the secondary button.
      await verifyPanelButtonsAndClick(appId, 'cancel', 'secondary');

      // Wait 500ms to ensure files aren't moved.
      await new Promise(r => setTimeout(r, 500));

      // Ensure progress panel item is gone.
      await remoteCall.waitForElementLost(
          appId, ['#progress-panel', 'xf-panel-item']);

      // Wait for the expected files to appear in the file list.

      // No file should be transferred, so there should be no new file at the
      // destination.
      const expectedEntries = [ENTRIES.hello];
      await verifyDirectoryRecursively(
          appId, expectedEntries, transferInfo.destination.breadcrumbsPath);

      // All files should still exist at the destination.
      await directoryTree.navigateToPath(transferInfo.source.breadcrumbsPath);
      const expectedSourceEntries = entryTestSet;
      // Wait for the expected files to appear in the file list.
      await verifyDirectoryRecursively(
          appId, expectedSourceEntries, transferInfo.source.breadcrumbsPath);

      // If the warning is cancelled, the transfer is also cancelled, so do not
      // perform any further checks, as there will be no further notifications,
      // etc.
      return;
    }
  }

  // Wait for the expected files to appear in the file list.
  // Files marked as 'blocked' should not appear.
  const expectedEntries =
      entryTestSet.concat([ENTRIES.hello])
          .filter(entry => !entry.targetPath.includes('blocked'));
  await verifyDirectoryRecursively(
      appId, expectedEntries, transferInfo.destination.breadcrumbsPath);

  // Verify contents of the source directory.
  await directoryTree.navigateToPath(transferInfo.source.breadcrumbsPath);
  let expectedSourceEntries = entryTestSet;
  if (transferInfo.isMove) {
    // For a move, paths that include "allowed" should not be present at the
    // source.
    expectedSourceEntries = expectedSourceEntries.filter(
        entry => !entry.targetPath.includes('allowed'));
  }
  // Wait for the expected files to appear in the file list.
  await verifyDirectoryRecursively(
      appId, expectedSourceEntries, transferInfo.source.breadcrumbsPath);

  // Check that the error appears in the feedback panel.
  const expectedNumberOfBlockedFilesByConnectors = Number(await sendTestMessage(
      {name: 'getExpectedNumberOfBlockedFilesByConnectors'}));
  if (usesNewFileTransferConnectorUI &&
      expectedNumberOfBlockedFilesByConnectors > 1) {
    // There should be a review button if there are at least two errors.
    await remoteCall.waitForFeedbackPanelItem(
        appId, new RegExp(`^${expectedFinalMsg}$`),
        new RegExp('^Review for further details$'));

    await verifyPanelButtonsAndClick(appId, 'dismiss', 'primary');
    await sendTestMessage({
      name: 'verifyFileTransferErrorDialogAndDismiss',
      app_id: appId,
    });
  } else if (usesNewFileTransferConnectorUI) {
    // For a single file error, this should show an error reason as secondary
    // text.
    await remoteCall.waitForFeedbackPanelItem(
        appId, new RegExp(`^${expectedFinalMsg}$`),
        new RegExp('was blocked because of content$'));

  } else {
    // Check that only one line of text is shown.
    await remoteCall.waitForFeedbackPanelItem(
        appId, new RegExp(`^${expectedFinalMsg}$`), new RegExp(`^$`));
  }
}

/**
 * Verify what happens after a paste in the case of report-only scans.
 *
 * @param appId The app id of the files app window.
 * @param transferInfo Options for the transfer.
 * @param entryTestSet The set of file and directory entries to be used for the
 *     test.
 */
async function verifyAfterPasteReportOnly(
    appId: string, transferInfo: TransferInfo, entryTestSet: TestEntryInfo[]) {
  // No check for scanning label, as there shouldn't be one.

  // Wait for the expected files to appear in the file list.
  // All files should appear, even those marked as 'blocked'.
  const expectedEntries = entryTestSet.concat([ENTRIES.hello]);
  await verifyDirectoryRecursively(
      appId, expectedEntries, transferInfo.destination.breadcrumbsPath);

  // Verify contents of the source directory.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath(transferInfo.source.breadcrumbsPath);
  let expectedSourceEntries = entryTestSet;
  if (transferInfo.isMove) {
    // For a move, the source directory should be empty.
    expectedSourceEntries = [];
  }
  // Wait for the expected files to appear in the file list.
  await verifyDirectoryRecursively(
      appId, expectedSourceEntries, transferInfo.source.breadcrumbsPath);

  // Check that the status panel automatically vanishes.
  // This means that there was no error.
  await remoteCall.waitForElementLost(
      appId, ['#progress-panel', 'xf-panel-item']);

  // After the transfer completed, we issue scanning responses.
  // This ensures that scanning does not impact the transfer.
  await sendTestMessage({name: 'issueFileTransferResponses'});

  // We have to wait for the scanning to be completed to fulfill the report
  // expectations.
  await sendTestMessage({name: 'waitForFileTransferScanningToComplete'});
}

/**
 * Verify what happens after a paste in the case of report-only scans if
 * there's a no space error.
 *
 * @param appId The app id of the files app window.
 * @param transferInfo Options for the transfer.
 * @param expectedFinalMsg The final message to expect at the progress center.
 * @param entryTestSet The set of file and directory entries to be used for the
 *     test.
 */
async function verifyAfterPasteReportOnlyNoSpace(
    appId: string, transferInfo: TransferInfo, expectedFinalMsg: string,
    entryTestSet: TestEntryInfo[]) {
  // No check for scanning label, as there shouldn't be one.

  // Wait for the expected files to appear in the file list.
  // Only the hello file should appear, as the destination is assumed to be out
  // of space.
  const expectedEntries = [ENTRIES.hello];
  await verifyDirectoryRecursively(
      appId, expectedEntries, transferInfo.destination.breadcrumbsPath);

  // Verify contents of the source directory.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath(transferInfo.source.breadcrumbsPath);
  // The source entries should be unchanged.
  const expectedSourceEntries = entryTestSet;
  // Wait for the expected files to appear in the file list.
  await verifyDirectoryRecursively(
      appId, expectedSourceEntries, transferInfo.source.breadcrumbsPath);

  // Check that only one line of text is shown.
  await remoteCall.waitForFeedbackPanelItem(
      appId, new RegExp(`^${expectedFinalMsg}$`), new RegExp(`^$`));

  // After the transfer completed, we issue scanning responses.
  // This ensures that scanning does not impact the transfer.
  await sendTestMessage({name: 'issueFileTransferResponses'});

  // We have to wait for the scanning to be completed to fulfill the report
  // expectations.
  await sendTestMessage({name: 'waitForFileTransferScanningToComplete'});
}

/**
 * Tests copying from android_files to Downloads.
 */
export async function transferConnectorFromAndroidFilesToDownloadsDeep() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.android_files,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      OLD_COPY_FAIL_MESSAGE,
  );
}

export async function transferConnectorFromAndroidFilesToDownloadsFlat() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.android_files,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      OLD_COPY_FAIL_MESSAGE,
  );
}

/**
 * Tests copying from Crostini to Downloads.
 */
export async function transferConnectorFromCrostiniToDownloadsDeep() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.crostini,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      OLD_COPY_FAIL_MESSAGE,
  );
}

export async function transferConnectorFromCrostiniToDownloadsFlat() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.crostini,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      OLD_COPY_FAIL_MESSAGE,
  );
}

/**
 * Tests copying from Drive to Downloads.
 */
export async function transferConnectorFromDriveToDownloadsDeep() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.drive,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      OLD_COPY_FAIL_MESSAGE,
  );
}
export async function transferConnectorFromDriveToDownloadsFlat() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.drive,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      OLD_COPY_FAIL_MESSAGE,
  );
}

export async function
transferConnectorFromDriveToDownloadsFlatDestinationNoSpaceForReportOnly() {
  return transferBetweenVolumesNoSpace(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.drive,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      COPY_OUT_OF_SPACE_ERROR_MESSAGE,
  );
}

/**
 * Tests moving from Drive to Downloads.
 */
export async function transferConnectorFromDriveToDownloadsMoveDeep() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.drive,
        destination: TRANSFER_LOCATIONS.downloads,
        isMove: true,
      }),
      CONNECTOR_ENTRIES_DEEP,
      OLD_MOVE_FAIL_DIRECTORY_MESSAGE,
  );
}

export async function transferConnectorFromDriveToDownloadsMoveFlat() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.drive,
        destination: TRANSFER_LOCATIONS.downloads,
        isMove: true,
      }),
      CONNECTOR_ENTRIES_FLAT,
      OLD_MOVE_FAIL_FILE_MESSAGE,
  );
}

/**
 * Tests copying from mtp to Downloads.
 */
export async function transferConnectorFromMtpToDownloadsDeep() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.mtp,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      OLD_COPY_FAIL_MESSAGE,
  );
}
export async function transferConnectorFromMtpToDownloadsFlat() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.mtp,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      OLD_COPY_FAIL_MESSAGE,
  );
}

/**
 * Tests copying from smbfs to Downloads.
 */
export async function transferConnectorFromSmbfsToDownloadsDeep() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.smbfs,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      OLD_COPY_FAIL_MESSAGE,
  );
}

export async function transferConnectorFromSmbfsToDownloadsFlat() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.smbfs,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      OLD_COPY_FAIL_MESSAGE,
  );
}

/**
 * Tests copying from usb to Downloads.
 */
export async function transferConnectorFromUsbToDownloadsDeep() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      OLD_COPY_FAIL_MESSAGE,
  );
}

export async function transferConnectorFromUsbToDownloadsFlat() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      OLD_COPY_FAIL_MESSAGE,
  );
}

/**
 * Tests for new UX.
 */
export async function transferConnectorFromUsbToDownloadsDeepNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      TWO_FILES_COPY_FAIL_MESSAGE,
  );
}

export async function transferConnectorFromUsbToDownloadsFlatNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      NEW_COPY_FAIL_MESSAGE,
  );
}

export async function transferConnectorFromUsbToDownloadsDeepMoveNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
        isMove: true,
      }),
      CONNECTOR_ENTRIES_DEEP,
      TWO_FILES_MOVE_FAIL_MESSAGE,
  );
}

export async function transferConnectorFromUsbToDownloadsFlatMoveNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
        isMove: true,
      }),
      CONNECTOR_ENTRIES_FLAT,
      NEW_MOVE_FAIL_MESSAGE,
  );
}

export async function
transferConnectorFromUsbToDownloadsFlatWarnProceedNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
        proceedOnWarning: true,
      }),
      CONNECTOR_ENTRIES_FLAT_WARNED,
      NEW_COPY_FAIL_MESSAGE,
      SINGLE_FILE_WARN_MESSAGE,
  );
}

export async function
transferConnectorFromUsbToDownloadsFlatWarnProceedWithJustificationNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
        proceedOnWarning: true,
      }),
      CONNECTOR_ENTRIES_FLAT_WARNED,
      NEW_COPY_FAIL_MESSAGE,
      SINGLE_FILE_WARN_MESSAGE,
  );
}

export async function
transferConnectorFromUsbToDownloadsDeepWarnProceedNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
        proceedOnWarning: true,
      }),
      CONNECTOR_ENTRIES_DEEP_WARNED,
      TWO_FILES_COPY_FAIL_MESSAGE,
      TWO_FILES_WARN_MESSAGE,
  );
}

export async function
transferConnectorFromUsbToDownloadsDeepWarnProceedWithJustificationNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
        proceedOnWarning: true,
      }),
      CONNECTOR_ENTRIES_DEEP_WARNED,
      TWO_FILES_COPY_FAIL_MESSAGE,
      TWO_FILES_WARN_MESSAGE,
  );
}

export async function transferConnectorFromUsbToDownloadsFlatWarnCancelNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
        isMove: true,
      }),
      CONNECTOR_ENTRIES_FLAT_WARNED,
      '',
      SINGLE_FILE_WARN_MESSAGE,
  );
}

export async function transferConnectorFromUsbToDownloadsDeepWarnCancelNewUX() {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
        isMove: true,
      }),
      CONNECTOR_ENTRIES_DEEP_WARNED,
      '',
      TWO_FILES_WARN_MESSAGE,
  );
}
