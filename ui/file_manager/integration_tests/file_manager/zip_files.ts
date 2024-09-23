// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, expectHistogramTotalCount, getCaller, pending, repeatUntil, RootPath, sendTestMessage} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_ZIP_ENTRY_SET, COMPLEX_ZIP_ENTRY_SET} from './test_data.js';

/** The name of the UMA to track the zip creation time. */
const ZipCreationTimeHistogramName = 'FileBrowser.ZipTask.Time';

/** The name of the UMA to track extract archive status. */
const ExtractArchiveStatusHistogramName = 'FileBrowser.ExtractTask.Status';

/**
 * Returns the expected file list row entries after opening (mounting) the
 * ENTRIES.zipArchive file list entry.
 */
function getUnzippedFileListRowEntries() {
  return [
    [
      'SUCCESSFULLY_PERFORMED_FAKE_MOUNT.txt',
      '21 bytes',
      'Plain text',
      'Dec 31, 1980, 12:00 AM',
    ],
  ];
}

/**
 * Tests ZIP mounting from Downloads.
 */
export async function zipFileOpenDownloads() {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch',
  });

  // Open Files app on Downloads containing a zip file.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchive], []);

  // Select the zip file.
  await remoteCall.waitUntilSelected(appId, ENTRIES.zipArchive.nameText);

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {'ignoreLastModifiedTime': true});
}

/**
 * Tests that Files app's ZIP mounting notifies FileTasks when mounted.
 */
export async function zipNotifyFileTasks() {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch',
  });

  // Open Files app on Downloads containing a zip file.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchive], []);

  // Open the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('openFile', appId, ['archive.zip']),
      'openFile failed');

  // Wait for the zip archive to mount.
  await remoteCall.waitForElement(appId, `[scan-completed="archive.zip"]`);
}

/**
 * Tests ZIP mounting from Google Drive.
 */
export async function zipFileOpenDrive() {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch',
  });

  // Open Files app on Drive containing a zip file.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.zipArchive]);

  // Select the zip file.
  await remoteCall.waitUntilSelected(appId, ENTRIES.zipArchive.nameText);

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {'ignoreLastModifiedTime': true});
}

/**
 * Tests ZIP mounting from a removable USB volume.
 */
export async function zipFileOpenUsb() {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch',
  });

  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.beautiful]);

  // Mount empty USB volume in the Drive window.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB mount and click to open the USB volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('removable');

  // Add zip file to the USB volume.
  await addEntries(['usb'], [ENTRIES.zipArchive]);

  // Verify the USB file list.
  const archive = [ENTRIES.zipArchive.getExpectedRow()];
  await remoteCall.waitForFiles(appId, archive);

  // Select the zip file.
  await remoteCall.waitUntilSelected(appId, ENTRIES.zipArchive.nameText);

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {'ignoreLastModifiedTime': true});
}

/**
 * Returns the expected file list rows after invoking the 'Zip selection' menu
 * command on the ENTRIES.photos file list item.
 */
function getZipSelectionFileListRowEntries() {
  return [
    ['photos', '--', 'Folder', 'Jan 1, 1980, 11:59 PM'],
    ['photos.zip', '134 bytes', 'ZIP archive', 'Oct 21, 1983, 11:55 AM'],
  ];
}

/**
 * Tests creating a ZIP file on Downloads.
 */
export async function zipCreateFileDownloads() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, ENTRIES.photos.nameText);

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

  // Check: a zip time histogram value should have been recorded.
  await expectHistogramTotalCount(ZipCreationTimeHistogramName, 1);
}

/**
 * Tests creating a ZIP file on Drive.
 */
export async function zipCreateFileDrive() {
  // Open Files app on Drive containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.photos]);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, ENTRIES.photos.nameText);

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

  // Check: a zip time histogram value should have been recorded.
  await expectHistogramTotalCount(ZipCreationTimeHistogramName, 1);
}

/**
 * Tests creating a ZIP file containing an Office file on Drive.
 */
export async function zipCreateFileDriveOffice() {
  // Open Files app on Drive containing ENTRIES.photos and ENTRIES.docxFile.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.photos, ENTRIES.docxFile]);

  // Select the files.
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${ENTRIES.photos.nameText}"]`);
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${ENTRIES.docxFile.nameText}"]`,
      {shift: true});

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
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="Archive.zip"]');

  // Check: a zip time histogram value should have been recorded.
  await expectHistogramTotalCount(ZipCreationTimeHistogramName, 1);
}

/**
 * Tests that creating a ZIP file containing an encrypted file is disabled.
 */
export async function zipDoesntCreateFileEncrypted() {
  // Open Files app on Drive containing a test CSE file.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.testCSEFile]);

  // Select the file.
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${ENTRIES.testCSEFile.nameText}"]`);

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Get the zip menu item.
  const element =
      await remoteCall.waitForElement(appId, '[command="#zip-selection"]');

  chrome.test.assertEq('disabled', element.attributes['disabled']);
}

/**
 * Tests creating a ZIP file on a removable USB volume.
 */
export async function zipCreateFileUsb() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.beautiful]);

  // Mount empty USB volume in the Drive window.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB mount and click to open the USB volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('removable');

  // Add ENTRIES.photos to the USB volume.
  await addEntries(['usb'], [ENTRIES.photos]);

  // Verify the USB file list.
  const photos = [ENTRIES.photos.getExpectedRow()];
  await remoteCall.waitForFiles(appId, photos);

  // Select the photos file list entry.
  await remoteCall.waitUntilSelected(appId, ENTRIES.photos.nameText);

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

  // Check: a zip time histogram value should have been recorded.
  await expectHistogramTotalCount(ZipCreationTimeHistogramName, 1);
}

/**
 * Tests that extraction of a ZIP archive produces a feedback panel.
 */
export async function zipExtractShowPanel() {
  const entry = ENTRIES.zipArchive;
  const targetDirectoryName = entry.nameText.split('.')[0];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

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

  // Resolves when the new app window opens.
  const waitForWindowPromise = remoteCall.waitForWindow();

  // Click the 'Extract all' menu command.
  const extract = '[command="#extract-all"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [extract]),
      'fakeMouseClick failed');

  // Check: The new window has navigated to the unzipped folder.
  const newAppId = await waitForWindowPromise;
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      newAppId, '/My files/Downloads/' + targetDirectoryName);

  // Check that the error appears in the feedback panel.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);
    const expectedMsg = `Extracting ${entry.nameText} to Downloads`;
    const actualMsg = element.attributes['primary-text'];

    if (actualMsg === expectedMsg) {
      return;
    }

    return pending(
        caller,
        `Expected feedback panel msg: "${expectedMsg}", got "${actualMsg}"`);
  });

  // Check: a extract archive status histogram value should have been recorded.
  await expectHistogramTotalCount(ExtractArchiveStatusHistogramName, 1);
}

/**
 * Tests that extraction of a multiple ZIP archives produces the correct
 * feedback panel string.
 */
export async function zipExtractShowMultiPanel() {
  const entries = COMPLEX_ZIP_ENTRY_SET;

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Select two ZIP files.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="archive.zip"]');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="sjis.zip"]', {shift: true});

  // Right-click the selection.
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

  // We don't verify that two new Files app windows have opened because there is
  // a race setting appId when opening multiple windows at the same time.

  // Check that the error appears in the feedback panel.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);
    const expectedMsg = `Extracting 2 items…`;
    const actualMsg = element.attributes['primary-text'];

    if (actualMsg === expectedMsg) {
      return;
    }

    return pending(
        caller,
        `Expected feedback panel msg: "${expectedMsg}", got "${actualMsg}"`);
  });

  // Check: a extract archive status histogram value should have been recorded.
  await expectHistogramTotalCount(ExtractArchiveStatusHistogramName, 1);
}

/**
 * Tests that various selections enable/hide the correct menu items.
 */
export async function zipExtractSelectionMenus() {
  const entries = BASIC_ZIP_ENTRY_SET;

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Select the first file (ENTRIES.hello).
  await remoteCall.waitUntilSelected(appId, entries[0]!.nameText);

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
  await remoteCall.waitUntilSelected(appId, entries[2]!.nameText);

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
}

/**
 * Tests that extraction of a ZIP archive generates correct output files.
 */
export async function zipExtractCheckContent() {
  const entry = ENTRIES.zipArchive;
  const targetDirectoryName = entry.nameText.split('.')[0];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Resolves when the new app window opens.
  const waitForWindowPromise = remoteCall.waitForWindow();

  // Click the 'Extract all' menu command.
  const extract = '[command="#extract-all"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [extract]),
      'fakeMouseClick failed');

  // Check: The new window has navigated to the unzipped folder.
  const newAppId = await waitForWindowPromise;
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      newAppId, '/My files/Downloads/' + targetDirectoryName);

  const directoryQuery = '#file-list [file-name="' + targetDirectoryName + '"]';
  // Check: the extract directory should appear.
  await remoteCall.waitForElement(appId, directoryQuery);

  // Double click the created directory to open it.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseDoubleClick', appId, [directoryQuery]),
      'fakeMouseDoubleClick failed');

  // Check: File content in the ZIP should appear.
  await remoteCall.waitForElement(appId, '#file-list [file-name="folder"]');
  await remoteCall.waitForElement(appId, '#file-list [file-name="text.txt"]');
  await remoteCall.waitForElement(appId, '#file-list [file-name="image.png"]');

  // Check: a extract archive status histogram value should have been recorded.
  await expectHistogramTotalCount(ExtractArchiveStatusHistogramName, 1);
}

/**
 * Tests that repeated extraction of a ZIP archive generates extra directories.
 */
export async function zipExtractCheckDuplicates() {
  const entry = ENTRIES.zipArchive;
  const directory = entry.nameText.split('.')[0];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Resolves when the new app window opens.
  const waitForWindowPromise = remoteCall.waitForWindow();

  // Click the 'Extract all' menu command.
  const extract = '[command="#extract-all"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [extract]),
      'fakeMouseClick failed');

  // Check: The new window has navigated to the unzipped folder.
  const newAppId = await waitForWindowPromise;
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      newAppId, '/My files/Downloads/' + directory);

  let directoryQuery = '#file-list [file-name="' + directory + '"]';
  // Check: the extract directory should appear.
  await remoteCall.waitForElement(appId, directoryQuery);

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Resolves when the second new app window opens.
  const waitForWindowPromise2 = remoteCall.waitForWindow();

  // Click the 'Extract all' menu command.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [extract]),
      'fakeMouseClick failed');

  // Check: The second new window has navigated to the second unzipped folder.
  const newAppId2 = await waitForWindowPromise2;
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      newAppId2, '/My files/Downloads/' + directory + ' (1)');

  directoryQuery = '#file-list [file-name="' + directory + ' (1)"]';
  // Check: the duplicate named extract directory should appear.
  await remoteCall.waitForElement(appId, directoryQuery);

  // Double click the duplicate created directory to open it.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseDoubleClick', appId, [directoryQuery]),
      'fakeMouseDoubleClick failed');

  // Check: File content in the ZIP should appear.
  await remoteCall.waitForElement(appId, '#file-list [file-name="folder"]');
  await remoteCall.waitForElement(appId, '#file-list [file-name="text.txt"]');
  await remoteCall.waitForElement(appId, '#file-list [file-name="image.png"]');

  // Check: 2 extract archive status histogram value should have been recorded.
  await expectHistogramTotalCount(ExtractArchiveStatusHistogramName, 2);
}

/**
 * Tests extraction of a ZIP archive can detect and unpack filename encodings.
 */
export async function zipExtractCheckEncodings() {
  const entry = ENTRIES.zipSJISArchive;
  const targetDirectoryName = entry.nameText.split('.')[0];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Resolves when the new app window opens.
  const waitForWindowPromise = remoteCall.waitForWindow();

  // Click the 'Extract all' menu command.
  const extract = '[command="#extract-all"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [extract]),
      'fakeMouseClick failed');

  // Check: The new window has navigated to the unzipped folder.
  const newAppId = await waitForWindowPromise;
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      newAppId, '/My files/Downloads/' + targetDirectoryName);

  const directoryQuery = '#file-list [file-name="' + targetDirectoryName + '"]';
  // Check: the extract directory should appear.
  await remoteCall.waitForElement(appId, directoryQuery);

  // Double click the created directory to open it.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseDoubleClick', appId, [directoryQuery]),
      'fakeMouseDoubleClick failed');

  // Check: File content in the ZIP with decoded name should appear.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="新しいフォルダ"]');

  // Check: a extract archive status histogram value should have been recorded.
  await expectHistogramTotalCount(ExtractArchiveStatusHistogramName, 1);
}

/**
 * Tests extract option menu item has proper a11y labels.
 */
export async function zipExtractA11y() {
  const entry = ENTRIES.zipArchive;

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check: Extract all menu item is properly labelled for ARIA.
  // NB: It's sufficient to check the ARIA role attribute is set correctly.
  await remoteCall.waitForElement(
      appId, '[command="#extract-all"][role="menuitem"]');
}

/**
 * Tests extraction of a ZIP archive fails if there's not enough disk space.
 */
export async function zipExtractNotEnoughSpace() {
  const entry = ENTRIES.zipExtArchive;  // 120TB fake archive.

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Click the 'Extract all' menu command.
  const extract = '[command="#extract-all"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [extract]),
      'fakeMouseClick failed');

  // Check: Error panel appears.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);
    const expectedMsg = 'Extract operation failed. There is not enough space.';
    const actualMsg = element.attributes['primary-text'];

    if (actualMsg === expectedMsg) {
      return;
    }

    return pending(
        caller,
        `Expected feedback panel msg: "${expectedMsg}", got "${actualMsg}"`);
  });

  // Check: a extract archive status histogram value should have been recorded.
  await expectHistogramTotalCount(ExtractArchiveStatusHistogramName, 1);
}

/**
 * Tests that extraction of a ZIP archive from a read only volume succeeds.
 */
export async function zipExtractFromReadOnly() {
  const entry = ENTRIES.readOnlyZipFile;
  const targetDirectoryName = entry.nameText.split('.')[0];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [entry]);

  // Navigate to Shared with me.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Shared with me');

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Make sure read-only indicator on toolbar is visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator:not([hidden])');

  // Make sure the file we are about to select is present.
  await remoteCall.waitForFiles(appId, [entry.getExpectedRow()]);

  // Select the ZIP file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Resolves when the new app window opens.
  const waitForWindowPromise = remoteCall.waitForWindow();

  // Click the 'Extract all' menu command.
  const extract = '[command="#extract-all"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [extract]),
      'fakeMouseClick failed');

  // Check: The new window has navigated to the unzipped folder.
  const newAppId = await waitForWindowPromise;
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      newAppId, '/My files/' + targetDirectoryName);

  // Navigate to My Files.
  await directoryTree.navigateToPath('/My files');

  const directoryQuery = '#file-list [file-name="' + targetDirectoryName + '"]';
  // Check: the extract directory should appear.
  await remoteCall.waitForElement(appId, directoryQuery);

  // Double click the created directory to open it.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseDoubleClick', appId, [directoryQuery]),
      'fakeMouseDoubleClick failed');

  // Check: File content in the ZIP should appear.
  await remoteCall.waitForElement(appId, '#file-list [file-name="folder"]');
  await remoteCall.waitForElement(appId, '#file-list [file-name="text.txt"]');
  await remoteCall.waitForElement(appId, '#file-list [file-name="image.png"]');
}

/**
 * Tests ZIP mounting can be closed from the "Close" context menu item.
 */
export async function zipCloseFromContextMenu() {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.zipArchive.targetPath],
    openType: 'launch',
  });

  // Open Files app on Downloads containing a zip file.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.zipArchive], []);

  // Select the zip file.
  await remoteCall.waitUntilSelected(appId, ENTRIES.zipArchive.nameText);

  // Press the Enter key.
  await remoteCall.fakeKeyDown(
      appId, '#file-list', 'Enter', false, false, false);

  // Check: the zip mount should show on the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel(ENTRIES.zipArchive.nameText);

  // Trigger context menu event by long pressing.
  await directoryTree.longPressItemByLabel(ENTRIES.zipArchive.nameText);

  // Wait and click "Close" context menu.
  await remoteCall.waitForElement(appId, '#roots-context-menu:not([hidden])');
  await remoteCall.waitAndClickElement(
      appId, '[command="#unmount"]:not([hidden]):not([disabled])');

  // Check: the zip mount should disappear from the directory tree.
  await directoryTree.waitForItemLostByLabel(ENTRIES.zipArchive.nameText);
}
