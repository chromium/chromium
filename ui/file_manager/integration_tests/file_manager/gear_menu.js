// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Expected files shown in Downloads with hidden enabled
 *
 * @type {!Array<!TestEntryInfo>}
 */
const BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.hiddenFile,
];

/**
 * Expected files shown in Drive with hidden enabled
 *
 * @type {!Array<!TestEntryInfo>}
 */
const BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument,
  ENTRIES.hiddenFile,
];

const BASIC_ANDROID_ENTRY_SET = [
  ENTRIES.directoryDocuments,
  ENTRIES.directoryMovies,
  ENTRIES.directoryMusic,
  ENTRIES.directoryPictures,
];

const BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN = [
  ENTRIES.directoryDocuments,
  ENTRIES.directoryMovies,
  ENTRIES.directoryMusic,
  ENTRIES.directoryPictures,
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.directoryA,
];

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
];

/**
 * Gets the common steps to toggle hidden files in the Files app
 * @param {!Array<!TestEntryInfo>} basicSet Files expected before showing hidden
 * @param {!Array<!TestEntryInfo>} hiddenEntrySet Files expected after showing
 * hidden
 * @return {!Array} The test steps to toggle hidden files
 */
function getTestCaseStepsForHiddenFiles(basicSet, hiddenEntrySet) {
  return getTestCaseStepsForHiddenFilesWithMenuItem(
      basicSet, hiddenEntrySet, '#gear-menu-toggle-hidden-files');
}

/**
 * Gets the common steps to toggle Android hidden files in the Files app
 * @param {!Array<!TestEntryInfo>} basicSet Files expected before showing hidden
 * @param {!Array<!TestEntryInfo>} hiddenEntrySet Files expected after showing
 * hidden
 * @return {!Array} The test steps to toggle hidden files
 */
function getTestCaseStepsForAndroidHiddenFiles(basicSet, hiddenEntrySet) {
  return getTestCaseStepsForHiddenFilesWithMenuItem(
      basicSet, hiddenEntrySet, '#gear-menu-toggle-hidden-android-folders');
}

/**
 * Gets the common steps to toggle hidden files in the Files app
 * @param {!Array<!TestEntryInfo>} basicSet Files expected before showing hidden
 * @param {!Array<!TestEntryInfo>} hiddenEntrySet Files expected after showing
 * hidden
 * @param {string} toggleMenuItemSelector Selector for the menu item that
 * toggles hidden file visibility
 * @return {!Array} The test steps to toggle hidden files
 */
function getTestCaseStepsForHiddenFilesWithMenuItem(
    basicSet, hiddenEntrySet, toggleMenuItemSelector) {
  var appId;
  return [
    function(id) {
      appId = id;
      remoteCall.waitForElement(appId, '#gear-button:not([hidden])')
          .then(this.next);
    },
    // Open the gear menu by clicking the gear button.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for menu to not be hidden.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])')
      .then(this.next);
    },
    // Wait for menu item to appear.
    function(result) {
      remoteCall
          .waitForElement(appId, toggleMenuItemSelector + ':not([disabled])')
          .then(this.next);
    },
    // Wait for menu item to appear.
    function(result) {
      remoteCall
          .waitForElement(appId, toggleMenuItemSelector + ':not([checked])')
          .then(this.next);
    },
    // Click the menu item.
    function(results) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [toggleMenuItemSelector], this.next);
    },
    // Wait for item to be checked.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, toggleMenuItemSelector + '[checked]')
          .then(this.next);
    },
    // Check the hidden files are displayed.
    function(result) {
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(
          hiddenEntrySet), {ignoreFileSize: true, ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Repeat steps to toggle again.
    function(inAppId) {
      remoteCall.waitForElement(appId, '#gear-button:not([hidden])')
          .then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])').then(
          this.next);
    },
    function(result) {
      remoteCall
          .waitForElement(appId, toggleMenuItemSelector + ':not([disabled])')
          .then(this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId, toggleMenuItemSelector + '[checked]')
          .then(this.next);
    },
    function(results) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [toggleMenuItemSelector], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall
          .waitForElement(appId, toggleMenuItemSelector + ':not([checked])')
          .then(this.next);
    },
    function(result) {
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(basicSet),
          {ignoreFileSize: true, ignoreLastModifiedTime: true}).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ];
}

/**
 * Tests toggling the show-hidden-files menu option on Downloads.
 */
testcase.showHiddenFilesDownloads = function() {
  var appId;
  var steps = [
    function() {
      addEntries(['local'], BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN, this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      openNewWindow(null, RootPath.DOWNLOADS).then(this.next);
    },
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, '#detail-table').then(this.next);
    },
    // Wait for volume list to be initialized.
    function() {
      remoteCall.waitForFileListChange(appId, 0).then(this.next);
    },
    function() {
      this.next(appId);
    },
  ];

  steps = steps.concat(getTestCaseStepsForHiddenFiles(BASIC_LOCAL_ENTRY_SET,
    BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN));
  StepsRunner.run(steps);
};


/**
 * Tests toggling the show-hidden-files menu option on Drive.
 */
testcase.showHiddenFilesDrive = function() {
  var appId;
  var steps = [
    function() {
      addEntries(['drive'], BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN, this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      openNewWindow(null, RootPath.DRIVE).then(this.next);
    },
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, '#detail-table').then(this.next);
    },
    // Wait for volume list to be initialized.
    function() {
      remoteCall.waitForFileListChange(appId, 0).then(this.next);
    },
    function() {
      this.next(appId);
    },
  ];

  steps = steps.concat(getTestCaseStepsForHiddenFiles(BASIC_DRIVE_ENTRY_SET,
    BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN));
  StepsRunner.run(steps);
};

/**
 * Tests toggling the show-google-docs option on Drive.
 */
testcase.toogleGoogleDocsDrive = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE).then(this.next);
    },
    function(results) {
      appId = results.windowId;
      remoteCall.waitForElement(appId, '#gear-button').then(this.next);
    },
    // Open the gear meny by a shortcut (Alt-E).
    function() {
      remoteCall.fakeKeyDown(appId, 'body', 'e', false, false, true)
          .then(this.next);
    },
    // Wait for menu to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu').then(this.next);
    },
    // Wait for menu to appear.
    function(result) {
      remoteCall.waitForElement(appId,
          '#gear-menu-drive-hosted-settings:not([disabled])').then(this.next);
    },
    function(results) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-menu-drive-hosted-settings'],
              this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(
          BASIC_DRIVE_ENTRY_SET_WITHOUT_GDOCS), {ignoreFileSize: true,
          ignoreLastModifiedTime: true}).then(this.next);
    },
    function(inAppId) {
      remoteCall.waitForElement(appId, '#gear-button').then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu').then(this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId,
          '#gear-menu-drive-hosted-settings:not([disabled])').then(this.next);
    },
    function(result) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-menu-drive-hosted-settings'],
              this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(
          BASIC_DRIVE_ENTRY_SET), {ignoreFileSize: true,
          ignoreLastModifiedTime: true}).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests that toggle-hidden-android-folders menu item exists when "Play files"
 * is selected, but hidden in Recents.
 */
testcase.showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles = function() {
  var appId;
  StepsRunner.run([
    // Open Files.App on Play Files.
    function() {
      openNewWindow(null, RootPath.ANDROID_FILES).then(this.next);
    },
    function(inAppId) {
      appId = inAppId;
      addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET, this.next);
    },
    // Wait for the file list to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#file-list').then(this.next);
    },
    // Wait for the gear menu button to appear.
    function() {
      remoteCall.waitForElement(appId, '#gear-button:not([hidden])')
          .then(this.next);
    },
    // Click the gear menu button.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for the gear menu to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])')
          .then(this.next);
    },
    // #toggle-hidden-android-folders command should be shown and disabled by
    // default.
    function() {
      remoteCall
          .waitForElement(
              appId,
              '#gear-menu-toggle-hidden-android-folders' +
                  ':not([checked]):not([hidden])')
          .then(this.next);
    },
    // Click the file list: the gear menu should hide.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#file-list'], this.next);
    },
    // Wait for the gear menu to hide.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu[hidden]').then(this.next);
    },
    // Navigate to Recent.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['span[root-type-icon=\'recent\']'],
          this.next);
    },
    // Click the gear menu button.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for the gear menu to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])')
          .then(this.next);
    },
    // #toggle-hidden-android-folders command should be hidden.
    function(result) {
      remoteCall
          .waitForElement(
              appId, '#gear-menu-toggle-hidden-android-folders[hidden]')
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests that "Play files" shows the full set of files after
 * toggle-hidden-android-folders is enabled.
 */
testcase.enableToggleHiddenAndroidFoldersShowsHiddenFiles = function() {
  var appId;
  var steps = [
    // Open Files.App on Play Files.
    function() {
      openNewWindow(null, RootPath.ANDROID_FILES).then(this.next);
    },
    function(inAppId) {
      appId = inAppId;
      addEntries(
          ['android_files'], BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN, this.next);
    },
    // Wait for the file list to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#file-list').then(this.next);
    },
    // Wait for the gear menu button to appear.
    function() {
      remoteCall.waitForElement(appId, '#gear-button:not([hidden])')
          .then(this.next);
    },
    function() {
      this.next(appId);
    }
  ];
  steps = steps.concat(getTestCaseStepsForAndroidHiddenFiles(
      BASIC_ANDROID_ENTRY_SET, BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN));
  StepsRunner.run(steps);
};

/**
 * Tests that the current directory is changed to "Play files" after the
 * current directory is hidden by toggle-hidden-android-folders option.
 */
testcase.hideCurrentDirectoryByTogglingHiddenAndroidFolders = function() {
  let appId;
  const MENU_ITEM_SELECTOR = '#gear-menu-toggle-hidden-android-folders';
  const steps = [
    function() {
      openNewWindow(null, RootPath.ANDROID_FILES).then(this.next);
    },
    function(inAppId) {
      appId = inAppId;
      addEntries(
          ['android_files'], BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN, this.next);
    },
    // Wait for the file list to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#file-list').then(this.next);
    },
    // Wait for the gear menu button to appear.
    function() {
      remoteCall.waitForElement(appId, '#gear-button:not([hidden])')
          .then(this.next);
    },
    // Open the gear menu by clicking the gear button.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for menu to not be hidden.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for menu item to appear.
    function() {
      remoteCall
          .waitForElement(
              appId, MENU_ITEM_SELECTOR + ':not([disabled]):not([checked])')
          .then(this.next);
    },
    // Click the menu item.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [MENU_ITEM_SELECTOR], this.next);
    },
    // Wait for item to be checked.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, MENU_ITEM_SELECTOR + '[checked]')
          .then(this.next);
    },
    // Check the hidden files are displayed.
    function() {
      remoteCall
          .waitForFiles(
              appId,
              TestEntryInfo.getExpectedRows(
                  BASIC_ANDROID_ENTRY_SET_WITH_HIDDEN),
              {ignoreFileSize: true, ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Navigate to "/My files/Play files/A".
    function() {
      remoteCall
          .navigateWithDirectoryTree(
              appId, '/A', 'My files/Play files', 'android_files')
          .then(this.next);
    },
    // Wait until current directory is changed to "/My files/Play files/A".
    function() {
      remoteCall
          .waitUntilCurrentDirectoryIsChanged(appId, '/My files/Play files/A')
          .then(this.next);
    },
    // Open the gear menu by clicking the gear button.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for menu to not be hidden.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for menu item to appear.
    function() {
      remoteCall
          .waitForElement(
              appId, MENU_ITEM_SELECTOR + '[checked]:not([disabled])')
          .then(this.next);
    },
    // Click the menu item.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [MENU_ITEM_SELECTOR], this.next);
    },
    // Wait until the current directory is changed from
    // "/My files/Play files/A" to "/My files/Play files" since
    // "/My files/Play files/A" is invisible now.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall
          .waitUntilCurrentDirectoryIsChanged(appId, '/My files/Play files')
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ];
  StepsRunner.run(steps);
};

/**
 * Tests the paste-into-current-folder menu item.
 */
testcase.showPasteIntoCurrentFolder = function() {
  const entrySet = [ENTRIES.hello, ENTRIES.world];
  var appId;
  StepsRunner.run([
    // Add files to Downloads volume.
    function() {
      addEntries(['local'], entrySet, this.next);
    },
    // Open Files.App on Downloads.
    function(result) {
      chrome.test.assertTrue(result);
      openNewWindow(null, RootPath.DOWNLOADS).then(this.next);
    },
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, '#file-list').then(this.next);
    },
    // Wait for the files to appear in the file list.
    function() {
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(entrySet))
          .then(this.next);
    },
    // Wait for the gear menu button to appear.
    function() {
      remoteCall.waitForElement(appId, '#gear-button').then(this.next);
    },

    // 1. Before selecting entries: click the gear menu button.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for the gear menu to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])')
          .then(this.next);
    },
    // #paste-into-current-folder command is shown. It should be disabled
    // because no file has been copied to clipboard.
    function(result) {
      remoteCall
          .waitForElement(
              appId,
              '#gear-menu cr-menu-item' +
                  '[command=\'#paste-into-current-folder\']' +
                  '[disabled]:not([hidden])')
          .then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#file-list'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu[hidden]').then(this.next);
    },

    // 2. Selecting a single regular file
    function(result) {
      remoteCall.callRemoteTestUtil(
          'selectFile', appId, [ENTRIES.hello.nameText], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for menu to appear.
    // The command is still shown.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall
          .waitForElement(
              appId,
              '#gear-menu:not([hidden]) cr-menu-item' +
                  '[command=\'#paste-into-current-folder\']' +
                  '[disabled]:not([hidden])')
          .then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#file-list'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu[hidden]').then(this.next);
    },

    // 3. When ready to paste a file
    function(result) {
      remoteCall.callRemoteTestUtil(
          'selectFile', appId, [ENTRIES.hello.nameText], this.next);
    },
    // Ctrl-C to copy the selected file
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall
          .fakeKeyDown(appId, '#file-list', 'c', true, false, false)
          .then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // The command appears enabled.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall
          .waitForElement(
              appId,
              '#gear-menu:not([hidden])' +
                  ' cr-menu-item[command=\'#paste-into-current-folder\']' +
                  ':not([disabled]):not([hidden])')
          .then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#file-list'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu[hidden]').then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests the "select-all" menu item.
 */
testcase.showSelectAllInCurrentFolder = function() {
  const entrySet = [ENTRIES.newlyAdded];
  var appId;
  StepsRunner.run([
    // Open Files.App on Downloads.
    function() {
      openNewWindow(null, RootPath.DOWNLOADS).then(this.next);
    },
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, '#file-list').then(this.next);
    },
    // Wait for the gear menu button to appear.
    function() {
      remoteCall.waitForElement(appId, '#gear-button').then(this.next);
    },
    // Click the gear menu button.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for the gear menu to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])')
          .then(this.next);
    },
    // Check: #select-all command is shown, but disabled (no files yet).
    function(result) {
      remoteCall
          .waitForElement(
              appId,
              '#gear-menu cr-menu-item' +
                  '[command=\'#select-all\']' +
                  '[disabled]:not([hidden])')
          .then(this.next);
    },
    // Click the file list: the gear menu should hide.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#file-list'], this.next);
    },
    // Wait for the gear menu to hide.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu[hidden]').then(this.next);
    },
    // Add a new file to Downloads.
    function() {
      addEntries(['local'], entrySet, this.next);
    },
    // Wait for the file list change.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(entrySet))
          .then(this.next);
    },
    // Click on the gear button again.
    function(result) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Check: #select-all command is shown, and enabled (there are files).
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall
          .waitForElement(
              appId,
              '#gear-menu:not([hidden]) cr-menu-item' +
                  '[command=\'#select-all\']' +
                  ':not([disabled]):not([hidden])')
          .then(this.next);
    },
    // Click on the #gear-menu-select-all item.
    function(results) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-menu-select-all'], this.next);
    },
    // Check: the file-list should be selected.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#file-list li[selected]')
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
