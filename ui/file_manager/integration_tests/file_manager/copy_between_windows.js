// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Query used to find USB removable volume.
 */
const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

/**
 * Opens two window of given root paths.
 * @param {string} rootPath1 Root path of the first window.
 * @param {string} rootPath2 Root path of the second window.
 * @return {Promise} Promise fulfilled with an array containing two window IDs.
 */
async function openTwoWindows(rootPath1, rootPath2) {
  const windowIds =
      await Promise.all([openNewWindow(rootPath1), openNewWindow(rootPath2)]);

  await Promise.all([
    remoteCall.waitForElement(windowIds[0], '#detail-table'),
    remoteCall.waitForElement(windowIds[1], '#detail-table'),
  ]);
  return windowIds;
}

/**
 * Copies a file between two windows.
 * @param {string} window1 ID of the source window.
 * @param {string} window2 ID of the destination window.
 * @param {TestEntryInfo} file Test entry info to be copied.
 * @return {Promise} Promise fulfilled on success.
 */
async function copyBetweenWindows(
    window1, window2, file, alreadyPresentFile = null) {
  if (!file || !file.nameText) {
    chrome.test.assertTrue(false, 'copyBetweenWindows invalid file name');
  }

  const flag = {ignoreLastModifiedTime: true};
  const name = file.nameText;

  await remoteCall.waitForFiles(window1, [file.getExpectedRow()]);

  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', window1, [name]),
      'Failed: selectFile ' + name);

  await remoteCall.callRemoteTestUtil('execCommand', window1, ['copy']);

  await remoteCall.callRemoteTestUtil('execCommand', window2, ['paste']);

  const expectedFiles = [file.getExpectedRow()];
  if (alreadyPresentFile) {
    expectedFiles.push(alreadyPresentFile.getExpectedRow());
  }
  await remoteCall.waitForFiles(window2, expectedFiles, flag);
}

/**
 * Tests file copy+paste from Drive to Downloads.
 */
testcase.copyBetweenWindowsDriveToLocal = async () => {
  // Open two Files app windows.
  const [window1, window2] =
      await openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE);

  // Add files.
  await Promise.all([
    addEntries(['drive'], [ENTRIES.hello]),
    addEntries(['local'], [ENTRIES.photos]),
  ]);

  // Check: Downloads photos file.
  await remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()]);

  // Copy Drive hello file to Downloads.
  await copyBetweenWindows(window2, window1, ENTRIES.hello, ENTRIES.photos);
};

/**
 * Tests file copy+paste from Downloads to Drive.
 */
testcase.copyBetweenWindowsLocalToDrive = async () => {
  // Open two Files app windows.
  const [window1, window2] =
      await openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE);

  // Add files.
  await Promise.all([
    addEntries(['local'], [ENTRIES.hello]),
    addEntries(['drive'], [ENTRIES.photos]),
  ]);

  // Check: Downloads hello file and Drive photos file.
  await remoteCall.waitForFiles(window2, [ENTRIES.photos.getExpectedRow()]);

  // Copy Downloads hello file to Drive.
  await copyBetweenWindows(window1, window2, ENTRIES.hello, ENTRIES.photos);
};

/**
 * Tests file copy+paste from Drive to USB.
 */
testcase.copyBetweenWindowsDriveToUsb = async () => {
  // Add photos to Downloads.
  await addEntries(['local'], [ENTRIES.photos]);

  // Open two Files app windows.
  const [window1, window2] =
      await openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE);

  // Check: Drive window is empty.
  await remoteCall.waitForFiles(window2, []);

  // Click to switch back to the Downloads window.
  await remoteCall.callRemoteTestUtil('fakeMouseClick', window1, []);

  // Check: Downloads window is showing photos.
  await remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()]);

  // Mount an empty USB volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(window1, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', window1, [USB_VOLUME_QUERY]);

  // Check: Downloads window is showing an empty USB volume.
  await remoteCall.waitForFiles(window1, []);

  // Add hello file to Drive.
  await addEntries(['drive'], [ENTRIES.hello]);

  // Check Drive hello file, copy it to USB.
  await copyBetweenWindows(window2, window1, ENTRIES.hello);
};

/**
 * Tests file copy+paste from Downloads to USB.
 */
testcase.copyBetweenWindowsLocalToUsb = async () => {
  // Add photos to Drive.
  await addEntries(['drive'], [ENTRIES.photos]);

  // Open two Files app windows.
  const [window1, window2] =
      await openTwoWindows(RootPath.DRIVE, RootPath.DOWNLOADS);

  // Check: Downloads window is empty.
  await remoteCall.waitForFiles(window2, []);

  // Click to switch back to the Drive window.
  await remoteCall.callRemoteTestUtil('fakeMouseClick', window1, []);

  // Check: Drive window is showing photos.
  await remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()]);

  // Mount an empty USB volume in the Drive window.
  await chrome.test.sendMessage(JSON.stringify({name: 'mountFakeUsbEmpty'}));

  // Wait for the USB mount.
  await remoteCall.waitForElement(window1, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', window1, [USB_VOLUME_QUERY]);

  // Check: Drive window is showing an empty USB volume.
  await remoteCall.waitForFiles(window1, []);

  // Add hello file to Downloads.
  await addEntries(['local'], [ENTRIES.hello]);

  // Check Downloads hello file, copy it to USB.
  await copyBetweenWindows(window2, window1, ENTRIES.hello);
};

/**
 * Tests file copy+paste from USB to Drive.
 */
testcase.copyBetweenWindowsUsbToDrive = async () => {
  // Add photos to Downloads.
  await addEntries(['local'], [ENTRIES.photos]);

  // Open two Files app windows.
  const [window1, window2] =
      await openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE);

  // Check: Drive window is empty.
  await remoteCall.waitForFiles(window2, []);

  // Click to switch back to the Downloads window.
  await remoteCall.callRemoteTestUtil('fakeMouseClick', window1, []);

  // Check: Downloads window is showing photos.
  await remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()]);

  // Mount an empty USB volume in the Downloads window.
  await chrome.test.sendMessage(JSON.stringify({name: 'mountFakeUsbEmpty'}));

  // Wait for the USB mount.
  await remoteCall.waitForElement(window1, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', window1, [USB_VOLUME_QUERY]);

  // Check: Downloads window is showing an empty USB volume.
  await remoteCall.waitForFiles(window1, []);

  // Add hello file to the USB volume.
  await addEntries(['usb'], [ENTRIES.hello]);

  // Check USB hello file, copy it to Drive.
  await copyBetweenWindows(window1, window2, ENTRIES.hello);
};

/**
 * Tests file copy+paste from USB to Downloads.
 */
testcase.copyBetweenWindowsUsbToLocal = async () => {
  // Add photos to Drive.
  await addEntries(['drive'], [ENTRIES.photos]);

  // Open two Files app windows.
  const [window1, window2] =
      await openTwoWindows(RootPath.DRIVE, RootPath.DOWNLOADS);

  // Check: Downloads window is empty.
  await remoteCall.waitForFiles(window2, []);

  // Click to switch back to the Drive window.
  await remoteCall.callRemoteTestUtil('fakeMouseClick', window1, []);

  // Check: Drive window is showing photos.
  await remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()]);

  // Mount an empty USB volume in the Drive window.
  await chrome.test.sendMessage(JSON.stringify({name: 'mountFakeUsbEmpty'}));

  // Wait for the USB mount.
  await remoteCall.waitForElement(window1, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', window1, [USB_VOLUME_QUERY]);

  // Check: Drive window is showing an empty USB volume.
  await remoteCall.waitForFiles(window1, []);

  // Add hello file to the USB volume.
  await addEntries(['usb'], [ENTRIES.hello]);

  // Check USB hello file, copy it to Downloads.
  await copyBetweenWindows(window1, window2, ENTRIES.hello);
};
