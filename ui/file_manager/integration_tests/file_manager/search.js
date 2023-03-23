// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppState} from '../files_app_state.js';
import {addEntries, ENTRIES, EntryType, getCaller, getDateWithDayDiff, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {mountCrostini, navigateWithDirectoryTree, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_FAKE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, NESTED_ENTRY_SET} from './test_data.js';

/**
 * Expected files shown in the search results for 'hello'
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
const SEARCH_RESULTS_ENTRY_SET = [
  ENTRIES.hello,
];

/**
 * @param {string} appId The ID that identifies the files app.
 * @param {string} type The search option type (location, recency, type).
 * @return {Promise<string>} The text of the element with 'selected-option' ID.
 */
async function getSelectedOptionText(appId, type) {
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
  return option.text;
}

/**
 * Tests searching inside Downloads with results.
 */
testcase.searchDownloadsWithResults = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Click on the search button to display the search box.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box [type="search"]', 'focus']));

  // Input a text.
  await remoteCall.inputText(appId, '#search-box [type="search"]', 'hello');

  // Notify the element of the input.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box [type="search"]', 'input']));

  // Wait file list to display the search result.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET));

  // Check that a11y message for results has been issued.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);

  return appId;
};

/**
 * Tests searching inside Downloads without results.
 */
testcase.searchDownloadsWithNoResults = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box [type="search"]', 'focus']));

  // Input a text.
  await remoteCall.inputText(
      appId, '#search-box [type="search"]', 'INVALID TERM');

  // Notify the element of the input.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box [type="search"]', 'input']));

  // Wait file list to display no results.
  await remoteCall.waitForFiles(appId, []);

  // Check that a11y message for no results has been issued.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq(
      'There are no results for INVALID TERM.', a11yMessages[0]);
};

/**
 * Tests that clearing the search box announces the A11y.
 */
testcase.searchDownloadsClearSearch = async () => {
  // Perform a normal search, to be able to clear the search box.
  const appId = await testcase.searchDownloadsWithResults();

  // Click on the clear search button.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');

  // Wait for the search box to fully collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Wait for file list to display all files.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));

  // Check that a11y message for clearing the search term has been issued.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(
      2, a11yMessages.length,
      `Want 2 messages got ${JSON.stringify(a11yMessages)}`);
  chrome.test.assertEq(
      'Search text cleared, showing all files and folders.', a11yMessages[1]);
};

/**
 * Tests that clearing the search box with keydown crbug.com/910068.
 */
testcase.searchDownloadsClearSearchKeyDown = async () => {
  // Perform a normal search, to be able to clear the search box.
  const appId = await testcase.searchDownloadsWithResults();

  const clearButton = '#search-box .clear';
  // Wait for clear button.
  await remoteCall.waitForElement(appId, clearButton);

  // Send a enter key to the clear button.
  const enterKey = [clearButton, 'Enter', false, false, false];
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
        await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
    if (activeElement.attributes['id'] !== 'search-button') {
      return pending(
          caller, 'Expected active element should be search-button, got %s',
          activeElement.attributes['id']);
    }
  });
};

/**
 * Tests that the search text entry box stays expanded until the end of user
 * interaction.
 */
testcase.searchHidingTextEntryField = async () => {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

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
};

/**
 * Tests that the search box collapses when empty and Tab out of the box.
 */
testcase.searchHidingViaTab = async () => {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Search box should start collapsed.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Wait for the search box to expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Returns $i18n{} label if code coverage is enabled.
  let expectedLabelText = 'Search';
  const isDevtoolsCoverageActive =
      await sendTestMessage({name: 'isDevtoolsCoverageActive'});
  if (isDevtoolsCoverageActive === 'true') {
    expectedLabelText = '$i18n{SEARCH_TEXT_LABEL}';
  }

  // Verify the search input has focus.
  const input =
      await remoteCall.callRemoteTestUtil('deepGetActiveElement', appId, []);
  chrome.test.assertEq(input.attributes['id'], 'input');
  chrome.test.assertEq(input.attributes['aria-label'], expectedLabelText);

  // Send Tab key to focus the next element.
  const result = await sendTestMessage({name: 'dispatchTabKey'});
  chrome.test.assertEq(result, 'tabKeyDispatched', 'Tab key dispatch failure');

  // Check: the search box should collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');
};

/**
 * Tests that clicking the search button expands and collapses the search box.
 */
testcase.searchButtonToggles = async () => {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

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
    if (collapsedSearchBox.renderedWidth >= element.renderedWidth) {
      return pending(caller, 'Waiting search box to expand');
    }
  });

  // Click the toolbar search button again.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Check: the search box should collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Check: the search box width should decrease.
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElementStyles(
        appId, '#search-wrapper', ['width']);
    if (collapsedSearchBox.renderedWidth < element.renderedWidth) {
      return pending(caller, 'Waiting search box to collapse');
    }
  });
};

/**
 * Tests that Files app performs a search at app start up when
 * LaunchParam.searchQuery is specified.
 */
testcase.searchQueryLaunchParam = async () => {
  addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
  addEntries(['drive'], BASIC_DRIVE_ENTRY_SET);

  // Open Files app with LaunchParam.searchQuery='gdoc'.
  const query = 'gdoc';
  /** @type {!FilesAppState} */
  const appState = {searchQuery: query};
  const appId =
      await remoteCall.callRemoteTestUtil('openMainWindow', null, [appState]);

  // Check: search box should be filled with the query.
  const caller = getCaller();
  await repeatUntil(async () => {
    const searchBoxInput =
        await remoteCall.waitForElement(appId, '#search-box cr-input');
    if (searchBoxInput.value !== query) {
      return pending(caller, 'Waiting search box to be filled with the query.');
    }
  });

  // Check: "My Drive" directory should be selected because it is the sole
  //        directory that contains query-matched files (*.gdoc).
  const selectedTreeRow = await remoteCall.waitForElement(
      appId, '#directory-tree .tree-row[selected][active]');
  chrome.test.assertTrue(selectedTreeRow.text.includes('My Drive'));

  // Check: Query-matched files should be shown in the files list.
  await repeatUntil(async () => {
    const filenameLabel =
        await remoteCall.waitForElement(appId, '#file-list .filename-label');
    if (!filenameLabel.text.includes(query)) {
      // Pre-search results might be shown only for a moment before the search
      // spinner is shown.
      return pending(caller, 'Waiting files list to be updated.');
    }
  });
};

/**
 * Checks that changing location options correctly filters search results.
 */
testcase.searchWithLocationOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Modify the basic entry set by adding nested directories and
  // a copy of the hello entry.
  const nestedHello = ENTRIES.hello.cloneWith({
    targetPath: 'A/hello.txt',
  });
  addEntries(['local'], [ENTRIES.directoryA, nestedHello]);

  // Start in the nested directory, as the default search location
  // is THIS_FOLDER. Expect to find one hello file. Then search on
  // THIS_CHROMEBOOK and expect to find two.
  await navigateWithDirectoryTree(appId, '/My files/Downloads/A');

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Verify that the search options are visible.
  await remoteCall.waitForElement(appId, 'xf-search-options:not([hidden])');

  // Expect only the nested hello to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    nestedHello,
  ]));

  // Click the second button, which is This Chromebook.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 2),
      'Failed to click "This Chromebook" location selector');

  // Expect all hello files to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    nestedHello,
  ]));
};

/**
 * Checks that changing recency options correctly filters search results.
 */
testcase.searchWithRecencyOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

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
};

/**
 * Checks that changing recency options correctly filters search results on
 * drive.
 */
testcase.searchDriveWithRecencyOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Modify the basic entry set by adding another hello file with
  // a recent date. We cannot make it today's date as those dates
  // are rendered with 'Today' string rather than actual date string.
  const recentHello = ENTRIES.hello.cloneWith({
    nameText: 'hello-recent.txt',
    lastModifiedTime: getDateWithDayDiff(3),
    targetPath: 'hello-recent.txt',
  });
  await addEntries(['drive'], [recentHello]);

  // Navigate to Google Drive. We are searching "local" directory, which limits
  // search results to Drive.
  await navigateWithDirectoryTree(appId, '/My Drive');

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
};

/**
 * Checks that changing file types options correctly filters local
 * search results.
 */
testcase.searchLocalWithTypeOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

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
};

/**
 * Checks that changing file types options correctly filters
 * Drive search results.
 */
testcase.searchDriveWithTypeOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Navigate to Google Drive; make sure we have the desired files.
  await navigateWithDirectoryTree(appId, '/My Drive');
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
};

/**
 * @param {boolean} withPartitions Whether or not USB has partitions.
 * @return {string} The label that can be used to query for elements.
 */
function getUsbVolumeQuery(withPartitions) {
  return `#directory-tree [entry-label=${
      withPartitions ? '"Drive Label"' : '"fake-usb"'}]`;
}

/**
 * @param {string} appId The ID of the files app under test.
 * @param {boolean} withPartitions Whether or not USB has partitions.
 */
async function mountUsb(appId, withPartitions) {
  const nameSuffix = withPartitions ? 'UsbWithPartitions' : 'FakeUsb';
  await sendTestMessage({name: `mount${nameSuffix}`});
  await remoteCall.waitForElement(appId, getUsbVolumeQuery(withPartitions));
}

/**
 * Checks that the new search correctly finds files on a USB drive.
 */
testcase.searchRemovableDevice = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  // Mount a USB with no partitions.
  mountUsb(appId, false);

  // Navigate to the root of the USB.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [getUsbVolumeQuery(false)]);

  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        ENTRIES.hello,
      ]),
      {ignoreLastModifiedTime: true});
};

/**
 * Checks that the new search correctly finds files on a USB drive with multiple
 * partitions.
 */
testcase.searchPartitionedRemovableDevice = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  mountUsb(appId, true);

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

  // Navigate to the root of the USB.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [getUsbVolumeQuery(true)]);

  // Search for the 'hello' and expect two files; ignore the modified time
  // as these were copied when mounting the USB.
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        ENTRIES.hello,
        ENTRIES.hello,
      ]),
      {ignoreLastModifiedTime: true});
};
/**
 * Checks that the search options are reset to default on folder change.
 */
testcase.resetSearchOptionsOnFolderChange = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Type something into the search query to see search options.
  await remoteCall.typeSearchText(appId, 'b');

  // Check the defaults.
  chrome.test.assertEq(
      'This folder', await getSelectedOptionText(appId, 'location'));
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

  await navigateWithDirectoryTree(appId, '/My files/Downloads/photos');

  // Start search again.
  await remoteCall.typeSearchText(appId, 'b');

  // Check that we are back to defaults.
  chrome.test.assertEq(
      'This folder', await getSelectedOptionText(appId, 'location'));
  chrome.test.assertEq(
      'Any time', await getSelectedOptionText(appId, 'recency'));
  chrome.test.assertEq('All types', await getSelectedOptionText(appId, 'type'));
};

/**
 * Checks that we are showing the correct message in breadcrumbs when search is
 * active.
 */
testcase.showSearchResultMessageWhenSearching = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

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
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Wait for the search to fully close.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Expect the path to return to the original path.
  const afterSearchPath =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq(beforeSearchPath, afterSearchPath);
};

/**
 * Tests that the educational nudge is displayed when the files
 * app is started with V2 version of search enabled.
 */
testcase.showsEducationNudge = async () => {
  // Open the Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Check that the nudge and its text is visible.
  await remoteCall.waitNudge(appId, 'New search features available');
};

/**
 * Checks that search works correctly when starting in My Files.
 */
testcase.searchFromMyFiles = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await navigateWithDirectoryTree(appId, '/My files');
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
  await mountCrostini(appId);
  // Add some Linux specific files.
  await addEntries(['crostini'], [ENTRIES.debPackage]);
  // Navigate back to /My files
  await navigateWithDirectoryTree(appId, '/My files');

  // Search for files containing ack (should include debPackage.
  await remoteCall.typeSearchText(appId, 'ack');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.desktop,
    ENTRIES.desktop,
    ENTRIES.debPackage,
  ]));
};

/**
 * Checks that the selection path correctly reflects paths of elements found by
 * search.
 */
testcase.selectionPath = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, NESTED_ENTRY_SET.concat([
        ENTRIES.hello,
        ENTRIES.desktop,
        ENTRIES.deeplyBurriedSmallJpeg,
      ]));
  // Search for files containing 'e'; should be three of those.
  await remoteCall.typeSearchText(appId, 'e');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    ENTRIES.desktop,
    ENTRIES.deeplyBurriedSmallJpeg,
  ]));
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);
  const singleSelectionPath = await remoteCall.waitForElement(appId, [
    'xf-path-display',
  ]);
  chrome.test.assertEq(
      'My files/Downloads', singleSelectionPath.attributes.path);
  // Select now the desktop entry, too. Both are in the same
  // directory, so the path should not change.
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${ENTRIES.desktop.nameText}"]`,
      {ctrl: true});
  const twoFilesOneFolderPath = await remoteCall.waitForElement(appId, [
    'xf-path-display',
  ]);
  chrome.test.assertEq(
      'My files/Downloads', twoFilesOneFolderPath.attributes.path);
  await remoteCall.waitAndClickElement(
      appId,
      `#file-list [file-name="${ENTRIES.deeplyBurriedSmallJpeg.nameText}"]`,
      {ctrl: true});
  const threeFilesTwoFolderPath = await remoteCall.waitForElement(appId, [
    'xf-path-display',
  ]);
  chrome.test.assertEq(
      'Multiple file locations', threeFilesTwoFolderPath.attributes.path);
};
