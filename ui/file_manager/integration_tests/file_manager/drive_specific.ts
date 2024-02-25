// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createTestFile, ENTRIES, EntryType, getCaller, getUserActionCount, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo, waitForMediaApp} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_DRIVE_ENTRY_SET, FakeTask, FILE_MANAGER_EXTENSIONS_ID, OFFLINE_ENTRY_SET, SHARED_WITH_ME_ENTRY_SET} from './test_data.js';

/**
 * Expected files shown in the search results for 'hello'
 */
const SEARCH_RESULTS_ENTRY_SET = [
  ENTRIES.hello,
];

/**
 * Expected text shown in the Enable Docs Offline dialog.
 */
const ENABLE_DOCS_OFFLINE_MESSAGE =
    'Enable Google Docs Offline to make Docs, Sheets and Slides ' +
    'available offline.';

/** The id attribute of the dismiss button in the educational banner. */
async function getDismissButtonId(appId: string) {
  return await remoteCall.isCrosComponents(appId) ? '#dismiss-button' :
                                                    '#dismiss-button-old';
}

/**
 * Opens the Enable Docs Offline dialog and waits for it to appear in the given
 * `appId` window.
 *
 */
async function openAndWaitForEnableDocsOfflineDialog(appId: string) {
  // Simulate Drive signalling Files App to open a dialog.
  await sendTestMessage({name: 'displayEnableDocsOfflineDialog'});

  // Check: the Enable Docs Offline dialog should appear.
  const dialogText = await remoteCall.waitForElement(
      appId, '.cr-dialog-container.shown .cr-dialog-text');
  chrome.test.assertEq(ENABLE_DOCS_OFFLINE_MESSAGE, dialogText.text);
}

/**
 * Waits for getLastDriveDialogResult to return the given |expectedResult|.
 */
async function waitForLastDriveDialogResult(expectedResult: string) {
  const caller = getCaller();
  await repeatUntil(async () => {
    const result = await sendTestMessage({name: 'getLastDriveDialogResult'});
    if (result === expectedResult) {
      return;
    }
    return pending(
        caller, 'Waiting for getLastDriveDialogResult: expected %s, actual %s',
        expectedResult, result);
  });
}

/**
 * Waits for a given notification to appear.
 * @param notificationId ID of notification to wait for.
 */
async function waitForNotification(notificationId: string) {
  const caller = getCaller();
  await repeatUntil(async () => {
    const idSet = await remoteCall.callRemoteTestUtil<Record<string, boolean>>(
        'getNotificationIDs', null, []);
    return !idSet[notificationId] ?
        pending(
            caller, 'Waiting for notification "%s" to appear.',
            notificationId) :
        null;
  });
}

/**
 * Tests opening the "Offline" on the sidebar navigation by clicking the icon,
 * and checks contents of the file list. Only the entries "available offline"
 * should be shown. "Available offline" entries are hosted documents and the
 * entries cached by DriveCache.
 */
export async function driveOpenSidebarOffline() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  // Click the icon of the Offline volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Offline');

  // Check: the file list should display the offline file set.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(OFFLINE_ENTRY_SET));
}

/**
 * Tests opening the "Shared with me" on the sidebar navigation by clicking the
 * icon, and checks contents of the file list. Only the entries labeled with
 * "shared-with-me" should be shown.
 */
export async function driveOpenSidebarSharedWithMe() {
  // Open Files app on Drive containing "Shared with me" file entries.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.sharedWithMeDirectory,
        ENTRIES.sharedWithMeDirectoryFile,
      ]));

  // Click the icon of the Shared With Me volume.
  // Use the icon for a click target.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Shared with me');

  // Wait until the breadcrumb path is updated.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Verify the file list.
  await remoteCall.waitForFiles(
      appId,
      TestEntryInfo.getExpectedRows(
          SHARED_WITH_ME_ENTRY_SET.concat([ENTRIES.sharedWithMeDirectory])));

  // Navigate to the directory within Shared with me.
  chrome.test.assertFalse(!await remoteCall.callRemoteTestUtil(
      'openFile', appId, ['Shared Directory']));

  // Wait until the breadcrumb path is updated.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/Shared with me/Shared Directory');

  // Verify the file list.
  await remoteCall.waitForFiles(
      appId,
      TestEntryInfo.getExpectedRows([ENTRIES.sharedWithMeDirectoryFile]));
}

/**
 * Tests that pressing enter after typing a search shows all of
 * the results for that query.
 */
export async function drivePressEnterToSearch() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  remoteCall.typeSearchText(appId, 'hello');

  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['#search-box cr-input', 'Enter', false, false, false]));
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET));

  // Fetch A11y messages.
  const a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);

  return appId;
}

/**
 * Tests that pressing the clear search button announces an a11y message and
 * shows all files/folders.
 */
export async function drivePressClearSearch() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);

  // Start the search from a sub-folder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My Drive/photos');

  // Search the text.
  remoteCall.typeSearchText(appId, 'hello');

  // Wait for the result in the file list.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET));

  // Click on the clear search button.
  await remoteCall.waitAndClickElement(appId, '#search-box cr-input .clear');

  // Wait for fil list to display all files.
  await remoteCall.waitForFiles(appId, []);

  // Check that a11y message for clearing the search term has been issued.
  const a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(2, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq(
      'Search text cleared, showing all files and folders.', a11yMessages[1]);

  // The breadcrumbs should return back to the previous original folder.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My Drive/photos');
}

/**
 * Tests that pinning multiple files affects the pin action of individual
 * files.
 */
export async function drivePinMultiple() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  // Select world.ogv.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="world.ogv"]');
  await remoteCall.waitForElement(appId, '[file-name="world.ogv"][selected]');

  // Open the context menu once the file is selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Check that the pin action is unticked, i.e. the action will pin the file.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"]:not([checked])');

  // Additionally select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {shift: true});
  await remoteCall.waitForElement(appId, '[file-name="hello.txt"][selected]');

  // Open the context menu with both files selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Pin both files.
  await remoteCall.waitAndClickElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"]:not([checked])');

  // Wait for the toggle pinned async action to finish, so the next call to
  // display context menu is after the action has finished.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Wait for the pinned action to finish, it's flagged in the file list by
  // removing CSS class "dim-offline" and adding class "pinned".
  await remoteCall.waitForElementLost(
      appId, '#file-list .dim-offline[file-name="world.ogv"]');
  await remoteCall.waitForElement(
      appId, '#file-list .pinned[file-name="world.ogv"] xf-inline-status');

  // Select world.ogv by itself.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="world.ogv"]');

  // Wait for hello.txt to be unselected.
  await remoteCall.waitForElement(
      appId, '[file-name="hello.txt"]:not([selected])');

  // Open the context menu for world.ogv.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Check that the pin action is ticked, i.e. the action will unpin the file.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"][checked]');
}

/**
 * Tests that pinning hosted files without the required extensions is disabled,
 * and that it does not affect multiple selections with non-hosted files.
 */
export async function drivePinHosted() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  // Select Test Document.gdoc.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="Test Document.gdoc"]');
  await remoteCall.waitForElement(
      appId, '[file-name="Test Document.gdoc"][selected]');

  // Open the context menu once the file is selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Check that the pin action is disabled and unticked.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"][disabled]:not([checked])');

  // Additionally select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {shift: true});
  await remoteCall.waitForElement(appId, '[file-name="hello.txt"][selected]');

  // Open the context menu with both files selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // The pin action should be enabled to pin only hello.txt, so select it.
  await remoteCall.waitAndClickElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"]:not([checked]):not([disabled])');

  // Wait for the toggle pinned async action to finish, so the next call to
  // display context menu is after the action has finished.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Wait for the pinned action to finish, it's flagged in the file list by
  // removing CSS class "dim-offline" and adding class "pinned".
  await remoteCall.waitForElementLost(
      appId, '#file-list .dim-offline[file-name="hello.txt"]');
  await remoteCall.waitForElement(
      appId, '#file-list .pinned[file-name="hello.txt"] xf-inline-status');

  // Test Document.gdoc should not be pinned however.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="Test Document.gdoc"]:not(.pinned)');


  // Open the context menu with both files selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Check that the pin action is ticked, i.e. the action will unpin the file.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"][checked]:not([disabled])');
}

/**
 * Tests pinning a file to a mobile network.
 * TODO(b/296960734): Fix this test once the notification has been fixed.
 */
export async function drivePinFileMobileNetwork() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);
  const caller = getCaller();
  await sendTestMessage({name: 'useCellularNetwork'});
  await remoteCall.waitUntilSelected(appId, 'hello.txt');
  await repeatUntil(() => {
    return navigator.connection.type !== 'cellular' ?
        pending(caller, 'Network state is not changed to cellular.') :
        null;
  });
  await remoteCall.waitForElement(appId, ['.table-row[selected]']);
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the menu to appear and click on toggle pinned.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');
  await remoteCall.waitAndClickElement(
      appId, '[command="#toggle-pinned"]:not([hidden]):not([disabled])');

  // Wait for the toggle pinned async action to finish, so the next call to
  // display context menu is after the action has finished.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Wait for the pinned action to finish, it's flagged in the file list by
  // removing CSS class "dim-offline".
  await remoteCall.waitForElementLost(
      appId, '#file-list .dim-offline[file-name="hello.txt"]');


  // Open context menu again.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#file-list', 'contextmenu']));

  // Check: File is pinned.
  await remoteCall.waitForElement(appId, '[command="#toggle-pinned"][checked]');
  await remoteCall.waitForElement(
      appId, '#file-list .pinned[file-name="hello.txt"] xf-inline-status');
  await waitForNotification('disabled-mobile-sync');
  await sendTestMessage({
    name: 'clickNotificationButton',
    extensionId: FILE_MANAGER_EXTENSIONS_ID,
    notificationId: 'disabled-mobile-sync',
    index: 0,
  });
  await repeatUntil(async () => {
    const preferences =
        await remoteCall
            .callRemoteTestUtil<chrome.fileManagerPrivate.Preferences>(
                'getPreferences', null, []);
    return !preferences.driveSyncEnabledOnMeteredNetwork ?
        pending(caller, 'Drive sync is still disabled.') :
        null;
  });
}

/**
 * Tests that the pinned toggle in the toolbar updates on pinned state changes
 * within fake entries.
 */
export async function drivePinToggleUpdatesInFakeEntries() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  // Navigate to the Offline fake entry.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Offline');

  // Bring up the context menu for test.txt.
  await remoteCall.waitAndRightClick(
      appId, '#file-list [file-name="test.txt"]');

  // The pinned toggle should update to be checked.
  await remoteCall.waitForElementJelly(
      appId, '#pinned-toggle-jelly[selected]', '#pinned-toggle[checked]');

  // Unpin the file.
  await remoteCall.waitAndClickElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"][checked]');

  // The pinned toggle should change to be unchecked.
  await remoteCall.waitForElementJelly(
      appId, '#pinned-toggle-jelly:not([selected])',
      '#pinned-toggle:not([checked])');

  // Navigate to the Shared with me fake entry.
  await directoryTree.navigateToPath('/Shared with me');

  // Bring up the context menu for test.txt.
  await remoteCall.waitAndRightClick(
      appId, '#file-list [file-name="test.txt"]');

  // The pinned toggle should remain unchecked.
  await remoteCall.waitForElementJelly(
      appId, '#pinned-toggle-jelly:not([selected])',
      '#pinned-toggle:not([checked])');

  // Pin the file.
  await remoteCall.waitAndClickElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"]:not([checked])');

  // The pinned toggle should change to be checked.
  await remoteCall.waitForElementJelly(
      appId, '#pinned-toggle-jelly[selected]', '#pinned-toggle[checked]');
}

/**
 * Tests that pressing Ctrl+A (select all files) from the search box doesn't
 * put the Files App into check-select mode (crbug.com/849253).
 */
export async function drivePressCtrlAFromSearch() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

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
}

/**
 * Verify that "Available Offline" is not available from the gear menu for a
 * drive file.
 */
export async function driveAvailableOfflineGearMenu() {
  const pinnedMenuQuery = '#file-context-menu:not([hidden]) ' +
      'cr-menu-item[command="#toggle-pinned"]:not([disabled])';

  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Select a file.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');

  // Wait for the entry to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Click on the icon of the file to check select it
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#file-list .table-row[selected] .detail-checkmark']);

  // Ensure gear button is available
  await remoteCall.waitForElement(appId, '#selection-menu-button');

  // Click on gear menu and ensure "Available Offline" is not shown.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#selection-menu-button']));

  // Check that "Available Offline" is not shown in the menu. This element is
  // hidden via a display:none css rule, so check that.
  const e = await remoteCall.waitForElementStyles(
      appId, pinnedMenuQuery, ['display']);
  chrome.test.assertEq('none', e.styles?.['display']);
}

/**
 * Verify that "Available Offline" is not available from the gear menu for a
 * drive directory.
 */
export async function driveAvailableOfflineDirectoryGearMenu() {
  const pinnedMenuQuery = '#file-context-menu:not([hidden]) ' +
      'cr-menu-item[command="#toggle-pinned"]:not([disabled])';

  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Select a file.
  await remoteCall.waitUntilSelected(appId, 'photos');

  // Wait for the entry to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Click on the icon of the file to check select it
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#file-list .table-row[selected] .detail-checkmark']);

  // Ensure gear button is available
  await remoteCall.waitForElement(appId, '#selection-menu-button');

  // Click on gear menu and ensure "Available Offline" is not shown.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#selection-menu-button']));

  // Check that "Available Offline" is not shown in the menu. This element is
  // hidden via a display:none css rule, so check that.
  const e = await remoteCall.waitForElementStyles(
      appId, pinnedMenuQuery, ['display']);
  chrome.test.assertEq('none', e.styles?.['display']);
}

/**
 * Verify that the "Available Offline" toggle in the action bar appears and
 * changes according to the selection.
 */
export async function driveAvailableOfflineActionBar() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Check the "Available Offline" toggle is currently hidden as no file is
  // currently selected.
  await remoteCall.waitForElement(
      appId, '#action-bar #pinned-toggle-wrapper[hidden]');

  // Select a hosted file.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="Test Document.gdoc"]');

  // Wait for the entry to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Check the "Available Offline" toggle is shown in the action bar, but
  // disabled.
  await remoteCall.waitForElementJelly(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle-jelly[disabled]:not([selected])',
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle[disabled]:not([checked])');

  // Now select a non-hosted file.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Check the "Available Offline" toggle is now enabled, and pin the file.
  remoteCall.waitAndClickElementJelly(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle-jelly:not([disabled]):not([selected])',
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle:not([disabled]):not([checked])');

  // Wait for the file to be pinned.
  await remoteCall.waitForElement(
      appId, '#file-list .pinned[file-name="hello.txt"]');


  // Hover cursor over the pinned icon.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId,
      ['#file-list .pinned[file-name="hello.txt"] xf-inline-status']));

  // Verify the correct tooltip is displayed.
  const tooltip = await remoteCall.waitForElement(
      appId, ['files-tooltip[visible=true]', '#label']);
  chrome.test.assertEq('Available offline', tooltip.text);

  // Check the "Available Offline" toggle is enabled and checked.
  await remoteCall.waitForElementJelly(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle-jelly[selected]:not([disabled])',
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle[checked]:not([disabled])');

  // Select another file that is not pinned.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="world.ogv"]');

  // Check the "Available Offline" toggle is enabled and unchecked.
  await remoteCall.waitForElementJelly(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle-jelly:not([disabled]):not([selected])',
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle:not([disabled]):not([checked])');

  // Reselect the previously pinned file.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Check the "Available Offline" toggle is enabled and checked.
  await remoteCall.waitForElementJelly(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle-jelly[selected]:not([disabled])',
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle[checked]:not([disabled])');

  // Focus on the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.focusTree();

  // Check the "Available Offline" toggle is still available in the action bar.
  await remoteCall.waitForElementJelly(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle-jelly[selected]:not([disabled])',
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle[checked]:not([disabled])');
}

/**
 * Tests following links to folders.
 */
export async function driveLinkToDirectory() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.directoryA,
        ENTRIES.directoryB,
        ENTRIES.directoryC,
        ENTRIES.deeplyBuriedSmallJpeg,
        ENTRIES.linkGtoB,
        ENTRIES.linkHtoFile,
        ENTRIES.linkTtoTransitiveDirectory,
      ]));

  // Select the link
  await remoteCall.waitUntilSelected(appId, 'G');
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Ensure the "G" directory has the shortcut class applied.
  await remoteCall.waitForElement(appId, '#file-list [file-name="G"].shortcut');

  // Open the link
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['G']));

  // Check the contents of current directory.
  await remoteCall.waitForFiles(appId, [ENTRIES.directoryC.getExpectedRow()]);
}

/**
 * Tests opening files through folder links.
 */
export async function driveLinkOpenFileThroughLinkedDirectory() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.directoryA,
        ENTRIES.directoryB,
        ENTRIES.directoryC,
        ENTRIES.deeplyBuriedSmallJpeg,
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
      appId, [ENTRIES.deeplyBuriedSmallJpeg.getExpectedRow()]);

  await sendTestMessage(
      {name: 'expectFileTask', fileNames: ['deep.jpg'], openType: 'launch'});
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['deep.jpg']));

  // The MediaApp window should open for the image.
  await waitForMediaApp();
}

/**
 * Tests opening files through transitive links.
 */
export async function driveLinkOpenFileThroughTransitiveLink() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.directoryA,
        ENTRIES.directoryB,
        ENTRIES.directoryC,
        ENTRIES.deeplyBuriedSmallJpeg,
        ENTRIES.linkGtoB,
        ENTRIES.linkHtoFile,
        ENTRIES.linkTtoTransitiveDirectory,
      ]));

  // Navigate through link.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['T']));
  await remoteCall.waitForFiles(
      appId, [ENTRIES.deeplyBuriedSmallJpeg.getExpectedRow()]);

  await sendTestMessage(
      {name: 'expectFileTask', fileNames: ['deep.jpg'], openType: 'launch'});
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['deep.jpg']));

  // The MediaApp window should open for the image.
  await waitForMediaApp();
}

/**
 * Tests that the welcome banner appears when a Drive volume is opened.
 */
export async function driveWelcomeBanner() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);

  await remoteCall.isolateBannerForTesting(appId, 'drive-welcome-banner');
  const driveWelcomeBannerQuery = '#banners > drive-welcome-banner';
  const driveWelcomeBannerDismissButtonQuery = [
    '#banners > drive-welcome-banner',
    'educational-banner',
    await getDismissButtonId(appId),
  ];

  // Open the Drive volume in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Google Drive');

  // Check: the Drive welcome banner should appear.
  await remoteCall.waitForElement(appId, driveWelcomeBannerQuery);

  // Close the Drive welcome banner.
  await remoteCall.waitAndClickElement(
      appId, driveWelcomeBannerDismissButtonQuery);

  await remoteCall.waitForElement(
      appId, '#banners > drive-welcome-banner[hidden]');
}

/**
 * Tests that the Drive offline info banner appears when a Drive volume is
 * opened.
 */
export async function driveOfflineInfoBanner() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);

  await remoteCall.isolateBannerForTesting(
      appId, 'drive-offline-pinning-banner');
  const driveOfflineBannerShownQuery =
      '#banners > drive-offline-pinning-banner:not([hidden])';
  const driveOfflineBannerHiddenQuery =
      '#banners > drive-offline-pinning-banner[hidden]';
  const driveOfflineLearnMoreLinkQuery =
      ['#banners > drive-offline-pinning-banner', '[slot="extra-button"]'];

  // Check: the Drive Offline info banner should appear.
  await remoteCall.waitForElement(appId, driveOfflineBannerShownQuery);

  // Click on the 'Learn more' button.
  await remoteCall.waitAndClickElement(appId, driveOfflineLearnMoreLinkQuery);

  // Check: the Drive offline info banner should disappear.
  await remoteCall.waitForElement(appId, driveOfflineBannerHiddenQuery);

  // Navigate to a different directory within Drive.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My Drive/photos');

  // Check: the Drive offline info banner should stay hidden.
  await remoteCall.waitForElement(appId, driveOfflineBannerHiddenQuery);
}

/**
 * Tests that the encryption badge appears next to the CSE file when a Drive
 *  volume is opened.
 */
export async function driveEncryptionBadge() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello, ENTRIES.testCSEFile]);

  // Check: encrypted file has a badge.
  const encrypted = await remoteCall.waitForElementStyles(
      appId, '#file-list [file-name="test-encrypted.txt"] .encrypted-icon',
      ['display', 'visibility']);
  chrome.test.assertNe('none', encrypted.styles?.['display']);
  chrome.test.assertEq('visible', encrypted.styles?.['visibility']);

  // Check: the badge is included in accessibility labels.
  const row = await remoteCall.waitForElementStyles(
      appId, '#file-list [file-name="test-encrypted.txt"]',
      ['aria-labelledby', 'display', 'visibility']);
  const ariaLabelledBy = row.attributes['aria-labelledby']!;
  const encryptedBadgeId = ariaLabelledBy.split(/ +/).filter(
      (id) => id.indexOf('encrypted') !== -1)[0];
  chrome.test.assertTrue(
      encryptedBadgeId !== undefined,
      'no encrypted label found in aria for the encrypted file');
  const encryptedBadgeElements =
      await remoteCall.queryElements(appId, `#${encryptedBadgeId}`);
  chrome.test.assertEq(
      1, encryptedBadgeElements.length,
      'no referenced encrypted label element found');

  // Check: non-encrypted file doesn't have a badge.
  const plain = await remoteCall.queryElements(
      appId, ['#file-list [file-name="hello.txt"] .encrypted-icon']);
  chrome.test.assertEq(0, plain.length);
}

/**
 * Tests that the inline sync status "in progress" icon is displayed in "My
 * Drive" as the file starts syncing then disappears as it finishes syncing
 * (i.e., the file reaches 100% progress).
 */
export async function driveInlineSyncStatusSingleFileProgressEvents() {
  const toBeUploaded = new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video.ogv',
    thumbnailFileName: 'image.png',
    targetPath: 'toBeUploaded.ogv',
    mimeType: 'video/ogg',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'toBeUploaded.ogv',
    sizeText: '56 KB',
    typeText: 'OGG video',
    availableOffline: true,
  });

  // Open Files app on Drive and copy over entry to be uploaded.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [toBeUploaded]);

  // Fake the file starting to sync.
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${toBeUploaded.targetPath}`,
    progress: 50,
  });

  // Verify this data reaches the UI as a progress value of 50%.
  const inlineStatus = await remoteCall.waitForElement(
      appId, 'xf-inline-status[sync-status=in_progress]');

  chrome.test.assertEq(Number(inlineStatus.attributes['progress']), 0.5);

  // Fake the file finishing syncing.
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${toBeUploaded.targetPath}`,
    progress: 100,
  });

  // Verify the "sync in progress" icon is no longer displayed.
  await remoteCall.waitForElementLost(
      appId, 'xf-inline-status[sync-status=in_progress]');
}

/**
 * Tests that the inline sync status icons are displayed in Drive on parent
 * folders containing entries and that child entries' statuses are aggregated
 * respecting the order of precedence (failed > in progress > completed).
 */
export async function driveInlineSyncStatusParentFolderProgressEvents() {
  const parentDir = new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'some_folder',
    lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
    nameText: 'some_folder',
    sizeText: '--',
    typeText: 'Folder',
  });

  const toBeUploaded = new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video.ogv',
    thumbnailFileName: 'image.png',
    targetPath: 'some_folder/toBeUploaded.ogv',
    mimeType: 'video/ogg',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'toBeUploaded.ogv',
    sizeText: '56 KB',
    typeText: 'OGG video',
    availableOffline: true,
  });

  const toFailUploading = new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video.ogv',
    thumbnailFileName: 'image.png',
    targetPath: 'some_folder/toFailUploading.ogv',
    mimeType: 'video/ogg',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'toFailUploading.ogv',
    sizeText: '56 KB',
    typeText: 'OGG video',
    availableOffline: true,
  });

  // Open Files app on Drive and copy over entry to be uploaded.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [parentDir, toBeUploaded, toFailUploading]);

  // Fake syncing both files to Drive.
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${toBeUploaded.targetPath}`,
    progress: 50,
  });
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${toFailUploading.targetPath}`,
    progress: 50,
  });
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${parentDir.targetPath}`,
    progress: 50,
  });
  // States:
  // some_folder - syncing in progress
  // some_folder/toBeUploaded - syncing in progress
  // some_folder/toFailUploading - syncing in progress

  const syncInProgressQuery = 'xf-inline-status[sync-status=in_progress]';
  const syncQueuedQuery = 'xf-inline-status[sync-status=queued]';

  // Verify the "sync in progress" icon is displayed in the parent "some_folder"
  // folder.
  await remoteCall.waitForElement(appId, syncInProgressQuery);

  // Go inside the some_folder folder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My Drive/some_folder');

  // Fake toFailUploading.ogv failing to sync to Drive.
  await sendTestMessage({
    name: 'setDriveSyncError',
    path: `/root/${toFailUploading.targetPath}`,
  });
  // States:
  // some_folder - syncing in progress
  // some_folder/toBeUploaded - syncing in progress
  // some_folder/toFailUploading - syncing failed (when file fail to sync, their
  // status changes back to "queued")

  // Verify the "sync queued" icon is displayed.
  // (failed > in progress)
  await remoteCall.waitForElement(appId, syncQueuedQuery);

  // Fake root/some_folder/world.ogv finishing syncing.
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${toBeUploaded.targetPath}`,
    progress: 100,
  });
  // States:
  // toBeUploaded - syncing completed
  // toFailUploading - syncing failed

  // Verify the "sync queued" icon is still displayed in the parent folder.
  // (failed > completed)
  await remoteCall.waitForElement(appId, syncQueuedQuery);
}

/**
 * Tests that the Enable Docs Offline dialog appears in the Files App.
 */
export async function driveEnableDocsOfflineDialog() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Open the Enable Docs Offline dialog.
  await openAndWaitForEnableDocsOfflineDialog(appId);

  // Click on the ok button.
  await remoteCall.waitAndClickElement(
      appId, '.cr-dialog-container.shown .cr-dialog-ok');

  // Check: the last dialog result should be 1 (accept).
  await waitForLastDriveDialogResult('1');

  // Open the Enable Docs Offline dialog.
  await openAndWaitForEnableDocsOfflineDialog(appId);

  // Click on the cancel button.
  await remoteCall.waitAndClickElement(
      appId, '.cr-dialog-container.shown .cr-dialog-cancel');

  // Check: the last dialog result should be 2 (reject).
  await waitForLastDriveDialogResult('2');

  // Open the Enable Docs Offline dialog.
  await openAndWaitForEnableDocsOfflineDialog(appId);

  // Check: the last dialog result should be 3 (dismiss).
  await waitForLastDriveDialogResult('3');
}

/**
 * Tests that the Enable Docs Offline dialog launches a Chrome notification if
 * there are no Files App windows open.
 */
export async function driveEnableDocsOfflineDialogWithoutWindow() {
  // Wait for the background page to listen to events from the browser.
  await remoteCall.callRemoteTestUtil('waitForBackgroundReady', null, []);

  // Simulate Drive signalling Files App to open a dialog.
  await sendTestMessage({name: 'displayEnableDocsOfflineDialog'});

  // Check: the Enable Docs Offline notification should appear.
  await waitForNotification('enable-docs-offline');

  // Click on the ok button.
  await sendTestMessage({
    name: 'clickNotificationButton',
    extensionId: FILE_MANAGER_EXTENSIONS_ID,
    notificationId: 'enable-docs-offline',
    index: 1,
  });

  // Check: the last dialog result should be 1 (accept).
  await waitForLastDriveDialogResult('1');

  // Simulate Drive signalling Files App to open a dialog.
  await sendTestMessage({name: 'displayEnableDocsOfflineDialog'});

  // Check: the Enable Docs Offline notification should appear.
  await waitForNotification('enable-docs-offline');

  // Click on the cancel button.
  await sendTestMessage({
    name: 'clickNotificationButton',
    extensionId: FILE_MANAGER_EXTENSIONS_ID,
    notificationId: 'enable-docs-offline',
    index: 0,
  });

  // Check: the last dialog result should be 2 (reject).
  await waitForLastDriveDialogResult('2');

  // Simulate Drive signalling Files App to open a dialog.
  await sendTestMessage({name: 'displayEnableDocsOfflineDialog'});

  // Check: the Enable Docs Offline notification should appear.
  await waitForNotification('enable-docs-offline');

  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Check: the Enable Docs Offline dialog should appear in Files app.
  const dialogText = await remoteCall.waitForElement(
      appId, '.cr-dialog-container.shown .cr-dialog-text');
  chrome.test.assertEq(ENABLE_DOCS_OFFLINE_MESSAGE, dialogText.text);

  // Check: the Enable Docs Offline notification should disappear.
  const caller = getCaller();
  await repeatUntil(async () => {
    const idSet = await remoteCall.callRemoteTestUtil<Record<string, boolean>>(
        'getNotificationIDs', null, []);
    return idSet['enable-docs-offline'] ?
        pending(
            caller, 'Waiting for Drive confirm notification to disappear.') :
        null;
  });
}

/**
 * Tests that the Enable Docs Offline dialog appears in the focused window if
 * there are more than one Files App windows open.
 */
export async function driveEnableDocsOfflineDialogMultipleWindows() {
  // Open two Files app windows on Drive, and the second one should be focused.
  const appId1 = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);
  const appId2 = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Open the Enable Docs Offline dialog and check that it appears in the second
  // window.
  await openAndWaitForEnableDocsOfflineDialog(appId2);

  // Check: there should be no dialog shown in the first window.
  await remoteCall.waitForElementLost(appId1, '.cr-dialog-container.shown');

  // Click on the ok button in the second window.
  await remoteCall.waitAndClickElement(
      appId2, '.cr-dialog-container.shown .cr-dialog-ok');

  // Check: the last dialog result should be 1 (accept).
  await waitForLastDriveDialogResult('1');
}

/**
 * Tests that the Enable Docs Offline dialog disappears when Drive is unmounted.
 */
export async function driveEnableDocsOfflineDialogDisappearsOnUnmount() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Open the Enable Docs Offline dialog.
  await openAndWaitForEnableDocsOfflineDialog(appId);

  // Unmount Drive.
  await sendTestMessage({name: 'unmountDrive'});

  // Check: the Enable Docs Offline dialog should disappear.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container.shown');
}

/**
 * Tests that when deleting a file on Google Drive the dialog has no mention of
 * permanent deletion (as the files aren't pemanently deleted but go to Google
 * Drive trash instead).
 */
export async function driveDeleteDialogDoesntMentionPermanentDelete() {
  // Open Files app on Drive.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Wait for the "hello.txt" file to appear.
  const helloTxtSelector = '#file-list [file-name="photos"]';
  await remoteCall.waitAndClickElement(appId, helloTxtSelector);

  // Ensure the move-to-trash command is hidden and disabled on Google Drive and
  // then click the enabled delete button
  await remoteCall.waitForElement(appId, '#move-to-trash[hidden][disabled]');
  await remoteCall.waitAndClickElement(
      appId, '#delete-button:not([hidden]):not([disabled])');

  // Check: the dialog 'Cancel' button should be focused by default.
  const dialogDefaultButton =
      await remoteCall.waitForElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', dialogDefaultButton.text);

  // Check: the dialog has no mention in the text of "permanent".
  const dialogText = await remoteCall.waitForElement(appId, '.cr-dialog-text');
  chrome.test.assertFalse(
      (dialogText.text ?? '').toLowerCase().includes('permanent'));

  // The dialog 'Delete' button should be only contain the text "Delete".
  const dialogDeleteButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');
  chrome.test.assertEq('Delete', dialogDeleteButton.text);

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(appId, helloTxtSelector);
}

/**
 * Tests that Google One offer banner appears if a user navigates to Drive
 * volume.
 */
export async function driveGoogleOneOfferBannerEnabled() {
  const userActionShown = 'FileBrowser.GoogleOneOffer.Shown';
  const userActionGetPerk = 'FileBrowser.GoogleOneOffer.GetPerk';
  const userActionDismiss = 'FileBrowser.GoogleOneOffer.Dismiss';

  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  // Visibility of a banner is controlled with hidden attribute once it gets
  // attached to the DOM.
  await remoteCall.waitForElement(
      appId, 'google-one-offer-banner:not([hidden])');
  chrome.test.assertEq(1, await getUserActionCount(userActionShown));

  // extra-button (get perk button) is provided by google-one-offer-banner.
  await remoteCall.waitAndClickElement(
      appId, ['google-one-offer-banner', '[slot="extra-button"]']);
  chrome.test.assertEq(1, await getUserActionCount(userActionGetPerk));
  // Check: dismiss event should not be recorded for dismiss caused by the get
  // perk button click.
  chrome.test.assertEq(0, await getUserActionCount(userActionDismiss));
  await remoteCall.waitForLastOpenedBrowserTabUrl(
      'https://www.google.com/chromebook/perks/?id=google.one.2019');
  await remoteCall.waitForElement(appId, 'google-one-offer-banner[hidden]');
  // Check: If Google One offer banner is shown, Drive welcome banner should not
  // be shown. Holding space welcome banner is the next one after the Drive
  // welcome banner.
  await remoteCall.waitForElement(
      appId, 'holding-space-welcome-banner:not([hidden])');
}

/**
 * Tests that Google One offer banner does not appear if the flag is off, which
 * is the default.
 */
export async function driveGoogleOneOfferBannerDisabled() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  // If Google One offer banner is not shown, Drive welcome banner should be
  // shown. We cannot test google-one-offer-banner[hidden] here as it should not
  // be in the DOM tree.
  await remoteCall.waitForElement(appId, 'drive-welcome-banner:not([hidden])');
}

/**
 * Test that Google One offer banner can get dismissed with a click of Dismiss
 * button.
 */
export async function driveGoogleOneOfferBannerDismiss() {
  const userActionDismiss = 'FileBrowser.GoogleOneOffer.Dismiss';

  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  // Visibility of a banner is controlled with hidden attribute once it gets
  // attached to the DOM.
  await remoteCall.waitForElement(
      appId, 'google-one-offer-banner:not([hidden])');

  // dismiss-button is provided by educational-banner.
  await remoteCall.waitAndClickElement(appId, [
    'google-one-offer-banner',
    'educational-banner',
    await getDismissButtonId(appId),
  ]);
  chrome.test.assertEq(1, await getUserActionCount(userActionDismiss));
  await remoteCall.waitForElement(appId, 'google-one-offer-banner[hidden]');
}

/**
 * Tests that when bulk pinning is enabled, the "Available offline" toggle
 * should not be visible. When the preference is updated, the toggle should
 * reappear.
 */
export async function
drivePinToggleIsDisabledAndHiddenWhenBulkPinningEnabled() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);


  const toggleId = await remoteCall.isCrosComponents(appId) ?
      'pinned-toggle-jelly' :
      'pinned-toggle';

  // Bring up the context menu for test.txt.
  await remoteCall.waitAndRightClick(
      appId, '#file-list [file-name="hello.txt"]');

  // The pinned toggle should be visible along with the command.
  await remoteCall.waitForElement(
      appId,
      `#pinned-toggle-wrapper:not([hidden]) #${toggleId}:not([disabled])`);
  await remoteCall.waitForElement(
      appId, '[command="#toggle-pinned"]:not([hidden][disabled])');

  // Mock the free space returned by spaced to be 4 GB and enable the bulk
  // pinning preference
  await remoteCall.setSpacedFreeSpace(4n << 30n);
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});

  // Wait for both the pinned toggle and the pinned command to become hidden and
  // disabled.
  await remoteCall.waitForElement(
      appId, `#pinned-toggle-wrapper[hidden] #${toggleId}[disabled]`);
  await remoteCall.waitForElement(
      appId, '[command="#toggle-pinned"][hidden][disabled]');

  // Disable the bulk pinning preference and wait for the pinned toggle and
  // command to become visible and available.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: false});
  await remoteCall.waitForElement(
      appId,
      `#pinned-toggle-wrapper:not([hidden]) #${toggleId}:not([disabled])`);
  await remoteCall.waitForElement(
      appId, '[command="#toggle-pinned"]:not([hidden][disabled])');
}

/**
 * Tests that "Shared with me" which is outside "My drive" retains the pinned
 * property and it is not updated when bulk pinning is enabled.
 */
export async function driveFoldersRetainPinnedPropertyWhenBulkPinningEnabled() {
  // Open Files app on Drive containing "Shared with me" file entries.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello, ENTRIES.sharedWithMeDirectory]);

  // Enable the bulk pinning preference first.
  await remoteCall.setSpacedFreeSpace(4n << 30n);
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForBulkPinningStage('Syncing');

  // Navigate to the shared with me directory and assert that the pinned
  // property is not set on the directory.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Shared with me');
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="Shared Directory"]:not(.pinned)');

  // Disable the bulk pinning preference.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: false});
  await remoteCall.waitForBulkPinningStage('Stopped');

  // Pin the "Shared Directory" folder in Shared with me and wait for the pinned
  // class to be updated.
  await remoteCall.showContextMenuFor(appId, 'Shared Directory');
  await remoteCall.waitAndClickElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"]:not([checked])');
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="Shared Directory"].pinned');

  // Enable and disable bulk pinning and ensure the pinned attribute is not
  // removed.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForBulkPinningStage('Syncing');
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="Shared Directory"].pinned');
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: false});
  await remoteCall.waitForBulkPinningStage('Stopped');
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="Shared Directory"].pinned');
}

/**
 * Tests that when bulk pinning is enabled, the "Available offline" toggle
 * should still be visible in the Shared with me section.
 */
export async function
drivePinToggleIsEnabledInSharedWithMeWhenBulkPinningEnabled() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [
    ENTRIES.hello,
    ENTRIES.sharedWithMeDirectory,
    ENTRIES.sharedWithMeDirectoryFile,
  ]);

  const toggleId = await remoteCall.isCrosComponents(appId) ?
      'pinned-toggle-jelly' :
      'pinned-toggle';

  // Click the Shared with me volume, it has no children so navigating using the
  // directory tree doesn't work.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Shared with me');

  // Wait until the breadcrumb path is updated.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Bring up the context menu for Shared Directory.
  await remoteCall.waitAndRightClick(
      appId, '#file-list [file-name="Shared Directory"]');

  // The pinned toggle should be visible along with the command.
  await remoteCall.waitForElement(
      appId,
      `#pinned-toggle-wrapper:not([hidden]) #${toggleId}:not([disabled])`);
  await remoteCall.waitForElement(
      appId, '[command="#toggle-pinned"]:not([hidden][disabled])');

  // Mock the free space returned by spaced to be 4 GB and enable the bulk
  // pinning preference.
  await remoteCall.setSpacedFreeSpace(4n << 30n);
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForBulkPinningStage('Syncing');

  // After bulk pinning is enabled and in the syncing stage, the toggle should
  // still be visible and enabled.
  await remoteCall.waitForElement(
      appId,
      `#pinned-toggle-wrapper:not([hidden]) #${toggleId}:not([disabled])`);
  await remoteCall.waitForElement(
      appId, '[command="#toggle-pinned"]:not([hidden][disabled])');

  // Disable the bulk pinning preference wait for it to reflect in the pin state
  // and ensure the pinned toggle has not changed.
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: false});
  await remoteCall.waitForBulkPinningStage('Stopped');
  await remoteCall.waitForElement(
      appId,
      `#pinned-toggle-wrapper:not([hidden]) #${toggleId}:not([disabled])`);
  await remoteCall.waitForElement(
      appId, '[command="#toggle-pinned"]:not([hidden][disabled])');
}

/**
 * Tests that files that can't be pinned should have the correct CSS class
 * applied to them. When they go back to being able to be pinned (e.g. from Docs
 * offline coming back online) then ensure the inline icon is updated.
 */
export async function
driveCantPinItemsShouldHaveClassNameAndGetUpdatedWhenCanPin() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.cantPinFile]);

  // Ensure the `cant_pin.txt` file has the cant-pin class.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="text.txt"].cant-pin');

  // Update the file metadata to ensure the file can now be pinned.
  await sendTestMessage(
      {name: 'setCanPin', path: '/root/text.txt', canPin: true});

  // Wait for the `.cant-pin` class to be removed.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="text.txt"]:not(.cant-pin)');
}

/**
 * Tests that items that are cached outside of their virtual list get their
 * inline sync status updated when they get attached back to the DOM.
 */
export async function driveItemsOutOfViewportShouldUpdateTheirSyncStatus() {
  const entries = [];
  const emptyFile = createTestFile('text.txt');
  for (let i = 0; i < 50; ++i) {
    entries.push(emptyFile.cloneWithNewName(`File ${i}`));
  }

  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], entries);

  // Sort the table by the `name` column.
  await remoteCall.waitAndClickElement(
      appId, ['.table-header-cell:nth-of-type(1)']);

  // Update the second file's metadata to "not pinnable" and wait for the
  // corresponding icon to be displayed.
  const secondFileName = entries[1]!.nameText;
  await sendTestMessage(
      {name: 'setCanPin', path: `/root/${secondFileName}`, canPin: false});
  await remoteCall.waitForElement(
      appId, `#file-list [file-name="${secondFileName}"].cant-pin`);

  // Wait for the first entry to appear in the file list.
  const firstFileName = entries[0]!.nameText;
  await remoteCall.waitForElement(
      appId, `#file-list [file-name="${firstFileName}"]`);

  // Scroll to the bottom of the virtual list which ensures the first element
  // should be removed from the DOM and cached.
  await remoteCall.callRemoteTestUtil(
      'setScrollTop', appId, ['#file-list', 10000]);

  await remoteCall.waitForElementLost(
      appId, `#file-list [file-name="${firstFileName}"]`);
  const lastFileName = entries[entries.length - 1]!.nameText;
  await remoteCall.waitForElement(
      appId, `#file-list [file-name="${lastFileName}"]`);

  // Send a file sync progress for the first file name.
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${firstFileName}`,
    progress: 50,
  });

  // Send a file sync progress event for the last file name to use as a marker
  // to know when the first file has made it to 50% pie progress.
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${lastFileName}`,
    progress: 50,
  });

  const inlineSyncSelector = (fileName: string) => `#file-list [file-name="${
      fileName}"] xf-inline-status[sync-status=in_progress]`;

  // Wait for the progress to appear on the last file and assert it received the
  // correct progress value.
  let lastFileInlineStatus =
      await remoteCall.waitForElement(appId, inlineSyncSelector(lastFileName));
  chrome.test.assertEq(
      Number(lastFileInlineStatus.attributes['progress']), 0.5);

  // Send a "completed" sync progress event for the second last file and wait
  // for its effect.
  const secondLastFileName = entries[entries.length - 2]!.nameText;
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${secondLastFileName}`,
    progress: 100,
  });

  // Scroll back up to the first element.
  await remoteCall.callRemoteTestUtil('setScrollTop', appId, ['#file-list', 0]);

  // Assert that the first element has the 50% progress as the event was sent
  // before the last file event was sent.
  const firstFileInlineStatus =
      await remoteCall.waitForElement(appId, inlineSyncSelector(firstFileName));
  chrome.test.assertEq(
      Number(firstFileInlineStatus.attributes['progress']), 0.5);

  // Ensure the second file is still displayed as "not pinnable".
  await remoteCall.waitForElement(
      appId, `#file-list [file-name="${secondFileName}"].cant-pin`);

  // Switch to grid view.
  await remoteCall.waitAndClickElement(appId, '#view-button');

  // Wait for the first entry to appear in the file grid.
  await remoteCall.waitForElement(
      appId, `grid#file-list [file-name="${firstFileName}"]`);

  // Scroll back down to the last element.
  await remoteCall.callRemoteTestUtil(
      'setScrollTop', appId, ['grid#file-list', 10000]);

  // Assert that the last element still has 50% progress.
  lastFileInlineStatus =
      await remoteCall.waitForElement(appId, inlineSyncSelector(lastFileName));
  chrome.test.assertEq(
      Number(lastFileInlineStatus.attributes['progress']), 0.5);
}

/**
 * Tests that when bulk pinning is enabled the queued state is shown for all
 * files that the PinningManager is tracking but has not yet pinned.
 */
export async function driveAllItemsShouldBeQueuedIfTrackedByPinningManager() {
  // Stop the PinningManager from pinning files.
  await sendTestMessage({name: 'setBulkPinningShouldPinFiles', enabled: false});

  // Add a single empty file and load Files app up at the Drive root.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);

  // Enable bulk pinning functionality.
  await remoteCall.setSpacedFreeSpace(4n << 30n);
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});

  // Wait for bulk pinning to enter the syncing stage.
  await remoteCall.waitForBulkPinningStage('Syncing');

  // The file should have a queued despite never getting set to pinned.
  await remoteCall.waitForElement(
      appId,
      '#file-list [file-name="hello.txt"] xf-inline-status[sync-status=queued]');

  // Disable bulk pinning and ensure the sync status gets removed (i.e. returns
  // to not found).
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: false});
  await remoteCall.waitForElement(
      appId,
      '#file-list [file-name="hello.txt"] xf-inline-status[sync-status=not_found]');

  // Ensure the pin manager pins files then re-enable the bulk pinning
  // preferece. The hello file should be pinned now.
  await sendTestMessage({name: 'setBulkPinningShouldPinFiles', enabled: true});
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForElement(
      appId,
      '#file-list [file-name="hello.txt"].pinned xf-inline-status[sync-status=not_found]');
}

/**
 * Tests that items that have the `dirty` metadata flag set to true have their
 * sync_status property returned as "QUEUED".
 */
export async function driveDirtyItemsShouldBeDisplayedAsQueued() {
  // Add a single test file with the dirty metadata set to "true" and load Files
  // app up at the Drive root.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.dirty]);

  // The file should be displayed as "queued" despite it not having received any
  // progress events yet because dirty=true.
  await remoteCall.waitForElement(
      appId,
      '#file-list [file-name="dirty.txt"] xf-inline-status[sync-status=queued]');

  // Fake the file starting to sync.
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${ENTRIES.dirty.targetPath}`,
    progress: 50,
  });

  // Verify that the sync_state transitions to "in_progress".
  await remoteCall.waitForElement(
      appId,
      '#file-list [file-name="dirty.txt"] xf-inline-status[sync-status=in_progress]');
}

/**
 * Tests that the Drive bulk pinning banner is disabled (i.e. doesn't appear
 * between the Drive welcome banner but before the Holding space banner).
 */
export async function driveBulkPinningBannerDisabled() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  // Visibility of a banner is controlled with hidden attribute once it gets
  // attached to the DOM.
  await remoteCall.waitForElement(appId, 'drive-welcome-banner:not([hidden])');

  // extra-button (get perk button) is provided by google-one-offer-banner.
  await remoteCall.waitAndClickElement(appId, [
    'drive-welcome-banner',
    'educational-banner',
    await getDismissButtonId(appId),
  ]);

  await remoteCall.waitForElement(appId, 'drive-welcome-banner[hidden]');
  // Check: If Google One offer banner is shown, Drive welcome banner should not
  // be shown. Holding space welcome banner is the next one after the Drive
  // welcome banner.
  await remoteCall.waitForElement(
      appId, 'holding-space-welcome-banner:not([hidden])');
}

/**
 * Tests that the Drive bulk pinning banner is enabled (i.e. it appears directly
 * after the Drive welcome banner).
 */
export async function driveBulkPinningBannerEnabled() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);

  // Visibility of a banner is controlled with hidden attribute once it gets
  // attached to the DOM.
  await remoteCall.waitForElement(appId, 'drive-welcome-banner:not([hidden])');

  // extra-button (get perk button) is provided by google-one-offer-banner.
  await remoteCall.waitAndClickElement(appId, [
    'drive-welcome-banner',
    'educational-banner',
    await getDismissButtonId(appId),
  ]);

  await remoteCall.waitForElement(appId, 'drive-welcome-banner[hidden]');
  // Check: If Google One offer banner is shown, Drive welcome banner should not
  // be shown. Holding space welcome banner is the next one after the Drive
  // welcome banner.
  await remoteCall.waitForElement(
      appId, 'drive-bulk-pinning-banner:not([hidden])');
}

/*
 * Checks that we cannot open Google Doc without network connection.
 */
export async function openDriveDocWhenOffline() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [
    ENTRIES.testDocument,
    ENTRIES.hello,
  ]);

  // Setup the open-with task for drive.
  const fakeOpenWith = new FakeTask(
      true, {appId: 'id', taskType: 'drive', actionId: 'open-with'},
      'DummyOpenWith');
  await remoteCall.callRemoteTestUtil('overrideTasks', appId, [
    [fakeOpenWith],
  ]);

  // Start bulk pinning.
  await remoteCall.setSpacedFreeSpace(4n << 30n);
  await sendTestMessage({name: 'setBulkPinningEnabledPref', enabled: true});
  await remoteCall.waitForBulkPinningStage('Syncing');

  // Wait for the hello.txt file to be pinned.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="hello.txt"].pinned');
  // Check that the gdoc file is not pinned.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="Test Document.gdoc"]:not(.pinned)');

  // Turn off all services.
  await sendTestMessage({name: 'setDeviceOffline'});

  // Check that hello.txt opens on double click without network.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId,
      ['#file-list li.table-row[selected] .filename-label span']));
  await remoteCall.waitUntilTaskExecutes(
      appId, fakeOpenWith.descriptor, ['hello.txt']);

  // Check that Test Document.gdoc does not open on double click. We do not
  // check that the task was NOT executed. Instead we check that the "You
  // are offline" dialog was shown.
  await remoteCall.waitUntilSelected(appId, 'Test Document.gdoc');
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId,
      ['#file-list li.table-row[selected] .filename-label span']));
  await remoteCall.waitForElement(
      appId, '.files-alert-dialog[aria-label="You are offline"]');
}

/*
 * Verifies that once a file completes syncing, its syncing status
 * indicator displays as "completed" and is dismissed about 300ms
 * later.
 */
export async function completedSyncStatusDismissesAfter300Ms() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [
    ENTRIES.hello,
  ]);

  const timeBeforeCompletion = Date.now();

  // Fake the file finishing syncing.
  await sendTestMessage({
    name: 'setDriveSyncProgress',
    path: `/root/${ENTRIES.hello.targetPath}`,
    progress: 100,
  });

  const completedQuery = '#file-list xf-inline-status[sync-status=completed]';

  // Verify the "sync completed" icon is displayed.
  await remoteCall.waitForElement(appId, completedQuery);

  // Verify the completed state is eventually dismissed.
  await remoteCall.waitForElementLost(appId, completedQuery);

  // Verify that at least 300ms have passed since the syncing completed.
  chrome.test.assertTrue(Date.now() - timeBeforeCompletion >= 300);
}

/**
 * Tests that when the organization limit has exceeded (not the user storage)
 * the out of organization space banner appears.
 */
export async function driveOutOfOrganizationSpaceBanner() {
  await remoteCall.setPooledStorageQuotaUsage(
      1 * 1024 * 1024, 2 * 1024 * 1024, true);

  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [ENTRIES.hello]);

  await remoteCall.waitForElement(
      appId, 'drive-out-of-organization-space-banner');
}

/**
 * Tests that copy operation of a directory will start, but a error message will
 * appear when encrypted files within that directory were skipped.
 */
export async function copyDirectoryWithEncryptedFile() {
  const dir = ENTRIES.testCSEDirectory;
  const file = ENTRIES.testCSEFileInDirectory;
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [dir, file]);
  await sendTestMessage({name: 'mockDriveReadFailure', path: file.targetPath});

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My Drive');

  await remoteCall.waitForFiles(appId, [dir.getExpectedRow()]);
  await remoteCall.waitUntilSelected(appId, dir.nameText);

  await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']);
  await directoryTree.navigateToPath('/My files/Downloads');
  await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']);

  const panelType = 3;  // panelTypeError from PanelItem
  const panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', `xf-panel-item[panel-type="${panelType}"]`]);

  chrome.test.assertEq(
      ENTRIES.testCSEFileInDirectory.nameText +
          ' could not be copied because it is encrypted.',
      panel.attributes['primary-text']);
}
