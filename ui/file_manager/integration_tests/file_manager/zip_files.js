// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchive file list entry.
 */
function getUnzippedFileListRowEntries() {
  return [
    ['image.png', '272 bytes', 'PNG image', 'Sep 2, 2013, 10:01 PM'],
    ['text.txt', '51 bytes', 'Plain text', 'Sep 2, 2013, 10:01 PM']
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveSJIS file list entry.
 */
function getUnzippedFileListRowEntriesSjisRoot() {
  return [
    // Folder name in Japanese language.
    ['新しいフォルダ', '--', 'Folder', 'Dec 31, 1980, 12:00 AM']
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveSJIS file list entry and moving into the subdirectory.
 */
function getUnzippedFileListRowEntriesSjisSubdir() {
  return [
    // ソ(SJIS:835C) contains backslash code on the 2nd byte. The app and the
    // extension should not confuse it with an escape characater.
    ['SJIS_835C_ソ.txt', '113 bytes', 'Plain text', 'Dec 31, 1980, 12:00 AM'],
    // Another file containing SJIS Japanese characters.
    [
      '新しいテキスト ドキュメント.txt', '52 bytes', 'Plain text',
      'Oct 2, 2001, 12:34 PM'
    ]
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveWithAbsolutePaths file list entry.
 */
function getUnzippedFileListRowEntriesAbsolutePathsRoot() {
  return [
    ['foo', '--', 'Folder', 'Oct 11, 2018, 9:44 AM'],
    ['hello.txt', '13 bytes', 'Plain text', 'Oct 11, 2018, 9:44 AM']
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveWithAbsolutePaths file list entry and moving into the
 * subdirectory.
 */
function getUnzippedFileListRowEntriesAbsolutePathsSubdir() {
  return [['bye.txt', '9 bytes', 'Plain text', 'Oct 11, 2018, 9:44 AM']];
}

/**
 * Tests zip file open (aka unzip) from Downloads.
 */
testcase.zipFileOpenDownloads = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Downloads containing a zip file.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.zipArchive], []);
    },
    // Select the zip file.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, ['archive.zip'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getUnzippedFileListRowEntries();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
  ]);
};

/**
 * Tests zip file, with absolute paths, open (aka unzip) from Downloads.
 */
testcase.zipFileOpenDownloadsWithAbsolutePaths = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Downloads containing a zip file.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next,
          [ENTRIES.zipArchiveWithAbsolutePaths], []);
    },
    // Select the zip file.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, ['absolute_paths.zip'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getUnzippedFileListRowEntriesAbsolutePathsRoot();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
    // Select the directory in the ZIP file.
    function(result) {
      remoteCall.callRemoteTestUtil('selectFile', appId, ['foo'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getUnzippedFileListRowEntriesAbsolutePathsSubdir();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
  ]);
};

/**
 * Tests zip file open (aka unzip) from Google Drive.
 */
testcase.zipFileOpenDrive = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive containing a zip file.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], [ENTRIES.zipArchive]);
    },
    // Select the zip file.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, ['archive.zip'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getUnzippedFileListRowEntries();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
  ]);
};

/**
 * Tests zip file open (aka unzip) from a removable USB volume.
 */
testcase.zipFileOpenUsb = function() {
  let appId;

  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], [ENTRIES.beautiful]);
    },
    // Mount empty USB volume in the Drive window.
    function(results) {
      appId = results.windowId;
      sendTestMessage({name: 'mountFakeUsbEmpty'}).then(this.next);
    },
    // Wait for the USB mount.
    function() {
      remoteCall.waitForElement(appId, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY], this.next);
    },
    // Add zip file to the USB volume.
    function() {
      addEntries(['usb'], [ENTRIES.zipArchive], this.next);
    },
    // Verify the USB file list.
    function() {
      const archive = [ENTRIES.zipArchive.getExpectedRow()];
      remoteCall.waitForFiles(appId, archive).then(this.next);
    },
    // Select the zip file.
    function() {
      remoteCall.callRemoteTestUtil('selectFile', appId, ['archive.zip'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getUnzippedFileListRowEntries();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
  ]);
};

/**
 * Returns the expected file list rows after invoking the 'Zip selection' menu
 * command on the ENTRIES.photos file list item.
 */
function getZipSelectionFileListRowEntries() {
  return [
    ['photos', '--', 'Folder', 'Jan 1, 1980, 11:59 PM'],
    ['photos.zip', '130 bytes', 'Zip archive', 'Oct 21, 1983, 11:55 AM']
  ];
}

/**
 * Tests creating a zip file on Downloads.
 */
testcase.zipCreateFileDownloads = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Downloads containing ENTRIES.photos.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.photos], []);
    },
    // Select the file.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, ['photos'])
          .then(this.next);
    },
    // Right-click the selected file.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]'], this.next);
    },
    // Check: the context menu should appear.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseRightClick failed');
      remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])')
          .then(this.next);
    },
    // Click the 'Zip selection' menu command.
    function() {
      const zip = '[command="#zip-selection"]';
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [zip], this.next);
    },
    // Check: a zip file should be created.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      const files = getZipSelectionFileListRowEntries();
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests creating a zip file on Drive.
 */
testcase.zipCreateFileDrive = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive containing ENTRIES.photos.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], [ENTRIES.photos]);
    },
    // Select the file.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, ['photos'])
          .then(this.next);
    },
    // Right-click the selected file.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]'], this.next);
    },
    // Check: the context menu should appear.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseRightClick failed');
      remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])')
          .then(this.next);
    },
    // Click the 'Zip selection' menu command.
    function() {
      const zip = '[command="#zip-selection"]';
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [zip], this.next);
    },
    // Check: a zip file should be created.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      const files = getZipSelectionFileListRowEntries();
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests creating a zip file on a removable USB volume.
 */
testcase.zipCreateFileUsb = function() {
  let appId;

  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], [ENTRIES.beautiful]);
    },
    // Mount empty USB volume in the Drive window.
    function(results) {
      appId = results.windowId;
      sendTestMessage({name: 'mountFakeUsbEmpty'}).then(this.next);
    },
    // Wait for the USB mount.
    function() {
      remoteCall.waitForElement(appId, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY], this.next);
    },
    // Add ENTRIES.photos to the USB volume.
    function() {
      addEntries(['usb'], [ENTRIES.photos], this.next);
    },
    // Verify the USB file list.
    function() {
      const photos = [ENTRIES.photos.getExpectedRow()];
      remoteCall.waitForFiles(appId, photos).then(this.next);
    },
    // Select the photos file list entry.
    function() {
      remoteCall.callRemoteTestUtil('selectFile', appId, ['photos'])
          .then(this.next);
    },
    // Right-click the selected file.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]'], this.next);
    },
    // Check: the context menu should appear.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseRightClick failed');
      remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])')
          .then(this.next);
    },
    // Click the 'Zip selection' menu command.
    function() {
      const zip = '[command="#zip-selection"]';
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [zip], this.next);
    },
    // Check: a zip file should be created.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      const files = getZipSelectionFileListRowEntries();
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests zip file open (aka unzip) from Downloads.
 * The file names are encoded in SJIS.
 */
testcase.zipFileOpenDownloadsShiftJIS = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Downloads containing a zip file.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.zipArchiveSJIS], []);
    },
    // Select the zip file.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, ['archive_sjis.zip'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getUnzippedFileListRowEntriesSjisRoot();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
    // Select the directory in the ZIP file.
    function(result) {
      remoteCall.callRemoteTestUtil('selectFile', appId, ['新しいフォルダ'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getUnzippedFileListRowEntriesSjisSubdir();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
  ]);
};
