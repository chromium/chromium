// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN, BASIC_DRIVE_ENTRY_SET, BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN, BASIC_LOCAL_ENTRY_SET, BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN, COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET} from './test_data.js';

/**
 * Gets the common steps to toggle hidden files in the Files app
 * @param basicSet Files expected before showing hidden
 * @param hiddenEntrySet Files expected after showing hidden
 */
function runHiddenFilesTest(
    appId: string, basicSet: TestEntryInfo[], hiddenEntrySet: TestEntryInfo[]) {
  return runHiddenFilesTestWithMenuItem(
      appId, basicSet, hiddenEntrySet, '#gear-menu-toggle-hidden-files');
}

/**
 * Gets the common steps to toggle Android hidden files in the Files app
 * @param basicSet Files expected before showing hidden
 * @param hiddenEntrySet Files expected after showing hidden
 */
function runAndroidHiddenFilesTest(
    appId: string, basicSet: TestEntryInfo[], hiddenEntrySet: TestEntryInfo[]) {
  return runHiddenFilesTestWithMenuItem(
      appId, basicSet, hiddenEntrySet,
      '#gear-menu-toggle-hidden-android-folders');
}

/**
 * Gets the common steps to toggle hidden files in the Files app
 * @param basicSet Files expected before showing hidden
 * @param hiddenEntrySet Files expected after showing hidden
 * @param toggleMenuItemSelector Selector for the menu item that toggles hidden
 * file visibility
 */
async function runHiddenFilesTestWithMenuItem(
    appId: string, basicSet: TestEntryInfo[], hiddenEntrySet: TestEntryInfo[],
    toggleMenuItemSelector: string) {
  await remoteCall.waitForElement(appId, '#gear-button:not([hidden])');

  // Open the gear menu by clicking the gear button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for menu to not be hidden.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Wait for menu item to appear.
  await remoteCall.waitForElement(
      appId, toggleMenuItemSelector + ':not([disabled])');

  // Wait for menu item to appear.
  await remoteCall.waitForElement(
      appId, toggleMenuItemSelector + ':not([checked])');

  // Click the menu item.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [toggleMenuItemSelector]));

  // Wait for item to be checked.
  await remoteCall.waitForElement(appId, toggleMenuItemSelector + '[checked]');

  // Check the hidden files are displayed.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(hiddenEntrySet),
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Repeat steps to toggle again.
  await remoteCall.waitForElement(appId, '#gear-button:not([hidden])');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');
  await remoteCall.waitForElement(
      appId, toggleMenuItemSelector + ':not([disabled])');
  await remoteCall.waitForElement(appId, toggleMenuItemSelector + '[checked]');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [toggleMenuItemSelector]));

  await remoteCall.waitForElement(
      appId, toggleMenuItemSelector + ':not([checked])');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(basicSet),
      {ignoreFileSize: true, ignoreLastModifiedTime: true});
}

/**
 * Tests toggling the show-hidden-files menu option on Downloads.
 */
export async function showHiddenFilesDownloads() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN, []);

  await runHiddenFilesTest(
      appId, BASIC_LOCAL_ENTRY_SET, BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN);
}

/**
 * Tests toggling the show-hidden-files menu option on Drive.
 */
export async function showHiddenFilesDrive() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN);

  await runHiddenFilesTest(
      appId, BASIC_DRIVE_ENTRY_SET, BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN);
}

/**
 * Tests that toggle-hidden-android-folders menu item exists when "Play files"
 * is selected, but hidden in Recents.
 */
export async function showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles() {
  // Open Files.App on Play Files.
  const appId = await remoteCall.openNewWindow(RootPath.ANDROID_FILES);
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_ANDROID_ENTRY_SET));

  // Click the gear menu button.
  const gearButton =
      await remoteCall.waitAndClickElement(appId, '#gear-button:not([hidden])');

  // Check: gear-button has aria-haspopup set to true
  chrome.test.assertEq(gearButton.attributes['aria-haspopup'], 'true');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // #toggle-hidden-android-folders command should be shown and disabled by
  // default.
  await remoteCall.waitForElement(
      appId,
      '#gear-menu-toggle-hidden-android-folders' +
          ':not([checked]):not([hidden])');

  // Click the file list: the gear menu should hide.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#file-list']));

  // Wait for the gear menu to hide.
  await remoteCall.waitForElement(appId, '#gear-menu[hidden]');

  // Navigate to Recent.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Recent');

  // Click the gear menu button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // #toggle-hidden-android-folders command should be hidden.
  await remoteCall.waitForElement(
      appId, '#gear-menu-toggle-hidden-android-folders[hidden]');
}

/**
 * Tests that "Play files" shows the full set of files after
 * toggle-hidden-android-folders is enabled.
 */
export async function enableToggleHiddenAndroidFoldersShowsHiddenFiles() {
  // Open Files.App on Play Files.
  const appId = await remoteCall.openNewWindow(RootPath.ANDROID_FILES);
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Wait for the gear menu button to appear.
  await remoteCall.waitForElement(appId, '#gear-button:not([hidden])');
  await runAndroidHiddenFilesTest(
      appId, BASIC_ANDROID_ENTRY_SET, BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN);
}

/**
 * Tests that the current directory is changed to "Play files" after the
 * current directory is hidden by toggle-hidden-android-folders option.
 */
export async function hideCurrentDirectoryByTogglingHiddenAndroidFolders() {
  const MENU_ITEM_SELECTOR = '#gear-menu-toggle-hidden-android-folders';
  const appId = await remoteCall.openNewWindow(RootPath.ANDROID_FILES);
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_ANDROID_ENTRY_SET));

  // Wait for the gear menu button to appear.
  await remoteCall.waitAndClickElement(appId, '#gear-button:not([hidden])');

  // Wait for menu to not be hidden.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Wait for menu item to appear.
  await remoteCall.waitForElement(
      appId, MENU_ITEM_SELECTOR + ':not([disabled]):not([checked])');

  // Click the menu item.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [MENU_ITEM_SELECTOR]));

  // Wait for item to be checked.
  await remoteCall.waitForElement(appId, MENU_ITEM_SELECTOR + '[checked]');

  // Check the hidden files are displayed.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN),
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Navigate to "/My files/Play files/A".
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Play files/A');

  // Wait until current directory is changed to "/My files/Play files/A".
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Play files/A');

  // Open the gear menu by clicking the gear button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for menu to not be hidden.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Wait for menu item to appear.
  await remoteCall.waitForElement(
      appId, MENU_ITEM_SELECTOR + '[checked]:not([disabled])');

  // Click the menu item.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [MENU_ITEM_SELECTOR]));

  // Wait until the current directory is changed from
  // "/My files/Play files/A" to "/My files/Play files" since
  // "/My files/Play files/A" is invisible now.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Play files');
}

/**
 * Tests the paste-into-current-folder menu item.
 */
export async function showPasteIntoCurrentFolder() {
  const entrySet = [ENTRIES.hello, ENTRIES.world];

  // Add files to Downloads volume.
  await addEntries(['local'], entrySet);

  // Open Files.App on Downloads.
  const appId = await remoteCall.openNewWindow(RootPath.DOWNLOADS);
  await remoteCall.waitForElement(appId, '#file-list');

  // Wait for the files to appear in the file list.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(entrySet));

  // Wait for the gear menu button to appear.
  await remoteCall.waitForElement(appId, '#gear-button');

  // 1. Before selecting entries: click the gear menu button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // #paste-into-current-folder command is shown. It should be disabled
  // because no file has been copied to clipboard.
  await remoteCall.waitForElement(
      appId,
      '#gear-menu cr-menu-item' +
          '[command=\'#paste-into-current-folder\']' +
          '[disabled]:not([hidden])');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#file-list']));

  await remoteCall.waitForElement(appId, '#gear-menu[hidden]');

  // 2. Selecting a single regular file
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);

  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for menu to appear.
  // The command is still shown.
  await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#paste-into-current-folder\']' +
          '[disabled]:not([hidden])');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#file-list']));

  await remoteCall.waitForElement(appId, '#gear-menu[hidden]');

  // 3. When ready to paste a file
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);

  // Ctrl-C to copy the selected file
  await remoteCall.fakeKeyDown(appId, '#file-list', 'c', true, false, false);
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // The command appears enabled.
  await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden])' +
          ' cr-menu-item[command=\'#paste-into-current-folder\']' +
          ':not([disabled]):not([hidden])');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#file-list']));

  await remoteCall.waitForElement(appId, '#gear-menu[hidden]');
}

/**
 * Tests the "select-all" menu item.
 */
export async function showSelectAllInCurrentFolder() {
  const entrySet = [ENTRIES.newlyAdded];

  // Open Files.App on Downloads.
  const appId = await remoteCall.openNewWindow(RootPath.DOWNLOADS);
  await remoteCall.waitForElement(appId, '#file-list');

  // Wait for the gear menu button to appear.
  await remoteCall.waitForElement(appId, '#gear-button');

  // Click the gear menu button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check: #select-all command is shown, but disabled (no files yet).
  await remoteCall.waitForElement(
      appId,
      '#gear-menu cr-menu-item' +
          '[command=\'#select-all\']' +
          '[disabled]:not([hidden])');

  // Click the file list: the gear menu should hide.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#file-list']));

  // Wait for the gear menu to hide.
  await remoteCall.waitForElement(appId, '#gear-menu[hidden]');

  // Add a new file to Downloads.
  await addEntries(['local'], entrySet);

  // Wait for the file list change.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(entrySet));

  // Click on the gear button again.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Check: #select-all command is shown, and enabled (there are files).
  await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#select-all\']' +
          ':not([disabled]):not([hidden])');

  // Click on the #gear-menu-select-all item.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-menu-select-all']));

  // Check: the file-list should be selected.
  await remoteCall.waitForElement(appId, '#file-list li[selected]');
}

/**
 * Tests that new folder appears in the gear menu with Downloads focused in the
 * directory tree.
 */
export async function newFolderInDownloads() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Focus the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.focusTree();

  // Open the gear menu.
  await remoteCall.waitForElement(appId, '#gear-button');

  // Open the gear meny by a shortcut (Alt-E).
  chrome.test.assertTrue(
      await remoteCall.fakeKeyDown(appId, 'body', 'e', false, false, true));

  // Wait for menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu');

  // Wait for menu to appear, containing new folder.
  await remoteCall.waitForElement(
      appId, '#gear-menu-newfolder:not([disabled]):not([hidden])');
}

/**
 * Tests that the "Files settings" button appears in the gear menu and properly
 * opens the Files section of the Settings page.
 */
export async function showFilesSettingsButton() {
  const settingsWindowOrigin = 'chrome://os-settings';
  const filesSettingsWindowURL = 'chrome://os-settings/files';

  // Open Files.App on Downloads and wait for the gear menu button to appear.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);
  await remoteCall.waitForElement(appId, '#gear-button');

  // Click the gear menu button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check that there is no Settings window opened.
  chrome.test.assertFalse(
      await remoteCall.windowOriginExists(settingsWindowOrigin));

  // Click #files-settings, which should be shown and enabled.
  await remoteCall.waitAndClickElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#files-settings\']' +
          ':not([disabled]):not([hidden])');

  // Check that the settings window is opened on the Files subpage.
  await remoteCall.waitForLastOpenedBrowserTabUrl(filesSettingsWindowURL);
}

/**
 * Tests that the "Send feedback" button appears in the gear menu and properly
 * opens the feedback window.
 */
export async function showSendFeedbackAction() {
  const feedbackWindowOrigin = 'chrome://os-feedback';

  // Open Files.App on Downloads.
  const appId = await remoteCall.openNewWindow(RootPath.DOWNLOADS);
  await remoteCall.waitForElement(appId, '#file-list');

  // Wait for the gear menu button to appear.
  await remoteCall.waitForElement(appId, '#gear-button');

  // Click the gear menu button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check that there is no feedback window opened.
  chrome.test.assertFalse(
      await remoteCall.windowOriginExists(feedbackWindowOrigin));

  // Click #send-feedback, which should be shown and enabled.
  await remoteCall.waitAndClickElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#send-feedback\']' +
          ':not([disabled]):not([hidden])');

  // Check that the feedback window is open.
  const caller = getCaller();
  return repeatUntil(async () => {
    if (!await remoteCall.windowOriginExists(feedbackWindowOrigin)) {
      return pending(caller, `Waiting for ${feedbackWindowOrigin} to open`);
    }

    return;
  });
}

/**
 * Tests that clicking the gear menu's help button from a Downloads location
 * navigates the user to the Files app's help page.
 */
export async function openHelpPageFromDownloadsVolume() {
  // Open Files App on Downloads.
  const appId = await remoteCall.openNewWindow(RootPath.DOWNLOADS);
  await remoteCall.waitForElement(appId, '#file-list');

  // Click the gear menu button.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check that #volume-help is shown and enabled and click it.
  const volumeHelpQuery = '#gear-menu:not([hidden]) ' +
      'cr-menu-item[command=\'#volume-help\']:not([disabled]):not([hidden])';
  await remoteCall.waitAndClickElement(appId, volumeHelpQuery);

  // Check that the last visited URL is the Files app's help page.
  const filesHelpURL = await remoteCall.callRemoteTestUtil(
      'getTranslatedString', appId, ['FILES_APP_HELP_URL']);
  chrome.test.assertEq(
      filesHelpURL,
      await remoteCall.callRemoteTestUtil('getLastVisitedURL', appId, []));
}

/**
 * Tests that clicking the gear menu's help button from a drive location
 * navigates the user to the Google drive help page.
 */
export async function openHelpPageFromDriveVolume() {
  // Open Files App on Downloads.
  const appId = await remoteCall.openNewWindow(RootPath.DRIVE);
  await remoteCall.waitForElement(appId, '#file-list');

  // Click the gear menu button.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check that #volume-help is shown and enabled and click it.
  const volumeHelpQuery = '#gear-menu:not([hidden]) ' +
      'cr-menu-item[command=\'#volume-help\']:not([disabled]):not([hidden])';
  await remoteCall.waitAndClickElement(appId, volumeHelpQuery);

  // Check that the last visited URL is the Google Drive's help page.
  const driveHelpURL = await remoteCall.callRemoteTestUtil(
      'getTranslatedString', appId, ['GOOGLE_DRIVE_HELP_URL']);
  chrome.test.assertEq(
      driveHelpURL,
      await remoteCall.callRemoteTestUtil('getLastVisitedURL', appId, []));
}

/**
 * Tests that the link of the volume space info item in the gear menu is
 * disabled when the files app is opened on the Google Drive section, and active
 * otherwise. The volume space info item should only link to the storage
 * settings page when the user is navigating within local folders.
 */
export async function enableDisableStorageSettingsLink() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN);

  // Click the gear menu button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Check: volume space info should be disabled for Drive.
  await remoteCall.waitForElement(appId, '#volume-space-info[disabled]');

  // Navigate to Android files.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Play files');

  // Click the gear menu button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Check: volume space info should be enabled for Play files.
  await remoteCall.waitForElement(appId, '#volume-space-info:not([disabled])');

  // Mount empty USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB mount.
  await directoryTree.selectItemByType('removable');

  // Click the gear menu button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Check: volume space info should be disabled for external volume.
  await remoteCall.waitForElement(appId, '#volume-space-info[disabled]');
}

/**
 * Tests that the "xGB available" message appears in the gear menu for the "My
 * Files" volume.
 */
export async function showAvailableStorageMyFiles() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Wait for the gear menu button to appear and click it.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check #volume-storage is shown and it's enabled.
  await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#volume-storage\']' +
          ':not([disabled]):not([hidden])');
}

/**
 * Tests that the "xGB available" message appears in the gear menu for the
 * "Google Drive" volume.
 */
export async function showAvailableStorageDrive() {
  // Mock the pooled storage quota to have 1 MB available.
  await remoteCall.setPooledStorageQuotaUsage(
      1 * 1024 * 1024, 2 * 1024 * 1024, false);

  // Open Files app on Drive containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);

  // Wait for the gear menu button to appear and click it.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check #volume-storage is displayed and get a reference to it.
  const driveMenuEntry = await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#volume-storage\']' +
          ':not([hidden])');

  // Check that it correctly indicates the available storage.
  chrome.test.assertTrue(driveMenuEntry.text?.trim() === '1 MB available');
}

/**
 * Tests that the "xGB available" message appears in the gear menu for
 * an SMB volume.
 */
export async function showAvailableStorageSmbfs() {
  // Populate Smbfs with some files.
  await addEntries(['smbfs'], BASIC_LOCAL_ENTRY_SET);

  // Mount Smbfs volume.
  await sendTestMessage({name: 'mountSmbfs'});

  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/SMB Share');

  // Wait for the volume's file list to appear.
  const files = TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Wait for the gear menu button to appear and click it.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check #volume-storage is shown and disabled (can't manage SMB
  // storage).
  await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#volume-storage\']' +
          ':not([hidden])');
}

/**
 * Tests that the "xGB available message appears in the gear menu for
 * the DocumentsProvider volume.
 */
export async function showAvailableStorageDocProvider() {
  const documentsProviderVolumeType = 'documents_provider';

  // Add files to the DocumentsProvider volume.
  await addEntries(
      ['documents_provider'], COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET);

  // Open Files app.
  const appId = await remoteCall.openNewWindow(RootPath.DOWNLOADS);

  // Wait for the DocumentsProvider volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToHaveChildrenByType(
      documentsProviderVolumeType, /* hasChildren= */ true);

  // Click to open the DocumentsProvider volume.
  await directoryTree.selectItemByType(documentsProviderVolumeType);

  // Check: the DocumentsProvider files should appear in the file list.
  const files =
      TestEntryInfo.getExpectedRows(COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Wait for the gear menu button to appear and click it.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check #volume-storage is shown and disabled (can't manage DocumentsProvider
  // storage).
  await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#volume-storage\']' +
          ':not([hidden])');
}

/**
 * Test that the "Mange synced folders" gear menu item is hidden and is also
 * disabled when the DriveFsMirroring flag is disabled.
 */
export async function showManageMirrorSyncShowsOnlyInLocalRoot() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Wait for the gear menu button to appear and click it.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // If the flag is disabled, the "Manage synced folders" menu item should be
  // hidden and disabled.
  if ((await sendTestMessage({name: 'isMirrorSyncEnabled'})) === 'false') {
    await remoteCall.waitForElement(
        appId,
        '#gear-menu:not([hidden]) cr-menu-item' +
            '[command=\'#manage-mirrorsync\'][disabled][hidden]');
    return;
  }

  // The "Manage synced folders" item should be visible and enabled.
  await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#manage-mirrorsync\']:not([disabled][hidden])');

  // Navigate to the Google Drive root.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My Drive');

  // Wait for the gear menu button to appear and click it.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // The "Manage synced folders" item should not be visible and should be
  // disabled.
  await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#manage-mirrorsync\'][disabled][hidden]');
}
