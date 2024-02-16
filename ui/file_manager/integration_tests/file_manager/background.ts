// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppState} from '../prod/file_manager/shared_types.js';
import {RemoteCallFilesApp} from '../remote_call.js';
import {addEntries, getCaller, GetRootPathsResult, pending, repeatUntil, RootPath, sendBrowserTestCommand, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {CHOOSE_ENTRY_PROPERTY} from './choose_entry_const.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_CROSTINI_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, FILE_MANAGER_EXTENSIONS_ID} from './test_data.js';

/** Application ID (URL) for File Manager System Web App (SWA). */
export const FILE_MANAGER_SWA_ID = 'chrome://file-manager';

export {FILE_MANAGER_EXTENSIONS_ID};

export let remoteCall: RemoteCallFilesApp;

/**
 * Opens a Files app's main window.
 * @param initialRoot Root path to be used as a default current directory during
 *     initialization. Can be null, for no default path.
 * @param appState App state to be passed with on opening the Files app.
 * @return Promise to be fulfilled after window creating.
 */
export async function openNewWindow(
    initialRoot: null|string, appState?: null|FilesAppState): Promise<string> {
  appState = appState ?? {};
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
 *
 * @return Promise fulfilled when a foreground window opens.
 */
export async function openEntryChoosingWindow(
    params: chrome.fileSystem.ChooseEntryOptions):
    Promise<chrome.windows.Window> {
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
 * @return the entry set by the dialog shown via chooseEntry().
 */
export async function pollForChosenEntry(caller: string):
    Promise<null|(Entry | Entry[])> {
  await repeatUntil(() => {
    if (window[CHOOSE_ENTRY_PROPERTY] !== undefined) {
      return;
    }
    return pending(caller, 'Waiting for chooseEntry() result');
  });
  return window[CHOOSE_ENTRY_PROPERTY]!;
}

/**
 * Opens a file dialog and waits for closing it.
 * @param dialogParams Dialog parameters to be passed to
 *     openEntryChoosingWindow() function.
 * @param volumeType Volume icon type passed to the directory page object's
 *     selectItemByType function.
 * @param expectedSet Expected set of the entries.
 * @param closeDialog Function to close the dialog.
 * @param useBrowserOpen Whether to launch the select file dialog via a browser
 *     OpenFile() call.
 * @param debug Whether to debug the waitForWindow().
 * @return Promise to be fulfilled with the result entry of the dialog.
 */
export async function openAndWaitForClosingDialog(
    dialogParams: chrome.fileSystem.ChooseEntryOptions, volumeType: string,
    expectedSet: TestEntryInfo[], closeDialog: (a: string) => Promise<void>,
    useBrowserOpen: boolean = false, debug: boolean = false): Promise<unknown> {
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
    if (windows[appId] !== appId) {
      return;
    }
    return pending(caller, 'Waiting for Window %s to hide.', appId);
  });
  return await resultPromise();
}

/**
 * Opens a Files app's main window and waits until it is initialized. Fills
 * the window with initial files. Should be called for the first window only.
 * @param initialRoot Root path to be used as a default current directory during
 *     initialization. Can be null, for no default path.
 * @param initialLocalEntries List of initial entries to load in Downloads
 *     (defaults to a basic entry set).
 * @param initialDriveEntries List of initial entries to load in Google Drive
 *     (defaults to a basic entry set).
 * @param appState App state to be passed with on opening the Files app.
 * @return Promise to be fulfilled with the window ID.
 */
export async function setupAndWaitUntilReady(
    initialRoot: null|string,
    initialLocalEntries: TestEntryInfo[] = BASIC_LOCAL_ENTRY_SET,
    initialDriveEntries: TestEntryInfo[] = BASIC_DRIVE_ENTRY_SET,
    appState?: FilesAppState): Promise<string> {
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
 * @param fileListEntry An entry in a file list.
 * @return Name of the file.
 */
export function getFileName(fileListEntry: string[]): string {
  return fileListEntry[0] ?? '';
}

/**
 * Returns the size of the given file list entry.
 * @param fileListEntry An entry in a file list.
 * @return Size of the file.
 */
export function getFileSize(fileListEntry: string[]): string {
  return fileListEntry[1] ?? '';
}

/**
 * Returns the type of the given file list entry.
 * @param fileListEntry An entry in a file list.
 * @return Type of the file.
 */
export function getFileType(fileListEntry: string[]): string {
  return fileListEntry[2] ?? '';
}

/**
 * For async function tests, wait for the test to complete, check for app errors
 * unless skipped, and report the results.
 * @param resultPromise A promise that resolves with the test result.
 */
async function awaitAsyncTestResult(resultPromise: Promise<void|any>) {
  chrome.test.assertTrue(
      resultPromise instanceof Promise, 'test did not return a Promise');

  try {
    await resultPromise;
  } catch (error: any) {
    // If the test has failed, ignore the exception and return.
    if (error === 'chrome.test.failure') {
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

  chrome.test.succeed();
}

/**
 * When the FileManagerBrowserTest harness loads this test extension, request
 * configuration and other details from that harness, including the test case
 * name to run. Use the configuration/details to setup the test environment,
 * then run the test case using chrome.test.RunTests.
 */
window.addEventListener('load', async () => {
  // Request the guest mode state.
  remoteCall = new RemoteCallFilesApp(FILE_MANAGER_SWA_ID);
  const mode = await sendBrowserTestCommand({name: 'isInGuestMode'});

  // Request the root entry paths.
  if (JSON.parse(mode) !== chrome.extension.inIncognitoContext) {
    return;
  }
  const paths = await sendBrowserTestCommand({name: 'getRootPaths'});
  // Request the test case name.
  const roots: GetRootPathsResult = JSON.parse(paths);
  RootPath.DOWNLOADS = roots.downloads;
  RootPath.MY_FILES = roots.my_files;
  RootPath.DRIVE = roots.drive;
  RootPath.ANDROID_FILES = roots.android_files;
  const testCaseName = await sendBrowserTestCommand({name: 'getTestName'});

  // Get the test function from testcase namespace testCaseName.
  const test = testcase[testCaseName];
  // Verify test is a Function without args.
  if (!(test instanceof Function && test.length === 0)) {
    chrome.test.fail('[' + testCaseName + '] not found.');
  }
  // Define the test case and its name for chrome.test logging.
  const testCase = {
    [testCaseName]: () => {
      return awaitAsyncTestResult(test());
    },
  };

  // Run the test.
  chrome.test.runTests([testCase[testCaseName]!]);
});

/**
 * Creates a folder shortcut to |directoryName| using the context menu. Note
 * the current directory must be a parent of the given |directoryName|.
 *
 * @param appId Files app windowId.
 * @param directoryName Directory of shortcut to be created.
 * @return Promise fulfilled on success.
 */
export async function createShortcut(
    appId: string, directoryName: string): Promise<void> {
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
 * @param appId Files app windowId.
 * @param initialEntries List of initial entries to load in Crostini (defaults
 *     to a basic entry set).
 */
export async function mountCrostini(
    appId: string, initialEntries: TestEntryInfo[] = BASIC_CROSTINI_ENTRY_SET) {
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);

  // Add entries to crostini volume, but do not mount.
  await addEntries(['crostini'], initialEntries);

  // Linux files fake root is shown.
  await directoryTree.waitForPlaceholderItemByType('crostini');

  // Mount crostini, and ensure real root and files are shown.
  await directoryTree.selectPlaceholderItemByType('crostini');
  await directoryTree.waitForItemByType('crostini');
  const files = TestEntryInfo.getExpectedRows(initialEntries);
  await remoteCall.waitForFiles(appId, files);
}

/**
 * Registers a GuestOS, mounts the volume, and populates it with tbe specified
 * entries.
 * @param appId Files app windowId.
 * @param initialEntries List of initial entries to load in the volume.
 */
export async function mountGuestOs(
    appId: string, initialEntries: TestEntryInfo[]) {
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
 * @param appId Files app windowId.
 */
export async function isSinglePartitionFormat(appId: string) {
  const dialog = await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog']);
  const flag = dialog.attributes['single-partition-format'] || '';
  return !!flag;
}

/** Waits until the MediaApp/Backlight shows up. */
export async function waitForMediaApp() {
  // The MediaApp window should open for the file.
  const caller = getCaller();
  const mediaAppAppId = 'jhdjimmaggjajfjphpljagpgkidjilnj';
  await repeatUntil(async () => {
    const result = await sendTestMessage({
      name: 'hasSwaStarted',
      swaAppId: mediaAppAppId,
    });

    if (result === 'true') {
      return;
    }
    return pending(caller, 'Waiting for MediaApp to open');
  });
}
