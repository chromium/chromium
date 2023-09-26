// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, EntryType, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
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

testcase.twoWarningsProceeded = async () => {
  // Add entry to Downloads.
  await addEntries(['local'], [warnedFileEntry]);

  // Open Files app.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [allowedFileEntry, warnedFileEntry], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.waitForItemByType('removable');

  // Set the mock to pause the first task.
  await sendTestMessage({
    name: 'setCheckFilesTransferMockToPause',
    taskId: 1,
    fileNames: [warnedFileEntry.nameText],
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
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);
  // Check: the file-list should be selected.
  await remoteCall.waitForElement(appId, '#file-list li[selected]');

  await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']);
  await directoryTree.navigateToPath('/fake-usb');
  await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']);

  // DLP warning.
  const dlpWarningPanel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  chrome.test.assertEq(
      'Review is required before copying',
      dlpWarningPanel.attributes['primary-text']);
  // Proceed DLP warning.
  await remoteCall.waitAndClickElement(appId, [
    '#progress-panel',
    'xf-panel-item',
    `xf-button#primary-action`,
  ]);

  // Scanning Label.
  await remoteCall.waitForFeedbackPanelItem(
      appId, new RegExp('^Copying.*$'), new RegExp('^Scanning$'));

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
  const ecWarningPanel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  chrome.test.assertEq(
      'Review is required before copying',
      ecWarningPanel.attributes['primary-text']);

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
};
