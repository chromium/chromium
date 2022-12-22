// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, formatDate, getCaller, getDateWithDayDiff, pending, repeatUntil, RootPath, sanitizeDate, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {mountCrostini, navigateWithDirectoryTree, openNewWindow, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_CROSTINI_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, NESTED_ENTRY_SET, RECENT_ENTRY_SET} from './test_data.js';

// Mock files with recently modified dates, be aware the days passed in should
// be larger than 3 to prevent file list from showing "Today/Yesterday", which
// will break the waitForFiles() function.
// Test entry for a recently-modified video file.
const RECENTLY_MODIFIED_VIDEO =
    ENTRIES.world.cloneWithModifiedDate(getDateWithDayDiff(7));
const RECENTLY_MODIFIED_MOV_VIDEO =
    ENTRIES.movFile.cloneWithModifiedDate(getDateWithDayDiff(10));

// Test entry for a recently-modified document file.
const RECENTLY_MODIFIED_DOCUMENT =
    ENTRIES.docxFile.cloneWithModifiedDate(getDateWithDayDiff(12));

// Test entries for recent-modified android files.
const RECENT_MODIFIED_ANDROID_DOCUMENT =
    ENTRIES.documentsText.cloneWithModifiedDate(getDateWithDayDiff(15));
const RECENT_MODIFIED_ANDROID_IMAGE =
    ENTRIES.picturesImage.cloneWithModifiedDate(getDateWithDayDiff(20));
const RECENT_MODIFIED_ANDROID_AUDIO =
    ENTRIES.musicAudio.cloneWithModifiedDate(getDateWithDayDiff(21));
const RECENT_MODIFIED_ANDROID_VIDEO =
    ENTRIES.moviesVideo.cloneWithModifiedDate(getDateWithDayDiff(25));

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

  // Test that the delete button's visibility based on v2 flag.
  const deleteButton = await remoteCall.waitForElement(appId, '#delete-button');
  chrome.test.assertFalse(
      deleteButton.hidden, 'delete button should be visible');
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
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, expectedPath);
}

/**
 * Select a file and right click to show the context menu, then click the
 * specified context menu item.
 *
 * @param {string} appId Files app windowId.
 * @param {string} fileName Name of the file to right click.
 * @param {string} commandId The command id for the context menu item.
 */
async function rightClickContextMenu(appId, fileName, commandId) {
  // Select the item.
  await remoteCall.waitUntilSelected(appId, fileName);

  // Right-click the selected file.
  await remoteCall.waitAndRightClick(appId, '.table-row[selected]');

  // Click the context menu item with the command id.
  const contextMenuItem = '#file-context-menu:not([hidden]) ' +
      `[command="#${commandId}"]:not([hidden]):not([disabled])`;
  await remoteCall.waitAndClickElement(appId, contextMenuItem);
}

/**
 * Opens given file's containing folder by choosing "Go to file location"
 * context menu item.
 *
 * @param {string} appId Files app windowId.
 * @param {string} fileName Name of the file to open containing folder.
 */
async function goToFileLocation(appId, fileName) {
  await rightClickContextMenu(appId, fileName, 'go-to-file-location');
}

/**
 * Delete a given file by choosing "Delete" context menu item.
 *
 * @param {string} appId Files app windowId.
 * @param {string} fileName Name of the file to delete.
 */
async function deleteFile(appId, fileName) {
  await rightClickContextMenu(appId, fileName, 'delete');
  // Click "Delete" on the Delete confirm dialog.
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog button.cr-dialog-ok');
}

/**
 * Rename a given file by choosing "Rename" context menu item.
 *
 * @param {string} appId Files app windowId.
 * @param {string} fileName Name of the file to rename.
 * @param {string} newName The new file name.
 */
async function renameFile(appId, fileName, newName) {
  const textInput = '#file-list .table-row[renaming] input.rename';
  await rightClickContextMenu(appId, fileName, 'rename');
  // Wait for the rename input field.
  await remoteCall.waitForElement(appId, textInput);
  // Input the new name.
  await remoteCall.inputText(appId, textInput, newName);
  const inputElement = await remoteCall.waitForElement(appId, textInput);
  chrome.test.assertEq(newName, inputElement.value);
  // Press Enter to commit renaming.
  const keyDown = [textInput, 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, keyDown));
  // Wait until renaming is complete.
  const renamingItem = '#file-list .table-row[renaming]';
  await remoteCall.waitForElementLost(appId, renamingItem);
}

/**
 * Cut a given file by choosing "Cut" context menu item and paste the file
 * to the new folder.
 *
 * @param {string} appId Files app windowId.
 * @param {string} fileName Name of the file to cut.
 * @param {string} newFolder Full breadcrumb path for the new folder to paste.
 */
async function cutFileAndPasteTo(appId, fileName, newFolder) {
  await rightClickContextMenu(appId, fileName, 'cut');
  // Go to the new folder to paste.
  await navigateWithDirectoryTree(appId, newFolder);
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));
  // Wait for the operation to be completed.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);
    const expectedPrimaryText =
        `Moving ${fileName} to ${newFolder.split('/').pop()}`;
    const expectedSecondaryText = 'Complete';
    const actualPrimaryText = element.attributes['primary-text'];
    const actualSecondaryText = element.attributes['secondary-text'];

    if (expectedPrimaryText === actualPrimaryText &&
        expectedSecondaryText === actualSecondaryText) {
      return;
    }

    return pending(
        caller,
        `Expected feedback panel msg: "${expectedPrimaryText} - ${
            expectedSecondaryText}", got "${actualPrimaryText} - ${
            actualSecondaryText}"`);
  });
}

/**
 * Wait for the empty folder element to show and assert the content to
 * match the expected message.
 *
 * @param {string} appId Files app windowId.
 * @param {string} expectedMessage The expected empty folder message
 */
async function waitForEmptyFolderMessage(appId, expectedMessage) {
  const caller = getCaller();
  // Use repeatUntil() here because when we switch between different filters,
  // the message changes but the element itself will always show there.
  await repeatUntil(async () => {
    const emptyMessage = await remoteCall.waitForElement(
        appId, '#empty-folder:not(.hidden) > .label');
    if (emptyMessage.text === expectedMessage) {
      return;
    }

    return pending(
        caller,
        `Expected empty folder message: "${expectedMessage}", got "${
            emptyMessage.text}"`);
  });
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
    RECENT_MODIFIED_ANDROID_DOCUMENT,
    RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO,
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
    RECENT_MODIFIED_ANDROID_DOCUMENT,
    RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO,
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
    RECENTLY_MODIFIED_VIDEO,
    RECENTLY_MODIFIED_VIDEO,
    RECENT_MODIFIED_ANDROID_VIDEO,
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
      [RECENTLY_MODIFIED_DOCUMENT, RECENTLY_MODIFIED_VIDEO]);
  // RECENTLY_MODIFIED_DOCUMENT exists in both local and drive folder, the
  // file will appear twice in the result. RECENTLY_MODIFIED_VIDEO won't
  // be included because it's not a Document.
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
    RECENTLY_MODIFIED_DOCUMENT,
    RECENTLY_MODIFIED_DOCUMENT,
    RECENT_MODIFIED_ANDROID_DOCUMENT,
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

/**
 * Tests the read only flag on Recents view should be hidden.
 */
testcase.recentsReadOnlyHidden = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await navigateToRecent(appId);
  const readOnlyIndicator =
      await remoteCall.waitForElement(appId, ['#read-only-indicator']);
  chrome.test.assertTrue(
      readOnlyIndicator.hidden, 'Read only indicator should be hidden');
};

/**
 * Tests delete operation can be performed in Recents view on files from
 * Downloads, Drive and Play Files.
 */
testcase.recentsAllowDeletion = async () => {
  await addPlayFileEntries();
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], [ENTRIES.desktop]);
  await navigateToRecent(appId);
  const files = TestEntryInfo.getExpectedRows([
    ENTRIES.beautiful,
    ENTRIES.desktop,
    RECENT_MODIFIED_ANDROID_DOCUMENT,
    RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO,
  ]);
  await remoteCall.waitForFiles(appId, files);

  // Delete a file originated from Downloads.
  await deleteFile(appId, ENTRIES.beautiful.nameText);
  const files1 = TestEntryInfo.getExpectedRows([
    ENTRIES.desktop,
    RECENT_MODIFIED_ANDROID_DOCUMENT,
    RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO,
  ]);
  await remoteCall.waitForFiles(appId, files1);

  // Delete a file originated from Drive.
  await deleteFile(appId, ENTRIES.desktop.nameText);
  const files2 = TestEntryInfo.getExpectedRows([
    RECENT_MODIFIED_ANDROID_DOCUMENT,
    RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO,
  ]);
  await remoteCall.waitForFiles(appId, files2);

  // Delete a file originated from Play Files.
  await deleteFile(appId, RECENT_MODIFIED_ANDROID_IMAGE.nameText);
  const files3 = TestEntryInfo.getExpectedRows(
      [RECENT_MODIFIED_ANDROID_DOCUMENT, RECENT_MODIFIED_ANDROID_VIDEO]);
  await remoteCall.waitForFiles(appId, files3);
};

/**
 * Tests delete operation can be performed in Recents view with multiple files
 * from different sources including Downloads, Drive and Play Files.
 */
testcase.recentsAllowMultipleFilesDeletion = async () => {
  await addPlayFileEntries();
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], [ENTRIES.desktop]);
  await navigateToRecent(appId);
  const files = TestEntryInfo.getExpectedRows([
    ENTRIES.beautiful,
    ENTRIES.desktop,
    RECENT_MODIFIED_ANDROID_DOCUMENT,
    RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO,
  ]);
  await remoteCall.waitForFiles(appId, files);

  // Select all files from the gear menu.
  await remoteCall.waitAndClickElement(appId, '#gear-button');
  const selectAllMenu = '#gear-menu:not([hidden]) ' +
      `[command="#select-all"]:not([hidden]):not([disabled])`;
  await remoteCall.waitAndClickElement(appId, selectAllMenu);
  await remoteCall.waitForElement(appId, '.table-row[selected]');
  // Wait for the files selection label.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element =
        await remoteCall.waitForElement(appId, '#files-selected-label');
    const expectedLabel = '5 files selected';

    if (element.text === expectedLabel) {
      return;
    }

    return pending(
        caller,
        `Expected files selection label: "${expectedLabel}", got "${
            element.text}"`);
  });
  // Delete all selected files via action bar.
  await remoteCall.waitAndClickElement(appId, '#delete-button');
  // Click okay on the confirm dialog.
  await remoteCall.waitAndClickElement(
      appId, '.files-confirm-dialog button.cr-dialog-ok');

  // Check all files should be deleted.
  await remoteCall.waitForFiles(appId, []);
};

/**
 * Tests rename operation can be performed in Recents view on files from
 * Downloads, Drive.
 */
testcase.recentsAllowRename = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], [ENTRIES.desktop]);
  await navigateToRecent(appId);
  const files =
      TestEntryInfo.getExpectedRows([ENTRIES.beautiful, ENTRIES.desktop]);
  await remoteCall.waitForFiles(appId, files);

  // Rename a file originated from Downloads.
  const newBeautiful = ENTRIES.beautiful.cloneWithNewName('new-beautiful.ogg');
  await renameFile(appId, ENTRIES.beautiful.nameText, newBeautiful.nameText);
  const files1 = TestEntryInfo.getExpectedRows([
    newBeautiful,
    ENTRIES.desktop,
  ]);
  await remoteCall.waitForFiles(appId, files1);

  // Rename a file originated from Drive.
  const newDesktop = ENTRIES.desktop.cloneWithNewName('new-desktop.png');
  await renameFile(appId, ENTRIES.desktop.nameText, newDesktop.nameText);
  const files2 = TestEntryInfo.getExpectedRows([
    newDesktop,
    newBeautiful,
  ]);
  await remoteCall.waitForFiles(appId, files2);
};

/**
 * Tests rename operation is not allowed in Recents view for files from
 * Play files.
 */
testcase.recentsNoRenameForPlayFiles = async () => {
  await addPlayFileEntries();
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);
  await navigateToRecent(appId);
  const files = TestEntryInfo.getExpectedRows([
    ENTRIES.beautiful,
    RECENT_MODIFIED_ANDROID_DOCUMENT,
    RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO,
  ]);
  await remoteCall.waitForFiles(appId, files);

  // Select the item.
  await remoteCall.waitUntilSelected(
      appId, RECENT_MODIFIED_ANDROID_DOCUMENT.nameText);

  // Right-click the selected file.
  await remoteCall.waitAndRightClick(appId, '.table-row[selected]');

  // Checks the rename menu should be disabled.
  const renameMenu = '#file-context-menu:not([hidden]) ' +
      '[command="#rename"][disabled]:not([hidden])';
  await remoteCall.waitForElement(appId, renameMenu);
};

/**
 * Tests cut operation can be performed in Recents view on files from
 * Downloads.
 */
testcase.recentsAllowCutForDownloads = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.directoryA], []);
  const files = [ENTRIES.beautiful.getExpectedRow()];
  const newFolderBreadcrumb =
      `/My files/Downloads/${ENTRIES.directoryA.nameText}`;

  // Cut/Paste a file originated from Downloads.
  await navigateToRecent(appId);
  await remoteCall.waitForFiles(appId, files);
  await cutFileAndPasteTo(
      appId, ENTRIES.beautiful.nameText, newFolderBreadcrumb);
  // The file being cut should appear in the new directory.
  await remoteCall.waitForFiles(appId, files);
  // Recents view still have the full file list because the file being cut just
  // moves to a new directory, but it still belongs to Recent.
  await navigateToRecent(appId);
  await remoteCall.waitForFiles(appId, files);
  // Use "go to location" to validate the file in Recents after cut is
  // collected from the new folder.
  await goToFileLocation(appId, ENTRIES.beautiful.nameText);
  await remoteCall.waitForFiles(appId, files);
  await verifyBreadcrumbsPath(appId, newFolderBreadcrumb);
};

/**
 * Tests cut operation can be performed in Recents view on files from
 * Drive.
 */
testcase.recentsAllowCutForDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.directoryA], [ENTRIES.desktop]);
  const files = TestEntryInfo.getExpectedRows([ENTRIES.desktop]);
  const newFolderBreadcrumb =
      `/My files/Downloads/${ENTRIES.directoryA.nameText}`;

  // Cut/Paste a file originated from Drive.
  await navigateToRecent(appId);
  await remoteCall.waitForFiles(appId, files);
  await cutFileAndPasteTo(appId, ENTRIES.desktop.nameText, newFolderBreadcrumb);
  // The file being cut should appear in the new directory.
  await remoteCall.waitForFiles(appId, files);
  // Recents view still have the full file list because the file being cut just
  // moves to a new directory, but it still belongs to Recent.
  await navigateToRecent(appId);
  await remoteCall.waitForFiles(appId, files);
  // Use "go to location" to validate the file in Recents after cut is
  // collected from the new folder.
  await goToFileLocation(appId, ENTRIES.desktop.nameText);
  await remoteCall.waitForFiles(appId, files);
  await verifyBreadcrumbsPath(appId, newFolderBreadcrumb);
};

/**
 * Tests cut operation can be performed in Recents view on files from
 * Play Files.
 */
testcase.recentsAllowCutForPlayFiles = async () => {
  await addPlayFileEntries();
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.directoryA], []);
  const files = TestEntryInfo.getExpectedRows([
    RECENT_MODIFIED_ANDROID_DOCUMENT,
    RECENT_MODIFIED_ANDROID_IMAGE,
    RECENT_MODIFIED_ANDROID_VIDEO,
  ]);
  const newFolderBreadcrumb =
      `/My files/Downloads/${ENTRIES.directoryA.nameText}`;

  // Cut/Paste a file originated from Play Files.
  await navigateToRecent(appId);
  await remoteCall.waitForFiles(appId, files);
  await cutFileAndPasteTo(
      appId, RECENT_MODIFIED_ANDROID_IMAGE.nameText, newFolderBreadcrumb);
  // The file being cut should appear in the new directory.
  const filesInNewDir =
      TestEntryInfo.getExpectedRows([RECENT_MODIFIED_ANDROID_IMAGE]);
  await remoteCall.waitForFiles(appId, filesInNewDir);
  // Recents view still have the full file list because the file being cut just
  // moves to a new directory, but it still belongs to Recent.
  await navigateToRecent(appId);
  await remoteCall.waitForFiles(appId, files);
  // Use "go to location" to validate the file in Recents after cut is
  // collected from the new folder.
  await goToFileLocation(appId, RECENT_MODIFIED_ANDROID_IMAGE.nameText);
  await remoteCall.waitForFiles(appId, filesInNewDir);
  await verifyBreadcrumbsPath(appId, newFolderBreadcrumb);
};

/**
 * Tests the time-period group heading can be displayed in Recents.
 */
testcase.recentsTimePeriodHeadings = async () => {
  const todayFile = ENTRIES.hello.cloneWithModifiedDate(getDateWithDayDiff(0));
  const yesterdayFile =
      ENTRIES.desktop.cloneWithModifiedDate(getDateWithDayDiff(1));
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [todayFile, yesterdayFile], []);
  await navigateToRecent(appId);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([todayFile, yesterdayFile]), {
        // Ignore last modified time because it will show Today/Yesterday
        // instead of the actual date.
        ignoreLastModifiedTime: true,
      });
  // Check headings in list view mode.
  await remoteCall.waitForElementsCount(appId, ['.group-heading'], 2);
  const groupHeadings = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['.group-heading']);
  const fileItems = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['.group-heading + .table-row']);
  chrome.test.assertEq(2, fileItems.length);

  chrome.test.assertEq('Today', groupHeadings[0].text);
  chrome.test.assertEq(
      todayFile.nameText, fileItems[0].attributes['file-name']);
  chrome.test.assertEq('Yesterday', groupHeadings[1].text);
  chrome.test.assertEq(
      yesterdayFile.nameText, fileItems[1].attributes['file-name']);

  // Switch to grid view.
  await remoteCall.waitAndClickElement(appId, '#view-button');
  await remoteCall.waitForElementsCount(appId, ['.grid-title'], 2);
  // Check headings in grid view mode.
  const groupTitles = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['.grid-title']);
  const gridItems = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['.grid-title + .thumbnail-item']);
  chrome.test.assertEq(2, gridItems.length);

  chrome.test.assertEq('Today', groupTitles[0].text);
  chrome.test.assertEq(
      todayFile.nameText, gridItems[0].attributes['file-name']);
  chrome.test.assertEq('Yesterday', groupTitles[1].text);
  chrome.test.assertEq(
      yesterdayFile.nameText, gridItems[1].attributes['file-name']);
};

/**
 * Tests message will show in Recents for empty folder.
 */
testcase.recentsEmptyFolderMessage = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.directoryA], []);
  await navigateToRecent(appId);
  // All filter is on by default.
  await waitForEmptyFolderMessage(appId, 'No recent files');
  // Activates to audio filter.
  await remoteCall.waitAndClickElement(appId, [`[file-type-filter="audio"]`]);
  await waitForEmptyFolderMessage(appId, 'No recent audio files');
  // Activates to documents filter.
  await remoteCall.waitAndClickElement(
      appId, [`[file-type-filter="document"]`]);
  await waitForEmptyFolderMessage(appId, 'No recent documents');
  // Activates to images filter.
  await remoteCall.waitAndClickElement(appId, [`[file-type-filter="image"]`]);
  await waitForEmptyFolderMessage(appId, 'No recent images');
  // Activates to videos filter.
  await remoteCall.waitAndClickElement(appId, [`[file-type-filter="video"]`]);
  await waitForEmptyFolderMessage(appId, 'No recent videos');
};


/**
 * Tests message will show in Recents after the last file is
 * deleted.
 */
testcase.recentsEmptyFolderMessageAfterDeletion = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);
  await navigateToRecent(appId);
  const files = TestEntryInfo.getExpectedRows([ENTRIES.beautiful]);
  await remoteCall.waitForFiles(appId, files);
  await deleteFile(appId, ENTRIES.beautiful.nameText);
  await waitForEmptyFolderMessage(appId, 'No recent files');
};

/**
 * Construct a file with modified date as 1am today in a specific timezone.
 * @param {string} timezone the timezone string
 * @return {!TestEntryInfo}
 */
function prepareFileFor1AMToday(timezone) {
  const nowDate = new Date();
  nowDate.setHours(1, 0, 0, 0);
  // Format: "May 2, 2021, 11:25 AM GMT+1000"
  const modifiedDate = sanitizeDate(nowDate.toLocaleString('default', {
    month: 'short',
    day: 'numeric',
    year: 'numeric',
    hour12: true,
    hour: 'numeric',
    minute: 'numeric',
    timeZone: timezone,
    timeZoneName: 'longOffset',
  }));
  return ENTRIES.beautiful.cloneWithModifiedDate(modifiedDate);
}

/**
 * Tests the group heading and modified date column in the list view will
 * change once the timezone changes.
 */
testcase.recentsRespondToTimezoneChangeForListView = async () => {
  // Set timezone to Brisbane (GMT+10).
  await sendTestMessage({name: 'setTimezone', timezone: 'Australia/Brisbane'});
  const testFile = prepareFileFor1AMToday('Australia/Brisbane');
  const isEarlierThan2AM = (new Date()).getHours() < 2;

  // Open Files app and go to Recent tab.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [testFile], []);
  await navigateToRecent(appId);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([testFile]), {
        // Ignore last modified time because it will show Today/Yesterday
        // instead of the actual date.
        ignoreLastModifiedTime: true,
      });
  // Check date modified column.
  const filesBefore =
      await remoteCall.callRemoteTestUtil('getFileList', appId, []);
  chrome.test.assertEq(filesBefore[0][3], 'Today 1:00 AM');
  // Check group heading.
  const groupHeadingBefore =
      await remoteCall.waitForElement(appId, ['.group-heading']);
  chrome.test.assertEq('Today', groupHeadingBefore.text);

  // Set timezone to Perth (GMT+8).
  await sendTestMessage({name: 'setTimezone', timezone: 'Australia/Perth'});

  // If the OS time before timezone change is earlier than 2am, then after
  // timezone change the current date will move to a day before, the OS and
  // date and modification date changes at the same time, so it will be "today".
  // For example:
  // before timezone change: os time 1:30am file modification time: today 1am
  // move by -2 hours: os time 11:30pm file modification time: today 11pm
  const targetDate = isEarlierThan2AM ? 'Today' : 'Yesterday';
  const targetTime = '11:00 PM';

  // Check date modified column.
  const caller = getCaller();
  await repeatUntil(async () => {
    const filesAfter =
        await remoteCall.callRemoteTestUtil('getFileList', appId, []);
    // We need to assert the exact time here, so the timezones before/after
    // should not involve daylight savings.
    if (filesAfter[0][3] === `${targetDate} ${targetTime}`) {
      return;
    }

    return pending(
        caller,
        `Expected modified date to be "${targetDate} ${targetTime}", got "${
            filesAfter[0][3]}"`);
  });

  // Check group heading.
  const groupHeadingAfter =
      await remoteCall.waitForElement(appId, ['.group-heading']);
  chrome.test.assertEq(targetDate, groupHeadingAfter.text);
};


/**
 * Tests the group heading in the grid view will change once the
 * timezone changes.
 */
testcase.recentsRespondToTimezoneChangeForGridView = async () => {
  // Set timezone to Brisbane (GMT+10).
  await sendTestMessage({name: 'setTimezone', timezone: 'Australia/Brisbane'});
  const testFile = prepareFileFor1AMToday('Australia/Brisbane');
  const isEarlierThan2AM = (new Date()).getHours() < 2;

  // Open Files app and go to Recent tab.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [testFile], []);
  await navigateToRecent(appId);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([testFile]), {
        // Ignore last modified time because it will show Today/Yesterday
        // instead of the actual date.
        ignoreLastModifiedTime: true,
      });
  // Switch to grid view.
  await remoteCall.waitAndClickElement(appId, '#view-button');
  // Check group heading.
  const groupHeadingBefore =
      await remoteCall.waitForElement(appId, ['.grid-title']);
  chrome.test.assertEq('Today', groupHeadingBefore.text);

  // Set timezone to Perth (GMT+8).
  await sendTestMessage({name: 'setTimezone', timezone: 'Australia/Perth'});

  const targetDate = isEarlierThan2AM ? 'Today' : 'Yesterday';

  // Check group heading.
  const caller = getCaller();
  await repeatUntil(async () => {
    const groupHeadingAfter =
        await remoteCall.waitForElement(appId, ['.grid-title']);
    if (groupHeadingAfter.text === targetDate) {
      return;
    }

    return pending(
        caller,
        `Expected group heading to be "${targetDate}", got "${
            groupHeadingAfter.text}"`);
  });
};


/**
 * Tests the search term will be respected when switching between different
 * filter buttons.
 */
testcase.recentsRespectSearchWhenSwitchingFilter = async () => {
  // tall.txt
  const txtFile1 =
      ENTRIES.tallText.cloneWithModifiedDate(getDateWithDayDiff(4));
  // utf8.txt
  const txtFile2 =
      ENTRIES.utf8Text.cloneWithModifiedDate(getDateWithDayDiff(5));
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, txtFile1, txtFile2], []);
  // Before search, 3 files shows in the Recent tab.
  await navigateToRecent(appId);
  const files =
      TestEntryInfo.getExpectedRows([ENTRIES.beautiful, txtFile1, txtFile2]);
  await remoteCall.waitForFiles(appId, files);

  // Search term "tall".
  await remoteCall.waitAndClickElement(appId, '#search-button');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box [type="search"]', 'focus']));
  await remoteCall.inputText(appId, '#search-box [type="search"]', 'tall');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box [type="search"]', 'input']));

  // Check only tall.txt should show.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([txtFile1]));

  // Switch to "Document" filter.
  await navigateToRecent(appId, RecentFilterType.DOCUMENT);

  // Check there is still only tall.txt in the file list (no utf8.txt).
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([txtFile1]));
};
