// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Shows the grid view and checks the label texts of entries.
 *
 * @param {string} rootPath Root path to be used as a default current directory
 *     during initialization. Can be null, for no default path.
 * @param {Array<TestEntryInfo>} expectedSet Set of entries that are expected
 *     to appear in the grid view.
 * @return {Promise} Promise to be fulfilled or rejected depending on the test
 *     result.
 */
async function showGridView(rootPath, expectedSet) {
  const caller = getCaller();
  const expectedLabels =
      expectedSet.map((entryInfo) => entryInfo.nameText).sort();

  // Open Files app on |rootPath|.
  const appId = await setupAndWaitUntilReady(rootPath);

  // Click the grid view button.
  await remoteCall.waitForElement(appId, '#view-button');
  await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#view-button', 'click']);

  // Compare the grid labels of the entries.
  await repeatUntil(async () => {
    const labels = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId,
        ['grid:not([hidden]) .thumbnail-item .entry-name']);
    const actualLabels = labels.map((label) => label.text).sort();

    if (chrome.test.checkDeepEq(expectedLabels, actualLabels)) {
      return true;
    }
    return pending(
        caller, 'Failed to compare the grid lables, expected: %j, actual %j.',
        expectedLabels, actualLabels);
  });
  return appId;
}

/**
 * Tests to show grid view on a local directory.
 */
testcase.showGridViewDownloads = () => {
  return showGridView(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests to show grid view on a drive directory.
 */
testcase.showGridViewDrive = () => {
  return showGridView(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET);
};

/**
 * Tests to view-button switches to thumbnail (grid) view and clicking again
 * switches back to detail (file list) view.
 */
testcase.showGridViewButtonSwitches = async () => {
  const appId = await showGridView(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);

  // Check that a11y message for switching to grid view.
  let a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq(
      'File list has changed to thumbnail view.', a11yMessages[0]);

  // Click view-button again to switch to detail view.
  await remoteCall.waitAndClickElement(appId, '#view-button');

  // Wait for detail-view to be visible and grid to be hidden.
  await remoteCall.waitForElement(appId, '#detail-table:not([hidden])');
  await remoteCall.waitForElement(appId, 'grid[hidden]');

  // Check that a11y message for switching to list view.
  a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(2, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq('File list has changed to list view.', a11yMessages[1]);
};

/**
 * Tests that selecting/de-selecting files with keyboard produces a11y
 * messages.
 */
testcase.showGridViewKeyboardSelectionA11y = async () => {
  const isGridView = true;
  return testcase.fileListKeyboardSelectionA11y(isGridView);
};

/**
 * Tests that selecting/de-selecting files with mouse produces a11y messages.
 */
testcase.showGridViewMouseSelectionA11y = async () => {
  const isGridView = true;
  return testcase.fileListMouseSelectionA11y(isGridView);
};
