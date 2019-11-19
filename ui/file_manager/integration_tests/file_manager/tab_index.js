// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests the focus behavior of the search box.
 */
testcase.tabindexSearchBoxFocus = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Check that the file list has the focus on launch.
  await remoteCall.waitForElement(appId, ['#file-list:focus']);

  // Press the Ctrl-F key.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'f', true, false, false]));

  // Check that the search box has the focus.
  await remoteCall.waitForElement(appId, ['#search-box cr-input:focus-within']);

  // Press the Esc key.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['#search-box cr-input', 'Escape', false, false, false]));

  // Check that the file list has the focus.
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'file-list'));
};

/**
 * Tests the tab focus behavior of the Files app when no file is selected.
 */
testcase.tabindexFocus = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Check that the file list has the focus on launch.
  await remoteCall.waitForElement(appId, ['#file-list:focus']);
  await remoteCall.waitForElement(appId, ['#drive-welcome-link']);
  const element =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq('list', element.attributes['class']);

  // Send Tab key events to cycle through the tabable elements.
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'search-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'view-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'sort-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'gear-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'directory-tree'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'drive-welcome-link'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'welcome-dismiss'));
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

  // Check that the file list has the focus on launch.
  await remoteCall.waitForElement(appId, ['#file-list:focus']);
  const element =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq('list', element.attributes['class']);

  // Send Tab key events to cycle through the tabable elements.
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'breadcrumb-path-0'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'search-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'view-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'sort-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'gear-button'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'directory-tree'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'file-list'));
};

/**
 * Tests for background color change when breadcrumb has focus.
 */
testcase.tabindexFocusBreadcrumbBackground = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Get background color for breadcrumb with no focus.
  const unfocused = await remoteCall.waitForElementStyles(
      appId, '#breadcrumb-path-0', ['background-color']);

  // Press the tab key.
  const result = await sendTestMessage({name: 'dispatchTabKey'});
  chrome.test.assertEq(result, 'tabKeyDispatched', 'Tab key dispatch failure');

  // Get background color for breadcrumb with focus.
  const focused = await remoteCall.waitForElementStyles(
      appId, '#breadcrumb-path-0:focus', ['background-color']);

  // Check that background colour has changed.
  chrome.test.assertFalse(
      focused.styles['background-color'] ===
      unfocused.styles['background-color']);
};

/**
 * Tests the tab focus behavior of the Files app when a directory is selected.
 */
testcase.tabindexFocusDirectorySelected = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Check that the file list has the focus on launch.
  await Promise.all([
    remoteCall.waitForElement(appId, ['#file-list:focus']),
    remoteCall.waitForElement(appId, ['#drive-welcome-link']),
  ]);
  const element =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq('list', element.attributes['class']);

  // Select the directory named 'photos'.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('selectFile', appId, ['photos']));

  await Promise.all([
    remoteCall.waitForElement(
        appId, ['#share-menu-button:not([hidden]):not([disabled])']),
    remoteCall.waitForElement(
        appId, ['#delete-button:not([hidden]):not([disabled])']),
  ]);

  // Send Tab key events to cycle through the tabable elements.
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'share-menu-button'));
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
      await remoteCall.checkNextTabFocus(appId, 'directory-tree'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'drive-welcome-link'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'welcome-dismiss'));
  chrome.test.assertTrue(
      await remoteCall.checkNextTabFocus(appId, 'file-list'));
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
 * @param {Array<string>} expectedTabOrder Array with the IDs of the element
 *     with the corresponding order of expected tab-indexes.
 */
async function tabindexFocus(
    dialogParams, volumeName, expectedSet, initialize, initialElements,
    expectedTabOrder) {
  await Promise.all([
    addEntries(['local'], BASIC_LOCAL_ENTRY_SET),
    addEntries(['drive'], BASIC_DRIVE_ENTRY_SET)
  ]);

  const selectAndCheckAndClose = async (appId) => {
    if (dialogParams.type === 'saveFile') {
      await remoteCall.waitForElement(
          appId, ['#filename-input-textbox:focus-within']);
    } else {
      await remoteCall.waitForElement(appId, ['#file-list:focus']);
    }

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
  return tabindexFocus(
      {type: 'openFile'}, 'downloads', BASIC_LOCAL_ENTRY_SET,
      async (appId) => {
        await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']);
      },
      ['#ok-button:not([disabled])'], [
        'cancel-button', 'ok-button', 'breadcrumb-path-0', 'search-button',
        'view-button', 'sort-button', 'gear-button', 'directory-tree',
        'file-list'
      ]);
};

/**
 * Tests the tab focus behavior of Open Dialog (Drive).
 */
testcase.tabindexOpenDialogDrive = async () => {
  return tabindexFocus(
      {type: 'openFile'}, 'drive', BASIC_DRIVE_ENTRY_SET,
      async (appId) => {
        await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']);
      },
      ['#ok-button:not([disabled])'], [
        'cancel-button', 'ok-button', 'search-button', 'view-button',
        'sort-button', 'gear-button', 'directory-tree', 'file-list'
      ]);
};

/**
 * Tests the tab focus behavior of Save File Dialog (Downloads).
 */
testcase.tabindexSaveFileDialogDownloads = async () => {
  return tabindexFocus(
      {
        type: 'saveFile',
        suggestedName: 'hoge.txt'  // Prevent showing a override prompt
      },
      'downloads', BASIC_LOCAL_ENTRY_SET, null, ['#ok-button:not([disabled])'],
      [
        'cancel-button', 'ok-button', 'breadcrumb-path-0', 'search-button',
        'view-button', 'sort-button', 'gear-button', 'directory-tree',
        'file-list', 'new-folder-button', 'filename-input-textbox'
      ]);
};

/**
 * Tests the tab focus behavior of Save File Dialog (Drive).
 */
testcase.tabindexSaveFileDialogDrive = async () => {
  return tabindexFocus(
      {
        type: 'saveFile',
        suggestedName: 'hoge.txt'  // Prevent showing a override prompt
      },
      'drive', BASIC_DRIVE_ENTRY_SET, null, ['#ok-button:not([disabled])'], [
        'cancel-button', 'ok-button', 'search-button', 'view-button',
        'sort-button', 'gear-button', 'directory-tree', 'file-list',
        'new-folder-button', 'filename-input-textbox'
      ]);
};
