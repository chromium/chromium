// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Send Tab keys to Files app until #file-list gets a selected item.
 *
 * Raise test failure if #file-list doesn't get anything selected in 20 tabs.
 *
 * NOTE:
 *   1. Sends a maximum of 20 tabs.
 *   2. The focused element after this function might NOT be #file-list, since
 *   updating the selected item which is async can be detected after an extra
 *   Tab has been sent.
 */
async function tabUntilFileSelected(appId) {
  // Check: the file-list should have nothing selected.
  const selectedRows = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['#file-list li[selected]']);
  chrome.test.assertEq(0, selectedRows.length);

  // Press the tab until file-list gets focus and select some file.
  for (let i = 0; i < 20; i++) {
    // Send Tab key.
    const result = await sendTestMessage({name: 'dispatchTabKey'});
    chrome.test.assertEq(result, 'tabKeyDispatched', 'Tab key dispatch failed');

    // Check if there is a file selected, and return if so.
    const selectedRows = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, ['#file-list li[selected]']);
    if (selectedRows.length > 0) {
      return;
    }
  }

  chrome.test.assertTrue(false, 'File list has no selection');
}

/**
 * Tests that file list column header have ARIA attributes.
 */
testcase.fileListAriaAttributes = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Fetch column header.
  const columnHeadersQuery = ['#detail-table .table-header [aria-describedby]'];
  const columnHeaders = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, [columnHeadersQuery, ['display']]);

  chrome.test.assertTrue(columnHeaders.length > 0);
  for (const header of columnHeaders) {
    // aria-describedby is used to tell users that they can click and which
    // type of sort asc/desc will happen.
    chrome.test.assertTrue('aria-describedby' in header.attributes);
    // role button is used so users know that it's clickable.
    chrome.test.assertEq('button', header.attributes['role']);
  }
};

/**
 * Tests using tab to focus the file list will select the first item, if
 * nothing is selected.
 */
testcase.fileListFocusFirstItem = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Send Tab keys to make file selected.
  await tabUntilFileSelected(appId);

  // Check: The first file only is selected in the file entry rows.
  const fileRows = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['#file-list li']);
  chrome.test.assertEq(5, fileRows.length);
  const selectedRows = fileRows.filter(item => 'selected' in item.attributes);
  chrome.test.assertEq(1, selectedRows.length);
  chrome.test.assertEq(0, fileRows.indexOf(selectedRows[0]));
};

/**
 * Tests that after a multiple selection, canceling the selection and using
 * Tab to focus the files list it selects the item that was last focused.
 */
testcase.fileListSelectLastFocusedItem = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Check: the file-list should have nothing selected.
  let selectedRows = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['#file-list li[selected]']);
  chrome.test.assertEq(0, selectedRows.length);

  // Move to item 2.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
        'ArrowDown failed');
  }

  // Select item 2 with Ctrl+Space.
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Move to item 3 and Cltr+Space to add it to multi-selection.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
      'Ctrl+ArrowDown failed');
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Check that items 2 and 3 are selected.
  selectedRows = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['#file-list li[selected]']);
  chrome.test.assertEq(2, selectedRows.length);

  // Cancel the selection.
  const button = ['#cancel-selection-button'];
  await remoteCall.waitAndClickElement(appId, '#cancel-selection-button');

  // Wait until selection is removed.
  await remoteCall.waitForElementLost(appId, '#file-list li[selected]');

  // Send Tab keys until a file item is selected.
  await tabUntilFileSelected(appId);

  // Check: The 3rd item only is selected.
  const fileRows = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['#file-list li']);
  chrome.test.assertEq(5, fileRows.length);
  selectedRows = fileRows.filter(item => 'selected' in item.attributes);
  chrome.test.assertEq(1, selectedRows.length);
  chrome.test.assertEq(2, fileRows.indexOf(selectedRows[0]));
};

/**
 * Verifies the total number of a11y messages and asserts the latest message
 * is the expected one.
 *
 * @param {string} appId
 * @param {number} expectedCount
 * @param {?string=} expectedMessage
 * @return {!Promise<string>} Latest a11y message.
 */
export async function countAndCheckLatestA11yMessage(
    appId, expectedCount, expectedMessage) {
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  if (expectedMessage === null || expectedMessage === undefined) {
    return '';
  }
  const latestMessage = a11yMessages[a11yMessages.length - 1];
  chrome.test.assertEq(
      expectedCount, a11yMessages.length,
      'Wrong number of a11y messages: latest message: ' + latestMessage);
  chrome.test.assertEq(expectedMessage, latestMessage);
  return latestMessage;
}

/**
 * Tests that selecting/de-selecting files with keyboard produces a11y
 * messages.
 *
 * NOTE: Test shared with grid_view.js.
 * @param {boolean=} isGridView if the test is testing the grid view.
 */
testcase.fileListKeyboardSelectionA11y = async (isGridView) => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  let a11yMsgCount = 0;
  const viewSelector = isGridView ? 'grid#file-list' : '#file-list';
  if (isGridView) {
    // Click view-button again to switch to detail view.
    await remoteCall.waitAndClickElement(appId, '#view-button');

    // Clicking #view-button adds 1 a11y message.
    ++a11yMsgCount;
  }

  // Keys used for keyboard navigation in the file list.
  const homeKey = [viewSelector, 'Home', false, false, false];
  const ctrlDownKey = [viewSelector, 'ArrowDown', true, false, false];
  const ctrlSpaceKey = [viewSelector, ' ', true, false, false];
  const shiftEndKey = [viewSelector, 'End', false, true, false];
  const ctrlAKey = [viewSelector + ' li', 'a', true, false, false];
  const escKey = [viewSelector, 'Escape', false, false, false];

  // Select first item with Home key.
  await remoteCall.fakeKeyDown(appId, ...homeKey);

  // Navigating with Home key doesn't use aria-live message, it only uses the
  // native listbox selection state messages.
  await countAndCheckLatestA11yMessage(appId, a11yMsgCount);

  // Ctrl+Down & Ctrl+Space to select second item: Beautiful Song.ogg
  await remoteCall.fakeKeyDown(appId, ...ctrlDownKey);
  await remoteCall.fakeKeyDown(appId, ...ctrlSpaceKey);

  // Check: Announced "Beautiful Song.add" added to selection.
  await countAndCheckLatestA11yMessage(
      appId, ++a11yMsgCount, 'Added Beautiful Song.ogg to selection.');

  // Shift+End to select from 2nd item to the last item.
  await remoteCall.fakeKeyDown(appId, ...shiftEndKey);

  // Check: Announced range selection from "Beautiful Song.add" to hello.txt.
  await countAndCheckLatestA11yMessage(
      appId, ++a11yMsgCount,
      'Selected a range of 4 entries from Beautiful Song.ogg to hello.txt.');

  // Ctrl+Space to de-select currently focused item (last item).
  await remoteCall.fakeKeyDown(appId, ...ctrlSpaceKey);

  // Check: Announced de-selecting hello.txt
  await countAndCheckLatestA11yMessage(
      appId, ++a11yMsgCount, 'Removed hello.txt from selection.');

  // Ctrl+A to select all items.
  await remoteCall.fakeKeyDown(appId, ...ctrlAKey);

  // Check: Announced selecting all entries.
  await countAndCheckLatestA11yMessage(
      appId, ++a11yMsgCount, 'Selected all entries.');

  // Esc key to deselect all.
  await remoteCall.fakeKeyDown(appId, ...escKey);

  // Check: Announced deselecting all entries.
  await countAndCheckLatestA11yMessage(
      appId, ++a11yMsgCount, 'Removed all entries from selection.');
};

/**
 * Tests that selecting/de-selecting files with mouse produces a11y messages.
 *
 * NOTE: Test shared with grid_view.js.
 * @param {boolean=} isGridView if the test is testing the grid view.
 */
testcase.fileListMouseSelectionA11y = async (isGridView) => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  let a11yMsgCount = 0;
  if (isGridView) {
    // Click view-button again to switch to detail view.
    await remoteCall.waitAndClickElement(appId, '#view-button');

    // Clicking #view-button adds 1 a11y message.
    ++a11yMsgCount;
  }

  // Click first item.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');

  // Simple click to select a file doesn't use aria-live message, it only
  // uses the native listbox selection state messages.
  await countAndCheckLatestA11yMessage(appId, a11yMsgCount);

  // Ctrl+Click second item.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="Beautiful Song.ogg"]', {ctrl: true});

  // Check: Announced "Beautiful Song.add" added to selection.
  await countAndCheckLatestA11yMessage(
      appId, ++a11yMsgCount, 'Added Beautiful Song.ogg to selection.');

  // Shift+Click last item.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {shift: true});

  // Check: Announced range selection from "Beautiful Song.add" to hello.txt.
  await countAndCheckLatestA11yMessage(
      appId, ++a11yMsgCount,
      'Selected a range of 4 entries from Beautiful Song.ogg to hello.txt.');

  // Ctrl+Click to de-select the last item.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {ctrl: true});

  // Check: Announced de-selecting hello.txt
  await countAndCheckLatestA11yMessage(
      appId, ++a11yMsgCount, 'Removed hello.txt from selection.');

  // Click on "Cancel selection" button.
  await remoteCall.waitAndClickElement(appId, '#cancel-selection-button');

  // Check: Announced deselecting all entries.
  await countAndCheckLatestA11yMessage(
      appId, ++a11yMsgCount, 'Removed all entries from selection.');
};

/**
 * Tests the deletion of one or multiple items. After deletion, one of the
 * remaining items should have the lead, but shouldn't be in check-select
 * mode.
 */
testcase.fileListDeleteMultipleFiles = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select first 2 items.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="world.ogv"]');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {shift: true});

  // Delete item and confirm delete.
  if (await sendTestMessage({name: 'isTrashEnabled'}) === 'true') {
    await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  } else {
    await remoteCall.waitAndClickElement(appId, '#delete-button');
    await remoteCall.waitAndClickElement(
        appId, '.files-confirm-dialog .cr-dialog-ok');
  }

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="world.ogv"]');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Check: no selection state.
  await remoteCall.waitForElementLost(appId, 'body.check-select');

  // Check: lead state of last item.
  let item = await remoteCall.waitForElement(
      appId, '#file-list [file-name="My Desktop Background.png"]');
  chrome.test.assertTrue('lead' in item.attributes);

  // Check: selected state of last item.
  chrome.test.assertTrue('selected' in item.attributes);

  // Select and delete first item.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="photos"]');
  if (await sendTestMessage({name: 'isTrashEnabled'}) === 'true') {
    await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  } else {
    await remoteCall.waitAndClickElement(appId, '#delete-button');
    await remoteCall.waitAndClickElement(
        appId, '.files-confirm-dialog .cr-dialog-ok');
  }

  // Wait for file deletion.
  await remoteCall.waitForElementLost(appId, '#file-list [file-name="photos"]');

  // Check: no selection state.
  await remoteCall.waitForElementLost(appId, 'body.check-select');

  // Check: lead state of first item.
  item = await remoteCall.waitForElement(
      appId, '#file-list [file-name="Beautiful Song.ogg"]');
  chrome.test.assertTrue('lead' in item.attributes);

  // Check: selected state of first item.
  chrome.test.assertTrue('selected' in item.attributes);
};

/**
 * Tests that in selection mode, the rename operation is applied to the
 * selected item, as seen by the selection model, rather than the lead item.
 * The lead and the selected item(s) are different when we deselect a file
 * list item in selection mode. crbug.com/1094260
 */
testcase.fileListRenameSelectedItem = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select 2 items.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="world.ogv"]', {ctrl: true});

  // Deselect first item.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {ctrl: true});

  // Check: the first item should have the lead state.
  const item = await remoteCall.waitForElement(
      appId, '#file-list li:not([selected])[file-name="hello.txt"]');
  chrome.test.assertTrue('lead' in item.attributes);

  // Check: selection should be on the second item.
  await remoteCall.waitForElement(
      appId, '#file-list li[selected][file-name="world.ogv"]');

  // Press Ctrl+Enter key to rename the selected file.
  const key = ['#file-list', 'Enter', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

  // Check: the renaming text input should be shown in the file list.
  const textInput = '#file-list .table-row[renaming] input.rename';
  await remoteCall.waitForElement(appId, textInput);

  // Type new file name.
  await remoteCall.inputText(appId, textInput, 'New File Name.txt');

  // Send Enter key to the text input.
  const key2 = [textInput, 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key2));

  // Check: the selected file should have been renamed.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="world.ogv"]');
  await remoteCall.waitForElement(
      appId, '#file-list li[selected][file-name="New File Name.txt"]');
};

/**
 * Tests that user can rename a file/folder after using "select all" without
 * having selected any file previously.
 */
testcase.fileListRenameFromSelectAll = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Select all the files.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA);

  // Check: the file-list should be selected.
  await remoteCall.waitForElement(appId, '#file-list li[selected]');

  // Wait and click on "selection menu button".
  await remoteCall.waitAndClickElement(
      appId, '#selection-menu-button:not([hidden])');

  // Wait and click on rename menu item.
  await remoteCall.waitAndClickElement(
      appId, '[command="#rename"]:not([hidden]):not([disabled])');

  // Check: the renaming text input should be shown in the file list.
  const textInput = '#file-list .table-row[renaming] input.rename';
  await remoteCall.waitForElement(appId, textInput);
};
