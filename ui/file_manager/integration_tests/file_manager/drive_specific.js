// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Expected autocomplete results for 'hello'.
 * @type {Array<string>}
 * @const
 */
const EXPECTED_AUTOCOMPLETE_LIST = [
  '\'hello\' - search Drive',
  'hello.txt',
];

/**
 * Expected files shown in the search results for 'hello'
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
const SEARCH_RESULTS_ENTRY_SET = [
  ENTRIES.hello,
];

/**
 * Returns the steps to start a search for 'hello' and wait for the
 * autocomplete results to appear.
 */
async function startDriveSearchWithAutoComplete() {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['#search-box cr-input', 'hello']);

  // Notify the element of the input.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']));

  // Wait for the auto complete list getting the expected contents.
  const caller = getCaller();
  await repeatUntil(async () => {
    const elements = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, ['#autocomplete-list li']);
    const list = elements.map((element) => element.text);
    return chrome.test.checkDeepEq(EXPECTED_AUTOCOMPLETE_LIST, list) ?
        undefined :
        pending(caller, 'Current auto complete list: %j.', list);
  });
  return appId;
}

/**
 * Tests opening the "Offline" on the sidebar navigation by clicking the icon,
 * and checks contents of the file list. Only the entries "available offline"
 * should be shown. "Available offline" entries are hosted documents and the
 * entries cached by DriveCache.
 */
testcase.driveOpenSidebarOffline = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Click the icon of the Offline volume.
  chrome.test.assertFalse(!await remoteCall.callRemoteTestUtil(
      'selectVolume', appId, ['drive_offline']));

  // Check: the file list should display the offline file set.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(OFFLINE_ENTRY_SET));
};

/**
 * Tests opening the "Shared with me" on the sidebar navigation by clicking the
 * icon, and checks contents of the file list. Only the entries labeled with
 * "shared-with-me" should be shown.
 */
testcase.driveOpenSidebarSharedWithMe = async () => {
  // Open Files app on Drive containing "Shared with me" file entries.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.sharedDirectory,
        ENTRIES.sharedDirectoryFile,
      ]));

  // Click the icon of the Shared With Me volume.
  // Use the icon for a click target.
  chrome.test.assertFalse(!await remoteCall.callRemoteTestUtil(
      'selectVolume', appId, ['drive_shared_with_me']));

  // Wait until the breadcrumb path is updated.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Verify the file list.
  await remoteCall.waitForFiles(
      appId,
      TestEntryInfo.getExpectedRows(
          SHARED_WITH_ME_ENTRY_SET.concat([ENTRIES.sharedDirectory])));

  // Navigate to the directory within Shared with me.
  chrome.test.assertFalse(!await remoteCall.callRemoteTestUtil(
      'openFile', appId, ['Shared Directory']));

  // Wait until the breadcrumb path is updated.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/Shared with me/Shared Directory');

  // Verify the file list.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.sharedDirectoryFile]));
};

/**
 * Tests autocomplete with a query 'hello'.
 */
testcase.driveAutoCompleteQuery = async () => {
  return startDriveSearchWithAutoComplete();
};

/**
 * Tests that clicking the first option in the autocomplete box shows all of
 * the results for that query.
 */
testcase.driveClickFirstSearchResult = async () => {
  const appId = await startDriveSearchWithAutoComplete();
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['#autocomplete-list', 'ArrowDown', false, false, false]));

  await remoteCall.waitForElement(appId, ['#autocomplete-list li[selected]']);
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDown', appId, ['#autocomplete-list li[selected]']));

  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET));

  // Fetch A11y messages.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);
};

/**
 * Tests that pressing enter after typing a search shows all of
 * the results for that query.
 */
testcase.drivePressEnterToSearch = async () => {
  const appId = await startDriveSearchWithAutoComplete();
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['#search-box cr-input', 'Enter', false, false, false]));
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET));

  // Fetch A11y messages.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);

  return appId;
};

/**
 * Tests that pressing the clear search button announces an a11y message and
 * shows all files/folders.
 */
testcase.drivePressClearSearch = async () => {
  const appId = await testcase.drivePressEnterToSearch();

  // Click on the clear search button.
  await remoteCall.waitAndClickElement(appId, '#search-box cr-input .clear');

  // Wait for fil list to display all files.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_DRIVE_ENTRY_SET));

  // Check that a11y message for clearing the search term has been issued.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(2, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq(
      'Search text cleared, showing all files and folders.', a11yMessages[1]);
};

/**
 * Tests pinning a file to a mobile network.
 */
testcase.drivePinFileMobileNetwork = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);
  const caller = getCaller();
  await sendTestMessage({name: 'useCellularNetwork'});
  await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']);
  await repeatUntil(() => {
    return navigator.connection.type != 'cellular' ?
        pending(caller, 'Network state is not changed to cellular.') :
        null;
  });
  await remoteCall.waitForElement(appId, ['.table-row[selected]']);
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait menu to appear and click on toggle pinned.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');
  await remoteCall.waitAndClickElement(
      appId, '[command="#toggle-pinned"]:not([hidden]):not([disabled])');

  // Wait the toggle pinned async action to finish, so the next call to display
  // context menu is after the action has finished.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Wait the pinned action to finish, it's flagged in the file list by
  // removing CSS class "dim-offline".
  await remoteCall.waitForElementLost(
      appId, '#file-list .dim-offline[file-name="hello.txt"]');


  // Open context menu again.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#file-list', 'contextmenu']));

  // Check: File is pinned.
  await remoteCall.waitForElement(appId, '[command="#toggle-pinned"][checked]');
  await repeatUntil(async () => {
    const idSet =
        await remoteCall.callRemoteTestUtil('getNotificationIDs', null, []);
    return !idSet['disabled-mobile-sync'] ?
        pending(caller, 'Sync disable notification is not found.') :
        null;
  });
  await sendTestMessage({
    name: 'clickNotificationButton',
    extensionId: FILE_MANAGER_EXTENSIONS_ID,
    notificationId: 'disabled-mobile-sync',
    index: 0
  });
  await repeatUntil(async () => {
    const preferences =
        await remoteCall.callRemoteTestUtil('getPreferences', null, []);
    return preferences.cellularDisabled ?
        pending(caller, 'Drive sync is still disabled.') :
        null;
  });
};

/**
 * Tests that pressing Ctrl+A (select all files) from the search box doesn't put
 * the Files App into check-select mode (crbug.com/849253).
 */
testcase.drivePressCtrlAFromSearch = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#search-button']));

  // Wait for the search box to be visible.
  await remoteCall.waitForElement(
      appId, ['#search-box cr-input:not([hidden])']);

  // Press Ctrl+A inside the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['#search-box cr-input', 'A', true, false, false]));

  // Check we didn't enter check-select mode.
  await remoteCall.waitForElement(appId, ['body:not(.check-select)']);
};

// Match the way the production version formats dates.
function formatDate(date) {
  const padAndConvert = i => {
    return (i < 10 ? '0' : '') + i.toString();
  };

  const year = date.getFullYear().toString();
  // Months are 0-based, but days aren't.
  const month = padAndConvert(date.getMonth() + 1);
  const day = padAndConvert(date.getDate());

  return `${year}-${month}-${day}`;
}

/**
 * Test that a images within a DCIM directory on removable media is backed up to
 * Drive, in the Chrome OS Cloud backup/<current date> directory.
 */
testcase.driveBackupPhotos = async () => {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';
  let date;

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeUsbDcim'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Navigate to the DCIM directory.
  await remoteCall.navigateWithDirectoryTree(
      appId, '/DCIM', 'fake-usb', 'removable');

  // Wait for the import button to be ready.
  await remoteCall.waitForElement(
      appId, '#cloud-import-button [icon="files:cloud-upload"]');

  // Start the import.
  date = new Date();
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#cloud-import-button']));

  // Wait for the image to be marked as imported.
  await remoteCall.waitForElement(
      appId, '.status-icon[file-status-icon="imported"]');

  // Navigate to today's backup directory in Drive.
  const formattedDate = formatDate(date);
  await remoteCall.navigateWithDirectoryTree(
      appId, `/root/Chrome OS Cloud backup/${formattedDate}`, 'My Drive',
      'drive');

  // Verify the backed-up file list contains only a copy of the image within
  // DCIM in the removable storage.
  const files = TestEntryInfo.getExpectedRows([ENTRIES.image3]);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Verify that "Available Offline" is available from the gear menu for a drive
 * file before the context menu has been opened.
 */
testcase.driveAvailableOfflineGearMenu = async () => {
  const pinnedMenuQuery = '#file-context-menu:not([hidden]) ' +
      'cr-menu-item[command="#toggle-pinned"]:not([disabled])';

  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Select a file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']),
      'selectFile failed');

  // Wait for the entry to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Click on the icon of the file to check select it
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#file-list .table-row[selected] .detail-checkmark']);

  // Ensure gear button is available
  await remoteCall.waitForElement(appId, '#selection-menu-button');

  // Click on gear menu and ensure "Available Offline" is shown.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#selection-menu-button']));

  // Check that "Available Offline" is shown in the menu.
  await remoteCall.waitForElement(appId, pinnedMenuQuery);
};

/**
 * Verify that "Available Offline" is available from the gear menu for a drive
 * directory before the context menu has been opened.
 */
testcase.driveAvailableOfflineDirectoryGearMenu = async () => {
  const pinnedMenuQuery = '#file-context-menu:not([hidden]) ' +
      'cr-menu-item[command="#toggle-pinned"]:not([disabled])';

  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Select a file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['photos']),
      'selectFile failed');

  // Wait for the entry to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Click on the icon of the file to check select it
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#file-list .table-row[selected] .detail-checkmark']);

  // Ensure gear button is available
  await remoteCall.waitForElement(appId, '#selection-menu-button');

  // Click on gear menu and ensure "Available Offline" is shown.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#selection-menu-button']));

  // Check that "Available Offline" is shown in the menu.
  await remoteCall.waitForElement(appId, pinnedMenuQuery);
};

/**
 * Tests following links to folders.
 */
testcase.driveLinkToDirectory = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.directoryA,
        ENTRIES.directoryB,
        ENTRIES.directoryC,
        ENTRIES.deeplyBurriedSmallJpeg,
        ENTRIES.linkGtoB,
        ENTRIES.linkHtoFile,
        ENTRIES.linkTtoTransitiveDirectory,
      ]));

  // Select the link
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['G']),
      'selectFile failed');
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Open the link
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['G']));

  // Check the contents of current directory.
  await remoteCall.waitForFiles(appId, [ENTRIES.directoryC.getExpectedRow()]);
};

/**
 * Tests opening files through folder links.
 */
testcase.driveLinkOpenFileThroughLinkedDirectory = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.directoryA,
        ENTRIES.directoryB,
        ENTRIES.directoryC,
        ENTRIES.deeplyBurriedSmallJpeg,
        ENTRIES.linkGtoB,
        ENTRIES.linkHtoFile,
        ENTRIES.linkTtoTransitiveDirectory,
      ]));

  // Navigate through link.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['G']));
  await remoteCall.waitForFiles(appId, [ENTRIES.directoryC.getExpectedRow()]);
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['C']));
  await remoteCall.waitForFiles(
      appId, [ENTRIES.deeplyBurriedSmallJpeg.getExpectedRow()]);

  await sendTestMessage(
      {name: 'expectFileTask', fileNames: ['deep.jpg'], openType: 'launch'});
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['deep.jpg']));

  // The Gallery window should open with the image in it.
  const galleryAppId = await galleryApp.waitForWindow('gallery.html');
  await galleryApp.waitForSlideImage(galleryAppId, 100, 100, 'deep');
  chrome.test.assertTrue(
      await galleryApp.closeWindowAndWait(galleryAppId),
      'Failed to close Gallery window');
};

/**
 * Tests opening files through transitive links.
 */
testcase.driveLinkOpenFileThroughTransitiveLink = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.directoryA,
        ENTRIES.directoryB,
        ENTRIES.directoryC,
        ENTRIES.deeplyBurriedSmallJpeg,
        ENTRIES.linkGtoB,
        ENTRIES.linkHtoFile,
        ENTRIES.linkTtoTransitiveDirectory,
      ]));

  // Navigate through link.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['T']));
  await remoteCall.waitForFiles(
      appId, [ENTRIES.deeplyBurriedSmallJpeg.getExpectedRow()]);

  await sendTestMessage(
      {name: 'expectFileTask', fileNames: ['deep.jpg'], openType: 'launch'});
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['deep.jpg']));

  // The Gallery window should open with the image in it.
  const galleryAppId = await galleryApp.waitForWindow('gallery.html');
  await galleryApp.waitForSlideImage(galleryAppId, 100, 100, 'deep');
  chrome.test.assertTrue(
      await galleryApp.closeWindowAndWait(galleryAppId),
      'Failed to close Gallery window');
};
