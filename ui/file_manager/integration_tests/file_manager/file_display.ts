// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {type ElementObject} from '../prod/file_manager/shared_types.js';
import {addEntries, ENTRIES, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_FAKE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, COMPUTERS_ENTRY_SET} from './test_data.js';

/**
 * Checks if the files initially added by the C++ side are displayed, and
 * that files subsequently added are also displayed.
 *
 * @param path Path to be tested, Downloads or Drive.
 * @param defaultEntries Default file entries.
 */
async function fileDisplay(path: string, defaultEntries: TestEntryInfo[]) {
  // Open Files app on the given |path| with default file entries.
  const appId = await remoteCall.setupAndWaitUntilReady(path);

  // Verify the default file list is present in |result|.
  const defaultList = TestEntryInfo.getExpectedRows(defaultEntries).sort();
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
export async function fileDisplayDownloads() {
  return fileDisplay(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
}

/**
 * Tests opening the files app navigating to a local folder. Uses
 * platform_util::OpenItem, a call to an API distinct from the one commonly used
 * in other tests for the same operation.
 */
export async function fileDisplayLaunchOnLocalFolder() {
  // Add a file to Downloads.
  await addEntries(['local'], [ENTRIES.photos]);

  // Open Files app on the Downloads directory.
  await sendTestMessage(
      {name: 'launchAppOnLocalFolder', localPath: 'Downloads/photos'});

  // Wait for app window to open.
  const appId = await remoteCall.waitForWindow();

  // Check: The current directory is MyFiles/Downloads/photos.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads/photos');
}

/**
 * Tests opening the files app navigating to a local folder. Uses
 * platform_util::OpenItem, a call to an API distinct from the one commonly used
 * in other tests for the same operation.
 */
export async function fileDisplayLaunchOnLocalFile() {
  // Add a file to Downloads.
  await addEntries(['local'], [ENTRIES.hello, ENTRIES.world]);

  // Open Files app on the Downloads directory selecting the target file.
  await sendTestMessage(
      {name: 'showItemInFolder', localPath: 'Downloads/hello.txt'});

  // Wait for app window to open.
  const appId = await remoteCall.waitForWindow();

  // Check: The current directory is MyFiles/Downloads.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads');

  // Check: The target file is selected.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="hello.txt"][selected]');
}

/**
 * Tests opening the files app navigating to the My Drive folder. Uses
 * platform_util::OpenItem, a call to an API distinct from the one commonly used
 * in other tests for the same operation.
 */
export async function fileDisplayLaunchOnDrive() {
  // Open Files app on the Drive directory.
  await sendTestMessage({name: 'launchAppOnDrive'});

  // Wait for app window to open.
  const appId = await remoteCall.waitForWindow();

  // Check: the app should be open on My Drive.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel('My Drive');
}

/**
 * Tests files display in Google Drive.
 */
export async function fileDisplayDrive() {
  return fileDisplay(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET);
}

/**
 * Tests file display rendering in offline Google Drive.
 */
export async function fileDisplayDriveOffline() {
  const driveFiles =
      [ENTRIES.hello, ENTRIES.pinned, ENTRIES.photos, ENTRIES.testDocument];

  // is not assignable to parameter of type 'TestEntryInfo[]'.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], driveFiles);

  // Retrieve all file list entries that could be rendered 'offline'.
  // Use "first-child" here because opacity for offline only applies on the
  // children elements.
  const offlineEntry =
      '#file-list .table-row.file.dim-offline > div:first-child';
  let elements =
      await remoteCall.queryElements(appId, offlineEntry, ['opacity']);

  // Check: the hello.txt file only should be rendered 'offline'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertEq(0, elements[0]!.text?.indexOf('hello.txt'));

  // Check: hello.txt must have 'offline' CSS render style (opacity).
  chrome.test.assertEq('0.38', elements[0]!.styles?.['opacity']);

  // Retrieve file entries that are 'available offline' (not dimmed).
  // Use "first-child" here because opacity for offline only applies on the
  // children elements.
  const availableEntry =
      '#file-list .table-row:not(.dim-offline) > div:first-child';
  elements = await remoteCall.queryElements(appId, availableEntry, ['opacity']);

  // Check: these files should have 'available offline' CSS style.
  chrome.test.assertEq(3, elements.length);

  function checkRenderedInAvailableOfflineStyle(
      element: ElementObject, fileName: string) {
    chrome.test.assertEq(0, element.text!.indexOf(fileName));
    chrome.test.assertEq('1', element.styles!['opacity']);
  }

  // Directories are shown as 'available offline'.
  checkRenderedInAvailableOfflineStyle(elements[0]!, 'photos');

  // Hosted documents are shown as 'available offline'.
  checkRenderedInAvailableOfflineStyle(elements[1]!, 'Test Document.gdoc');

  // Pinned files are shown as 'available offline'.
  checkRenderedInAvailableOfflineStyle(elements[2]!, 'pinned');
}

/**
 * Tests file display rendering in online Google Drive.
 * @param appId the id for the window to check the file display.
 */
async function checkDriveOnlineDisplay(appId: string) {
  // Retrieve all file list row entries.
  const fileEntry = '#file-list .table-row';
  const elements =
      await remoteCall.queryElements(appId, fileEntry, ['opacity']);

  // Check: all files must have 'online' CSS style (not dimmed).
  chrome.test.assertEq(BASIC_DRIVE_ENTRY_SET.length, elements.length);
  for (const element of elements) {
    chrome.test.assertEq('1', element.styles?.['opacity']);
  }
}

/**
 * Tests file display rendering in online Google Drive.
 */
export async function fileDisplayDriveOnline() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);

  await checkDriveOnlineDisplay(appId);
}

/**
 * Tests file display rendering in online Google Drive when opening via OpenItem
 * function.
 */
export async function fileDisplayDriveOnlineNewWindow() {
  // Open Files app on the Drive directory.
  await addEntries(['drive'], BASIC_DRIVE_ENTRY_SET);
  await sendTestMessage({name: 'launchAppOnDrive'});

  // Wait for app window to open.
  const appId = await remoteCall.waitForWindow();

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  await checkDriveOnlineDisplay(appId);
}

/**
 * Tests files display in the "Computers" section of Google Drive. Testing that
 * we can navigate to folders inside /Computers also has the side effect of
 * testing that the breadcrumbs are working.
 */
export async function fileDisplayComputers() {
  // Open Files app on Drive with Computers registered.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], COMPUTERS_ENTRY_SET);

  // Navigate to Computer Grand Root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Computers');

  // Navigate to a Computer Root.
  await directoryTree.navigateToPath('/Computers/Computer A');

  // Navigate to a subdirectory under a Computer Root.
  await directoryTree.navigateToPath('/Computers/Computer A/A');
}


/**
 * Tests files display in an MTP volume.
 */
export async function fileDisplayMtp() {
  const MTP_VOLUME_TYPE = 'mtp';

  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount MTP volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeMtp'});

  // Wait for the MTP mount and click to open the MTP volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType(MTP_VOLUME_TYPE);

  // Verify the MTP file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
}

/**
 * Tests files display in a removable USB volume.
 */
export async function fileDisplayUsb() {
  const USB_VOLUME_TYPE = 'removable';

  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB mount and click to open the USB volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType(USB_VOLUME_TYPE);

  // Verify the USB file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
}

/**
 * Tests files display on a removable USB volume with and without partitions.
 */
export async function fileDisplayUsbPartition() {
  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB device containing partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});
  // Mount unpartitioned USB device.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for removable root to appear in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Drive Label');

  // Wait for removable partition-1 to appear in the directory tree.
  await directoryTree.expandTreeItemByLabel('Drive Label');
  const partitionOne = await directoryTree.waitForItemByLabel('partition-1');
  chrome.test.assertEq(
      'removable', directoryTree.getItemVolumeType(partitionOne));

  // Wait for removable partition-2 to appear in the directory tree.
  const partitionTwo = await directoryTree.waitForItemByLabel('partition-2');
  chrome.test.assertEq(
      'removable', directoryTree.getItemVolumeType(partitionTwo));

  // Check partitions are children of the root label.
  const childEntries =
      await directoryTree.getChildItemsByParentLabel('Drive Label');
  const childEntryLabels =
      childEntries.map(child => directoryTree.getItemLabel(child));
  chrome.test.assertEq(['partition-1', 'partition-2'], childEntryLabels);

  if (await remoteCall.isSinglePartitionFormat(appId)) {
    // Wait for USB to appear in the directory tree.
    await directoryTree.waitForItemByLabel('FAKEUSB');
    // Expand it before checking children items.
    await directoryTree.expandTreeItemByLabel('FAKEUSB');
    // Check unpartitioned USB has single partition as tree child.
    const itemEntries =
        await directoryTree.getChildItemsByParentLabel('FAKEUSB');
    chrome.test.assertEq(1, itemEntries.length);
    const childVolumeType = directoryTree.getItemVolumeType(itemEntries[0]!);
    chrome.test.assertTrue('removable' === childVolumeType);
  } else {
    // Wait for USB to appear in the directory tree.
    const fakeUsb = await directoryTree.waitForItemByLabel('fake-usb');
    chrome.test.assertEq('removable', directoryTree.getItemVolumeType(fakeUsb));
    // Expand it before checking children items.
    await directoryTree.expandTreeItemByLabel('fake-usb');
    // Check unpartitioned USB does not have partitions as tree children.
    const itemEntries =
        await directoryTree.getChildItemsByParentLabel('fake-usb');
    chrome.test.assertEq(1, itemEntries.length);
    const childVolumeType = directoryTree.getItemVolumeType(itemEntries[0]!);
    chrome.test.assertTrue('removable' !== childVolumeType);
  }
}

/**
 * Tests that the file system type is properly displayed in the type
 * column. Checks that the entries can be properly sorted by type.
 * crbug.com/973743
 */
export async function fileDisplayUsbPartitionSort() {
  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount removable device with partitions.
  await sendTestMessage({name: 'mountUsbWithMultiplePartitionTypes'});

  // Wait and select the removable group by clicking the label.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectGroupRootItemByType('removable');

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
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(3)']);
  const iconSortedAsc =
      '.table-header-cell .sorted [iron-icon="files16:arrow_up_small"]';
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
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(3)']);
  const iconSortedDesc =
      '.table-header-cell .sorted [iron-icon="files16:arrow_down_small"]';
  await remoteCall.waitForElement(appId, iconSortedDesc);

  // Check that partitions are sorted in descending order based on the partition
  // type.
  expectedRows = [
    ['partition-3', '--', 'vfat'],
    ['partition-1', '--', 'ntfs'],
    ['partition-2', '--', 'ext4'],
  ];
  await remoteCall.waitForFiles(appId, expectedRows, options);
}

/**
 * Tests display of partitions in file list after mounting a removable USB
 * volume.
 */
export async function fileDisplayPartitionFileTable() {
  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount removable partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Wait for removable group to appear in the directory tree and select the
  // first removable group by clicking the label.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectGroupRootItemByType('removable');

  // Wait for removable partitions to appear in the file table.
  const partitionOne = await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-1"] .type');
  chrome.test.assertEq('ext4', partitionOne.text);

  const partitionTwo = await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-2"] .type');
  chrome.test.assertEq('ext4', partitionTwo.text);
}

/**
 * Searches for a string in Downloads and checks that the correct results
 * are displayed.
 *
 * @param searchTerm The string to search for.
 * @param expectedResults The results set.
 *
 */
async function searchDownloads(
    searchTerm: string, expectedResults: TestEntryInfo[]) {
  // Open Files app on local downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.inputText(appId, '#search-box cr-input', searchTerm);

  // Notify the element of the input.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']));


  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(expectedResults));
}

/**
 * Tests case-sensitive search for an entry in Downloads.
 */
export async function fileSearch() {
  return searchDownloads('hello', [ENTRIES.hello]);
}

/**
 * Tests case-insenstive search for an entry in Downloads.
 */
export async function fileSearchCaseInsensitive() {
  return searchDownloads('HELLO', [ENTRIES.hello]);
}

/**
 * Tests searching for a string doesn't match anything in Downloads and that
 * there are no displayed items that match the search string.
 */
export async function fileSearchNotFound() {
  const searchTerm = 'blahblah';

  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.inputText(appId, '#search-box cr-input', searchTerm);

  // Notify the element of the input.
  await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']);

  await remoteCall.waitForFiles(appId, []);
}

/**
 * Tests Files app opening without errors when there isn't Downloads which is
 * the default volume.
 */
export async function fileDisplayWithoutDownloadsVolume() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq('0', await sendTestMessage({name: 'getVolumesCount'}));

  // Mount Drive.
  await sendTestMessage({name: 'mountDrive'});

  await remoteCall.waitForVolumesCount(1);

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
}

/**
 * Tests Files app opening without errors when there are no volumes at all.
 */
export async function fileDisplayWithoutVolumes() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq('0', await sendTestMessage({name: 'getVolumesCount'}));

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
}

/**
 * Tests Files app opening without errors when there are no volumes at all and
 * then mounting Downloads volume which should appear and be able to display its
 * files.
 */
export async function fileDisplayWithoutVolumesThenMountDownloads() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq('0', await sendTestMessage({name: 'getVolumesCount'}));

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Wait until Downloads is mounted.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Downloads should appear in My files in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('My files');
  const expectedRows =
      [['Downloads', '--', 'Folder'], ['Linux files', '--', 'Folder']];
  await remoteCall.waitForFiles(
      appId, expectedRows,
      {ignoreFileSize: true, ignoreLastModifiedTime: true});
}

/**
 * Tests Files app opening without errors when there are no volumes at all and
 * then mounting Drive volume which should appear and be able to display its
 * files.
 */
export async function fileDisplayWithoutVolumesThenMountDrive() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq('0', await sendTestMessage({name: 'getVolumesCount'}));

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Navigate to the Drive FakeItem.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectGroupRootItemByType('drive');

  // The fake Google Drive should be empty and read only label should show.
  await remoteCall.waitForFiles(appId, []);
  await remoteCall.waitForElement(appId, '#read-only-indicator:not([hidden])');

  // Remount Drive. The current directory should be changed from the Google
  // Drive FakeItem to My Drive.
  await sendTestMessage({name: 'mountDrive'});

  // Wait until Drive is mounted.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Add an entry to Drive.
  await addEntries(['drive'], [ENTRIES.newlyAdded]);

  // Wait for "My Drive" files to display in the file list and read only label
  // should hide.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My Drive');
  await remoteCall.waitForFiles(appId, [ENTRIES.newlyAdded.getExpectedRow()]);
  await remoteCall.waitForElement(appId, '#read-only-indicator[hidden]');
}

/**
 * Tests Files app opening without Drive mounted.
 */
export async function fileDisplayWithoutDrive() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq('0', await sendTestMessage({name: 'getVolumesCount'}));

  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Wait until downloads is re-added
  await remoteCall.waitForVolumesCount(1);

  // Open the files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.newlyAdded], []);

  // Wait for the loading indicator blink to finish.
  await remoteCall.waitForElement(
      appId, '#list-container .loading-indicator[hidden]');

  // Navigate to the fake Google Drive.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectGroupRootItemByType('drive');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Google Drive');

  // Check that the scanner have finished.
  await remoteCall.waitForElement(appId, `[scan-completed="Google Drive"]`);

  // Check: the fake Google Drive should be empty.
  await remoteCall.waitForFiles(appId, []);
}

/**
 * Tests Files app opening without Drive mounted and then disabling and
 * re-enabling Drive.
 */
export async function fileDisplayWithoutDriveThenDisable() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq('0', await sendTestMessage({name: 'getVolumesCount'}));

  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Add a file to Downloads.
  await addEntries(['local'], [ENTRIES.newlyAdded]);

  // Wait until it mounts.
  await remoteCall.waitForVolumesCount(1);

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
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
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectGroupRootItemByType('drive');

  // The fake Google Drive should be empty.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Google Drive');
  await remoteCall.waitForFiles(appId, []);

  // Disable Drive.
  await sendTestMessage({name: 'setDriveEnabled', enabled: false});
  await directoryTree.waitForItemLostByLabel('Google Drive');

  // The current directory should change to the default (Downloads).
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Ensure Downloads has loaded.
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Re-enabled Drive.
  await sendTestMessage({name: 'setDriveEnabled', enabled: true});

  // Wait for the fake drive to reappear.
  await directoryTree.waitForItemByLabel('Google Drive');
}

/**
 * Tests that mounting a hidden Volume does not mount the volume in file
 * manager.
 */
export async function fileDisplayWithHiddenVolume() {
  const initialVolumeCount = await sendTestMessage({name: 'getVolumesCount'});

  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Get the directory tree elements.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  const visibleLabelsBefore = await directoryTree.getVisibleItemLabels();

  // Mount a hidden volume.
  await sendTestMessage({name: 'mountHidden'});

  const visibleLabelsAfter = await directoryTree.getVisibleItemLabels();

  // The directory tree should NOT display the hidden volume.
  chrome.test.assertEq(visibleLabelsBefore, visibleLabelsAfter);

  // The hidden volume should not be counted in the number of volumes.
  chrome.test.assertEq(
      initialVolumeCount, await sendTestMessage({name: 'getVolumesCount'}));
}

/**
 * Tests Files app resisting the urge to switch to Downloads when mounts change.
 */
export async function fileDisplayMountWithFakeItemSelected() {
  // Open Files app on Drive with the given test files.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.newlyAdded], []);

  // Ensure Downloads has loaded.
  await remoteCall.waitForFiles(appId, [ENTRIES.newlyAdded.getExpectedRow()]);

  // Navigate to My files.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('My files');

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Mount a USB drive.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the mount to appear.
  await directoryTree.waitForItemByType('removable');
  chrome.test.assertEq(
      '/My files',
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []));
}

/**
 * Tests Files app switching away from Drive virtual folders when Drive is
 * unmounted.
 */
export async function fileDisplayUnmountDriveWithSharedWithMeSelected() {
  // Open Files app on Drive with the given test files.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [ENTRIES.newlyAdded],
      [ENTRIES.testSharedDocument, ENTRIES.hello]);

  // Navigate to Shared with me.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Shared with me');

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
}

/**
 * Navigates to a removable volume, then unmounts it. Check to see whether
 * Files App switches away to the default Downloads directory.
 *
 * @param removableDirectory The removable directory to be inside
 *    before unmounting the USB.
 */
async function unmountRemovableVolume(removableDirectory: string) {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Mount a device containing two partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Wait for the removable root to appear in the directory tree and navigate to
  // the removable root directory.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectGroupRootItemByType('removable');

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
export async function fileDisplayUnmountRemovableRoot() {
  return unmountRemovableVolume('Drive Label');
}

/**
 * Tests Files app switches away from a partition inside the USB after the USB
 * is unmounted.
 */
export async function fileDisplayUnmountFirstPartition() {
  return unmountRemovableVolume('partition-1');
}

/**
 * Tests Files app switches away from a partition inside the USB after the USB
 * is unmounted. Partition-1 will be ejected first.
 */
export async function fileDisplayUnmountLastPartition() {
  return unmountRemovableVolume('partition-2');
}

/**
 * Tests files display in Downloads while the default blocking file I/O task
 * runner is blocked.
 */
export async function fileDisplayDownloadsWithBlockedFileTaskRunner() {
  await sendTestMessage({name: 'blockFileTaskRunner'});
  await fileDisplay(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
  await sendTestMessage({name: 'unblockFileTaskRunner'});
}

/**
 * Tests to make sure check-select mode enables when selecting one item
 */
export async function fileDisplayCheckSelectWithFakeItemSelected() {
  // Open files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Select ENTRIES.hello.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);

  // Select all.
  const ctrlA = ['#file-list', 'a', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA));

  // Make sure check-select is enabled.
  await remoteCall.waitForElement(appId, 'body.check-select');
}

/**
 * Tests to make sure read-only indicator is visible when the current directory
 * is read-only.
 */
export async function fileDisplayCheckReadOnlyIconOnFakeDirectory() {
  // Open Files app on Drive with the given test files.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [ENTRIES.newlyAdded],
      [ENTRIES.testSharedDocument, ENTRIES.hello]);

  // Navigate to Shared with me.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Shared with me');

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Make sure read-only indicator on toolbar is visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator:not([hidden])');
}

/**
 * Tests to make sure read-only indicator is NOT visible when the current
 * is writable.
 */
export async function fileDisplayCheckNoReadOnlyIconOnDownloads() {
  // Open files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Make sure read-only indicator on toolbar is NOT visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator[hidden]');
}

/**
 * Tests to make sure read-only indicator is NOT visible when the current
 * directory is the "Linux files" fake root.
 */
export async function fileDisplayCheckNoReadOnlyIconOnLinuxFiles() {
  // Block mounts from progressing. This should cause the file manager to always
  // show the loading bar for linux files.
  await sendTestMessage({name: 'blockMounts'});

  // Open files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Click on Linux files.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectPlaceholderItemByType('crostini');

  // Check: the loading indicator should be visible.
  await remoteCall.waitForElement(
      appId, '#list-container .loading-indicator:not([hidden])');

  // Check: the toolbar read-only indicator should not be visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator[hidden]');
}

/**
 * Tests to make sure read-only indicator is NOT visible when the current
 * directory is a "GuestOs" fake root.
 */
export async function fileDisplayCheckNoReadOnlyIconOnGuestOs() {
  // Create a Bruschetta guest for this test.
  await sendTestMessage({
    name: 'registerMountableGuest',
    displayName: 'mogsaur',
    canMount: true,
    vmType: 'bruschetta',
  });

  // Block mounts from progressing. This should cause the file manager to always
  // show the loading bar for our mount.
  await sendTestMessage({name: 'blockMounts'});

  // Open files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Click on the placeholder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectPlaceholderItemByType('bruschetta');

  // Check: the loading indicator should be visible.
  await remoteCall.waitForElement(
      appId, '#list-container .loading-indicator:not([hidden])');

  // Check: the toolbar read-only indicator should not be visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator[hidden]');
}
