// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './android_photos.js';
import './breadcrumbs.js';
import './context_menu.js';
import './copy_between_windows.js';
import './create_new_folder.js';
import './crostini.js';
import './directory_tree.js';
import './directory_tree_context_menu.js';
import './dlp.js';
import './dlp_enterprise_connectors.js';
import './drive_specific.js';
import './file_dialog.js';
import './file_display.js';
import './file_list.js';
import './file_transfer_connector.js';
import './files_tooltip.js';
import './folder_shortcuts.js';
import './format_dialog.js';
import './gear_menu.js';
import './grid_view.js';
import './guest_os.js';
import './holding_space.js';
import './install_linux_package_dialog.js';
import './keyboard_operations.js';
import './metadata.js';
import './metrics.js';
import './my_files.js';
import './navigation.js';
import './office.js';
import './open_audio_media_app.js';
import './open_files_in_web_drive.js';
import './open_image_media_app.js';
import './open_sniffed_files.js';
import './open_video_media_app.js';
import './providers.js';
import './quick_view.js';
import './recents.js';
import './restore_prefs.js';
import './search.js';
import './share.js';
import './share_and_manage_dialog.js';
import './sort_columns.js';
import './tab_index.js';
import './tasks.js';
import './toolbar.js';
import './transfer.js';
import './trash.js';
import './traverse.js';
import './zip_files.js';

import {FilesAppState} from '../files_app_state.js';
import {RemoteCallFilesApp} from '../remote_call.js';
import {addEntries, getCaller, getRootPathsResult, pending, repeatUntil, RootPath, sendBrowserTestCommand, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {CHOOSE_ENTRY_PROPERTY} from './choose_entry_const.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_CROSTINI_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, FILE_MANAGER_EXTENSIONS_ID} from './test_data.js';

/**
 * Application ID (URL) for File Manager System Web App (SWA).
 * @type {string}
 * @const
 */
export const FILE_MANAGER_SWA_ID = 'chrome://file-manager';

export {FILE_MANAGER_EXTENSIONS_ID};

/**
 * @type {!RemoteCallFilesApp}
 */
export let remoteCall;

/**
 * Opens a Files app's main window.
 *
 * TODO(mtomasz): Pass a volumeId or an enum value instead of full paths.
 *
 * @param {?string} initialRoot Root path to be used as a default current
 *     directory during initialization. Can be null, for no default path.
 * @param {?FilesAppState=} appState App state to be passed with on opening the
 *     Files app.
 * @return {Promise} Promise to be fulfilled after window creating.
 */
export async function openNewWindow(initialRoot, appState = {}) {
  // TODO(mtomasz): Migrate from full paths to a pair of a volumeId and a
  // relative path. To compose the URL communicate via messages with
  // file_manager_browser_test.cc.
  if (initialRoot) {
    const tail = `external${initialRoot}`;
    appState.currentDirectoryURL = `filesystem:${FILE_MANAGER_SWA_ID}/${tail}`;
  }

  const launchDir = appState ? appState.currentDirectoryURL : undefined;
  const type = appState ? appState.type : undefined;
  const volumeFilter = appState ? appState.volumeFilter : undefined;
  const searchQuery = appState ? appState.searchQuery : undefined;
  const appId = await sendTestMessage({
    name: 'launchFileManager',
    launchDir,
    type,
    volumeFilter,
    searchQuery,
  });

  return appId;
}

/**
 * Opens a foreground window that makes a call to chrome.fileSystem.chooseEntry.
 * This is due to the fact that this API shouldn't be called in the background
 * page (see crbug.com/736930).
 * Returns a promise that is fulfilled once the foreground window is opened.
 *
 * @param {!chrome.fileSystem.ChooseEntryOptions} params
 * @return {!Promise<Window>} Promise fulfilled when a foreground window opens.
 */
export async function openEntryChoosingWindow(params) {
  const json = JSON.stringify(params);
  const url = 'file_manager/choose_entry.html?' +
      new URLSearchParams({value: json}).toString();
  return new Promise((resolve, reject) => {
    chrome.windows.create({url, height: 600, width: 400}, (win) => {
      if (chrome.runtime.lastError) {
        reject(chrome.runtime.lastError);
      } else {
        resolve(win);
      }
    });
  });
}

/**
 * Companion function to openEntryChoosingWindow function. This function waits
 * until entry selected in a dialog shown by chooseEntry() is set.
 * @return {!Promise<?(Entry|Array<Entry>)>} the entry set by the dialog shown
 *     via chooseEntry().
 */
export async function pollForChosenEntry(caller) {
  await repeatUntil(() => {
    if (window[CHOOSE_ENTRY_PROPERTY] === undefined) {
      return pending(caller, 'Waiting for chooseEntry() result');
    }
  });
  return /** @type{FileEntry|Array<FileEntry>} */ (
      window[CHOOSE_ENTRY_PROPERTY]);
}

/**
 * Opens a file dialog and waits for closing it.
 *
 * @param {chrome.fileSystem.AcceptsOption} dialogParams Dialog parameters to be
 *     passed to openEntryChoosingWindow() function.
 * @param {string} volumeType Volume icon type passed to the directory page
 *     object's selectItemByType function.
 * @param {!Array<!TestEntryInfo>} expectedSet Expected set of the entries.
 * @param {function(string):Promise} closeDialog Function to close the
 *     dialog.
 * @param {boolean} useBrowserOpen Whether to launch the select file dialog via
 *     a browser OpenFile() call.
 * @param {boolean=} debug Whether to debug the waitForWindow().
 * @return {Promise} Promise to be fulfilled with the result entry of the
 *     dialog.
 */
export async function openAndWaitForClosingDialog(
    dialogParams, volumeType, expectedSet, closeDialog, useBrowserOpen = false,
    debug = false) {
  const caller = getCaller();
  let resultPromise;
  if (useBrowserOpen) {
    await sendTestMessage({name: 'runSelectFileDialog'});
    resultPromise = async () => {
      return await sendTestMessage({name: 'waitForSelectFileDialogNavigation'});
    };
  } else {
    await openEntryChoosingWindow(dialogParams);
    resultPromise = () => {
      return pollForChosenEntry(caller);
    };
  }

  const appId = await remoteCall.waitForWindow(debug);
  await remoteCall.waitForElement(appId, '#file-list');
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.selectItemByType(volumeType);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(expectedSet));
  await closeDialog(appId);
  await repeatUntil(async () => {
    const windows = await remoteCall.getWindows();
    if (windows[appId] === appId) {
      return pending(caller, 'Waiting for Window %s to hide.', appId);
    }
  });
  return await resultPromise();
}

/**
 * Opens a Files app's main window and waits until it is initialized. Fills
 * the window with initial files. Should be called for the first window only.
 *
 * TODO(mtomasz): Pass a volumeId or an enum value instead of full paths.
 *
 * @param {?string} initialRoot Root path to be used as a default current
 *     directory during initialization. Can be null, for no default path.
 * @param {!Array<TestEntryInfo>} initialLocalEntries List of initial
 *     entries to load in Downloads (defaults to a basic entry set).
 * @param {!Array<TestEntryInfo>} initialDriveEntries List of initial
 *     entries to load in Google Drive (defaults to a basic entry set).
 * @param {?FilesAppState=} appState App state to be passed with on opening the
 *     Files app.
 * @return {Promise} Promise to be fulfilled with the window ID.
 */
export async function setupAndWaitUntilReady(
    initialRoot, initialLocalEntries = BASIC_LOCAL_ENTRY_SET,
    initialDriveEntries = BASIC_DRIVE_ENTRY_SET, appState = {}) {
  const localEntriesPromise = addEntries(['local'], initialLocalEntries);
  const driveEntriesPromise = addEntries(['drive'], initialDriveEntries);

  const appId = await openNewWindow(initialRoot, appState);
  await remoteCall.waitForElement(appId, '#detail-table');

  // Wait until the elements are loaded in the table.
  await Promise.all([
    remoteCall.waitForFileListChange(appId, 0),
    localEntriesPromise,
    driveEntriesPromise,
  ]);
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
  return appId;
}

/**
 * Returns the name of the given file list entry.
 * @param {Array<string>} fileListEntry An entry in a file list.
 * @return {string} Name of the file.
 */
export function getFileName(fileListEntry) {
  return fileListEntry[0];
}

/**
 * Returns the size of the given file list entry.
 * @param {Array<string>} fileListEntry An entry in a file list.
 * @return {string} Size of the file.
 */
export function getFileSize(fileListEntry) {
  return fileListEntry[1];
}

/**
 * Returns the type of the given file list entry.
 * @param {Array<string>} fileListEntry An entry in a file list.
 * @return {string} Type of the file.
 */
export function getFileType(fileListEntry) {
  return fileListEntry[2];
}

/**
 * A value that when returned by an async test indicates that app errors should
 * not be checked following completion of the test.
 */
export const IGNORE_APP_ERRORS = Symbol('IGNORE_APP_ERRORS');

/**
 * For async function tests, wait for the test to complete, check for app errors
 * unless skipped, and report the results.
 * @param {Promise} resultPromise A promise that resolves with the test result.
 * @private
 */
export async function awaitAsyncTestResult(resultPromise) {
  chrome.test.assertTrue(
      resultPromise instanceof Promise, 'test did not return a Promise');

  // Hold a pending callback to ensure the test doesn't complete early.
  const passCallback = chrome.test.callbackPass();

  try {
    await resultPromise;
  } catch (error) {
    // If the test has failed, ignore the exception and return.
    if (error == 'chrome.test.failure') {
      return;
    }

    // Otherwise, report the exception as a test failure. chrome.test.fail()
    // emits an exception; catch it to avoid spurious logging about an uncaught
    // exception.
    try {
      chrome.test.fail(error.stack || error);
    } catch (_) {
      return;
    }
  }

  passCallback();
}

/**
 * When the FileManagerBrowserTest harness loads this test extension, request
 * configuration and other details from that harness, including the test case
 * name to run. Use the configuration/details to setup the test environment,
 * then run the test case using chrome.test.RunTests.
 */
window.addEventListener('load', () => {
  const steps = [
    // Request the guest mode state.
    () => {
      remoteCall = new RemoteCallFilesApp(FILE_MANAGER_SWA_ID);
      sendBrowserTestCommand({name: 'isInGuestMode'}, steps.shift());
    },
    // Request the root entry paths.
    mode => {
      if (JSON.parse(mode) != chrome.extension.inIncognitoContext) {
        return;
      }
      sendBrowserTestCommand({name: 'getRootPaths'}, steps.shift());
    },
    // Request the test case name.
    paths => {
      const roots = /** @type {getRootPathsResult} */ (JSON.parse(paths));
      RootPath.DOWNLOADS = roots.downloads;
      RootPath.MY_FILES = roots.my_files;
      RootPath.DRIVE = roots.drive;
      RootPath.ANDROID_FILES = roots.android_files;
      sendBrowserTestCommand({name: 'getTestName'}, steps.shift());
    },
    // Run the test case.
    testCaseName => {
      // Get the test function from testcase namespace testCaseName.
      const test = testcase[testCaseName];
      // Verify test is an unnamed (aka 'anonymous') Function.
      if (!(test instanceof Function) || test.name) {
        chrome.test.fail('[' + testCaseName + '] not found.');
        return;
      }
      // Define the test case and its name for chrome.test logging.
      test.generatedName = testCaseName;
      const testCaseSymbol = Symbol(testCaseName);
      const testCase = {
        [testCaseSymbol]: () => {
          return awaitAsyncTestResult(test());
        },
      };
      // Run the test.
      chrome.test.runTests([testCase[testCaseSymbol]]);
    },
  ];
  steps.shift()();
});

/**
 * Creates a folder shortcut to |directoryName| using the context menu. Note the
 * current directory must be a parent of the given |directoryName|.
 *
 * @param {string} appId Files app windowId.
 * @param {string} directoryName Directory of shortcut to be created.
 * @return {Promise} Promise fulfilled on success.
 */
export async function createShortcut(appId, directoryName) {
  await remoteCall.waitUntilSelected(appId, directoryName);

  await remoteCall.waitForElement(appId, ['.table-row[selected]']);
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');
  await remoteCall.waitForElement(
      appId, '[command="#pin-folder"]:not([hidden]):not([disabled])');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['[command="#pin-folder"]:not([hidden]):not([disabled])']));

  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.waitForShortcutItemByLabel(directoryName);
}

/**
 * Mounts crostini volume by clicking on the fake crostini root.
 * @param {string} appId Files app windowId.
 * @param {!Array<TestEntryInfo>} initialEntries List of initial entries to
 *     load in Crostini (defaults to a basic entry set).
 */
export async function mountCrostini(
    appId, initialEntries = BASIC_CROSTINI_ENTRY_SET) {
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Add entries to crostini volume, but do not mount.
  await addEntries(['crostini'], initialEntries);

  // Linux files fake root is shown.
  await directoryTree.waitForPlaceholderItemByType('crostini');

  // Mount crostini, and ensure real root and files are shown.
  await directoryTree.selectPlaceholderItemByType('crostini');
  await directoryTree.waitForItemByType('crostini');
  const files = TestEntryInfo.getExpectedRows(BASIC_CROSTINI_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files);
}

/**
 * Registers a GuestOS, mounts the volume, and populates it with tbe specified
 * entries.
 * @param {string} appId Files app windowId.
 * @param {!Array<!TestEntryInfo>} initialEntries List of initial entries to
 *     load in the volume.
 */
export async function mountGuestOs(appId, initialEntries) {
  await sendTestMessage({
    name: 'registerMountableGuest',
    displayName: 'Bluejohn',
    canMount: true,
    vmType: 'bruschetta',
  });
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Wait for the GuestOS fake root then click it.
  await directoryTree.selectPlaceholderItemByType('bruschetta');

  // Wait for the volume to get mounted.
  await directoryTree.waitForItemByType('bruschetta');

  // Add entries to GuestOS volume
  await addEntries(['guest_os_0'], initialEntries);

  // Ensure real root and files are shown.
  const files = TestEntryInfo.getExpectedRows(initialEntries);
  await remoteCall.waitForFiles(appId, files);
}

/**
 * Returns true if the SinglePartitionFormat flag is on.
 * @param {string} appId Files app windowId.
 */
export async function isSinglePartitionFormat(appId) {
  const dialog = await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog']);
  const flag = dialog.attributes['single-partition-format'] || '';
  return !!flag;
}

/**
 * Waits until the MediaApp/Backlight shows up.
 */
export async function waitForMediaApp() {
  // The MediaApp window should open for the file.
  const caller = getCaller();
  const mediaAppAppId = 'jhdjimmaggjajfjphpljagpgkidjilnj';
  await repeatUntil(async () => {
    const result = await sendTestMessage({
      name: 'hasSwaStarted',
      swaAppId: mediaAppAppId,
    });

    if (result !== 'true') {
      return pending(caller, 'Waiting for MediaApp to open');
    }
  });
}
