// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, RootPath, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {openAndWaitForClosingDialog, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Tests the focus behavior of the search box.
 */
testcase.tabindexSearchBoxFocus = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Check that the file list has the focus on launch.
  await remoteCall.waitForElement(appId, ['#file-list:focus']);

  // Check that the search UI is in the collapsed state (hidden from the user).
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Press the Ctrl-F key.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'f', true, false, false]));

  // Wait for the search box to fully open. Only once the search wrapper
  // is fully expanded the collapsed attribute is removed.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Check that the search box has the focus.
  await remoteCall.waitForElement(appId, ['#search-box cr-input:focus-within']);

  // Press the Esc key.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['#search-box cr-input', 'Escape', false, false, false]));

  // Check that the focus moves to the next button: #view-button.
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'view-button'));
};

/**
 * Tests the tab focus behavior of the Files app when no file is selected.
 */
testcase.tabindexFocus = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  await remoteCall.isolateBannerForTesting(appId, 'drive-welcome-banner');
  const driveWelcomeLinkQuery = '#banners > drive-welcome-banner:not([hidden])';

  // Check that the file list has the focus on launch.
  await Promise.all([
    remoteCall.waitForElement(appId, ['#file-list:focus']),
    remoteCall.waitForElement(appId, [driveWelcomeLinkQuery]),
  ]);
  const element =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq('list', element.attributes['class']);

  // Send Tab key events to cycle through the tabable elements.
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'directory-tree'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'search-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'view-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'sort-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'gear-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'drive-learn-more-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'dismiss-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'file-list'));
};

/**
 * Tests the tab focus behavior of the Files app when no file is selected in
 * Downloads directory.
 */
testcase.tabindexFocusDownloads = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  await remoteCall.isolateBannerForTesting(
      appId, 'holding-space-welcome-banner');

  // Check that the file list has the focus on launch.
  await remoteCall.waitForElement(appId, ['#file-list:focus']);
  const element =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq('list', element.attributes['class']);

  // Send Tab key events to cycle through the tabable elements.
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'directory-tree'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'breadcrumbs'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'search-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'view-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'sort-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'gear-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'dismiss-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'file-list'));
};

/**
 * Tests the tab focus behavior of the Files app when a directory is selected.
 */
testcase.tabindexFocusDirectorySelected = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  await remoteCall.isolateBannerForTesting(appId, 'drive-welcome-banner');
  const driveWelcomeLinkQuery = '#banners > drive-welcome-banner:not([hidden])';

  // Check that the file list has the focus on launch.
  await Promise.all([
    remoteCall.waitForElement(appId, ['#file-list:focus']),
    remoteCall.waitForElement(appId, [driveWelcomeLinkQuery]),
  ]);
  const element =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq('list', element.attributes['class']);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  const fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets': ['static_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Select the directory named 'photos'.
  await remoteCall.waitUntilSelected(appId, 'photos');

  await Promise.all([

    // Wait for share button to to be visible and enabled.
    remoteCall.waitForElement(
        appId, ['#sharesheet-button:not([hidden]):not([disabled])']),

    // Wait for delete button to to be visible and enabled.
    remoteCall.waitForElement(
        appId, ['#delete-button:not([hidden]):not([disabled])']),
  ]);

  // Send Tab key events to cycle through the tabable elements.
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'directory-tree'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'pinned-toggle'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'sharesheet-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'delete-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'search-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'view-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'sort-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'gear-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'drive-learn-more-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'dismiss-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'file-list'));

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

/**
 * Tests the tab focus in the dialog and closes the dialog.
 *
 * @param {!Object} dialogParams Dialog parameters to be passed to
 *     chrome.fileSystem.chooseEntry.
 * @param {string} volumeName Volume name passed to the selectVolume remote
 *     function.
 * @param {!Array<TestEntryInfo>} expectedSet Expected set of the entries.
 * @param {?function(string):(!Promise|Object)} initialize Initialization before
 *     test runs. The window ID is passed as an argument. If null, do nothing as
 *     initialization.
 * @param {!Array<string>} initialElements Selectors of the elements which
 *     shows the Files app is ready. After all the elements show up, the
 *     tabfocus tests starts.
 * @param {!Array<string>} expectedTabOrder Array with the IDs of the element
 *     with the corresponding order of expected tab-indexes.
 */
async function tabindexFocus(
    dialogParams, volumeName, expectedSet, initialize, initialElements,
    expectedTabOrder) {
  await Promise.all([
    addEntries(['local'], BASIC_LOCAL_ENTRY_SET),
    addEntries(['drive'], BASIC_DRIVE_ENTRY_SET),
  ]);

  const selectAndCheckAndClose = async (appId) => {
    if (dialogParams.type === 'saveFile') {
      await remoteCall.waitForElement(
          appId, ['#filename-input-textbox:focus-within']);
    } else {
      await remoteCall.waitForElement(appId, ['#file-list:focus']);
    }

    // Wait for Files app to finish loading.
    await remoteCall.waitFor('isFileManagerLoaded', appId, true);

    if (initialize) {
      await initialize(appId);
    }

    // Wait for the initial element.
    await Promise.all(initialElements.map((selector) => {
      return remoteCall.waitForElement(appId, [selector]);
    }));

    // Checks tabfocus.
    for (const className of expectedTabOrder) {
      chrome.test.assertTrue(
          await remoteCall.checkNextTabFocus(appId, className), className);
    }
    // Closes the window by pressing Escape.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeKeyDown', appId, ['#file-list', 'Escape', false, false, false]));
  };

  await openAndWaitForClosingDialog(
      dialogParams, volumeName, expectedSet, selectAndCheckAndClose);
}

/**
 * Tests the tab focus behavior of Open Dialog (Downloads).
 */
testcase.tabindexOpenDialogDownloads = async () => {
  const tabindexIds = [
    'cancel-button',
    'ok-button',
    'directory-tree',
    /* first breadcrumb */ 'first',
    'search-button',
    'view-button',
    'sort-button',
    'gear-button',
    'dismiss-button',
    'file-list',
  ];
  return tabindexFocus(
      {type: 'openFile'}, 'downloads', BASIC_LOCAL_ENTRY_SET, async (appId) => {
        await remoteCall.waitUntilSelected(appId, 'hello.txt');
        await remoteCall.isolateBannerForTesting(
            appId, 'holding-space-welcome-banner');
      }, ['#ok-button:not([disabled])'], tabindexIds);
};


/**
 * Tests the tab focus behavior of Open Dialog (Drive).
 */
testcase.tabindexOpenDialogDrive = async () => {
  const tabindexIds = [
    'cancel-button',
    'ok-button',
    'search-button',
    'view-button',
    'sort-button',
    'gear-button',
    'drive-learn-more-button',
    'dismiss-button',
    'directory-tree',
    'file-list',
  ];
  return tabindexFocus(
      {type: 'openFile'}, 'drive', BASIC_DRIVE_ENTRY_SET, async (appId) => {
        await remoteCall.waitUntilSelected(appId, 'hello.txt');
        await remoteCall.isolateBannerForTesting(appId, 'drive-welcome-banner');
      }, ['#ok-button:not([disabled])'], tabindexIds);
};

/**
 * Tests the tab focus behavior of Save File Dialog (Downloads).
 */
testcase.tabindexSaveFileDialogDownloads = async () => {
  return tabindexFocus(
      {
        type: 'saveFile',
        suggestedName: 'hoge.txt',  // Prevent showing a override prompt
      },
      'downloads', BASIC_LOCAL_ENTRY_SET, null, ['#ok-button:not([disabled])'],
      [
        'cancel-button',
        'ok-button',
        'directory-tree',
        /* first breadcrumb */ 'first',
        'search-button',
        'view-button',
        'sort-button',
        'gear-button',
        'file-list',
        'new-folder-button',
        'filename-input-textbox',
      ]);
};


/**
 * Tests the tab focus behavior of Save File Dialog (Drive).
 */
testcase.tabindexSaveFileDialogDrive = async () => {
  return tabindexFocus(
      {
        type: 'saveFile',
        suggestedName: 'hoge.txt',  // Prevent showing a override prompt
      },
      'drive', BASIC_DRIVE_ENTRY_SET, null, ['#ok-button:not([disabled])'], [
        'cancel-button',
        'ok-button',
        'directory-tree',
        'search-button',
        'view-button',
        'sort-button',
        'gear-button',
        'file-list',
        'new-folder-button',
        'filename-input-textbox',
      ]);
};
