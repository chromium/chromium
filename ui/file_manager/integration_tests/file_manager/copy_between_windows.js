// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {openNewWindow, remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

/**
 * Volume type used to find USB removable volume.
 */
const USB_VOLUME_TYPE = 'removable';

/**
 * Opens two window of given root paths.
 * @param {string} rootPath1 Root path of the first window.
 * @param {string} rootPath2 Root path of the second window.
 * @return {Promise<[string, string]>} Promise fulfilled with an array
 *     containing two window IDs.
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
 * @param {?TestEntryInfo} alreadyPresentFile Test entry info for file that
 *     should already exist.
 * @return {Promise<void>} Promise fulfilled on success.
 */
async function copyBetweenWindows(
    window1, window2, file, alreadyPresentFile = null) {
  if (!file || !file.nameText) {
    chrome.test.assertTrue(false, 'copyBetweenWindows invalid file name');
  }

  const flag = {ignoreLastModifiedTime: true};
  const name = file.nameText;

  await remoteCall.waitForFiles(window1, [file.getExpectedRow()]);

  await remoteCall.waitUntilSelected(window1, name);

  await remoteCall.callRemoteTestUtil('execCommand', window1, ['copy']);

  await remoteCall.callRemoteTestUtil('execCommand', window2, ['paste']);

  const expectedFiles = [file.getExpectedRow()];
  if (alreadyPresentFile) {
    expectedFiles.push(alreadyPresentFile.getExpectedRow());
  }
  // @ts-ignore: error TS2345: Argument of type '{ ignoreLastModifiedTime:
  // boolean; }' is not assignable to parameter of type '{ orderCheck: boolean |
  // null | undefined; ignoreFileSize: boolean | null | undefined;
  // ignoreLastModifiedTime: boolean | null | undefined; }'.
  await remoteCall.waitForFiles(window2, expectedFiles, flag);
}

/**
 * Tests file copy+paste from Drive to Downloads.
 */
// @ts-ignore: error TS4111: Property 'copyBetweenWindowsDriveToLocal' comes
// from an index signature, so it must be accessed with
// ['copyBetweenWindowsDriveToLocal'].
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
// @ts-ignore: error TS4111: Property 'copyBetweenWindowsLocalToDrive' comes
// from an index signature, so it must be accessed with
// ['copyBetweenWindowsLocalToDrive'].
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
// @ts-ignore: error TS4111: Property 'copyBetweenWindowsDriveToUsb' comes from
// an index signature, so it must be accessed with
// ['copyBetweenWindowsDriveToUsb'].
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

  // Wait for the USB mount and click to open the USB volume.
  const directoryTree =
      await DirectoryTreePageObject.create(window1, remoteCall);
  await directoryTree.selectItemByType(USB_VOLUME_TYPE);

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
// @ts-ignore: error TS4111: Property 'copyBetweenWindowsLocalToUsb' comes from
// an index signature, so it must be accessed with
// ['copyBetweenWindowsLocalToUsb'].
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

  // Wait for the USB mount and click to open the USB volume.
  const directoryTree =
      await DirectoryTreePageObject.create(window1, remoteCall);
  await directoryTree.selectItemByType(USB_VOLUME_TYPE);

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
// @ts-ignore: error TS4111: Property 'copyBetweenWindowsUsbToDrive' comes from
// an index signature, so it must be accessed with
// ['copyBetweenWindowsUsbToDrive'].
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

  // Wait for the USB mount and click to open the USB volume.
  const directoryTree =
      await DirectoryTreePageObject.create(window1, remoteCall);
  await directoryTree.selectItemByType(USB_VOLUME_TYPE);
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
// @ts-ignore: error TS4111: Property 'copyBetweenWindowsUsbToLocal' comes from
// an index signature, so it must be accessed with
// ['copyBetweenWindowsUsbToLocal'].
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

  // Wait for the USB mount and click to open the USB volume.
  const directoryTree =
      await DirectoryTreePageObject.create(window1, remoteCall);
  await directoryTree.selectItemByType(USB_VOLUME_TYPE);

  // Check: Drive window is showing an empty USB volume.
  await remoteCall.waitForFiles(window1, []);

  // Add hello file to the USB volume.
  await addEntries(['usb'], [ENTRIES.hello]);

  // Check USB hello file, copy it to Downloads.
  await copyBetweenWindows(window1, window2, ENTRIES.hello);
};
