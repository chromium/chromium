// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, EntryType, RootPath, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {mountCrostini, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_CROSTINI_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, NESTED_ENTRY_SET, RECENT_ENTRY_SET} from './test_data.js';

// Test entry for a recently-modified video file.
const RECENTLY_MODIFIED_VIDEO = new TestEntryInfo({
  type: EntryType.FILE,
  sourceFileName: 'video.ogv',
  targetPath: 'world.ogv',
  mimeType: 'video/ogg',
  lastModifiedTime: 'Jul 4, 2038, 10:35 AM',
  nameText: 'world.ogv',
  sizeText: '59 KB',
  typeText: 'OGG video'
});

async function verifyCurrentEntries(appId, expectedEntries) {
  // Verify Recents contains the expected files - those with an mtime in the
  // future.
  const files = TestEntryInfo.getExpectedRows(expectedEntries);
  await remoteCall.waitForFiles(appId, files);

  // Select all the files and check that the delete button isn't visible.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA);

  // Check: the file-list should be selected.
  await remoteCall.waitForElement(appId, '#file-list li[selected]');

  // Test that the delete button isn't visible.
  const deleteButton = await remoteCall.waitForElement(appId, '#delete-button');
  chrome.test.assertTrue(deleteButton.hidden, 'delete button should be hidden');
}

async function verifyRecents(appId, expectedEntries = RECENT_ENTRY_SET) {
  // Navigate to Recents.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['span[root-type-icon=\'recent\']']));

  await verifyCurrentEntries(appId, expectedEntries);
}

async function verifyRecentAudio(appId, expectedEntries) {
  // Navigate to Audio.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['span[root-type-icon=\'recent\'][recent-file-type=\'audio\']']));

  await verifyCurrentEntries(appId, expectedEntries);
}

async function verifyRecentImages(appId, expectedEntries) {
  // Navigate to Images.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['span[root-type-icon=\'recent\'][recent-file-type=\'image\']']));

  await verifyCurrentEntries(appId, expectedEntries);
}

async function verifyRecentVideos(appId, expectedEntries) {
  // Navigate to Images.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['span[root-type-icon=\'recent\'][recent-file-type=\'video\']']));

  await verifyCurrentEntries(appId, expectedEntries);
}

async function verifyBreadcrumbsPath(appId, expectedPath) {
  const path =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq(expectedPath, path);
}

/**
 * Opens given file's containing folder by choosing "Go to file location"
 * context menu item.
 *
 * @param {string} appId Files app windowId.
 * @param {string} itemName Name of the file to open containing folder.
 */
async function goToFileLocation(appId, itemName) {
  // Select the item.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [itemName]));

  // Right-click the selected file.
  await remoteCall.waitAndRightClick(appId, '.table-row[selected]');

  // Click 'Go to file location' menu command.
  const goToLocationMenu = '#file-context-menu:not([hidden]) ' +
      '[command="#go-to-file-location"]:not([hidden]):not([disabled])';
  remoteCall.waitAndClickElement(appId, goToLocationMenu);
}

testcase.recentsDownloads = async () => {
  // Populate downloads.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Verifies file list in Recents.
  await verifyRecents(appId);

  // Tests that selecting "Go to file location" for a file navigates to
  // Downloads since the file in Recents is from Downloads.
  await goToFileLocation(appId, ENTRIES.desktop.nameText);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));
  await verifyBreadcrumbsPath(appId, '/My files/Downloads');
};

testcase.recentsDrive = async () => {
  // Populate drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);

  // Verifies file list in Recents.
  await verifyRecents(appId);

  // Tests that selecting "Go to file location" for a file navigates to
  // My Drive since the file in Recents is from Google Drive.
  await goToFileLocation(appId, ENTRIES.desktop.nameText);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_DRIVE_ENTRY_SET));
  await verifyBreadcrumbsPath(appId, '/My Drive');
};

testcase.recentsCrostiniNotMounted = async () => {
  // Add entries to crostini volume, but do not mount.
  // The crostini entries should not show up in recents.
  await addEntries(['crostini'], BASIC_CROSTINI_ENTRY_SET);

  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.photos], []);
  await verifyRecents(appId, [ENTRIES.beautiful]);
};

testcase.recentsCrostiniMounted = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.photos], []);
  // Mount crostini and both downloads and crostini entries will be in recents.
  await mountCrostini(appId);
  await verifyRecents(appId);
};

testcase.recentsDownloadsAndDrive = async () => {
  // Populate both downloads and drive with disjoint sets of files.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.hello, ENTRIES.photos],
      [ENTRIES.desktop, ENTRIES.world, ENTRIES.testDocument]);
  await verifyRecents(appId);
};

testcase.recentsDownloadsAndDriveWithOverlap = async () => {
  // Populate both downloads and drive with overlapping sets of files.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await verifyRecents(appId, RECENT_ENTRY_SET.concat(RECENT_ENTRY_SET));
};

testcase.recentsNested = async () => {
  // Populate downloads with nested folder structure. |desktop| is added to
  // ensure Recents has different files to Downloads/A/B/C
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS,
      NESTED_ENTRY_SET.concat([ENTRIES.deeplyBurriedSmallJpeg]), []);

  // Verifies file list in Recents.
  await verifyRecents(appId, [ENTRIES.deeplyBurriedSmallJpeg]);

  // Tests that selecting "Go to file location" for a file navigates to
  // Downloads/A/B/C since the file in Recents is from Downloads/A/B/C.
  await goToFileLocation(appId, ENTRIES.deeplyBurriedSmallJpeg.nameText);
  await remoteCall.waitForElement(appId, `[scan-completed="C"]`);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.deeplyBurriedSmallJpeg]));
  await verifyBreadcrumbsPath(appId, '/My files/Downloads/A/B/C');

  // Check: The directory should be highlighted in the directory tree.
  await remoteCall.waitForElement(
      appId,
      '.tree-item[full-path-for-testing="/Downloads/A/B/C"] > ' +
          '.tree-row[selected][active]');
};

testcase.recentAudioDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  // ENTRIES.beautiful is recently-modified and has .ogg file extension.
  await verifyRecentAudio(appId, [ENTRIES.beautiful]);

  // Breadcrumbs path should be '/Audio'.
  await verifyBreadcrumbsPath(appId, '/Audio');
};

testcase.recentAudioDownloadsAndDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, BASIC_DRIVE_ENTRY_SET);
  // ENTRIES.beautiful in BASIC_DRIVE_ENTRY_SET does not have mime type.
  // Since Drive files should be filtered based on mime types, only the file in
  // Downloads should be shown even though the Drive one also has .ogg file
  // extension.
  await verifyRecentAudio(appId, [ENTRIES.beautiful]);
};

testcase.recentImagesDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  // ENTRIES.desktop is recently-modified and has .png file extension.
  await verifyRecentImages(appId, [ENTRIES.desktop]);

  // Breadcrumbs path should be '/Images'.
  await verifyBreadcrumbsPath(appId, '/Images');
};

testcase.recentImagesDownloadsAndDrive = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  // ENTRIES.desktop has 'image/png' mime type, too. Both the file in Downloads
  // and the file in Drive should be shown in Images.
  await verifyRecentImages(appId, [ENTRIES.desktop, ENTRIES.desktop]);
};

testcase.recentVideosDownloads = async () => {
  // RECENTLY_MODIFIED_VIDEO is recently-modified and has .ogv file extension.
  // It should be shown in Videos.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS,
      BASIC_LOCAL_ENTRY_SET.concat([RECENTLY_MODIFIED_VIDEO]), []);
  await verifyRecentVideos(appId, [RECENTLY_MODIFIED_VIDEO]);

  // Breadcrumbs path should be '/Videos'.
  await verifyBreadcrumbsPath(appId, '/Videos');
};

testcase.recentVideosDownloadsAndDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS,
      BASIC_LOCAL_ENTRY_SET.concat([RECENTLY_MODIFIED_VIDEO]),
      BASIC_DRIVE_ENTRY_SET.concat([RECENTLY_MODIFIED_VIDEO]));
  // RECENTLY_MODIFIED_VIDEO has video mime type (video/ogg) too, so the file
  // from Drive should be shown too.
  await verifyRecentVideos(
      appId, [RECENTLY_MODIFIED_VIDEO, RECENTLY_MODIFIED_VIDEO]);
};
