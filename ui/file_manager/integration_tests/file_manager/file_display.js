// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Checks if the files initially added by the C++ side are displayed, and
 * that files subsequently added are also displayed.
 *
 * @param {string} path Path to be tested, Downloads or Drive.
 * @param {Array<TestEntryInfo>} defaultEntries Default file entries.
 */
async function fileDisplay(path, defaultEntries) {
  const defaultList = TestEntryInfo.getExpectedRows(defaultEntries).sort();

  // Open Files app on the given |path| with default file entries.
  const appId = await setupAndWaitUntilReady(path);

  // Verify the default file list is present in |result|.
  await remoteCall.waitForFiles(appId, defaultList);

  // Add new file entries.
  await addEntries(['local', 'drive'], [ENTRIES.newlyAdded]);

  // Verify the newly added entries appear in the file list.
  const expectedList =
      defaultList.concat([ENTRIES.newlyAdded.getExpectedRow()]);
  await remoteCall.waitForFiles(appId, expectedList);
}

/**
 * Tests files display in Downloads.
 */
testcase.fileDisplayDownloads = () => {
  return fileDisplay(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests opening the files app navigating to a local folder. Uses
 * platform_util::OpenItem, a call to an API distinct from the one commonly used
 * in other tests for the same operation.
 */
testcase.fileDisplayLaunchOnLocalFolder = async () => {
  // Add a file to Downloads.
  await addEntries(['local'], [ENTRIES.photos]);

  // Open Files app on the Downloads directory.
  await sendTestMessage(
      {name: 'launchAppOnLocalFolder', localPath: 'Downloads/photos'});

  // Wait for app window to open.
  const appId = await remoteCall.waitForWindow('files#');

  // Check: the Downloads/photos folder should be selected.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="photos"][selected]');

  // The API used to launch the Files app does not set the IN_TEST flag to true:
  // error when attempting to retrieve Web Store access token.
  return IGNORE_APP_ERRORS;
};

/**
 * Tests opening the files app navigating to the My Drive folder. Uses
 * platform_util::OpenItem, a call to an API distinct from the one commonly used
 * in other tests for the same operation.
 */
testcase.fileDisplayLaunchOnDrive = async () => {
  // Open Files app on the Drive directory.
  await sendTestMessage({name: 'launchAppOnDrive'});

  // Wait for app window to open.
  const appId = await remoteCall.waitForWindow('files#');

  // Check: the app should be open on My Drive.
  await remoteCall.waitForElement(
      appId, '#directory-tree .tree-item[selected] [volume-type-icon="drive"]');

  // The API used to launch the Files app does not set the IN_TEST flag to true:
  // error when attempting to retrieve Web Store access token.
  return IGNORE_APP_ERRORS;
};

/**
 * Tests files display in Google Drive.
 */
testcase.fileDisplayDrive = () => {
  return fileDisplay(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET);
};

/**
 * Tests file display rendering in offline Google Drive.
 */
testcase.fileDisplayDriveOffline = async () => {
  const driveFiles =
      [ENTRIES.hello, ENTRIES.pinned, ENTRIES.photos, ENTRIES.testDocument];

  // Open Files app on Drive with the given test files.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, [], driveFiles);

  // Retrieve all file list entries that could be rendered 'offline'.
  const offlineEntry = '#file-list .table-row.file.dim-offline';
  let elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, [offlineEntry, ['opacity']]);

  // Check: the hello.txt file only should be rendered 'offline'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertEq(0, elements[0].text.indexOf('hello.txt'));

  // Check: hello.txt must have 'offline' CSS render style (opacity).
  chrome.test.assertEq('0.4', elements[0].styles.opacity);

  // Retrieve file entries that are 'available offline' (not dimmed).
  const availableEntry = '#file-list .table-row:not(.dim-offline)';
  elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, [availableEntry, ['opacity']]);

  // Check: these files should have 'available offline' CSS style.
  chrome.test.assertEq(3, elements.length);

  function checkRenderedInAvailableOfflineStyle(element, fileName) {
    chrome.test.assertEq(0, element.text.indexOf(fileName));
    chrome.test.assertEq('1', element.styles.opacity);
  }

  // Directories are shown as 'available offline'.
  checkRenderedInAvailableOfflineStyle(elements[0], 'photos');

  // Hosted documents are shown as 'available offline'.
  checkRenderedInAvailableOfflineStyle(elements[1], 'Test Document.gdoc');

  // Pinned files are shown as 'available offline'.
  checkRenderedInAvailableOfflineStyle(elements[2], 'pinned');
};

/**
 * Tests file display rendering in online Google Drive.
 */
testcase.fileDisplayDriveOnline = async () => {
  // Open Files app on Drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);

  // Retrieve all file list row entries.
  const fileEntry = '#file-list .table-row';
  const elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, [fileEntry, ['opacity']]);

  // Check: all files must have 'online' CSS style (not dimmed).
  chrome.test.assertEq(BASIC_DRIVE_ENTRY_SET.length, elements.length);
  for (let i = 0; i < elements.length; ++i) {
    chrome.test.assertEq('1', elements[i].styles.opacity);
  }
};

/**
 * Tests files display in the "Computers" section of Google Drive. Testing that
 * we can navigate to folders inside /Computers also has the side effect of
 * testing that the breadcrumbs are working.
 */
testcase.fileDisplayComputers = async () => {
  // Open Files app on Drive with Computers registered.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], COMPUTERS_ENTRY_SET);

  // Navigate to Comuter Grand Root.
  await remoteCall.navigateWithDirectoryTree(
      appId, '/Computers', 'Computers', 'drive');

  // Navigiate to a Computer Root.
  await remoteCall.navigateWithDirectoryTree(
      appId, '/Computers/Computer A', 'Computers', 'drive');

  // Navigiate to a subdirectory under a Computer Root.
  await remoteCall.navigateWithDirectoryTree(
      appId, '/Computers/Computer A/A', 'Computers', 'drive');
};


/**
 * Tests files display in an MTP volume.
 */
testcase.fileDisplayMtp = async () => {
  const MTP_VOLUME_QUERY = '#directory-tree [volume-type-icon="mtp"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount MTP volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeMtp'});

  // Wait for the MTP mount.
  await remoteCall.waitForElement(appId, MTP_VOLUME_QUERY);

  // Click to open the MTP volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [MTP_VOLUME_QUERY]);

  // Verify the MTP file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests files display in a removable USB volume.
 */
testcase.fileDisplayUsb = async () => {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [USB_VOLUME_QUERY]);

  // Verify the USB file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests files display on a removable USB volume with and without partitions.
 */
testcase.fileDisplayUsbPartition = async () => {
  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB device containing partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});
  // Mount unpartitioned USB device.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for removable root to appear in the directory tree.
  const removableRoot = await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="Drive Label"]');

  // Wait for removable partition-1 to appear in the directory tree.
  const partitionOne = await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="partition-1"]');
  chrome.test.assertEq(
      'removable', partitionOne.attributes['volume-type-for-testing']);

  // Wait for removable partition-2 to appear in the directory tree.
  const partitionTwo = await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="partition-2"]');
  chrome.test.assertEq(
      'removable', partitionTwo.attributes['volume-type-for-testing']);

  // Check partitions are children of the root label.
  const childEntriesQuery =
      ['[entry-label="Drive Label"] .tree-children .tree-item'];
  const childEntries = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, childEntriesQuery);
  const childEntryLabels =
      childEntries.map(child => child.attributes['entry-label']);
  chrome.test.assertEq(['partition-1', 'partition-2'], childEntryLabels);

  // Wait for USB to appear in the directory tree.
  const fakeUsb = await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="fake-usb"]');
  chrome.test.assertEq(
      'removable', fakeUsb.attributes['volume-type-for-testing']);

  // Check unpartitioned USB does not have partitions as tree children.
  const itemEntriesQuery =
      ['[entry-label="fake-usb"] .tree-children .tree-item'];
  const itemEntries = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, itemEntriesQuery);
  chrome.test.assertEq(1, itemEntries.length);
  const childVolumeType = itemEntries[0].attributes['volume-type-for-testing'];
  chrome.test.assertTrue('removable' !== childVolumeType);
};

/**
 * Tests that the file system type is properly displayed in the type
 * column. Checks that the entries can be properly sorted by type.
 * crbug.com/973743
 */
testcase.fileDisplayUsbPartitionSort = async () => {
  const removableGroup = '#directory-tree [root-type-icon="removable"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount removable device with partitions.
  await sendTestMessage({name: 'mountUsbWithMultiplePartitionTypes'});

  // Wait and select the removable group by clicking the label.
  await remoteCall.waitAndClickElement(appId, removableGroup);

  // Wait for partitions to appear in the file table list.
  let expectedRows = [
    ['partition-3', '--', 'vfat'],
    ['partition-2', '--', 'ext4'],
    ['partition-1', '--', 'ntfs'],
  ];
  const options = {orderCheck: true, ignoreLastModifiedTime: true};
  await remoteCall.waitForFiles(appId, expectedRows, options);

  // Sort by type in ascending order.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(4)']);
  const iconSortedAsc = (await isFilesNg(appId)) ?
      '.table-header-cell .sorted [iron-icon="files16:arrow_up_small"]' :
      '.table-header-sort-image-asc';
  await remoteCall.waitForElement(appId, iconSortedAsc);

  // Check that partitions are sorted in ascending order based on the partition
  // type.
  expectedRows = [
    ['partition-2', '--', 'ext4'],
    ['partition-1', '--', 'ntfs'],
    ['partition-3', '--', 'vfat'],
  ];
  await remoteCall.waitForFiles(appId, expectedRows, options);

  // Sort by type in descending order.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(4)']);
  const iconSortedDesc = (await isFilesNg(appId)) ?
      '.table-header-cell .sorted [iron-icon="files16:arrow_down_small"]' :
      '.table-header-sort-image-desc';
  await remoteCall.waitForElement(appId, iconSortedDesc);

  // Check that partitions are sorted in descending order based on the partition
  // type.
  expectedRows = [
    ['partition-3', '--', 'vfat'],
    ['partition-1', '--', 'ntfs'],
    ['partition-2', '--', 'ext4'],
  ];
  await remoteCall.waitForFiles(appId, expectedRows, options);
};

/**
 * Tests display of partitions in file list after mounting a removable USB
 * volume.
 */
testcase.fileDisplayPartitionFileTable = async () => {
  const removableGroup = '#directory-tree [root-type-icon="removable"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount removable partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Wait for removable group to appear in the directory tree.
  await remoteCall.waitForElement(appId, removableGroup);

  // Select the first removable group by clicking the label.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [removableGroup]));

  // Wait for removable partitions to appear in the file table.
  const partitionOne = await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-1"] .type');
  chrome.test.assertEq('ext4', partitionOne.text);

  const partitionTwo = await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-2"] .type');
  chrome.test.assertEq('ext4', partitionTwo.text);
};

/**
 * Searches for a string in Downloads and checks that the correct results
 * are displayed.
 *
 * @param {string} searchTerm The string to search for.
 * @param {Array<Object>} expectedResults The results set.
 *
 */
async function searchDownloads(searchTerm, expectedResults) {
  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['#search-box cr-input', searchTerm]);

  // Notify the element of the input.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']));


  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(expectedResults));
}

/**
 * Tests case-senstive search for an entry in Downloads.
 */
testcase.fileSearch = () => {
  return searchDownloads('hello', [ENTRIES.hello]);
};

/**
 * Tests case-insenstive search for an entry in Downloads.
 */
testcase.fileSearchCaseInsensitive = () => {
  return searchDownloads('HELLO', [ENTRIES.hello]);
};

/**
 * Tests searching for a string doesn't match anything in Downloads and that
 * there are no displayed items that match the search string.
 */
testcase.fileSearchNotFound = async () => {
  const searchTerm = 'blahblah';

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['#search-box cr-input', searchTerm]);

  // Notify the element of the input.
  await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']);
  const element =
      await remoteCall.waitForElement(appId, ['#empty-folder-label b']);
  chrome.test.assertEq(element.text, '\"' + searchTerm + '\"');
};

/**
 * Tests Files app opening without errors when there isn't Downloads which is
 * the default volume.
 */
testcase.fileDisplayWithoutDownloadsVolume = async () => {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Mount Drive.
  await sendTestMessage({name: 'mountDrive'});

  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
};

/**
 * Tests Files app opening without errors when there are no volumes at all.
 */
testcase.fileDisplayWithoutVolumes = async () => {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
};

/**
 * Tests Files app opening without errors when there are no volumes at all and
 * then mounting Downloads volume which should appear and be able to display its
 * files.
 */
testcase.fileDisplayWithoutVolumesThenMountDownloads = async () => {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Wait until Downloads is mounted.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Downloads should appear in My files in the directory tree.
  await remoteCall.waitForElement(appId, '[volume-type-icon="downloads"]');
  const downloadsRow = ['Downloads', '--', 'Folder'];
  const crostiniRow = ['Linux files', '--', 'Folder'];
  await remoteCall.waitForFiles(
      appId, [downloadsRow, crostiniRow],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});
};

/**
 * Tests Files app opening without errors when there are no volumes at all and
 * then mounting Drive volume which should appear and be able to display its
 * files.
 */
testcase.fileDisplayWithoutVolumesThenMountDrive = async () => {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Navigate to the Drive FakeItem.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[root-type-icon=\'drive\']']);

  // The fake Google Drive should be empty.
  await remoteCall.waitForFiles(appId, []);

  // Remount Drive. The curent directory should be changed from the Google
  // Drive FakeItem to My Drive.
  await sendTestMessage({name: 'mountDrive'});

  // Wait until Drive is mounted.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Add an entry to Drive.
  await addEntries(['drive'], [ENTRIES.newlyAdded]);

  // Wait for "My Drive" files to display in the file list.
  await remoteCall.waitForFiles(appId, [ENTRIES.newlyAdded.getExpectedRow()]);
};

/**
 * Tests Files app opening without Drive mounted.
 */
testcase.fileDisplayWithoutDrive = async () => {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Wait until downloads is re-added
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Open the files app.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.newlyAdded], []);

  // Wait for the loading indicator blink to finish.
  await remoteCall.waitForElement(
      appId, '#list-container .loading-indicator[hidden]');

  // Navigate to the fake Google Drive.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[root-type-icon=\'drive\']']);
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Google Drive');

  // Check: the fake Google Drive should be empty.
  await remoteCall.waitForFiles(appId, []);

  // Check: the loading indicator should be visible.
  await remoteCall.waitForElement(
      appId, '#list-container .loading-indicator:not([hidden])');
};

/**
 * Tests Files app opening without Drive mounted and then disabling and
 * re-enabling Drive.
 */
testcase.fileDisplayWithoutDriveThenDisable = async () => {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Add a file to Downloads.
  await addEntries(['local'], [ENTRIES.newlyAdded]);

  // Wait until it mounts.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // We should navigate to MyFiles.
  const expectedRows = [
    ['Downloads', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Navigate to Drive.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[root-type-icon=\'drive\']']);

  // The fake Google Drive should be empty.
  await remoteCall.waitForFiles(appId, []);

  // Disable Drive.
  await sendTestMessage({name: 'setDriveEnabled', enabled: false});

  // The current directory should change to the default (Downloads).
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Ensure Downloads has loaded.
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Re-enabled Drive.
  await sendTestMessage({name: 'setDriveEnabled', enabled: true});

  // Wait for the fake drive to reappear.
  await remoteCall.waitForElement(appId, ['[root-type-icon=\'drive\']']);
};

/**
 * Tests Files app resisting the urge to switch to Downloads when mounts change.
 * re-enabling Drive.
 */
testcase.fileDisplayMountWithFakeItemSelected = async () => {
  // Open Files app on Drive with the given test files.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.newlyAdded], []);

  // Ensure Downloads has loaded.
  await remoteCall.waitForFiles(appId, [ENTRIES.newlyAdded.getExpectedRow()]);

  // Navigate to My files.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[root-type-icon=\'my_files\']']);

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Mount a USB drive.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the mount to appear.
  await remoteCall.waitForElement(
      appId, ['#directory-tree [volume-type-icon="removable"]']);
  chrome.test.assertEq(
      '/My files',
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []));
};

/**
 * Tests Files app switching away from Drive virtual folders when Drive is
 * unmounted.
 */
testcase.fileDisplayUnmountDriveWithSharedWithMeSelected = async () => {
  // Open Files app on Drive with the given test files.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [ENTRIES.newlyAdded],
      [ENTRIES.testSharedDocument, ENTRIES.hello]);

  // Navigate to Shared with me.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[volume-type-icon=\'drive_shared_with_me\']']);

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Check that the file is visible.
  await remoteCall.waitForFiles(
      appId, [ENTRIES.testSharedDocument.getExpectedRow()]);

  // Unmount drive.
  await sendTestMessage({name: 'unmountDrive'});

  // We should navigate to MyFiles.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Which should contain a file.
  const expectedRows = [
    ['Play files', '--', 'Folder'],
    ['Downloads', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});
};

/**
 * Navigates to a removable volume, then unmounts it. Check to see whether
 * Files App switches away to the default Downloads directory.
 *
 * @param {string} removableDirectory The removable directory to be inside
 *    before unmounting the USB.
 */
async function unmountRemovableVolume(removableDirectory) {
  const removableRootQuery = '#directory-tree [root-type-icon="removable"]';

  // Open Files app on Downloads containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Mount a device containing two partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Wait for the removable root to appear in the directory tree.
  await remoteCall.waitForElement(appId, removableRootQuery);

  // Navigate to the removable root directory.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [removableRootQuery]);

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Drive Label');

  // Wait for partition entries to appear in the directory.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-1"]');
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-2"]');

  if (removableDirectory === 'partition-1' ||
      removableDirectory === 'partition-2') {
    const partitionQuery = `#file-list [file-name="${removableDirectory}"]`;
    const partitionFiles = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
    await remoteCall.callRemoteTestUtil(
        'fakeMouseDoubleClick', appId, [partitionQuery]);
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        appId, `/Drive Label/${removableDirectory}`);
    await remoteCall.waitForFiles(
        appId, partitionFiles, {ignoreLastModifiedTime: true});
  }

  // Unmount partitioned device.
  await sendTestMessage({name: 'unmountPartitions'});

  // We should navigate to MyFiles.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // And contains the expected files.
  const expectedRows = [
    ['Play files', '--', 'Folder'],
    ['Downloads', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});
}

/**
 * Tests Files app switches away from a removable device root after the USB is
 * unmounted.
 */
testcase.fileDisplayUnmountRemovableRoot = () => {
  return unmountRemovableVolume('Drive Label');
};

/**
 * Tests Files app switches away from a partition inside the USB after the USB
 * is unmounted.
 */
testcase.fileDisplayUnmountFirstPartition = () => {
  return unmountRemovableVolume('partition-1');
};

/**
 * Tests Files app switches away from a partition inside the USB after the USB
 * is unmounted. Partition-1 will be ejected first.
 */
testcase.fileDisplayUnmountLastPartition = () => {
  return unmountRemovableVolume('partition-2');
};

/**
 * Tests files display in Downloads while the default blocking file I/O task
 * runner is blocked.
 */
testcase.fileDisplayDownloadsWithBlockedFileTaskRunner = async () => {
  await sendTestMessage({name: 'blockFileTaskRunner'});
  await fileDisplay(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
  await sendTestMessage({name: 'unblockFileTaskRunner'});
};

/**
 * Tests to make sure check-select mode enables when selecting one item
 */
testcase.fileDisplayCheckSelectWithFakeItemSelected = async () => {
  // Open files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Select ENTRIES.hello.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']));

  // Select all.
  const ctrlA = ['#file-list', 'a', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA));

  // Make sure check-select is enabled.
  await remoteCall.waitForElement(appId, 'body.check-select');
};

/**
 * Tests to make sure read-only indicator is visible when the current directory
 * is read-only.
 */
testcase.fileDisplayCheckReadOnlyIconOnFakeDirectory = async () => {
  // Open Files app on Drive with the given test files.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [ENTRIES.newlyAdded],
      [ENTRIES.testSharedDocument, ENTRIES.hello]);

  // Navigate to Shared with me.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[volume-type-icon=\'drive_shared_with_me\']']);

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Make sure read-only indicator on toolbar is visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator:not([hidden])');
};

/**
 * Tests to make sure read-only indicator is NOT visible when the current
 * is writable.
 */
testcase.fileDisplayCheckNoReadOnlyIconOnDownloads = async () => {
  // Open files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Make sure read-only indicator on toolbar is NOT visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator[hidden]');
};

/**
 * Tests to make sure read-only indicator is NOT visible when the current
 * directory is the "Linux files" fake root.
 */
testcase.fileDisplayCheckNoReadOnlyIconOnLinuxFiles = async () => {
  const fakeRoot = '#directory-tree [root-type-icon="crostini"]';

  // Block mounts from progressing. This should cause the file manager to always
  // show the loading bar for linux files.
  await sendTestMessage({name: 'blockMounts'});

  // Open files app on Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Linux files fake root is shown.
  await remoteCall.waitForElement(appId, fakeRoot);

  // Click on Linux files.
  await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [fakeRoot]);

  // Check: the loading indicator should be visible.
  await remoteCall.waitForElement(
      appId, '#list-container .loading-indicator:not([hidden])');

  // Check: the toolbar read-only indicator should not be visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator[hidden]');
};

/**
 * Tests that a failure opening one window won't block opening other windows.
 */
testcase.fileDisplayStartupError = async () => {
  // Fake chrome.app.window.create to return undefined.
  const fakeData = {
    'chrome.app.window.create': ['static_fake', [undefined]],
  };
  await remoteCall.callRemoteTestUtil('backgroundFake', null, [fakeData]);

  // Check: opening a Files app window should fail and return null.
  const failedAppId = await openNewWindow(RootPath.DOWNLOADS);
  chrome.test.assertEq(null, failedAppId);

  // Remove fakes.
  const removedCount =
      await remoteCall.callRemoteTestUtil('removeAllBackgroundFakes', null, []);
  chrome.test.assertEq(1, removedCount);

  // Check: opening a Files app window should succeed.
  const appId = await openNewWindow(RootPath.DOWNLOADS);
  chrome.test.assertTrue(null !== appId);

  // The failed attempt logs the error.
  return IGNORE_APP_ERRORS;
};
