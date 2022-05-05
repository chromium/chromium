// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, EntryType, getDateWithinLastMonth, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {mountCrostini, openNewWindow, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_CROSTINI_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, NESTED_ENTRY_SET, RECENT_ENTRY_SET} from './test_data.js';

// Test entry for a recently-modified video file.
const RECENTLY_MODIFIED_VIDEO =
    ENTRIES.world.cloneWithModifiedDate(getDateWithinLastMonth());
const RECENTLY_MODIFIED_MOV_VIDEO =
    ENTRIES.movFile.cloneWithModifiedDate(getDateWithinLastMonth());

// Test entry for a recently-modified document file.
const RECENTLY_MODIFIED_DOCUMENT =
    ENTRIES.docxFile.cloneWithModifiedDate(getDateWithinLastMonth());

// Test entries for recent-modified android files.
const RECENT_MODIFIED_ANDROID_DOCUMENT =
    ENTRIES.documentsText.cloneWithModifiedDate(getDateWithinLastMonth());
const RECENT_MODIFIED_ANDROID_IMAGE =
    ENTRIES.picturesImage.cloneWithModifiedDate(getDateWithinLastMonth());
const RECENT_MODIFIED_ANDROID_AUDIO =
    ENTRIES.musicAudio.cloneWithModifiedDate(getDateWithinLastMonth());
const RECENT_MODIFIED_ANDROID_VIDEO =
    ENTRIES.moviesVideo.cloneWithModifiedDate(getDateWithinLastMonth());

/**
 * Enum for supported recent filter types.
 * @enum {string}
 */
const RecentFilterType = {
  ALL: 'all',
  AUDIO: 'audio',
  IMAGE: 'image',
  VIDEO: 'video',
  DOCUMENT: 'document',
};

/**
 * Add file entries to the Play Files folder and update media view root.
 */
async function addPlayFileEntries() {
  // We can't add file entries to Play Files ('android_files') directly,
  // because they won't be picked up by the fake ARC file system. Instead,
  // we need to add file entries to the corresponding media view root.
  await sendTestMessage({name: 'mountMediaView'});
  await addEntries(['media_view_audio'], [RECENT_MODIFIED_ANDROID_AUDIO]);
  await addEntries(['media_view_images'], [RECENT_MODIFIED_ANDROID_IMAGE]);
  await addEntries(['media_view_videos'], [RECENT_MODIFIED_ANDROID_VIDEO]);
  await addEntries(
      ['media_view_documents'], [RECENT_MODIFIED_ANDROID_DOCUMENT]);
}

/**
 * Checks if the #file-filters-in-recents flag has been enabled or not.
 *
 * @return {!Promise<boolean>} Flag enabled or not.
 */
async function isFiltersInRecentsEnabled() {
  const isFiltersInRecentsEnabled =
      await sendTestMessage({name: 'isFiltersInRecentsEnabled'});
  return isFiltersInRecentsEnabled === 'true';
}

/**
 * Navigate to Recent folder with specific type and verify the breadcrumb path.
 *
 * @param {string} appId Files app windowId.
 * @param {!RecentFilterType=} type Recent file type.
 */
async function navigateToRecent(appId, type = RecentFilterType.ALL) {
  const breadcrumbMap = {
    [RecentFilterType.ALL]: '/Recent',
    [RecentFilterType.AUDIO]: '/Audio',
    [RecentFilterType.IMAGE]: '/Images',
    [RecentFilterType.VIDEO]: '/Videos',
    [RecentFilterType.DOCUMENT]: '/Documents',
  };

  if (await isFiltersInRecentsEnabled()) {
    await remoteCall.waitAndClickElement(appId, ['[root-type-icon="recent"]']);
    // "All" button is activated by default, no need to click.
    if (type !== RecentFilterType.ALL) {
      await remoteCall.waitAndClickElement(
          appId, [`[file-type-filter="${type}"]`]);
    }
    // Check the corresponding filter button is activated.
    await remoteCall.waitForElement(
        appId, [`[file-type-filter="${type}"].active`]);
    // Breadcrumb should always be "/Recents" if the flag is on.
    await verifyBreadcrumbsPath(appId, breadcrumbMap[RecentFilterType.ALL]);
  } else {
    await remoteCall.waitAndClickElement(
        appId, [`[root-type-icon="recent"][recent-file-type="${type}"]`]);
    await verifyBreadcrumbsPath(appId, breadcrumbMap[type]);
  }
}

/**
 * Verifies the current folder has the expected entries and checks the
 * delete button is hidden after selecting these files.
 *
 * @param {string} appId Files app windowId.
 * @param {!Array<!TestEntryInfo>} expectedEntries Expected file entries.
 */
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

/**
 * Opens the Recent folder and checks the expected entries are showing
 * there.
 *
 * @param {string} appId Files app windowId.
 * @param {!Array<!TestEntryInfo>=} expectedEntries Expected file
 *     entries, by default `RECENT_ENTRY_SET` is used.
 */
async function verifyRecents(appId, expectedEntries = RECENT_ENTRY_SET) {
  await navigateToRecent(appId);
  await verifyCurrentEntries(appId, expectedEntries);
}


/**
 * Opens the Recent Audio folder and checks the expected entries are
 * showing there.
 *
 * @param {string} appId Files app windowId.
 * @param {!Array<!TestEntryInfo>} expectedEntries Expected file entries.
 */
async function verifyRecentAudio(appId, expectedEntries) {
  await navigateToRecent(appId, RecentFilterType.AUDIO);
  await verifyCurrentEntries(appId, expectedEntries);
}

/**
 * Opens the Recent Image folder and checks the expected entries are
 * showing there.
 *
 * @param {string} appId Files app windowId.
 * @param {!Array<!TestEntryInfo>} expectedEntries Expected file entries.
 */
async function verifyRecentImages(appId, expectedEntries) {
  await navigateToRecent(appId, RecentFilterType.IMAGE);
  await verifyCurrentEntries(appId, expectedEntries);
}

/**
 * Opens the Recent Video folder and checks the expected entries are
 * showing there.
 *
 * @param {string} appId Files app windowId.
 * @param {!Array<!TestEntryInfo>} expectedEntries Expected file entries.
 */
async function verifyRecentVideos(appId, expectedEntries) {
  await navigateToRecent(appId, RecentFilterType.VIDEO);
  await verifyCurrentEntries(appId, expectedEntries);
}

/**
 * Opens the Recent Document folder and checks the expected entries are
 * showing there.
 *
 * @param {string} appId Files app windowId.
 * @param {!Array<!TestEntryInfo>} expectedEntries Expected file entries.
 */
async function verifyRecentDocuments(appId, expectedEntries) {
  await navigateToRecent(appId, RecentFilterType.DOCUMENT);
  await verifyCurrentEntries(appId, expectedEntries);
}

/**
 * Verifies the breadcrumb has the expected path.
 *
 * @param {string} appId Files app windowId.
 * @param {string} expectedPath Expected breadcrumb path.
 */
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

/**
 * Tests that file entries populated in the Downloads folder recently will be
 * displayed in Recent folder.
 */
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

/**
 * Tests that file entries populated in My Drive folder recently will be
 * displayed in Recent folder.
 */
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

/**
 * Tests that file entries populated in Play Files folder recently will be
 * displayed in Recent folder.
 */
testcase.recentsPlayFiles = async () => {
  // Populate Play Files.
  await addPlayFileEntries();
  const appId = await openNewWindow(RootPath.ANDROID_FILES, {});
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Verifies file list in Recents. Audio files from Play Files folder are
  // not supported in Recents.
  await verifyRecents(appId, [
    RECENT_MODIFIED_ANDROID_DOCUMENT, RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO
  ]);
};

/**
 * Tests that file entries populated in Crostini folder recently won't be
 * displayed in Recent folder when Crostini has not been mounted.
 */
testcase.recentsCrostiniNotMounted = async () => {
  // Add entries to crostini volume, but do not mount.
  // The crostini entries should not show up in recents.
  await addEntries(['crostini'], BASIC_CROSTINI_ENTRY_SET);

  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.photos], []);
  await verifyRecents(appId, [ENTRIES.beautiful]);
};

/**
 * Tests that file entries populated in Downloads folder and Crostini folder
 * recently will be displayed in Recent folder when Crostini has been mounted.
 */
testcase.recentsCrostiniMounted = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.photos], []);
  // Mount crostini and both downloads and crostini entries will be in recents.
  await mountCrostini(appId);
  await verifyRecents(appId);
};

/**
 * Tests that file entries populated in Downloads folder and My Drive folder
 * recently will be displayed in Recent folder.
 */
testcase.recentsDownloadsAndDrive = async () => {
  // Populate both downloads and drive with disjoint sets of files.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.hello, ENTRIES.photos],
      [ENTRIES.desktop, ENTRIES.world, ENTRIES.testDocument]);
  await verifyRecents(appId);
};

/**
 * Tests that file entries populated in Downloads, Drive and Play Files folder
 * recently will be displayed in Recent folder.
 */
testcase.recentsDownloadsAndDriveAndPlayFiles = async () => {
  // Populate downloads, drive and play files.
  await addPlayFileEntries();
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.hello, ENTRIES.photos],
      [ENTRIES.desktop, ENTRIES.world, ENTRIES.testDocument]);

  await verifyRecents(appId, RECENT_ENTRY_SET.concat([
    RECENT_MODIFIED_ANDROID_DOCUMENT, RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO
  ]));
};

/**
 * Tests that the same file entries populated in Downloads folder and My Drive
 * folder recently will be displayed in Recent folder twice when the file
 * entries are the same.
 */
testcase.recentsDownloadsAndDriveWithOverlap = async () => {
  // Populate both downloads and drive with overlapping sets of files.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await verifyRecents(appId, RECENT_ENTRY_SET.concat(RECENT_ENTRY_SET));
};


/**
 * Tests that the nested file entries populated in Downloads folder recently
 * will be displayed in Recent folder.
 */
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

/**
 * Tests that the audio file entries populated in Downloads folder recently
 * will be displayed in Recent Audio folder.
 */
testcase.recentAudioDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  // ENTRIES.beautiful is recently-modified and has .ogg file extension.
  await verifyRecentAudio(appId, [ENTRIES.beautiful]);
};

/**
 * Tests that if the audio file entries without MIME type are being populated
 * in both Downloads folder and My Drive folder, only the ones from Downloads
 * folder will be displayed in Recent Audio folder.
 */
testcase.recentAudioDownloadsAndDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, BASIC_DRIVE_ENTRY_SET);
  // ENTRIES.beautiful in BASIC_DRIVE_ENTRY_SET does not have mime type.
  // Since Drive files should be filtered based on mime types, only the file in
  // Downloads should be shown even though the Drive one also has .ogg file
  // extension.
  await verifyRecentAudio(appId, [ENTRIES.beautiful]);

  // Tests that selecting "Go to file location" for the file navigates to
  // Downloads since the same file from My Drive doesn't appear in Recent
  // Audio folder.
  await goToFileLocation(appId, ENTRIES.beautiful.nameText);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));
  await verifyBreadcrumbsPath(appId, '/My files/Downloads');
};

/**
 * Tests that the audio file entries populated in Downloads, Drive and Play
 * Files folder recently will be displayed in Recent Audio folder.
 */
testcase.recentAudioDownloadsAndDriveAndPlayFiles = async () => {
  await addPlayFileEntries();
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, BASIC_DRIVE_ENTRY_SET);
  // ENTRIES.beautiful in BASIC_DRIVE_ENTRY_SET does not have mime type, so it
  // won't be included. Also, Play Files recents doesn't support audio root,
  // so audio file in Play Files won't be included.
  await verifyRecentAudio(appId, [ENTRIES.beautiful]);
};

/**
 * Tests that the image file entries populated in Downloads folder recently will
 * be displayed in Recents Image folder.
 */
testcase.recentImagesDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  // ENTRIES.desktop is recently-modified and has .png file extension.
  await verifyRecentImages(appId, [ENTRIES.desktop]);
};

/**
 * Tests that if the image file entries with MIME type are being populated
 * in both Downloads folder and My Drive folder, the file entries will be
 * displayed in Recent Audio folder regardless of whether it's from Downloads
 * or My Drive.
 */
testcase.recentImagesDownloadsAndDrive = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  // ENTRIES.desktop has 'image/png' mime type, too. Both the file in Downloads
  // and the file in Drive should be shown in Images.
  await verifyRecentImages(appId, [ENTRIES.desktop, ENTRIES.desktop]);
};

/**
 * Tests that the image file entries populated in Downloads, Drive and Play
 * Files folder recently will be displayed in Recents Image folder.
 */
testcase.recentImagesDownloadsAndDriveAndPlayFiles = async () => {
  await addPlayFileEntries();
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await verifyRecentImages(
      appId, [ENTRIES.desktop, ENTRIES.desktop, RECENT_MODIFIED_ANDROID_IMAGE]);
};

/**
 * Tests that the video file entries populated in Downloads folder recently will
 * be displayed in Recent Videos folder.
 */
testcase.recentVideosDownloads = async () => {
  // RECENTLY_MODIFIED_VIDEO is recently-modified and has .ogv file extension.
  // It should be shown in Videos.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS,
      BASIC_LOCAL_ENTRY_SET.concat(
          [RECENTLY_MODIFIED_VIDEO, RECENTLY_MODIFIED_MOV_VIDEO]),
      []);
  await verifyRecentVideos(
      appId, [RECENTLY_MODIFIED_VIDEO, RECENTLY_MODIFIED_MOV_VIDEO]);
};

/**
 * Tests that if the video file entries with MIME type are being populated
 * in both Downloads folder and My Drive folder, the file entries will be
 * displayed in Recent Video folder regardless of whether it's from Downloads
 * or My Drive.
 */
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

/**
 * Tests that the video file entries populated in Downloads, Drive and Play
 * Files folder recently will be displayed in Recent Image folder.
 */
testcase.recentVideosDownloadsAndDriveAndPlayFiles = async () => {
  await addPlayFileEntries();
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS,
      BASIC_LOCAL_ENTRY_SET.concat([RECENTLY_MODIFIED_VIDEO]),
      BASIC_DRIVE_ENTRY_SET.concat([RECENTLY_MODIFIED_VIDEO]));
  await verifyRecentVideos(appId, [
    RECENTLY_MODIFIED_VIDEO, RECENTLY_MODIFIED_VIDEO,
    RECENT_MODIFIED_ANDROID_VIDEO
  ]);
};

/**
 * Tests that the document file entries populated in Downloads folder recently
 * will be displayed in Recent Document folder.
 */
testcase.recentDocumentsDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [RECENTLY_MODIFIED_DOCUMENT], []);
  await verifyRecentDocuments(appId, [RECENTLY_MODIFIED_DOCUMENT]);
};

/**
 * Tests that if the video file entries with MIME type are being populated
 * in both Downloads folder and My Drive folder, the file entries will be
 * displayed in Recent Document folder regardless of whether it's from Downloads
 * or My Drive.
 */
testcase.recentDocumentsDownloadsAndDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [RECENTLY_MODIFIED_DOCUMENT],
      [RECENTLY_MODIFIED_DOCUMENT]);
  // RECENTLY_MODIFIED_DOCUMENT exists in both local and drive folder, the
  // file will appear twice in the result.
  await verifyRecentDocuments(
      appId, [RECENTLY_MODIFIED_DOCUMENT, RECENTLY_MODIFIED_DOCUMENT]);
};

/**
 * Tests that the document file entries populated in Downloads, Drive and Play
 * Files folder recently will be displayed in Recent Document folder.
 */
testcase.recentDocumentsDownloadsAndDriveAndPlayFiles = async () => {
  await addPlayFileEntries();
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [RECENTLY_MODIFIED_DOCUMENT],
      [RECENTLY_MODIFIED_DOCUMENT]);
  await verifyRecentDocuments(appId, [
    RECENTLY_MODIFIED_DOCUMENT, RECENTLY_MODIFIED_DOCUMENT,
    RECENT_MODIFIED_ANDROID_DOCUMENT
  ]);
};

/**
 * Tests if an active filter button is clicked again, it will become inactive
 * and the "All" filter button will become active and focus.
 */
testcase.recentsFilterResetToAll = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await navigateToRecent(appId, RecentFilterType.AUDIO);
  // Clicks the active "Audio" filter button.
  await remoteCall.waitAndClickElement(
      appId, ['[file-type-filter="audio"].active']);
  // Verifies the "All" button is focus and all recent files are shown.
  await remoteCall.waitForElement(appId, ['[file-type-filter="all"].active']);
  const focusedElement =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq('all', focusedElement.attributes['file-type-filter']);
  await verifyCurrentEntries(appId, RECENT_ENTRY_SET);
};

/**
 * Tests when we switch the active filter button between All and others, the
 * correct a11y messages will be announced.
 */
testcase.recentsA11yMessages = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await navigateToRecent(appId, RecentFilterType.IMAGE);
  // Checks "images filter on" a11y message is announced.
  let a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(
      'Images filter is on.', a11yMessages[a11yMessages.length - 1]);

  // Clicks the "Videos" filter button to activate it.
  await remoteCall.waitAndClickElement(appId, ['[file-type-filter="video"]']);
  await remoteCall.waitForElement(appId, ['[file-type-filter="video"].active']);
  // Checks "video filter on" a11y message is announced.
  a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(
      'Images filter is off. Videos filter is on.',
      a11yMessages[a11yMessages.length - 1]);

  // Clicks the active "Videos" filter button again.
  await remoteCall.waitAndClickElement(
      appId, ['[file-type-filter="video"].active']);
  await remoteCall.waitForElement(appId, ['[file-type-filter="all"].active']);
  // Checks "filter reset" a11y message is announced.
  a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(
      'Videos filter is off. Filter is reset.',
      a11yMessages[a11yMessages.length - 1]);
};
