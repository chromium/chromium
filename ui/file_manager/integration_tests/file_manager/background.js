// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Extension ID of the Files app.
 * @type {string}
 * @const
 */
const FILE_MANAGER_EXTENSIONS_ID = 'hhaomjibdihmijegdhdafkllkbggdgoj';

const remoteCall = new RemoteCallFilesApp(FILE_MANAGER_EXTENSIONS_ID);

/**
 * Extension ID of Gallery.
 * @type {string}
 * @const
 */
const GALLERY_APP_ID = 'nlkncpkkdoccmpiclbokaimcnedabhhm';

const galleryApp = new RemoteCallGallery(GALLERY_APP_ID);

/**
 * Extension ID of Audio Player.
 * @type {string}
 * @const
 */
const AUDIO_PLAYER_APP_ID = 'cjbfomnbifhcdnihkgipgfcihmgjfhbf';

const audioPlayerApp = new RemoteCall(AUDIO_PLAYER_APP_ID);

/**
 * App ID of Video Player.
 * @type {string}
 * @const
 */
const VIDEO_PLAYER_APP_ID = 'jcgeabjmjgoblfofpppfkcoakmfobdko';

const videoPlayerApp = new RemoteCall(VIDEO_PLAYER_APP_ID);

/**
 * Basic entry set for the local volume.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const BASIC_LOCAL_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
];

/**
 * Expected files shown in Downloads with hidden enabled
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
const BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN = BASIC_LOCAL_ENTRY_SET.concat([
  ENTRIES.hiddenFile,
  ENTRIES.dotTrash,
]);

/**
 * Basic entry set for the drive volume that only includes read-write entries
 * (no read-only or similar entries).
 *
 * TODO(hirono): Add a case for an entry cached by FileCache. For testing
 *               Drive, create more entries with Drive specific attributes.
 * TODO(sashab): Merge items from COMPLEX_DRIVE_ENTRY_SET into here (so all
 *               tests run with read-only files) once crbug.com/850834 is fixed.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const BASIC_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument,
  ENTRIES.testSharedFile,
];

/**
 * Expected files shown in Drive with hidden enabled
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
const BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN = BASIC_DRIVE_ENTRY_SET.concat([
  ENTRIES.hiddenFile,
]);


/**
 * Expected files shown in Drive with Google Docs disabled
 *
 * @type {!Array<!TestEntryInfo>}
 */
const BASIC_DRIVE_ENTRY_SET_WITHOUT_GDOCS = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testSharedFile,
];

/**
 * Basic entry set for the local crostini volume.
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
const BASIC_CROSTINI_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
];

/**
 * More complex entry set for Drive that includes entries with varying
 * permissions (such as read-only entries).
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const COMPLEX_DRIVE_ENTRY_SET = [
  ENTRIES.hello, ENTRIES.photos, ENTRIES.readOnlyFolder,
  ENTRIES.readOnlyDocument, ENTRIES.readOnlyStrictDocument, ENTRIES.readOnlyFile
];

/**
 * More complex entry set for DocumentsProvider that includes entries with
 * arying permissions (such as read-only entries).
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET = [
  ENTRIES.hello, ENTRIES.photos, ENTRIES.readOnlyFolder, ENTRIES.readOnlyFile,
  ENTRIES.deletableFile, ENTRIES.renamableFile
];

/**
 * Nested entry set (directories inside each other).
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const NESTED_ENTRY_SET = [
  ENTRIES.directoryA,
  ENTRIES.directoryB,
  ENTRIES.directoryC,
];

/**
 * Expected list of preset entries in fake test volumes. This should be in sync
 * with FakeTestVolume::PrepareTestEntries in the test harness.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const BASIC_FAKE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.directoryA,
];

/**
 * Expected files shown in "Recent". Directories (e.g. 'photos') are not in this
 * list as they are not expected in "Recent".
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const RECENT_ENTRY_SET = [
  ENTRIES.desktop,
  ENTRIES.beautiful,
];

/**
 * Expected files shown in "Offline", which should have the files
 * "available offline". Google Documents, Google Spreadsheets, and the files
 * cached locally are "available offline".
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const OFFLINE_ENTRY_SET = [
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument,
  ENTRIES.testSharedFile,
];

/**
 * Expected files shown in "Shared with me", which should be the entries labeled
 * with "shared-with-me".
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const SHARED_WITH_ME_ENTRY_SET = [
  ENTRIES.testSharedDocument,
  ENTRIES.testSharedFile,
];

/**
 * Entry set for Drive that includes team drives of various permissions and
 * nested files with various permissions.
 *
 * TODO(sashab): Add support for capabilities of Shared Drive roots.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const SHARED_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.teamDriveA,
  ENTRIES.teamDriveAFile,
  ENTRIES.teamDriveADirectory,
  ENTRIES.teamDriveAHostedFile,
  ENTRIES.teamDriveB,
  ENTRIES.teamDriveBFile,
  ENTRIES.teamDriveBDirectory,
];

/**
 * Entry set for Drive that includes Computers, including nested computers with
 * files and nested "USB and External Devices" with nested devices.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const COMPUTERS_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.computerA,
  ENTRIES.computerAFile,
  ENTRIES.computerAdirectoryA,
];

/**
 * Basic entry set for the android volume.
 *
 * @type {!Array<TestEntryInfo>}
 * @const
 */
const BASIC_ANDROID_ENTRY_SET = [
  ENTRIES.directoryDocuments,
  ENTRIES.directoryMovies,
  ENTRIES.directoryMusic,
  ENTRIES.directoryPictures,
];

/**
 * Expected files shown in Android with hidden enabled
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
const BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN = BASIC_ANDROID_ENTRY_SET.concat([
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.directoryA,
]);

/**
 * Opens a Files app's main window.
 *
 * TODO(mtomasz): Pass a volumeId or an enum value instead of full paths.
 *
 * @param {?string} initialRoot Root path to be used as a default current
 *     directory during initialization. Can be null, for no default path.
 * @param {Object} appState App state to be passed with on opening the Files
 *     app.
 * @return {Promise} Promise to be fulfilled after window creating.
 */
function openNewWindow(initialRoot, appState = {}) {
  // TODO(mtomasz): Migrate from full paths to a pair of a volumeId and a
  // relative path. To compose the URL communicate via messages with
  // file_manager_browser_test.cc.
  if (initialRoot) {
    appState.currentDirectoryURL = 'filesystem:chrome-extension://' +
        FILE_MANAGER_EXTENSIONS_ID + '/external' + initialRoot;
  }

  return remoteCall.callRemoteTestUtil('openMainWindow', null, [appState]);
}

/**
 * Opens a file dialog and waits for closing it.
 *
 * @param {chrome.fileSystem.AcceptsOption} dialogParams Dialog parameters to be
 *     passed to chrome. fileSystem.chooseEntry() API.
 * @param {string} volumeName Volume name passed to the selectVolume remote
 *     function.
 * @param {Array<TestEntryInfo>} expectedSet Expected set of the entries.
 * @param {function(string):Promise} closeDialog Function to close the
 *     dialog.
 * @param {boolean} useBrowserOpen Whether to launch the select file dialog via
 *     a browser OpenFile() call.
 * @return {Promise} Promise to be fulfilled with the result entry of the
 *     dialog.
 */
async function openAndWaitForClosingDialog(
    dialogParams, volumeName, expectedSet, closeDialog,
    useBrowserOpen = false) {
  const caller = getCaller();
  let resultPromise;
  if (useBrowserOpen) {
    resultPromise = sendTestMessage({name: 'runSelectFileDialog'});
  } else {
    resultPromise = new Promise(fulfill => {
      chrome.fileSystem.chooseEntry(dialogParams, entry => {
        fulfill(entry);
      });
      chrome.test.assertTrue(!chrome.runtime.lastError, 'chooseEntry failed.');
    });
  }

  const appId = await remoteCall.waitForWindow('dialog#');
  await remoteCall.waitForElement(appId, '#file-list');
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('selectVolume', appId, [volumeName]),
      'selectVolume failed');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(expectedSet));
  await closeDialog(appId);
  await repeatUntil(async () => {
    const windows = await remoteCall.callRemoteTestUtil('getWindows', null, []);
    if (windows[appId]) {
      return pending(caller, 'Waiting for Window %s to hide.', appId);
    }
  });
  return resultPromise;
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
 * @param {Object} appState App state to be passed with on opening the Files
 *     app.
 * @return {Promise} Promise to be fulfilled with the window ID.
 */
async function setupAndWaitUntilReady(
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
function getFileName(fileListEntry) {
  return fileListEntry[0];
}

/**
 * Returns the size of the given file list entry.
 * @param {Array<string>} fileListEntry An entry in a file list.
 * @return {string} Size of the file.
 */
function getFileSize(fileListEntry) {
  return fileListEntry[1];
}

/**
 * Returns the type of the given file list entry.
 * @param {Array<string>} fileListEntry An entry in a file list.
 * @return {string} Type of the file.
 */
function getFileType(fileListEntry) {
  return fileListEntry[2];
}

/**
 * A value that when returned by an async test indicates that app errors should
 * not be checked following completion of the test.
 */
const IGNORE_APP_ERRORS = Symbol('IGNORE_APP_ERRORS');

/**
 * For async function tests, wait for the test to complete, check for app errors
 * unless skipped, and report the results.
 * @param {Promise} resultPromise A promise that resolves with the test result.
 * @private
 */
async function awaitAsyncTestResult(resultPromise) {
  chrome.test.assertTrue(
      resultPromise instanceof Promise, 'test did not return a Promise');

  // Hold a pending callback to ensure the test doesn't complete early.
  const passCallback = chrome.test.callbackPass();

  try {
    const result = await resultPromise;
    if (result !== IGNORE_APP_ERRORS) {
      await checkIfNoErrorsOccuredOnApp(remoteCall);
    }
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
 * Namespace for test cases.
 */
const testcase = {};

/**
 * When the FileManagerBrowserTest harness loads this test extension, request
 * configuration and other details from that harness, including the test case
 * name to run. Use the configuration/details to setup the test ennvironment,
 * then run the test case using chrome.test.RunTests.
 */
window.addEventListener('load', () => {
  const steps = [
    // Request the guest mode state.
    () => {
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
    }
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
async function createShortcut(appId, directoryName) {
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [directoryName]));

  await remoteCall.waitForElement(appId, ['.table-row[selected]']);
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');
  await remoteCall.waitForElement(
      appId, '[command="#pin-folder"]:not([hidden]):not([disabled])');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['[command="#pin-folder"]:not([hidden]):not([disabled])']));

  await remoteCall.waitForElement(
      appId, `.tree-item[entry-label="${directoryName}"]`);
}

/**
 * Expands a single tree item by clicking on its expand icon.
 *
 * @param {string} appId Files app windowId.
 * @param {string} treeItem Query to the tree item that should be expanded.
 * @return {Promise} Promise fulfilled on success.
 */
async function expandTreeItem(appId, treeItem) {
  const expandIcon = treeItem + '> .tree-row[has-children=true] .expand-icon';
  await remoteCall.waitAndClickElement(appId, expandIcon);

  const expandedSubtree = treeItem + '> .tree-children[expanded]';
  await remoteCall.waitForElement(appId, expandedSubtree);
}

/**
 * Uses directory tree to expand each directory in the breadcrumbs path.
 *
 * @param {string} appId Files app windowId.
 * @param {string} breadcrumbsPath Path based in the entry labels like:
 *    /My files/Downloads/photos
 * @return {Promise<string>} Promise fulfilled on success with the selector
 *    query of the last directory expanded.
 */
async function recursiveExpand(appId, breadcrumbsPath) {
  const paths = breadcrumbsPath.split('/').filter(path => path);
  const hasChildren = ' > .tree-row[has-children=true]';

  // Expand each directory in the breadcrumb.
  let query = '#directory-tree';
  for (const parentLabel of paths) {
    // Wait for parent element to be displayed.
    query += ` [entry-label="${parentLabel}"]`;
    await remoteCall.waitForElement(appId, query);

    // Only expand if element isn't expanded yet.
    const elements = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, [query + '[expanded]']);
    if (!elements.length) {
      await remoteCall.waitForElement(appId, query + hasChildren);
      await expandTreeItem(appId, query);
    }
  }

  return query;
}

/**
 * Focus the directory tree and navigates using mouse clicks.
 *
 * @param {!string} appId
 * @param {!string} breadcrumbsPath Path based on the entry labels like:
 *     /My files/Downloads/photos to item that should navigate to.
 * @param {string=} shortcutToPath For shortcuts it navigates to a different
 *   breadcrumbs path, like /My Drive/ShortcutName.
 *   @return {!Promise<string>} the final selector used to click on the desired
 * tree item.
 */
async function navigateWithDirectoryTree(
    appId, breadcrumbsPath, shortcutToPath) {

  // Focus the directory tree.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'focus', appId, ['#directory-tree']),
      'focus failed: #directory-tree');

  const paths = breadcrumbsPath.split('/');
  const leaf = paths.pop();

  // Expand all parents of the leaf entry.
  let query = await recursiveExpand(appId, paths.join('/'));

  // Navigate to the final entry.
  query += ` [entry-label="${leaf}"]`;
  await remoteCall.waitAndClickElement(appId, query);

  // Wait directory to finish scanning its content.
  await remoteCall.waitForElement(appId, `[scan-completed="${leaf}"]`);

  // Wait to navigation to final entry to finish.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, (shortcutToPath || breadcrumbsPath));

  return query;
}

/**
 * Mounts crostini volume by clicking on the fake crostini root.
 * @param {string} appId Files app windowId.
 * @param {!Array<TestEntryInfo>} initialEntries List of initial entries to
 *     load in Crostini (defaults to a basic entry set).
 */
async function mountCrostini(appId, initialEntries = BASIC_CROSTINI_ENTRY_SET) {
  const fakeLinuxFiles = '#directory-tree [root-type-icon="crostini"]';
  const realLinxuFiles = '#directory-tree [volume-type-icon="crostini"]';

  // Add entries to crostini volume, but do not mount.
  await addEntries(['crostini'], initialEntries);

  // Linux files fake root is shown.
  await remoteCall.waitForElement(appId, fakeLinuxFiles);

  // Mount crostini, and ensure real root and files are shown.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [fakeLinuxFiles]);
  await remoteCall.waitForElement(appId, realLinxuFiles);
  const files = TestEntryInfo.getExpectedRows(BASIC_CROSTINI_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files);
}

/**
 * Returns true if the Files app is running with the flag FilesNg.
 * @param {string} appId Files app windowId.
 */
async function isFilesNg(appId) {
  const body = await remoteCall.waitForElement(appId, 'body');
  const cssClass = body.attributes['class'] || '';
  return cssClass.includes('files-ng');
}

/**
 * Returns true if the SinglePartitionFormat flag is on.
 * @param {string} appId Files app windowId.
 */
async function isSinglePartitionFormat(appId) {
  const dialog = await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog']);
  const flag = dialog.attributes['single-partition-format'] || '';
  return !!flag;
}
