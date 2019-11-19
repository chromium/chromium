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
    ['folder', '--', 'Folder', 'Dec 11, 2018, 5:08 PM'],
    ['image.png', '272 bytes', 'PNG image', 'Sep 2, 2013, 11:01 PM'],
    ['text.txt', '51 bytes', 'Plain text', 'Sep 2, 2013, 11:01 PM']
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveEncrypted file list entry.
 */
function getUnzippedFileListRowEntriesEncrypted() {
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
 * ENTRIES.zipArchiveMacOs file list entry.
 */
function getUnzippedFileListRowEntriesMacOsRoot() {
  return [
    // File name in non-ASCII (UTF-8) characters.
    ['ファイル.dat', '16 bytes', 'DAT file', 'Jul 8, 2001, 12:34 PM']
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
  await remoteCall.waitForFiles(appId, files);
};

/**
 * Tests zip file, with absolute paths, open (aka unzip) from Downloads.
 */
testcase.zipFileOpenDownloadsWithAbsolutePaths = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchiveWithAbsolutePaths.targetPath],
    openType: 'launch'
  });

  // Open Files app on Downloads containing a zip file.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchiveWithAbsolutePaths], []);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['absolute_paths.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntriesAbsolutePathsRoot();
  await remoteCall.waitForFiles(appId, files);

  // Select the directory in the ZIP file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['foo']),
      'selectFile failed');

  // Press the Enter key.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files2 = getUnzippedFileListRowEntriesAbsolutePathsSubdir();
  await remoteCall.waitForFiles(appId, files2);
};

/**
 * Tests encrypted zip file open, and canceling the passphrase dialog.
 */
testcase.zipFileOpenDownloadsEncryptedCancelPassphrase = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchiveEncrypted.targetPath],
    openType: 'launch'
  });

  const zipArchiverAppId = 'dmboannefpncccogfdikhmhpmdnddgoe';
  const zipArchiverPassphraseDialogUrl =
      'chrome-extension://dmboannefpncccogfdikhmhpmdnddgoe/html/passphrase.html';

  const passphraseCloseScript = `
      function clickClose() {
        HTMLImports.whenReady(() => {
          let dialog = document.querySelector("passphrase-dialog");
          dialog.shadowRoot.querySelector("#cancelButton").click();
        });
      }
      if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", clickClose);
      } else {
        clickClose();
      }
      `;
  const cancelPassphraseDialog = windowId => {
    return sendTestMessage({
      'name': 'runJsInAppWindow',
      'windowId': windowId,
      'script': passphraseCloseScript
    });
  };

  let passphraseCloseCount = 0;
  const waitForAllPassphraseWindowsClosed = () => {
    const caller = getCaller();

    const passphraseWindowCountCommand = {
      'name': 'countAppWindows',
      'appId': zipArchiverAppId
    };

    const getPassphraseWindowIdCommand = {
      'name': 'getAppWindowId',
      'windowUrl': zipArchiverPassphraseDialogUrl
    };

    let lastWindowId;
    return repeatUntil(async () => {
      const windowCount = await sendTestMessage(passphraseWindowCountCommand);
      if (windowCount == 0) {
        lastWindowId = 'none';
        return true;
      }

      const windowId = await sendTestMessage(getPassphraseWindowIdCommand);
      if (windowId == 'none') {
        lastWindowId = 'none';
        return true;
      }

      // Track the last window id to ensure that only one attempt is made to
      // cancel a passphrase dialog.
      if (windowId != lastWindowId) {
        await cancelPassphraseDialog(windowId);
        passphraseCloseCount++;
      }
      lastWindowId = windowId;
      return pending(caller, 'waitForAllPassphraseWindowsClosed');
    });
  };

  // Open Files app on Downloads containing a zip file.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchiveEncrypted], []);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['encrypted.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntriesEncrypted();
  await remoteCall.waitForFiles(appId, files, {'ignoreLastModifiedTime': true});

  const selectAndOpenFile = async () => {
    // Select the text file in the ZIP file.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'selectFile', appId, ['text.txt']),
        'selectFile failed');

    // Press the Enter key.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
        'fakeKeyDown failed');
  };
  selectAndOpenFile();

  // Wait for the external passphrase dialog window to appear.
  await waitForAppWindow(zipArchiverPassphraseDialogUrl);

  // Close the dialog by pressing the 'Cancel' button. Repeat for any new
  // dialogs that pop up.
  chrome.test.assertTrue(
      !!await waitForAllPassphraseWindowsClosed(),
      'waitForAllPassphraseWindowsClosed failed');

  for (let i = 0; i < 2; i++) {
    selectAndOpenFile();

    // Wait for a bit to see if any windows show up. One might appear on the
    // second attempt to open a file, but given interactions with other
    // components, we can't be sure.
    await wait(500);

    // Close any dialogs that show up by pressing the 'Cancel' button.
    chrome.test.assertTrue(
        !!await waitForAllPassphraseWindowsClosed(),
        'waitForAllPassphraseWindowsClosed failed');
  }

  chrome.test.assertTrue(
      passphraseCloseCount <= 2, 'passphrase window shown too many times');

  // Check: the zip file content should still be shown.
  const files2 = getUnzippedFileListRowEntriesEncrypted();
  await remoteCall.waitForFiles(appId, files, {'ignoreLastModifiedTime': true});
};

/**
 * Tests zip file open (aka unzip) from Google Drive.
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
  await remoteCall.waitForFiles(appId, files);
};

/**
 * Tests zip file open (aka unzip) from a removable USB volume.
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
  await remoteCall.waitForFiles(appId, files);
};

/**
 * Returns the expected file list rows after invoking the 'Zip selection' menu
 * command on the ENTRIES.photos file list item.
 */
function getZipSelectionFileListRowEntries() {
  return [
    ['photos', '--', 'Folder', 'Jan 1, 1980, 11:59 PM'],
    ['photos.zip', '214 bytes', 'Zip archive', 'Oct 21, 1983, 11:55 AM']
  ];
}

/**
 * Tests creating a zip file on Downloads.
 */
testcase.zipCreateFileDownloads = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.photos.targetPath],
    openType: 'launch'
  });

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
 * Tests creating a zip file on Drive.
 */
testcase.zipCreateFileDrive = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.photos.targetPath],
    openType: 'launch'
  });

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
 * Tests creating a zip file on a removable USB volume.
 */
testcase.zipCreateFileUsb = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.photos.targetPath],
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
 * Tests zip file open (aka unzip) from Downloads.
 * The file names are encoded in SJIS.
 */
testcase.zipFileOpenDownloadsShiftJIS = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchiveSJIS.targetPath],
    openType: 'launch'
  });

  // Open Files app on Downloads containing a zip file.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchiveSJIS], []);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive_sjis.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntriesSjisRoot();
  await remoteCall.waitForFiles(appId, files);

  // Select the directory in the ZIP file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['新しいフォルダ']),
      'selectFile failed');

  // Press the Enter key.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files2 = getUnzippedFileListRowEntriesSjisSubdir();
  await remoteCall.waitForFiles(appId, files2);
};

/**
 * Tests zip file open (aka unzip) from Downloads. The file name in the archive
 * is encoded in UTF-8, but the language encoding flag bit is set to 0.
 */
testcase.zipFileOpenDownloadsMacOs = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchiveMacOs.targetPath],
    openType: 'launch'
  });

  // Open Files app on Downloads containing a zip file.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchiveMacOs], []);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive_macos.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntriesMacOsRoot();
  await remoteCall.waitForFiles(appId, files);
};
