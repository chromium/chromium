// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ElementObject} from '../prod/file_manager/shared_types.js';
import {addEntries, ENTRIES, EntryType, getCaller, getDateWithDayDiff, pending, repeatUntil, RootPath, sendTestMessage, SharedOption, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET, COMPUTERS_ENTRY_SET, NESTED_ENTRY_SET} from './test_data.js';

/**
 * @param appId The ID that identifies the files app.
 * @param type The search option type (location, recency, type).
 * @return The text of the element with 'selected-option' ID.
 */
async function getSelectedOptionText(
    appId: string, type: string): Promise<string> {
  // Force refresh of the element by showing the dropdown menu.
  await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [
    [
      'xf-search-options',
      `xf-select#${type}-selector`,
    ],
  ]);
  // Fetch the current selected item.
  const option = await remoteCall.waitForElement(appId, [
    'xf-search-options',
    `xf-select#${type}-selector`,
    '#selected-option',
  ]);
  return option.text ?? '';
}

/**
 * Tests searching inside Downloads with results.
 */
export async function searchDownloadsWithResults() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Wait file list to display the search result.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));

  // Check that a11y message for results has been issued.
  const a11yMessages: string[] =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);

  return appId;
}

/**
 * Tests searching inside Downloads without results.
 */
export async function searchDownloadsWithNoResults() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Search for name not present among basic entry set.
  await remoteCall.typeSearchText(appId, 'INVALID TERM');

  // Wait file list to display no results.
  await remoteCall.waitForFiles(appId, []);

  // Check that a11y message for no results has been issued.
  const a11yMessages: string[] =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq(
      'There are no results for INVALID TERM.', a11yMessages[0]);
}

/**
 * Tests that clearing the search box announces the A11y.
 */
export async function searchDownloadsClearSearch() {
  // Perform a normal search, to be able to clear the search box.
  const appId = await searchDownloadsWithResults();

  // Click on the clear search button.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');

  // Wait for the search box to fully collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Wait for file list to display all files.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));

  // Check that a11y message for clearing the search term has been issued.
  const a11yMessages: string[] =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(
      2, a11yMessages.length,
      `Want 2 messages got ${JSON.stringify(a11yMessages)}`);
  chrome.test.assertEq(
      'Search text cleared, showing all files and folders.', a11yMessages[1]);
}

/**
 * Tests that clearing the search box with keydown crbug.com/910068.
 */
export async function searchDownloadsClearSearchKeyDown() {
  // Perform a normal search, to be able to clear the search box.
  const appId = await searchDownloadsWithResults();

  const clearButton = '#search-box .clear';
  // Wait for clear button.
  await remoteCall.waitForElement(appId, clearButton);

  // Send a enter key to the clear button.
  const enterKey = [clearButton, 'Enter', false, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...enterKey);

  // Check: Search input field is empty.
  const searchInput =
      await remoteCall.waitForElement(appId, '#search-box [type="search"]');
  chrome.test.assertEq('', searchInput.value);

  // Wait until the search button get the focus.
  // Use repeatUntil() here because the focus won't shift to search button
  // until the CSS animation is finished.
  const caller = getCaller();
  await repeatUntil(async () => {
    const activeElement =
        await remoteCall.callRemoteTestUtil<ElementObject|null>(
            'getActiveElement', appId, []);
    if (activeElement && activeElement.attributes['id'] !== 'search-button') {
      return pending(
          caller, 'Expected active element should be search-button, got %s',
          activeElement.attributes['id']);
    }
    return;
  });
}

/**
 * Tests that the search text entry box stays expanded until the end of user
 * interaction.
 */
export async function searchHidingTextEntryField() {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select an entry in the file list.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Verify the toolbar search text entry box is enabled.
  let textInputElement =
      await remoteCall.waitForElement(appId, ['#search-box cr-input', 'input']);
  chrome.test.assertEq(undefined, textInputElement.attributes['disabled']);

  // Send a 'mousedown' to the toolbar 'delete' button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#delete-button', 'mousedown']));

  // Verify the toolbar search text entry is still enabled.
  textInputElement =
      await remoteCall.waitForElement(appId, ['#search-box cr-input', 'input']);
  chrome.test.assertEq(undefined, textInputElement.attributes['disabled']);

  // Send a 'mouseup' to the toolbar 'delete' button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#delete-button', 'mouseup']));

  // Verify the toolbar search text entry is still enabled.
  textInputElement =
      await remoteCall.waitForElement(appId, ['#search-box cr-input', 'input']);
  chrome.test.assertEq(undefined, textInputElement.attributes['disabled']);
}

/**
 * Tests that the search box collapses when empty and Tab out of the box.
 */
export async function searchHidingViaTab() {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Search box should start collapsed.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Wait for the search box to expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Verify the search input has focus.
  const input = await remoteCall.callRemoteTestUtil<ElementObject>(
      'deepGetActiveElement', appId, []);
  chrome.test.assertEq(input.attributes['id'], 'input');
  chrome.test.assertEq(input.attributes['aria-label'], 'Search');

  // Send Tab key to focus the next element.
  const result = await sendTestMessage({name: 'dispatchTabKey'});
  chrome.test.assertEq(result, 'tabKeyDispatched', 'Tab key dispatch failure');

  // Check: the search box should collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');
}

/**
 * Tests that clicking the search button expands and collapses the search box.
 */
export async function searchButtonToggles() {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Search box should start collapsed.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Measure the width of the search box when it's collapsed.
  const collapsedSearchBox = await remoteCall.waitForElementStyles(
      appId, '#search-wrapper', ['width']);

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Wait for the search box to expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Check: The search box width should have increased.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElementStyles(
        appId, '#search-wrapper', ['width']);
    if (collapsedSearchBox.renderedWidth! >= element.renderedWidth!) {
      return pending(caller, 'Waiting search box to expand');
    }
    return;
  });

  // Click the toolbar search button again.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Check: the search box should collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Check: the search box width should decrease.
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElementStyles(
        appId, '#search-wrapper', ['width']);
    if (collapsedSearchBox.renderedWidth! < element.renderedWidth!) {
      return pending(caller, 'Waiting search box to collapse');
    }
    return;
  });
}

/**
 * Tests that Files app performs a search at app start up when
 * LaunchParam.searchQuery is specified.
 */
export async function searchQueryLaunchParam() {
  // Open Files app with LaunchParam.searchQuery='gdoc'.
  const query = 'gdoc';
  const appState = {searchQuery: query};
  const appId = await remoteCall.setupAndWaitUntilReady(
      null, BASIC_LOCAL_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, appState);

  // Check: search box should be filled with the query.
  const caller = getCaller();
  await repeatUntil(async () => {
    const searchBoxInput =
        await remoteCall.waitForElement(appId, '#search-box cr-input');
    if (searchBoxInput.value !== query) {
      return pending(caller, 'Waiting search box to be filled with the query.');
    }
    return;
  });

  // Check: "My Drive" directory should be selected because it is the sole
  //        directory that contains query-matched files (*.gdoc).
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel('My Drive');
  await directoryTree.waitForFocusableItemByLabel('My Drive');

  // Check: Query-matched files should be shown in the files list.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.testDocument,
    ENTRIES.testSharedDocument,
  ]));
}

/**
 * Checks that changing location options correctly filters search results.
 */
export async function searchWithLocationOptions() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Modify the basic entry set by adding nested directories and
  // a copy of the hello entry.
  const nestedHello = ENTRIES.hello.cloneWith({
    targetPath: 'A/hello.txt',
  });
  addEntries(['local'], [ENTRIES.directoryA, nestedHello]);

  // Start in the nested directory, as the default search location
  // is THIS_FOLDER. Expect to find one hello file. Then search on
  // THIS_CHROMEBOOK and expect to find two.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/A');

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Verify that the search options are visible.
  await remoteCall.waitForElement(appId, 'xf-search-options:not([hidden])');

  // Expect only the nested hello to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    nestedHello,
  ]));

  // Click the second button, which is My files.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 2),
      'Failed to click "My files" location selector');

  // Expect all hello files to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    nestedHello,
  ]));
}

/**
 * Checks that changing recency options correctly filters search results.
 */
export async function searchWithRecencyOptions() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Modify the basic entry set by adding another hello file with
  // a recent date. We cannot make it today's date as those dates
  // are rendered with 'Today' string rather than actual date string.
  const recentHello = ENTRIES.hello.cloneWith({
    nameText: 'hello-recent.txt',
    lastModifiedTime: new Date().toDateString(),
    targetPath: 'hello-recent.txt',
  });
  await addEntries(['local'], [recentHello]);
  // Unfortunately, today's files use custom date string. Make it so.
  const todayHello = recentHello.cloneWith({
    lastModifiedTime: 'Today 12:00 AM',
  });

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Expect two files, with no recency restrictions.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    todayHello,
  ]));

  // Click the fourth button, which is "Last week" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'recency', 4),
      'Failed to click "Last week" recency selector');

  // Expect only the recent hello file to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    todayHello,
  ]));
}

/**
 * Checks that when searching Google Drive we correctly match on name, not on
 * contents.
 */
export async function matchDriveFilesByName() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, BASIC_LOCAL_ENTRY_SET, [
        ENTRIES.image2,
      ]);
  await remoteCall.typeSearchText(appId, 'image2');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.image2]));
}

/**
 * Checks that changing recency options correctly filters search results on
 * drive.
 */
export async function searchDriveWithRecencyOptions() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Modify the basic entry set by adding another hello file with
  // a recent date. We cannot make it today's date as those dates
  // so it must be accessed with ['hello'].
  const recentHello = ENTRIES.hello.cloneWith({
    nameText: 'hello-recent.txt',
    lastModifiedTime: getDateWithDayDiff(3),
    targetPath: 'hello-recent.txt',
  });
  await addEntries(['drive'], [recentHello]);

  // Navigate to Google Drive. We are searching "local" directory, which limits
  // search results to Drive.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My Drive');

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Expect two files, with no recency restrictions.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    recentHello,
  ]));

  // Click the fourth button, which is "Last week" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'recency', 4),
      'Failed to click "Last week" recency selector');

  // Expect only the recent hello file to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    recentHello,
  ]));
}

/**
 * Checks that changing file types options correctly filters local
 * search results.
 */
export async function searchLocalWithTypeOptions() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'o');

  // Expect all basic files, with no type restrictions.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));

  // Click the fifth button, which is "Video" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'type', 5),
      'Failed to click "Videos" type selector');

  // Expect only world, which is a video file.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.world,
  ]));
}

/**
 * Checks that changing file types options correctly filters
 * Drive search results.
 */
export async function searchDriveWithTypeOptions() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Navigate to Google Drive; make sure we have the desired files.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My Drive');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_DRIVE_ENTRY_SET));

  // Search the Drive for all files with "b" in their name.
  await remoteCall.typeSearchText(appId, 'b');

  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.desktop,
    ENTRIES.beautiful,
  ]));

  // Click the second button, which is "Audio" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'type', 2),
      'Failed to click "Audio" type selector');

  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.beautiful,
  ]));
}

/**
 * @param withPartitions Whether or not USB has partitions.
 * @return The label that can be used to query for elements.
 */
function getUsbVolumeQuery(withPartitions: boolean): string {
  return withPartitions ? 'Drive Label' : 'fake-usb';
}

/**
 * @param appId The ID of the files app under test.
 * @param withPartitions Whether or not USB has partitions.
 */
async function mountUsb(appId: string, withPartitions: boolean) {
  const nameSuffix = withPartitions ? 'UsbWithPartitions' : 'FakeUsb';
  await sendTestMessage({name: `mount${nameSuffix}`});
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel(getUsbVolumeQuery(withPartitions));
}

/**
 * Checks that the new search correctly finds files on a USB drive.
 */
export async function searchRemovableDevice() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  // Mount a USB with no partitions.
  await mountUsb(appId, false);

  // Navigate to the root of the USB.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel(getUsbVolumeQuery(false));

  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        ENTRIES.hello,
      ]),
      {ignoreLastModifiedTime: true});
}

/**
 * Checks that the new search correctly finds files on a USB drive with multiple
 * partitions.
 */
export async function searchPartitionedRemovableDevice() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await mountUsb(appId, /* withPartitions= */ true);

  // Wait for removable partition-1 to appear in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel(
      getUsbVolumeQuery(/* withPartitions= */ true));
  const partitionOne = await directoryTree.waitForItemByLabel('partition-1');
  chrome.test.assertEq(
      'removable', directoryTree.getItemVolumeType(partitionOne));

  // Wait for removable partition-2 to appear in the directory tree.
  const partitionTwo = await directoryTree.waitForItemByLabel('partition-2');
  chrome.test.assertEq(
      'removable', directoryTree.getItemVolumeType(partitionTwo));

  // Navigate to the root of the USB.
  await directoryTree.selectItemByLabel(getUsbVolumeQuery(true));

  // Search for the 'hello' and expect two files; ignore the modified time
  // as these were copied when mounting the USB.
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        ENTRIES.hello,
        ENTRIES.hello,
      ]),
      {ignoreLastModifiedTime: true});
}
/**
 * Checks that the search options are reset to default on folder change.
 */
export async function resetSearchOptionsOnFolderChange() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Type something into the search query to see search options.
  await remoteCall.typeSearchText(appId, 'b');

  // Check the defaults.
  chrome.test.assertEq(
      'Downloads', await getSelectedOptionText(appId, 'location'));
  chrome.test.assertEq(
      'Any time', await getSelectedOptionText(appId, 'recency'));
  chrome.test.assertEq('All types', await getSelectedOptionText(appId, 'type'));

  // Change options.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'type', 2),
      'Failed to change to "Audio" type selector');
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'recency', 4),
      'Failed to change to "Last week" recency selector');

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');

  // Start search again.
  await remoteCall.typeSearchText(appId, 'b');

  // Check that we are back to defaults.
  chrome.test.assertEq(
      'photos', await getSelectedOptionText(appId, 'location'));
  chrome.test.assertEq(
      'Any time', await getSelectedOptionText(appId, 'recency'));
  chrome.test.assertEq('All types', await getSelectedOptionText(appId, 'type'));
}

/**
 * Checks that we are showing the correct message in breadcrumbs when search is
 * active.
 */
export async function showSearchResultMessageWhenSearching() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check that we start with My Files
  const beforeSearchPath =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq('/My files/Downloads', beforeSearchPath);

  // Type something into the search query to start search.
  await remoteCall.typeSearchText(appId, 'b');

  // Wait for the search to fully expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Check that the breadcumb shows that we are searching.
  const duringSearchPath =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq('/Search results', duringSearchPath);

  // Clear and close search.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');

  // Wait for the search to fully close.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Expect the path to return to the original path.
  const afterSearchPath =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq(beforeSearchPath, afterSearchPath);
}

/**
 * Checks that search works correctly when starting in My Files.
 */
export async function searchFromMyFiles() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files');
  const beforeSearchPath =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq('/My files', beforeSearchPath);

  // Make sure the search returns a matching file even when originating in My
  // Files rather than My Files/Downloads directory.
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
  ]));

  // Close the search before adding Linux files.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Add Linux files.
  await remoteCall.mountCrostini(appId);
  // Add some Linux specific files.
  await addEntries(['crostini'], [ENTRIES.debPackage]);
  // Navigate back to /My files
  await directoryTree.navigateToPath('/My files');

  // Search for files containing ack (should include debPackage.
  await remoteCall.typeSearchText(appId, 'ack');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.desktop,
    ENTRIES.desktop,
    ENTRIES.debPackage,
  ]));
}

/**
 * Checks that the selection path correctly reflects paths of elements found by
 * search.
 */
export async function selectionPath() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, NESTED_ENTRY_SET.concat([
        ENTRIES.hello,
        ENTRIES.desktop,
        ENTRIES.deeplyBuriedSmallJpeg,
      ]));
  // Search for files containing 'e'; should be three of those.
  await remoteCall.typeSearchText(appId, 'e');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    ENTRIES.desktop,
    ENTRIES.deeplyBuriedSmallJpeg,
  ]));
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);
  const breadcrumbSingleSelection = await remoteCall.waitForElement(appId, [
    '#search-breadcrumb',
  ]);
  chrome.test.assertFalse(breadcrumbSingleSelection.hidden);
  chrome.test.assertEq(
      'My files/Downloads/' + ENTRIES.hello.nameText,
      breadcrumbSingleSelection.attributes['path']);
  // Select now the desktop entry, too. Two or more selected files,
  // regardless of the directory in which they sit, result in no path.
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${ENTRIES.desktop.nameText}"]`,
      {ctrl: true});
  const breadcrumbDoubleSelection = await remoteCall.waitForElement(appId, [
    '#search-breadcrumb',
  ]);
  chrome.test.assertTrue(breadcrumbDoubleSelection.hidden);
  chrome.test.assertEq('', breadcrumbDoubleSelection.attributes['path']);
  await remoteCall.waitAndClickElement(
      appId,
      `#file-list [file-name="${ENTRIES.deeplyBuriedSmallJpeg.nameText}"]`,
      {ctrl: true});
  const breadcrumbTripleSelection = await remoteCall.waitForElement(appId, [
    '#search-breadcrumb',
  ]);
  chrome.test.assertTrue(breadcrumbTripleSelection.hidden);
  chrome.test.assertEq('', breadcrumbTripleSelection.attributes['path']);

  // Close search. Select any file. Confirm that the path display is not shown,
  // now that the search is inactive.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);
  const pathDisplayWhileNotSearching = await remoteCall.waitForElement(appId, [
    '#search-breadcrumb',
  ]);
  chrome.test.assertTrue(pathDisplayWhileNotSearching.hidden);
}

/**
 * Checks that we correctly traverse search hierarchy. If you start searching in
 * a local folder, the search should search it and its subfolders only. If we
 * change the location to be the root directory, it should correctly search any
 * folders (including Linux and Playfiles, if necessary) that are visually
 * under the root folder. Finally, search everywhere should search everything
 * we can search (Google Doc, removable drives, local file syste, * etc.).
 */
export async function searchHierarchy() {
  // hello file stored in My files/Downloads/photos.
  const photosHello = ENTRIES.hello.cloneWith({
    targetPath: 'photos/photos-hello.txt',
    nameText: 'photos-hello.txt',
  });
  // hello file stored in My files
  const myFilesHello = ENTRIES.hello.cloneWith({
    targetPath: 'my-files-hello.txt',
    nameText: 'my-files-hello.txt',
  });
  // hello file stored on Linux.
  const linuxHello = ENTRIES.hello.cloneWith({
    targetPath: 'linux-hello.txt',
    nameText: 'linux-hello.txt',
  });
  // hello file stored on a removable drive.
  const usbHello = ENTRIES.hello.cloneWith({
    targetPath: 'usb-hello.txt',
    nameText: 'usb-hello.txt',
  });
  // hello file stored on Google Drive.
  const driveHello = ENTRIES.hello.cloneWith({
    targetPath: 'drive-hello.txt',
    nameText: 'drive-hello.txt',
  });
  // hello file stored in playfiles.
  const playfilesHello = ENTRIES.hello.cloneWith({
    targetPath: 'Documents/playfile-hello.txt',
    nameText: 'playfile-hello.txt',
  });

  // Set up the app. This creates entries in My files and Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [myFilesHello, ENTRIES.photos, photosHello],
      [driveHello]);

  // Mount USBs.
  await mountUsb(appId, false);

  // Add Linux files.
  await remoteCall.mountCrostini(appId);

  // Add custom hello files to Linux, USB, and PlayFiles.
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET.concat([
    ENTRIES.directoryDocuments,
    playfilesHello,
  ]));
  await addEntries(['usb'], [usbHello]);
  await addEntries(['crostini'], [linuxHello]);

  // Move to a nested directory under My files.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');

  // Expect photosHello, as the only result when searching in photos.
  await remoteCall.typeSearchText(appId, '-hello.txt');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([photosHello]),
      {ignoreLastModifiedTime: true});

  // Select the second button, which is root directory (My Fles in our case).
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 2),
      'Failed to click "My files" location selector');

  // Expect files from My files, Play files and Linux files.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        myFilesHello,
        photosHello,
        linuxHello,
        playfilesHello,
      ]),
      {ignoreLastModifiedTime: true});

  // Select the first button, which is Everywhere.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 1),
      'Failed to click "Everywhere" location selector');

  // Expect files from My files, Play files, Linux files, USB, and Drive.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        myFilesHello,
        photosHello,
        linuxHello,
        playfilesHello,
        driveHello,
        usbHello,
      ]),
      {ignoreLastModifiedTime: true});
}

/**
 * Checks that search is not visible when in the Trash volume.
 */
export async function hideSearchInTrash() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  // Make sure that the search button is visible in Downloads.
  await remoteCall.waitForElement(appId, '#search-button');
  let searchButton = await remoteCall.waitForElementStyles(
      appId, ['#search-button'], ['display']);
  chrome.test.assertTrue(searchButton.styles!['display'] !== 'none');

  // Navigate to Trash and confirm that the search button is now hidden.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  searchButton = await remoteCall.waitForElementStyles(
      appId, ['#search-button'], ['visibility']);
  chrome.test.assertTrue(searchButton.styles!['visibility'] === 'hidden');

  // Try to use keyboard shortcuts Ctrl+F to launch search anyway.
  const ctrlF = ['#file-list', 'f', true, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...ctrlF);

  const searchWrapper =
      await remoteCall.waitForElement(appId, ['#search-wrapper']);
  // Confirm that search wrapper is still in collapsed state.
  chrome.test.assertEq(searchWrapper.attributes['collapsed'], '');

  // Go back to Downloads and confirm that the search button is visible again.
  await directoryTree.navigateToPath('/My files/Downloads');
  searchButton = await remoteCall.waitForElementStyles(
      appId, ['#search-button'], ['visibility']);
  chrome.test.assertTrue(searchButton.styles!['visibility'] !== 'hidden');

  // Make sure that search still works.
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));
}

/**
 * Checks that files in trash do not appear in the search results when trash
 * is enabled, and appear when it is disabled.
 */
export async function searchTrashedFiles() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');
  // Confirm that we can find hello.txt.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));

  // Clear and close search.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  // Delete hello.txt and wait for it to be moved to trash.
  await remoteCall.clickTrashButton(appId);

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');
  // Confirm that we cannot find it.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([]));

  // Disable trash.
  await sendTestMessage({name: 'setTrashEnabled', enabled: false});

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');
  // Confirm that we cannot find it because these files are under .Trash which
  // won't be shown unless "Show hidden files" is on.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([]));
  // Turn on "Show hidden files" in the menu.
  await remoteCall.showHiddenFiles(appId);
  // Confirm that we can find it. We also expect trashinfo file to appear.
  const helloTrashinfo = ENTRIES.hello.cloneWith({
    nameText: 'hello.txt.trashinfo',
    typeText: 'TRASHINFO file',
  });
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello, helloTrashinfo]),
      {ignoreLastModifiedTime: true, ignoreFileSize: true});
}

/**
 * Checcks that finding files directly in Shared with me, or in folders nested
 * in Shared with me, works.
 */
export async function searchSharedWithMe() {
  // Create a shared file for nested directory. It must have SHARED_WITH_ME
  // attribute on it, as NESTED_SHARED_WITH_ME does not have shared metadata
  // set on it.
  const nestedTestSharedFile = ENTRIES.sharedWithMeDirectoryFile.cloneWith({
    sharedOption: SharedOption.SHARED_WITH_ME,
    targetPath: 'Shared Directory/nested.txt',
    nameText: 'nested.txt',
  });
  // Open Files app on Drive containing "Shared with me" file entries.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [
    ENTRIES.testSharedFile,
    ENTRIES.sharedWithMeDirectory,
    nestedTestSharedFile,
  ]);

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Shared with me');

  // Find the specific file, test.txt
  await remoteCall.typeSearchText(appId, 'test.txt');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.testSharedFile]));

  // Search for the file nested in the shared directory.
  await remoteCall.typeSearchText(appId, 'nested.txt');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([nestedTestSharedFile]));
}

/**
 * Checks that the simple search from the root of a documents provider directory
 * works. No file category or modified time filters are used.
 */
export async function searchDocumentsProvider() {
  await addEntries(
      ['documents_provider'], COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET);
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Wait for DocumentsProvider to mount and Verify that the files are visible.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToHaveChildrenByType(
      'documents_provider', /* hasChildren= */ true);

  // Search for all files with "nam" in their name.
  await directoryTree.navigateToPath('/DocumentsProvider');
  await remoteCall.typeSearchText(appId, 'nam');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.renamableFile]),
      {ignoreLastModifiedTime: true});
}

/**
 * Checks that changing file types options correctly filters
 * files exposed via Documents Provider.
 */
export async function searchDocumentsProviderWithTypeOptions() {
  await addEntries(
      ['documents_provider'], COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET);
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Wait for DocumentsProvider to mount and Verify that the files are visible.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToHaveChildrenByType(
      'documents_provider', /* hasChildren= */ true);
  await directoryTree.navigateToPath('/DocumentsProvider');

  // Search the DocumentsProvider folder for files with "File" in their name.
  await remoteCall.typeSearchText(appId, 'File');

  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.readOnlyFile,
    ENTRIES.deletableFile,
    ENTRIES.renamableFile,
  ]));

  // Click the second button, which is "Images" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'type', 4),
      'Failed to click "Images" type selector');

  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.readOnlyFile,
  ]));
}

/**
 * Checks that changing file types options correctly filters
 * files exposed via Documents Provider.
 */
export async function searchDocumentsProviderWithRecencyOptions() {
  const recentHellos = [];
  for (let i = 0; i < 10; ++i) {
    // The lastModifiedTime is set so that we cannot hit days close to current
    // date. These often are rephrased as Today, Yesterday. We use 6, so that at
    // most we get something that is 4 days old. This way, we avoid phrases
    // such as Today, Yesterday, or Two Days Ago (which exist in Japanese,
    // Polish and other languages).
    recentHellos.push(ENTRIES.hello.cloneWith({
      nameText: `hello-recent-${i}.txt`,
      lastModifiedTime: getDateWithDayDiff(6 - (i % 3)),
      targetPath: `hello-recent-${i}.txt`,
    }));
  }
  await addEntries(
      ['documents_provider'],
      COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET.concat(recentHellos));

  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Wait for DocumentsProvider to mount and Verify that the files are visible.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToHaveChildrenByType(
      'documents_provider', /* hasChildren= */ true);
  await directoryTree.navigateToPath('/DocumentsProvider');

  // Search the DocumentsProvider for files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Expect the original hello and recent hello files to be present.
  await remoteCall.waitForFiles(
      appId,
      TestEntryInfo.getExpectedRows(recentHellos.concat([ENTRIES.hello])));

  // Click the fourth button, which is "Last week" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'recency', 4),
      'Failed to click "Last week" recency selector');

  // Expect all rececent hello files to be present.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(recentHellos));
}

/**
 * Checks that search works on volumes mounted via fileSystemProvider.
 */
export async function searchFileSystemProvider() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await sendTestMessage({
    name: 'launchProviderExtension',
    manifest: 'manifest_source_device.json',
  });
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('provided');
  await directoryTree.navigateToPath('/Test (1)');
  await directoryTree.waitForFocusedItemByType('provided');
  await remoteCall.typeSearchText(appId, 'folder');
  const expectedFolder = new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'folder',
    lastModifiedTime: 'Jan 1, 2000, 1:00 PM',
    nameText: 'folder',
    sizeText: '--',
    typeText: 'Folder',
  });
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([expectedFolder]),
      {ignoreLastModifiedTime: true});
}

/**
 * Test searching images by content. There are two modes supported: search by
 * text contained in the image and search by keywords associtated with
 * objects detected in the image. The first search is known as optical character
 * recorgnition (OCR), the second as image content annotation (ICA). However,
 * from the Files app point of view there is no difference and all it knows is
 * that there are "terms" associated with images processed by the local image
 * search service. So that's all we test: that we can find images by terms
 * associated with them.
 */
export async function searchImageByContent() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello, ENTRIES.desktop, ENTRIES.image3]);

  // Pretend that the desktop image was processed by the local search service
  // and was assigned the keywords 'marsupial' and 'duck'.
  await sendTestMessage({
    name: 'setupImageTerms',
    path: ENTRIES.desktop.targetPath,
    terms: 'marsupial,duck',
  });
  // The second image, image2, had 'ghost' assigned to it.
  await sendTestMessage({
    name: 'setupImageTerms',
    path: ENTRIES.image3.targetPath,
    terms: 'ghost',
  });

  await remoteCall.typeSearchText(appId, 'marsupial');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.desktop]),
      {ignoreLastModifiedTime: true});

  // Search again, using the second term 'duck'.
  await remoteCall.typeSearchText(appId, 'duck');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.desktop]),
      {ignoreLastModifiedTime: true});

  // Search, using the term 'ghost', assigned to image3.
  await remoteCall.typeSearchText(appId, 'ghost');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.image3]),
      {ignoreLastModifiedTime: true});
}

/**
 * Checks that any search, regardless if it has results or not, is closed if we
 * navigate to another directory.
 */
export async function changingDirectoryClosesSearch() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');
}

/**
 * Check that if we are either at the top directory of Google Drive or in one of
 * the nested directories, we show the correct location. As we always search the
 * entire Google Drive, we should always show My Drive as the selected location.
 */
export async function verifyDriveLocationOption() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [
    ENTRIES.hello,
    ENTRIES.sharedDirectory,
    ENTRIES.sharedDirectoryFile,
  ]);

  // Navigate to Google Drive; make sure we have the desired files.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My Drive');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.sharedDirectory,
    ENTRIES.hello,
  ]));

  // Search the Drive for all files with "b" in their name.
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
  ]));

  // Check that the location shows My Drive.
  chrome.test.assertEq(
      'Google Drive', await getSelectedOptionText(appId, 'location'));

  await directoryTree.navigateToPath('/My Drive/Shared');
  await remoteCall.typeSearchText(appId, 'file');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.sharedDirectoryFile,
  ]));
  chrome.test.assertEq(
      'Google Drive', await getSelectedOptionText(appId, 'location'));
}

/*
 * Checks that search with non-current folder in Downloads should unselect the
 * current directory item in the tree.
 */
export async function unselectCurrentDirectoryInTreeOnSearchInDownloads() {
  // Setup default file set within Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel('Downloads');

  // Search "hello".
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));
  chrome.test.assertEq(
      'Downloads', await getSelectedOptionText(appId, 'location'));
  // Expect the Downloads folder is still selected.
  await directoryTree.waitForSelectedItemByLabel('Downloads');

  // Change location to "My files".
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 2),
      'Failed to click "My files" location selector');
  // Expect the Downloads folder to be unselected.
  await directoryTree.waitForSelectedItemLostByLabel('Downloads');

  // Change location back to "Downloads".
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 3),
      'Failed to click "Downloads" location selector');
  // Expect the Downloads folder to be selected again.
  await directoryTree.waitForSelectedItemByLabel('Downloads');

  // Change location to "My files" and click the Downloads.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 2),
      'Failed to click "My files" location selector');
  await directoryTree.waitForSelectedItemLostByLabel('Downloads');
  await directoryTree.selectItemByLabel('Downloads');
  // Expect the search will be cleared.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');
}

/**
 * Checks that search with non-root folder in Drive should unselect the current
 * directory item in the tree.
 */
export async function unselectCurrentDirectoryInTreeOnSearchInDrive() {
  // Setup Drive with Computers files.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], COMPUTERS_ENTRY_SET);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForSelectedItemByLabel('My Drive');

  // Search "txt", both My Drive and Computers folder will be searched.
  await remoteCall.typeSearchText(appId, 'txt');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    ENTRIES.computerAFile,
  ]));
  chrome.test.assertEq(
      'Google Drive', await getSelectedOptionText(appId, 'location'));
  // Expect the My Drive folder is still selected.
  await directoryTree.waitForSelectedItemByLabel('My Drive');

  // Change location to "Everywhere".
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 1),
      'Failed to click "Everywhere" location selector');
  // Expect the My Drive folder to be unselected.
  await directoryTree.waitForSelectedItemLostByLabel('My Drive');

  // Change location back to "Google Drive".
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 2),
      'Failed to click "Google Drive" location selector');
  // Expect the My Drive folder to be selected again.
  await directoryTree.waitForSelectedItemByLabel('My Drive');

  // Change location to "Everywhere" and click the My Drive.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 1),
      'Failed to click "Everywhere" location selector');
  await directoryTree.waitForSelectedItemLostByLabel('My Drive');
  await directoryTree.selectItemByLabel('My Drive');

  // Expect the search will be cleared.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');
}
