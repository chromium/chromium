// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {isSinglePartitionFormat, navigateWithDirectoryTree, remoteCall, setupAndWaitUntilReady} from './background.js';

/**
 * Lanuches file manager and stubs out the formatVolume private api.
 *
 * @return {!Promise<string>} Files app window ID.
 */
async function setupFormatDialogTest() {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await remoteCall.callRemoteTestUtil('overrideFormat', appId, []);
  return appId;
}

/**
 * Opens a format dialog for the USB with label |usbLabel|.
 *
 * @param {string} appId Files app window ID.
 * @param {string} usbLabel Label of usb to format.
 */
async function openFormatDialog(appId, usbLabel) {
  if (await isSinglePartitionFormat(appId)) {
    await openFormatDialogWithSinglePartitionFormat(appId, usbLabel, 'FAKEUSB');
    return;
  }

  // Focus the directory tree.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'focus', appId, ['#directory-tree']),
      'focus failed: #directory-tree');

  // Right click on the USB's directory tree entry.
  const treeQuery = `#directory-tree [entry-label="${usbLabel}"]`;
  await remoteCall.waitForElement(appId, treeQuery);
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, [treeQuery]),
      'fakeMouseRightClick failed');

  // Click on the format menu item.
  const formatItemQuery = '#roots-context-menu:not([hidden])' +
      ' cr-menu-item[command="#format"]:not([hidden]):not([disabled])';
  await remoteCall.waitAndClickElement(appId, formatItemQuery);

  // Check the dialog is open.
  await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog[open]']);
}

/**
 * Opens a format dialog for the USB with label |usbLabel| and device with
 * label |deviceLabel|.
 *
 * @param {string} appId Files app window ID.
 * @param {string} usbLabel Label of usb to format.
 * @param {string} deviceLabel Label of the parent device of usb.
 */
async function openFormatDialogWithSinglePartitionFormat(
    appId, usbLabel, deviceLabel) {
  // Focus the directory tree.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'focus', appId, ['#directory-tree']),
      'focus failed: #directory-tree');

  // Expand device tree entry to access partition entry.
  await remoteCall.expandTreeItemInDirectoryTree(
      appId, `#directory-tree [entry-label="${deviceLabel}"]`);

  // Right click on the USB's directory tree entry.
  const treeQuery = `#directory-tree [entry-label="${usbLabel}"]`;
  await remoteCall.waitForElement(appId, treeQuery);
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, [treeQuery]),
      'fakeMouseRightClick failed');

  // Click on the format menu item.
  const formatItemQuery = '#directory-tree-context-menu:not([hidden])' +
      ' cr-menu-item[command="#format"]:not([hidden]):not([disabled])';
  await remoteCall.waitAndClickElement(appId, formatItemQuery);

  // Check the dialog is open.
  await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog[open]']);
}

/**
 * Tests the format dialog for a sample USB with files on it.
 */
testcase.formatDialog = async () => {
  await sendTestMessage({name: 'mountFakeUsb'});
  const appId = await setupFormatDialogTest();

  // Open the format dialog on fake-usb.
  await openFormatDialog(appId, 'fake-usb');

  // Check the correct size is displayed.
  const warning = await remoteCall.waitForElement(appId, [
    'files-format-dialog',
    '#warning-container:not([hidden]) #warning-message',
  ]);
  chrome.test.assertEq(
      '51 bytes of files will be deleted', warning.text.trim());

  // Click format button.
  const formatButtonQuery = ['files-format-dialog', 'cr-button#format-button'];
  await remoteCall.waitAndClickElement(appId, formatButtonQuery);

  // Check the dialog is closed.
  await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog:not([open])']);
};

/**
 * Tests the format dialog is a modal dialog.
 */
testcase.formatDialogIsModal = async () => {
  await sendTestMessage({name: 'mountFakeUsb'});
  const appId = await setupFormatDialogTest();

  // Open the format dialog on fake-usb.
  await openFormatDialog(appId, 'fake-usb');

  // Focus the <cr-input> inner <input> element.
  const driveNameQuery = ['files-format-dialog', 'cr-input#label', 'input'];
  await remoteCall.simulateUiClick(appId, driveNameQuery);

  // Send a select-all keyboard event to the <input> element.
  const ctrlA = [driveNameQuery, 'a', true, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA);

  // Check: the file-list should have nothing selected.
  const selectedRows = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['#file-list li[selected]']);
  chrome.test.assertEq(0, selectedRows.length);
};

/**
 * Tests the format dialog for an empty USB.
 */
testcase.formatDialogEmpty = async () => {
  await sendTestMessage({name: 'mountFakeUsbEmpty'});
  const appId = await setupFormatDialogTest();

  // Open the format dialog on fake-usb.
  await openFormatDialog(appId, 'fake-usb');

  // Check the warning message is hidden.
  const warning = await remoteCall.waitForElement(
      appId, ['files-format-dialog', '#warning-container[fully-initialized]']);
  chrome.test.assertTrue(warning.hidden);

  // Click format button.
  const formatButtonQuery = ['files-format-dialog', 'cr-button#format-button'];
  await remoteCall.waitAndClickElement(appId, formatButtonQuery);

  // Check the dialog is closed.
  await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog:not([open])']);
};

/**
 * Tests cancelling out of the format dialog.
 */
testcase.formatDialogCancel = async () => {
  await sendTestMessage({name: 'mountFakeUsb'});
  const appId = await setupFormatDialogTest();

  // Open the format dialog on fake-usb.
  await openFormatDialog(appId, 'fake-usb');

  // Click cancel button.
  const formatButtonQuery = ['files-format-dialog', 'cr-button#cancel'];
  await remoteCall.waitAndClickElement(appId, formatButtonQuery);

  // Check the dialog is closed.
  await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog:not([open])']);
};

/**
 * Checks that formatting gives error |errorMessage| when given |label| and
 * |format|.
 *
 * @param {string} appId Files app window ID.
 * @param {string} label New label of usb drive.
 * @param {string} format New filesystem of drive.
 * @param {string} errorMessage Expected error message to be displayed.
 */
async function checkError(appId, label, format, errorMessage) {
  // Enter in a label.
  const driveNameQuery = ['files-format-dialog', 'cr-input#label'];
  await remoteCall.inputText(appId, driveNameQuery, label);

  // Select a format.
  const driveFormatQuery = ['files-format-dialog', '#disk-format select'];
  await remoteCall.inputText(appId, driveFormatQuery, format);

  // Check error message is not there.
  let driveNameElement = await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog[open] cr-input#label']);
  chrome.test.assertFalse(
      driveNameElement.attributes.hasOwnProperty('invalid'));

  // Click format button.
  const formatButtonQuery = ['files-format-dialog', 'cr-button#format-button'];
  await remoteCall.waitAndClickElement(appId, formatButtonQuery);

  // Check for error message.
  driveNameElement = await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog[open] cr-input#label']);
  chrome.test.assertTrue(driveNameElement.attributes.hasOwnProperty('invalid'));
  chrome.test.assertEq(
      errorMessage, driveNameElement.attributes['error-message']);
}

/**
 * Checks that formatting succeeds when given |label| and |format|.
 *
 * @param {string} appId Files app window ID.
 * @param {string} label New label of usb drive.
 * @param {string} format New filesystem of drive.
 */
async function checkSuccess(appId, label, format) {
  // Enter in a label.
  const driveNameQuery = ['files-format-dialog', 'cr-input#label'];
  await remoteCall.inputText(appId, driveNameQuery, label);

  // Select a format.
  const driveFormatQuery = ['files-format-dialog', '#disk-format select'];
  await remoteCall.inputText(appId, driveFormatQuery, format);

  // Check error message is not there.
  const driveNameElement = await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog[open] cr-input#label']);
  chrome.test.assertFalse(
      driveNameElement.attributes.hasOwnProperty('invalid'));

  // Click format button.
  const formatButtonQuery = ['files-format-dialog', 'cr-button#format-button'];
  await remoteCall.waitAndClickElement(appId, formatButtonQuery);

  // Check the dialog is closed.
  await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog:not([open])']);
}

/**
 * Tests validations for drive name length.
 */
testcase.formatDialogNameLength = async () => {
  await sendTestMessage({name: 'mountFakeUsb'});
  const appId = await setupFormatDialogTest();

  // Open the format dialog on fake-usb.
  await openFormatDialog(appId, 'fake-usb');

  // Check that a 12 character name fails on vfat.
  await checkError(
      appId, 'ABCDEFGHIJKL', 'vfat',
      'Use a name that\'s 11 characters or less');

  // Check that a 11 character name succeeds on vfat.
  await checkSuccess(appId, 'ABCDEFGHIJK', 'vfat');

  // Open the format dialog on fake-usb.
  await openFormatDialog(appId, 'fake-usb');

  // Check that a 16 character name fails on exfat.
  await checkError(
      appId, 'ABCDEFGHIJKLMNOP', 'exfat',
      'Use a name that\'s 15 characters or less');

  // Check that a 15 character name succeeds on exfat.
  await checkSuccess(appId, 'ABCDEFGHIJKLMNO', 'exfat');

  // Open the format dialog on fake-usb.
  await openFormatDialog(appId, 'fake-usb');

  // Check that a 33 character name fails on ntfs.
  await checkError(
      appId, 'ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFG', 'ntfs',
      'Use a name that\'s 32 characters or less');

  // Also test both invalid character and long name.
  await checkError(
      appId, '*ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFG', 'ntfs',
      'Use a name that\'s 32 characters or less');

  // Check that a 32 character name succeeds on ntfs.
  await checkSuccess(appId, 'ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF', 'ntfs');
};

/**
 * Test validations for invalid characters.
 */
testcase.formatDialogNameInvalid = async () => {
  await sendTestMessage({name: 'mountFakeUsb'});
  const appId = await setupFormatDialogTest();

  // Open the format dialog on fake-usb.
  await openFormatDialog(appId, 'fake-usb');

  // Check that a name with invalid characters fails.
  await checkError(appId, '<invalid>', 'vfat', 'Invalid character: <');

  // Check that a name without invalid characters succeeds.
  await checkSuccess(appId, 'Nice name', 'vfat');
};

/**
 * Tests opening the format dialog from the gear menu.
 */
testcase.formatDialogGearMenu = async () => {
  await sendTestMessage({name: 'mountFakeUsb'});
  const appId = await setupFormatDialogTest();

  // Focus the directory tree.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'focus', appId, ['#directory-tree']),
      'focus failed: #directory-tree');

  let usbNavigationPath = '/fake-usb';
  if (await isSinglePartitionFormat(appId)) {
    usbNavigationPath = '/FAKEUSB/fake-usb';
  }
  // Navigate to the USB via the directory tree.
  await navigateWithDirectoryTree(appId, usbNavigationPath);

  // Click on the gear menu button.
  await remoteCall.waitAndClickElement(appId, '#gear-button:not([hidden])');

  // Click on the format menu item.
  await remoteCall.waitAndClickElement(
      appId, '#gear-menu-format:not([disabled]):not([hidden])');

  // Check the format dialog is open and the title is correct
  const title = await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog[open] div[slot="title"]']);
  chrome.test.assertEq('Format fake-usb', title.text.trim());

  // Click cancel button.
  const cancelButtonQuery = ['files-format-dialog', 'cr-button#cancel'];
  await remoteCall.waitAndClickElement(appId, cancelButtonQuery);

  // Check the dialog is closed.
  await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog:not([open])']);

  // Focus the file list.
  await remoteCall.callRemoteTestUtil('focus', appId, ['#file-list']);

  // Click an item in the list.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);

  // Click on the gear menu button.
  await remoteCall.waitAndClickElement(appId, '#gear-button:not([hidden])');

  // Click on the format menu item.
  await remoteCall.waitAndClickElement(
      appId, '#gear-menu-format:not([disabled]):not([hidden])');

  // Check the format dialog is open and the title is correct
  const title2 = await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog[open] div[slot="title"]']);
  chrome.test.assertEq('Format fake-usb', title2.text.trim());

  // Click cancel button.
  await remoteCall.waitAndClickElement(appId, cancelButtonQuery);

  // Click on the gear menu button.
  await remoteCall.waitAndClickElement(appId, '#gear-button:not([hidden])');

  // Ensure the format menu item has appeared.
  await remoteCall.waitForElement(
      appId, '#gear-menu-format:not([disabled]):not([hidden])');

  // Unmount the USB.
  await sendTestMessage({name: 'unmountUsb'});

  // Ensure the file manager has navigated back to My files.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Ensure the format menu item has disappeared.
  await remoteCall.waitForElement(appId, '#gear-menu-format[disabled][hidden]');
};
