// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage, wait, waitForAppWindow} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_ZIP_ENTRY_SET} from './test_data.js';

/**
 * Returns the expected file list row entries after opening (mounting) the
 * ENTRIES.zipArchive file list entry.
 */
function getUnzippedFileListRowEntries() {
  return [
    [
      'SUCCESSFULLY_PERFORMED_FAKE_MOUNT.txt', '21 bytes', 'Plain text',
      'Dec 31, 1980, 12:00 AM'
    ],
  ];
}

/**
 * Tests ZIP mounting from Downloads.
 */
testcase.zipFileOpenDownloads = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch'
  });

  // Open Files app on Downloads containing a zip file.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchive], []);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {'ignoreLastModifiedTime': true});
};

/**
 * Tests that Files app's ZIP mounting notifies FileTasks when mounted.
 */
testcase.zipNotifyFileTasks = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch'
  });

  // Open Files app on Downloads containing a zip file.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchive], []);

  // Open the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('openFile', appId, ['archive.zip']),
      'openFile failed');

  // Wait for the zip archive to mount.
  await remoteCall.waitForElement(appId, `[scan-completed="archive.zip"]`);
};

/**
 * Tests ZIP mounting from Google Drive.
 */
testcase.zipFileOpenDrive = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch'
  });

  // Open Files app on Drive containing a zip file.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.zipArchive]);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {'ignoreLastModifiedTime': true});
};

/**
 * Tests ZIP mounting from a removable USB volume.
 */
testcase.zipFileOpenUsb = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch'
  });

  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on Drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.beautiful]);

  // Mount empty USB volume in the Drive window.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [USB_VOLUME_QUERY]);

  // Add zip file to the USB volume.
  await addEntries(['usb'], [ENTRIES.zipArchive]);

  // Verify the USB file list.
  const archive = [ENTRIES.zipArchive.getExpectedRow()];
  await remoteCall.waitForFiles(appId, archive);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {'ignoreLastModifiedTime': true});
};

/**
 * Returns the expected file list rows after invoking the 'Zip selection' menu
 * command on the ENTRIES.photos file list item.
 */
function getZipSelectionFileListRowEntries() {
  return [
    ['photos', '--', 'Folder', 'Jan 1, 1980, 11:59 PM'],
    ['photos.zip', '134 bytes', 'Zip archive', 'Oct 21, 1983, 11:55 AM']
  ];
}

/**
 * Tests creating a ZIP file on Downloads.
 */
testcase.zipCreateFileDownloads = async () => {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Select the file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['photos']),
      'selectFile failed');

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Click the 'Zip selection' menu command.
  const zip = '[command="#zip-selection"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [zip]),
      'fakeMouseClick failed');

  // Check: a zip file should be created.
  const files = getZipSelectionFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests creating a ZIP file on Drive.
 */
testcase.zipCreateFileDrive = async () => {
  // Open Files app on Drive containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.photos]);

  // Select the file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['photos']),
      'selectFile failed');

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Click the 'Zip selection' menu command.
  const zip = '[command="#zip-selection"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [zip]),
      'fakeMouseClick failed');

  // Check: a zip file should be created.
  const files = getZipSelectionFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests creating a ZIP file on a removable USB volume.
 */
testcase.zipCreateFileUsb = async () => {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on Drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.beautiful]);

  // Mount empty USB volume in the Drive window.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [USB_VOLUME_QUERY]);

  // Add ENTRIES.photos to the USB volume.
  await addEntries(['usb'], [ENTRIES.photos]);

  // Verify the USB file list.
  const photos = [ENTRIES.photos.getExpectedRow()];
  await remoteCall.waitForFiles(appId, photos);

  // Select the photos file list entry.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['photos']),
      'selectFile failed');

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Click the 'Zip selection' menu command.
  const zip = '[command="#zip-selection"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [zip]),
      'fakeMouseClick failed');

  // Check: a zip file should be created.
  const files = getZipSelectionFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests that extraction of a ZIP archive produces a feedback panel.
 */
testcase.zipExtractShowPanel = async () => {
  const entry = ENTRIES.zipArchive;

  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Tell the background page to never finish the file extraction.
  await remoteCall.callRemoteTestUtil(
      'progressCenterNeverNotifyCompleted', appId, []);

  // Click the 'Extract all' menu command.
  const extract = '[command="#extract-all"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [extract]),
      'fakeMouseClick failed');

  // Check that the error appears in the feedback panel.
  let element = {};
  const caller = getCaller();
  await repeatUntil(async () => {
    element = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);
    const expectedMsg = `Extracting ${entry.nameText}…`;
    const actualMsg = element.attributes['primary-text'];

    if (actualMsg === expectedMsg) {
      return;
    }

    return pending(
        caller,
        `Expected feedback panel msg: "${expectedMsg}", got "${actualMsg}"`);
  });
};

/**
 * Tests that various selections enable/hide the correct menu items.
 */
testcase.zipExtractSelectionMenus = async () => {
  const entries = BASIC_ZIP_ENTRY_SET;

  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Select the first file (ENTRIES.hello).
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entries[0].nameText]));

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check: the Zip selection menu item should be visible.
  await remoteCall.waitForElement(
      appId, '[command="#zip-selection"]:not([hidden])');

  // Check: the Extract all menu item should be hidden.
  await remoteCall.waitForElement(appId, '[command="#extract-all"][hidden]');

  // Click the main dialog area to hide the context menu.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['.dialog-main']),
      'fakeMouseClick failed');

  // Select the third file (ENTRIES.zipArchive).
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entries[2].nameText]));

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check: the Extract all menu item should be visible.
  await remoteCall.waitForElement(
      appId, '[command="#extract-all"]:not([hidden])');

  // Check: the Zip selection menu item should be hidden.
  await remoteCall.waitForElement(appId, '[command="#zip-selection"][hidden]');

  // Click the main dialog area to hide the context menu.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['.dialog-main']),
      'fakeMouseClick failed');

  // Select the first and second file (ENTRIES.hello, ENTRIES.world).
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="world.ogv"]');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {shift: true});

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check: the Extract all menu item should be hidden.
  await remoteCall.waitForElement(appId, '[command="#extract-all"][hidden]');

  // Check: the Zip selection menu item should be visible.
  await remoteCall.waitForElement(
      appId, '[command="#zip-selection"]:not([hidden])');

  // Click the main dialog area to hide the context menu.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['.dialog-main']),
      'fakeMouseClick failed');

  // Select the first and third file (ENTRIES.hello, ENTRIES.zipArchive).
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="archive.zip"]');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {shift: true});

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check: the Extract all menu item should be visible.
  await remoteCall.waitForElement(
      appId, '[command="#extract-all"]:not([hidden])');

  // Check: the Zip selection menu item should be visible.
  await remoteCall.waitForElement(
      appId, '[command="#zip-selection"]:not([hidden])');
};
