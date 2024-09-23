// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, EntryType, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

const allowedFileEntry = new TestEntryInfo({
  type: EntryType.FILE,
  targetPath: 'a_allowed.jpg',
  sourceFileName: 'small.jpg',
  mimeType: 'image/jpeg',
  lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
  nameText: 'a_allowed.jpg',
  sizeText: '886 bytes',
  typeText: 'JPEG image',
});

const warnedFileEntry = new TestEntryInfo({
  type: EntryType.FILE,
  targetPath: 'c_warned.jpg',
  sourceFileName: 'small.jpg',
  mimeType: 'image/jpeg',
  lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
  nameText: 'c_warned.jpg',
  sizeText: '886 bytes',
  typeText: 'JPEG image',
});

const blockedFileEntry1 = new TestEntryInfo({
  type: EntryType.FILE,
  targetPath: 'b_blocked.jpg',
  sourceFileName: 'small.jpg',
  mimeType: 'image/jpeg',
  lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
  nameText: 'b_blocked.jpg',
  sizeText: '886 bytes',
  typeText: 'JPEG image',
});

const blockedFileEntry2 = new TestEntryInfo({
  type: EntryType.FILE,
  targetPath: 'd_blocked.jpg',
  sourceFileName: 'small.jpg',
  mimeType: 'image/jpeg',
  lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
  nameText: 'd_blocked.jpg',
  sizeText: '886 bytes',
  typeText: 'JPEG image',
});

// Tests that proceeding two warnings, the first is triggered by DLP and the
// second is triggered by Enterprise Connectors, will move all the copied files.
export async function twoWarningsProceeded() {
  // Add entry to Downloads.
  await addEntries(['local'], [allowedFileEntry, warnedFileEntry]);

  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [allowedFileEntry, warnedFileEntry], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('removable');

  // Set the mock to pause the first task.
  await sendTestMessage({
    name: 'setCheckFilesTransferMockToPause',
    taskId: 1,
    fileNames: [warnedFileEntry.targetPath],
    action: 'copy',
  });

  // Setup policy.
  await sendTestMessage({
    name: 'setupFileTransferPolicy',
    source: 'MY_FILES',
    destination: 'REMOVABLE',
  });

  // Setup reporting expectations.
  await sendTestMessage({
    name: 'expectFileTransferReports',
    source_volume: 'MY_FILES',
    destination_volume: 'REMOVABLE',
    entry_paths: [allowedFileEntry.targetPath, warnedFileEntry.targetPath],
  });

  // Setup the scanning closure to be able to wait for the scanning to be
  // complete.
  await sendTestMessage({
    name: 'setupScanningRunLoop',
    number_of_expected_delegates: 2,
  });

  // Copy and paste the two files to USB.
  await directoryTree.navigateToPath('/My files/Downloads');
  await remoteCall.waitForFiles(
      appId,
      [allowedFileEntry.getExpectedRow(), warnedFileEntry.getExpectedRow()]);

  // Focus the file list.
  await remoteCall.focus(appId, ['#file-list:not([hidden])']);

  // Select all files.
  const ctrlA = ['#file-list', 'a', true, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...ctrlA);
  // Check: the file-list should be selected.
  await remoteCall.waitForElement(appId, '#file-list li[selected]');

  await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']);
  await directoryTree.navigateToPath('/fake-usb');
  await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']);

  // DLP warning.
  await remoteCall.waitForFeedbackPanelItem(
      appId, new RegExp('Review is required before copying'),
      new RegExp(`^${warnedFileEntry.targetPath}.*$`));
  // Proceed DLP warning.
  await remoteCall.waitAndClickElement(appId, [
    '#progress-panel',
    'xf-panel-item',
    `xf-button#primary-action`,
  ]);

  // Scanning Label.
  await remoteCall.waitForFeedbackPanelItem(
      appId, new RegExp('^Copying.*$'),
      new RegExp(
          '^Checking files against your organization\'s security policies…$'));

  // Issue the responses, s.t., the transfer can continue.
  await sendTestMessage({name: 'issueFileTransferResponses'});

  // Expect warning proceeded messages.
  await sendTestMessage({
    name: 'expectFileTransferReports',
    source_volume: 'MY_FILES',
    destination_volume: 'REMOVABLE',
    entry_paths: [warnedFileEntry.targetPath],
    expect_proceed_warning_reports: true,
  });

  // Enterprise Connectors warning.
  await remoteCall.waitForFeedbackPanelItem(
      appId, new RegExp('Review is required before copying'),
      new RegExp(`^${warnedFileEntry.targetPath}.*$`));

  // Proceed Enterprise Connectors warning.
  await remoteCall.waitAndClickElement(appId, [
    '#progress-panel',
    'xf-panel-item',
    `xf-button#primary-action`,
  ]);

  // Verify that the two files were copied.
  await directoryTree.navigateToPath('/fake-usb');
  await remoteCall.waitForFiles(
      appId,
      [allowedFileEntry.getExpectedRow(), warnedFileEntry.getExpectedRow()]);
}

// Tests that blocking different files by DLP and Enterprise Connectors will
// copy all the file except the blocked ones. A block panel will be shown in the
// end  with the blocked files count.
export async function differentBlockPolicies() {
  // Add entry to Downloads.
  await addEntries(
      ['local'], [allowedFileEntry, blockedFileEntry1, blockedFileEntry2]);

  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS,
      [allowedFileEntry, blockedFileEntry1, blockedFileEntry2], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('removable');

  // Set the mock to block one file.
  await sendTestMessage({
    name: 'setBlockedFilesTransfer',
    fileNames: [blockedFileEntry1.targetPath],
  });

  // Setup policy.
  await sendTestMessage({
    name: 'setupFileTransferPolicy',
    source: 'MY_FILES',
    destination: 'REMOVABLE',
  });

  // Setup the scanning closure to be able to wait for the scanning to be
  // complete.
  await sendTestMessage({
    name: 'setupScanningRunLoop',
    number_of_expected_delegates: 3,
  });

  // Copy and paste all the files to USB.
  await directoryTree.navigateToPath('/My files/Downloads');
  await remoteCall.waitForFiles(appId, [
    allowedFileEntry.getExpectedRow(),
    blockedFileEntry1.getExpectedRow(),
    blockedFileEntry2.getExpectedRow(),
  ]);

  // Focus the file list.
  await remoteCall.focus(appId, ['#file-list:not([hidden])']);

  // Select all files.
  const ctrlA = ['#file-list', 'a', true, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...ctrlA);
  // Check: the file-list should be selected.
  await remoteCall.waitForElement(appId, '#file-list li[selected]');

  await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']);
  await directoryTree.navigateToPath('/fake-usb');
  await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']);

  // Scanning Label.
  await remoteCall.waitForFeedbackPanelItem(
      appId, new RegExp('^Copying.*$'),
      new RegExp(
          '^Checking files against your organization\'s security policies…$'));

  // Issue the responses, s.t., the transfer can continue.
  await sendTestMessage({name: 'issueFileTransferResponses'});

  await remoteCall.waitForFeedbackPanelItem(
      appId, new RegExp('^Copying 3 items.*$'), new RegExp('$'));

  // Verify that the two files were blocked.
  await remoteCall.waitForFeedbackPanelItem(
      appId, new RegExp('2 files blocked from copying'),
      new RegExp('^Review.*$'));
  await directoryTree.navigateToPath('/fake-usb');
  // blockedFileEntry1 is expected since the DLP daemon actual blocking with
  // Fanotify isn't mocked.
  await remoteCall.waitForFiles(
      appId,
      [allowedFileEntry.getExpectedRow(), blockedFileEntry1.getExpectedRow()]);
}
