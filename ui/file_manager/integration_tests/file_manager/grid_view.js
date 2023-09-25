// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, getCaller, pending, repeatUntil, RootPath, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {openNewWindow, remoteCall, setupAndWaitUntilReady} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET} from './test_data.js';

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
  // Disable all banners.
  await remoteCall.disableBannersForTesting(appId);

  // Click the grid view button.
  await remoteCall.waitAndClickElement(appId, '#view-button');

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

/**
 * Tests that Grid View shows "Folders" and "Files" titles before folders and
 * files respectively.
 */
testcase.showGridViewTitles = async () => {
  const appId = await showGridView(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);

  const titles = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, ['.thumbnail-grid .grid-title']);
  chrome.test.assertEq(2, titles.length, 'Grid view should show 2 titles');
  const titleTexts = titles.map((title) => title.text).sort();
  chrome.test.checkDeepEq(['Files', 'Folders'], titleTexts);
};

/**
 * Tests that Grid View shows DocumentsProvider thumbnails.
 */
testcase.showGridViewDocumentsProvider = async () => {
  const caller = getCaller();

  // Add files to the DocumentsProvider volume.
  await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

  // Open Files app.
  const appId = await openNewWindow(RootPath.DOWNLOADS);

  // Wait for the DocumentsProvider volume to mount.
  const documentsProviderVolumeType = 'documents_provider';
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.waitForItemToHaveChildrenByType(
      documentsProviderVolumeType, /* hasChildren= */ true);

  // Click to open the DocumentsProvider volume.
  await directoryTree.selectItemByType(documentsProviderVolumeType);

  // Click the grid view button.
  await remoteCall.waitForElement(appId, '#view-button');
  await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#view-button', 'click']);

  // Wait for the grid view to load.
  await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, ['grid:not([hidden])']);

  // Check that all DocumentsProvider thumbnails are loaded where expected.
  await repeatUntil(async () => {
    for (const [fname, hasThumbnail] of [
             [ENTRIES.hello.targetPath, false],
             [ENTRIES.world.targetPath, true],
             [ENTRIES.desktop.targetPath, true],
             [ENTRIES.beautiful.targetPath, false],
             [ENTRIES.photos.targetPath, false],
    ]) {
      const item = await remoteCall.waitForElement(
          appId, `#file-list [file-name="${fname}"]`);
      const thumbnailLoaded =
          item.attributes['class'].split(/\s+/).includes('thumbnail-loaded');
      if (thumbnailLoaded !== hasThumbnail) {
        return pending(
            caller, 'Unexpected thumbnail state for %j: %j', fname,
            hasThumbnail);
      }
    }
    return true;
  });
};

/**
 * Tests that an encrypted file will have a corresponding icon.
 */
testcase.showGridViewEncryptedFile = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.testCSEFile]);

  // Click the grid view button.
  await remoteCall.waitAndClickElement(appId, '#view-button');

  // Check the file's icon.
  const icon = await remoteCall.waitForElementStyles(
      appId, '.thumbnail-grid .no-thumbnail', ['-webkit-mask-image']);
  chrome.test.assertTrue(
      icon.styles['-webkit-mask-image'].includes('encrypted.svg'),
      'Icon does not seem to be the encrypted one');

  // Move mouse out of the view change button, so we won't have its hover text.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOut', appId, ['#view-button']));
  await remoteCall.waitForElementLost(appId, 'files-tooltip[visible=true]');

  // Hover over an icon: a tooltip should appear.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId,
      ['[file-name="test-encrypted.txt"] .no-thumbnail']));
  const label = await remoteCall.waitForElement(
      appId, ['files-tooltip[visible=true]', '#label']);
  chrome.test.assertEq('Encrypted file', label.text);
};
