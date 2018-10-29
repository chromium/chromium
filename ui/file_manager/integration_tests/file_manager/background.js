// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Extension ID of the Files app.
 * @type {string}
 * @const
 */
var FILE_MANAGER_EXTENSIONS_ID = 'hhaomjibdihmijegdhdafkllkbggdgoj';

var remoteCall = new RemoteCallFilesApp(FILE_MANAGER_EXTENSIONS_ID);

/**
 * Extension ID of Gallery.
 * @type {string}
 * @const
 */
var GALLERY_APP_ID = 'nlkncpkkdoccmpiclbokaimcnedabhhm';

var galleryApp = new RemoteCallGallery(GALLERY_APP_ID);

/**
 * Extension ID of Audio Player.
 * @type {string}
 * @const
 */
var AUDIO_PLAYER_APP_ID = 'cjbfomnbifhcdnihkgipgfcihmgjfhbf';

var audioPlayerApp = new RemoteCall(AUDIO_PLAYER_APP_ID);

/**
 * App ID of Video Player.
 * @type {string}
 * @const
 */
var VIDEO_PLAYER_APP_ID = 'jcgeabjmjgoblfofpppfkcoakmfobdko';

var videoPlayerApp = new RemoteCall(VIDEO_PLAYER_APP_ID);

/**
 * Adds check of chrome.test to the end of the given promise.
 * @param {Promise} promise Promise.
 */
function testPromise(promise) {
  return testPromiseAndApps(
      promise,
      [remoteCall, galleryApp, audioPlayerApp, videoPlayerApp]);
}

/**
 * Executes a sequence of test steps.
 * @constructor
 */
function StepsRunner() {
  /**
   * Function to notify the end of the current closure.
   * @type {?function}
   * @private
   */
  this.next_ = null;
}

/**
 * Creates a StepsRunner instance and runs the passed steps.
 * @param {!Array<function>} steps
 * @return {Promise} Promise to be fulfilled after test finishes.
 */
StepsRunner.run = function(steps) {
  var stepsRunner = new StepsRunner();
  return stepsRunner.run_(steps);
};

/**
 * Creates a StepsRunner instance and runs multiple groups of steps.
 * @param {!Array<!Array<function>>} groups
 */
StepsRunner.runGroups = function(groups) {
  // Squash all groups into a flat list of steps.
  StepsRunner.run(Array.prototype.concat.apply([], groups));
};

StepsRunner.prototype = {
  /**
   * @return {?function} The next closure.
   */
  get next() {
    var next = this.next_;
    this.next_ = null;
    return next;
  }
};

/**
 * Runs a sequence of the added test steps.
 * @type {Array<function>} List of the sequential steps.
 * @return {Promise} Promise to be fulfilled after test finishes.
 */
StepsRunner.prototype.run_ = function(steps) {
  return steps.reduce(function(previousPromise, currentClosure) {
    return previousPromise.then(function(arg) {
      return new Promise(function(resolve, reject) {
        this.next_ = resolve;
        currentClosure.apply(this, [arg]);
      }.bind(this));
    }.bind(this));
  }.bind(this), Promise.resolve())
  // Adds the last closure to notify the completion of the run.
  .then(chrome.test.callbackPass(function() {
    return true;
  }));
};

/**
 * Basic entry set for the local volume.
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var BASIC_LOCAL_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos
];

/**
 * Basic entry set for the drive volume that only includes read-write entries
 * (no read-only or similar entries).
 *
 * TODO(hirono): Add a case for an entry cached by FileCache. For testing
 *               Drive, create more entries with Drive specific attributes.
 * TODO(sashab): Merge items from COMPLEX_DRIVE_ENTRY_SET into here (so all
 *               tests run with read-only files) once crbug.com/850834 is fixed.
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var BASIC_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument
];

/**
 * Basic entry set for the local crostini volume.
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
var BASIC_CROSTINI_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
];

/**
 * More complex entry set for Drive that includes entries with varying
 * permissions (such as read-only entries).
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var COMPLEX_DRIVE_ENTRY_SET = [
  ENTRIES.hello, ENTRIES.photos, ENTRIES.readOnlyFolder,
  ENTRIES.readOnlyDocument, ENTRIES.readOnlyStrictDocument, ENTRIES.readOnlyFile
];

/**
 * Nested entry set (directories inside each other).
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var NESTED_ENTRY_SET = [
  ENTRIES.directoryA,
  ENTRIES.directoryB,
  ENTRIES.directoryC
];

/**
 * Expected list of preset entries in fake test volumes. This should be in sync
 * with FakeTestVolume::PrepareTestEntries in the test harness.
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var BASIC_FAKE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.directoryA
];

/**
 * Expected files shown in "Recent". Directories (e.g. 'photos') are not in this
 * list as they are not expected in "Recent".
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var RECENT_ENTRY_SET = [
  ENTRIES.desktop,
  ENTRIES.beautiful,
];

/**
 * Expected files shown in "Offline", which should have the files
 * "available offline". Google Documents, Google Spreadsheets, and the files
 * cached locally are "available offline".
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var OFFLINE_ENTRY_SET = [
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument
];

/**
 * Expected files shown in "Shared with me", which should be the entries labeled
 * with "shared-with-me".
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var SHARED_WITH_ME_ENTRY_SET = [
  ENTRIES.testSharedDocument
];

/**
 * Entry set for Drive that includes team drives of various permissions and
 * nested files with various permissions.
 *
 * TODO(sashab): Add support for capabilities of Team Drive roots.
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var TEAM_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.teamDriveA,
  ENTRIES.teamDriveAFile,
  ENTRIES.teamDriveAHostedFile,
  ENTRIES.teamDriveB,
  ENTRIES.teamDriveBFile,
];

/**
 * Opens a Files app's main window.
 *
 * TODO(mtomasz): Pass a volumeId or an enum value instead of full paths.
 *
 * @param {Object} appState App state to be passed with on opening the Files
 *     app. Can be null.
 * @param {?string} initialRoot Root path to be used as a default current
 *     directory during initialization. Can be null, for no default path.
 * @param {function(string)=} opt_callback Callback with the app id.
 * @return {Promise} Promise to be fulfilled after window creating.
 */
function openNewWindow(appState, initialRoot, opt_callback) {
  // TODO(mtomasz): Migrate from full paths to a pair of a volumeId and a
  // relative path. To compose the URL communicate via messages with
  // file_manager_browser_test.cc.
  var processedAppState = appState || {};
  if (initialRoot) {
    processedAppState.currentDirectoryURL =
        'filesystem:chrome-extension://' + FILE_MANAGER_EXTENSIONS_ID +
        '/external' + initialRoot;
  }

  return remoteCall.callRemoteTestUtil('openMainWindow',
                                       null,
                                       [processedAppState],
                                       opt_callback);
}

/**
 * Opens a file dialog and waits for closing it.
 *
 * @param {Object} dialogParams Dialog parameters to be passed to chrome.
 *     fileSystem.chooseEntry() API.
 * @param {string} volumeName Volume name passed to the selectVolume remote
 *     funciton.
 * @param {Array<TestEntryInfo>} expectedSet Expected set of the entries.
 * @param {function(windowId:string):Promise} closeDialog Function to close the
 *     dialog.
 * @return {Promise} Promise to be fulfilled with the result entry of the
 *     dialog.
 */
function openAndWaitForClosingDialog(
    dialogParams, volumeName, expectedSet, closeDialog) {
  var caller = getCaller();
  var resultPromise = new Promise(function(fulfill) {
    chrome.fileSystem.chooseEntry(
        dialogParams,
        function(entry) { fulfill(entry); });
    chrome.test.assertTrue(!chrome.runtime.lastError, 'chooseEntry failed.');
  });

  return remoteCall.waitForWindow('dialog#').then(function(windowId) {
    return remoteCall.waitForElement(windowId, '#file-list').
        then(function() {
          // Wait for initialization of the Files app.
          return remoteCall.waitForFiles(
              windowId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));
        }).
        then(function() {
          return remoteCall.callRemoteTestUtil(
              'selectVolume', windowId, [volumeName]);
        }).
        then(function() {
          var expectedRows = TestEntryInfo.getExpectedRows(expectedSet);
          return remoteCall.waitForFiles(windowId, expectedRows);
        }).
        then(closeDialog.bind(null, windowId)).
        then(function() {
          return repeatUntil(function() {
            return remoteCall.callRemoteTestUtil('getWindows', null, []).
                then(function(windows) {
                  if (windows[windowId])
                    return pending(
                        caller, 'Window %s does not hide.', windowId);
                  else
                    return resultPromise;
                });
          });
        });
  });
}

/**
 * Opens a Files app's main window and waits until it is initialized. Fills
 * the window with initial files. Should be called for the first window only.
 *
 * TODO(mtomasz): Pass a volumeId or an enum value instead of full paths.
 *
 * @param {Object} appState App state to be passed with on opening the Files
 *     app. Can be null.
 * @param {?string} initialRoot Root path to be used as a default current
 *     directory during initialization. Can be null, for no default path.
 * @param {function(string, Array<Array<string>>)=} opt_callback Callback with
 *     the window ID and with the file list.
 * @param {!Array<TestEntryInfo>>} opt_initialLocalEntries List of initial
 *     entries to load in Google Drive (defaults to a basic entry set).
 * @param {!Array<TestEntryInfo>>} opt_initialDriveEntries List of initial
 *     entries to load in Google Drive (defaults to a basic entry set).
 * @return {Promise} Promise to be fulfilled with the result object, which
 *     contains the window ID and the file list.
 */
function setupAndWaitUntilReady(
    appState, initialRoot, opt_callback, opt_initialLocalEntries,
    opt_initialDriveEntries) {
  var initialLocalEntries = opt_initialLocalEntries || BASIC_LOCAL_ENTRY_SET;
  var initialDriveEntries = opt_initialDriveEntries || BASIC_DRIVE_ENTRY_SET;
  var windowPromise = openNewWindow(appState, initialRoot);
  var localEntriesPromise = addEntries(['local'], initialLocalEntries);
  var driveEntriesPromise = addEntries(['drive'], initialDriveEntries);
  var detailedTablePromise = windowPromise.then(function(windowId) {
    return remoteCall.waitForElement(windowId, '#detail-table').
      then(function() {
        // Wait until the elements are loaded in the table.
        return remoteCall.waitForFileListChange(windowId, 0);
      });
  });

  if (opt_callback)
    opt_callback = chrome.test.callbackPass(opt_callback);

  return Promise.all([
    windowPromise,
    localEntriesPromise,
    driveEntriesPromise,
    detailedTablePromise
  ]).then(function(results) {
    var result = {windowId: results[0], fileList: results[3]};
    if (opt_callback)
      opt_callback(result);
    return result;
  }).catch(function(e) {
    chrome.test.fail(e.stack || e);
  });
}

/**
 * Verifies if there are no Javascript errors in any of the app windows.
 * @param {function()} Completion callback.
 */
function checkIfNoErrorsOccured(callback) {
  checkIfNoErrorsOccuredOnApp(remoteCall, callback);
}

/**
 * Returns the name of the given file list entry.
 * @param {Array<string>} file An entry in a file list.
 * @return {string} Name of the file.
 */
function getFileName(fileListEntry) {
  return fileListEntry[0];
}

/**
 * Returns the size of the given file list entry.
 * @param {Array<string>} An entry in a file list.
 * @return {string} Size of the file.
 */
function getFileSize(fileListEntry) {
  return fileListEntry[1];
}

/**
 * Returns the type of the given file list entry.
 * @param {Array<string>} An entry in a file list.
 * @return {string} Type of the file.
 */
function getFileType(fileListEntry) {
  return fileListEntry[2];
}

/**
 * Namespace for test cases.
 */
var testcase = {};

/**
 * When the FileManagerBrowserTest harness loads this test extension, request
 * configuration and other details from that harness, including the test case
 * name to run. Use the configuration/details to setup the test ennvironment,
 * then run the test case using chrome.test.RunTests.
 */
window.addEventListener('load', function() {
  var steps = [
    // Request the guest mode state.
    function() {
      sendBrowserTestCommand({name: 'isInGuestMode'}, steps.shift());
    },
    // Request the root entry paths.
    function(mode) {
      if (JSON.parse(mode) != chrome.extension.inIncognitoContext)
        return;
      sendBrowserTestCommand({name: 'getRootPaths'}, steps.shift());
    },
    // Request the test case name.
    function(paths) {
      var roots = JSON.parse(paths);
      RootPath.DOWNLOADS = roots.downloads;
      RootPath.DRIVE = roots.drive;
      RootPath.ANDROID_FILES = roots.android_files;
      sendBrowserTestCommand({name: 'getTestName'}, steps.shift());
    },
    // Run the test case.
    function(testCaseName) {
      // Get the test function from testcase namespace testCaseName.
      var test = testcase[testCaseName];
      // Verify test is an unnamed (aka 'anonymous') Function.
      if (!(test instanceof Function) || test.name) {
        chrome.test.fail('[' + testCaseName + '] not found.');
        return;
      }
      // Define the test case and its name for chrome.test logging.
      test.generatedName = testCaseName;
      var testCaseSymbol = Symbol(testCaseName);
      var testCase = {
        [testCaseSymbol] :() => {
          return test();
        },
      };
      // Run the test.
      chrome.test.runTests([testCase[testCaseSymbol]]);
    }
  ];
  steps.shift()();
});
