// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, getCaller, pending, repeatUntil, sendBrowserTestCommand, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {openAndWaitForClosingDialog, openEntryChoosingWindow, pollForChosenEntry, remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Sends a key event to an open file dialog, after selecting the file |name|
 * entry in the file list.
 *
 * @param {!string} name File name shown in the dialog.
 * @param {!Array<any>} key Key detail for fakeKeyDown event.
 * @param {!string} dialog ID of the file dialog window.
 * @return {!Promise<void>} Promise to be fulfilled on success.
 */
async function sendOpenFileDialogKey(name, key, dialog) {
  await remoteCall.waitUntilSelected(dialog, name);
  await remoteCall.callRemoteTestUtil('fakeKeyDown', dialog, key);
}

/**
 * Clicks a button in the open file dialog, after selecting the file |name|
 * entry in the file list and checking that |button| exists.
 *
 * @param {!string} name File name shown in the dialog.
 * @param {!string} button Selector of the dialog button.
 * @param {!string} dialog ID of the file dialog window.
 * @return {!Promise<void>} Promise to be fulfilled on success.
 */
async function clickOpenFileDialogButton(name, button, dialog) {
  await remoteCall.waitUntilSelected(dialog, name);
  await remoteCall.waitForElement(dialog, button);
  const event = [button, 'click'];
  await remoteCall.callRemoteTestUtil('fakeEvent', dialog, event);
}

/**
 * Sends an unload event to an open file dialog (after it is drawn) causing
 * the dialog to shut-down and close.
 *
 * @param {!string} dialog ID of the file dialog window.
 * @param {string} element Element to query for drawing.
 * @return {!Promise<void>} Promise to be fulfilled on success.
 */
async function unloadOpenFileDialog(
    dialog, element = '.button-panel button.ok') {
  await remoteCall.waitForElement(dialog, element);
  await remoteCall.callRemoteTestUtil('unload', dialog, []);
  const errorCount =
      await remoteCall.callRemoteTestUtil('getErrorCount', dialog, []);
  chrome.test.assertEq(0, errorCount);
}

/**
 * Adds basic file entry sets for both 'local' and 'drive', and returns the
 * entry set of the given |volume|.
 *
 * @param {!string} volume Name of the volume.
 * @return {!Promise<TestEntryInfo[]>} Promise to
 *     resolve({Array<TestEntryInfo>}) on success, the Array being the basic
 *     file entry set of the |volume|.
 */
async function setUpFileEntrySet(volume) {
  const localEntryPromise = addEntries(['local'], BASIC_LOCAL_ENTRY_SET);

  const driveEntries = [
    ENTRIES.hello,
    ENTRIES.pinned,
    ENTRIES.testCSEDocument,
    ENTRIES.testCSEFile,
    ENTRIES.testDocument,
    ENTRIES.docxFile,
  ];
  const driveEntryPromise = addEntries(['drive'], driveEntries);

  await Promise.all([localEntryPromise, driveEntryPromise]);
  if (volume === 'drive') {
    return driveEntries;
  }
  return BASIC_LOCAL_ENTRY_SET;
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and click the Ok button.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @param {boolean} useBrowserOpen Whether to launch the dialog from the
 *     browser.
 * @return {!Promise<void>} Promise to be fulfilled on success.
 */
async function openFileDialogClickOkButton(
    volume, name, useBrowserOpen = false) {
  const okButton = '.button-panel button.ok:enabled';
  await sendTestMessage(
      {name: 'expectFileTask', fileNames: [name], openType: 'open'});
  const closer = clickOpenFileDialogButton.bind(null, name, okButton);

  const entrySet = await setUpFileEntrySet(volume);
  const result = await openAndWaitForClosingDialog(
      // @ts-ignore: error TS2353: Object literal may only specify known
      // properties, and 'type' does not exist in type 'AcceptsOption'.
      {type: 'openFile'}, volume, entrySet, closer, useBrowserOpen);
  // If the file is opened via the filesystem API, check the name matches.
  // Otherwise, the caller is responsible for verifying the returned URL.
  if (!useBrowserOpen) {
    // @ts-ignore: error TS18046: 'result' is of type 'unknown'.
    chrome.test.assertEq(name, result.name);
  }
  // @ts-ignore: error TS2322: Type 'unknown' is not assignable to type 'void'.
  return result;
}

/**
 * Clicks the OK button in the provided dialog, expecting the provided `name` to
 * be passed into the `OnFilesImpl()` observer in the C++ test harness.
 *
 * @param {string} appId App window Id.
 * @param {string} name The (single) filename passed to the EXPECT_CALL when
 *     verifying the mocked OnFilesOpenedImpl().
 * @param {string} openType Type of the dialog ('open' or 'saveAs').
 */
async function clickOkButtonExpectName(appId, name, openType) {
  await sendTestMessage({name: 'expectFileTask', fileNames: [name], openType});

  const okButton = '.button-panel button.ok:enabled';
  await remoteCall.waitAndClickElement(appId, okButton);
}

/**
 * Adds the basic file entry sets then opens the save file dialog on the volume.
 * Once file |name| is shown, select it and click the Ok button, again clicking
 * Ok in the confirmation dialog.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise<void>} Promise to be fulfilled on success.
 */
async function saveFileDialogClickOkButton(volume, name) {
  const caller = getCaller();

  // @ts-ignore: error TS7006: Parameter 'appId' implicitly has an 'any' type.
  const closer = async (appId) => {
    await remoteCall.waitUntilSelected(appId, name);
    // @ts-ignore: error TS7030: Not all code paths return a value.
    await repeatUntil(async () => {
      const element =
          await remoteCall.waitForElement(appId, '#filename-input-textbox');
      // @ts-ignore: error TS2339: Property 'value' does not exist on type
      // 'ElementObject'.
      if (element.value !== name) {
        return pending(caller, 'Text field not updated');
      }
    });

    await clickOkButtonExpectName(appId, name, 'saveAs');

    const confirmOkButton = '.files-confirm-dialog .cr-dialog-ok';
    await remoteCall.waitForElement(appId, confirmOkButton);
    await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, [confirmOkButton, 'click']);
  };

  const entrySet = await setUpFileEntrySet(volume);
  const result = await openAndWaitForClosingDialog(
      // @ts-ignore: error TS2353: Object literal may only specify known
      // properties, and 'type' does not exist in type 'AcceptsOption'.
      {type: 'saveFile'}, volume, entrySet, closer, false);
  // @ts-ignore: error TS18046: 'result' is of type 'unknown'.
  chrome.test.assertEq(name, result.name);
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and verify that the Ok button is
 * disabled.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog where the OK button
 *     should be disabled
 * @param {!string} enabledName File name to select where the OK button should
 *     be enabled, used to ensure that switching to |name| results in the OK
 *     button becoming disabled.
 * @param {!string} type The dialog type to open.
 * @return {!Promise<void>} Promise to be fulfilled on success.
 */
async function openFileDialogExpectOkButtonDisabled(
    volume, name, enabledName, type = 'openFile') {
  const okButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';
  // @ts-ignore: error TS7006: Parameter 'dialog' implicitly has an 'any' type.
  const closer = async (dialog) => {
    await remoteCall.waitUntilSelected(dialog, enabledName);
    await remoteCall.waitForElement(dialog, okButton);
    await remoteCall.waitUntilSelected(dialog, name);
    await remoteCall.waitForElement(dialog, disabledOkButton);
    clickOpenFileDialogButton(name, cancelButton, dialog);
  };

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      // @ts-ignore: error TS2353: Object literal may only specify known
      // properties, and 'type' does not exist in type 'AcceptsOption'.
      await openAndWaitForClosingDialog({type}, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, verifies that it's dimmed according to added
 * classes.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to check for being dimmed in the dialog.
 * @return {!Promise<void>} Promise to be fulfilled on success.
 */
async function openFileDialogExpectEntryDimmed(volume, name) {
  const type = 'openFile';
  const cancelButton = '.button-panel button.cancel';
  const fileEntry = `#file-list [file-name="${name}"]`;
  // @ts-ignore: error TS7006: Parameter 'dialog' implicitly has an 'any' type.
  const closer = async (dialog) => {
    const element = await remoteCall.waitForElement(dialog, fileEntry);
    let dimmed = false;
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    for (const className of element.attributes['class'].split(' ')) {
      if (className === 'dim-offline') {
        // The 'dim-offline' class dims an element only if the connection
        // status is 'OFFLINE', which is not something this test is verifying.
        continue;
      }
      if (className.startsWith('dim')) {
        dimmed = true;
        break;
      }
    }
    chrome.test.assertTrue(dimmed, 'The file entry should be dimmed');
    clickOpenFileDialogButton(name, cancelButton, dialog);
  };

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      // @ts-ignore: error TS2353: Object literal may only specify known
      // properties, and 'type' does not exist in type 'AcceptsOption'.
      await openAndWaitForClosingDialog({type}, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and click the Cancel button.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise<void>} Promise to be fulfilled on success.
 */
async function openFileDialogClickCancelButton(volume, name) {
  const type = {type: 'openFile'};

  const cancelButton = '.button-panel button.cancel';
  const closer = clickOpenFileDialogButton.bind(null, name, cancelButton);

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      // @ts-ignore: error TS2559: Type '{ type: string; }' has no properties in
      // common with type 'AcceptsOption'.
      await openAndWaitForClosingDialog(type, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and send an Escape key.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise<void>} Promise to be fulfilled on success.
 */
async function openFileDialogSendEscapeKey(volume, name) {
  const type = {type: 'openFile'};

  const escapeKey = ['#file-list', 'Escape', false, false, false];
  const closer = sendOpenFileDialogKey.bind(null, name, escapeKey);

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      // @ts-ignore: error TS2559: Type '{ type: string; }' has no properties in
      // common with type 'AcceptsOption'.
      await openAndWaitForClosingDialog(type, volume, entrySet, closer));
}

/**
 * Waits for the dialog window and waits it to fully load.
 * @returns {!Promise<string>} dialog's id.
 */
export async function waitForDialog() {
  const dialog = await remoteCall.waitForWindow();

  // Wait for Files app to finish loading.
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', dialog, true);

  return dialog;
}

/**
 * Tests for display:none status of feedback panels in Files app.
 *
 * @param {string} type Type of dialog to open.
 */
async function checkFeedbackDisplayHidden(type) {
  // Open dialog of the specified 'type'.
  // @ts-ignore: error TS2322: Type 'string' is not assignable to type
  // 'ChooseEntryType | undefined'.
  await openEntryChoosingWindow({type: type});
  const appId = await waitForDialog();

  // Wait to finish initial load.
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
  // Check the display style of the feedback panels container.
  const element = await remoteCall.waitForElementStyles(
      appId, ['.files-feedback-panels'], ['display']);
  // Check that CSS display style is 'none'.
  // @ts-ignore: error TS18048: 'element.styles' is possibly 'undefined'.
  chrome.test.assertTrue(element.styles['display'] === 'none');
}

/**
 * Test file present in Downloads.
 * @return {!string}
 */
function getTestFileName() {
  // Type TestEntryInfo's targetPath can be undefined, but the first item
  // from BASIC_LOCAL_ENTRY_SET has value, we need to do type casting here.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  return /** @type {!string} */ (BASIC_LOCAL_ENTRY_SET[0].targetPath);
}

/**
 * Tests opening file dialog on Downloads and closing it with Ok button.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDownloads' comes from an
// index signature, so it must be accessed with ['openFileDialogDownloads'].
testcase.openFileDialogDownloads = () => {
  return openFileDialogClickOkButton('downloads', getTestFileName());
};

/**
 * Tests opening file dialog sets aria-multiselect true on grid and list.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogAriaMultipleSelect' comes
// from an index signature, so it must be accessed with
// ['openFileDialogAriaMultipleSelect'].
testcase.openFileDialogAriaMultipleSelect = async () => {
  // Open File dialog.
  await openEntryChoosingWindow({type: 'openFile'});
  const appId = await waitForDialog();

  // Wait to finish initial load.
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: <list> has aria-multiselect set to true.
  const list = 'list#file-list[aria-multiselectable=true]';
  await remoteCall.waitForElement(appId, list);

  // Check: <grid> has aria-multiselect set to true.
  const grid = 'grid#file-list[aria-multiselectable=true]';
  await remoteCall.waitForElement(appId, grid);
};

/**
 * Tests opening save file dialog sets aria-multiselect false on grid and list.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogAriaSingleSelect' comes
// from an index signature, so it must be accessed with
// ['saveFileDialogAriaSingleSelect'].
testcase.saveFileDialogAriaSingleSelect = async () => {
  // Open Save as dialog.
  await openEntryChoosingWindow({type: 'saveFile'});
  const appId = await waitForDialog();

  // Wait to finish initial load.
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: <list> has aria-multiselect set to false.
  const list = 'list#file-list[aria-multiselectable=false]';
  await remoteCall.waitForElement(appId, list);

  // Check: <grid> has aria-multiselect set to false.
  const grid = 'grid#file-list[aria-multiselectable=false]';
  await remoteCall.waitForElement(appId, grid);
};

/**
 * Tests opening save file dialog on Downloads and closing it
 * with Ok button.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogDownloads' comes from an
// index signature, so it must be accessed with ['saveFileDialogDownloads'].
testcase.saveFileDialogDownloads = () => {
  return saveFileDialogClickOkButton('downloads', getTestFileName());
};

/**
 * Tests opening save file dialog on Downloads and using New Folder button.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogDownloadsNewFolderButton'
// comes from an index signature, so it must be accessed with
// ['saveFileDialogDownloadsNewFolderButton'].
testcase.saveFileDialogDownloadsNewFolderButton = async () => {
  // Open Save as dialog.
  await openEntryChoosingWindow({type: 'saveFile'});
  const appId = await waitForDialog();

  // Wait to finish initial load.
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: New Folder button should be enabled and click on it.
  const query = '#new-folder-button:not([disabled])';
  // @ts-ignore: error TS6133: 'newFolderButton' is declared but its value is
  // never read.
  const newFolderButton = await remoteCall.waitAndClickElement(appId, query);

  // Wait for the new folder with input to appear, assume the rest of the
  // process works (covered by other tests).
  const textInput = '#file-list .table-row[renaming] input.rename';
  await remoteCall.waitForElement(appId, textInput);
};

/**
 * Tests opening file dialog on Downloads and closing it with Cancel button.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogCancelDownloads' comes from
// an index signature, so it must be accessed with
// ['openFileDialogCancelDownloads'].
testcase.openFileDialogCancelDownloads = () => {
  return openFileDialogClickCancelButton('downloads', getTestFileName());
};

/**
 * Tests opening file dialog on Downloads and closing it with ESC key.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogEscapeDownloads' comes from
// an index signature, so it must be accessed with
// ['openFileDialogEscapeDownloads'].
testcase.openFileDialogEscapeDownloads = () => {
  return openFileDialogSendEscapeKey('downloads', getTestFileName());
};

/**
 * Tests the feedback panels are hidden when using an open file dialog.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogPanelsDisabled' comes from
// an index signature, so it must be accessed with
// ['openFileDialogPanelsDisabled'].
testcase.openFileDialogPanelsDisabled = () => {
  return checkFeedbackDisplayHidden('openFile');
};

/**
 * Tests the feedback panels are hidden when using a save file dialog.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogPanelsDisabled' comes from
// an index signature, so it must be accessed with
// ['saveFileDialogPanelsDisabled'].
testcase.saveFileDialogPanelsDisabled = () => {
  return checkFeedbackDisplayHidden('saveFile');
};

/**
 * Test file present in Drive only.
 * @const {!string}
 */
const TEST_DRIVE_FILE = ENTRIES.hello.targetPath;

/**
 * Test file present in Drive only.
 * @const {!string}
 */
// @ts-ignore: error TS4111: Property 'pinned' comes from an index signature, so
// it must be accessed with ['pinned'].
const TEST_DRIVE_PINNED_FILE = ENTRIES.pinned.targetPath;

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDrive' comes from an index
// signature, so it must be accessed with ['openFileDialogDrive'].
testcase.openFileDialogDrive = () => {
  // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
  // assignable to parameter of type 'string'.
  return openFileDialogClickOkButton('drive', TEST_DRIVE_FILE);
};

/**
 * Tests save file dialog on Drive and closing it with Ok button.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogDrive' comes from an index
// signature, so it must be accessed with ['saveFileDialogDrive'].
testcase.saveFileDialogDrive = () => {
  // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
  // assignable to parameter of type 'string'.
  return saveFileDialogClickOkButton('drive', TEST_DRIVE_FILE);
};

/**
 * Tests that an unpinned file cannot be selected in file open dialogs while
 * offline.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDriveOffline' comes from an
// index signature, so it must be accessed with ['openFileDialogDriveOffline'].
testcase.openFileDialogDriveOffline = () => {
  return openFileDialogExpectOkButtonDisabled(
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      'drive', TEST_DRIVE_FILE, TEST_DRIVE_PINNED_FILE);
};

/**
 * Tests that an unpinned file cannot be selected in save file dialogs while
 * offline.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogDriveOffline' comes from an
// index signature, so it must be accessed with ['saveFileDialogDriveOffline'].
testcase.saveFileDialogDriveOffline = () => {
  return openFileDialogExpectOkButtonDisabled(
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      'drive', TEST_DRIVE_FILE, TEST_DRIVE_PINNED_FILE, 'saveFile');
};

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDriveOfflinePinned' comes
// from an index signature, so it must be accessed with
// ['openFileDialogDriveOfflinePinned'].
testcase.openFileDialogDriveOfflinePinned = () => {
  // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
  // assignable to parameter of type 'string'.
  return openFileDialogClickOkButton('drive', TEST_DRIVE_PINNED_FILE);
};

/**
 * Tests save file dialog on Drive and closing it with Ok button.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogDriveOfflinePinned' comes
// from an index signature, so it must be accessed with
// ['saveFileDialogDriveOfflinePinned'].
testcase.saveFileDialogDriveOfflinePinned = () => {
  // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
  // assignable to parameter of type 'string'.
  return saveFileDialogClickOkButton('drive', TEST_DRIVE_PINNED_FILE);
};

/**
 * Tests opening a file from Drive in the browser, ensuring it correctly
 * opens the file URL.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDriveFromBrowser' comes
// from an index signature, so it must be accessed with
// ['openFileDialogDriveFromBrowser'].
testcase.openFileDialogDriveFromBrowser = async () => {
  const url = new URL(
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      await openFileDialogClickOkButton('drive', TEST_DRIVE_FILE, true));

  chrome.test.assertEq(url.protocol, 'file:');
  chrome.test.assertTrue(
      url.pathname.endsWith(`/root/${TEST_DRIVE_FILE}`), url.pathname);
};

/**
 * Tests opening a hosted doc in the browser, ensuring it correctly navigates to
 * the doc's URL.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDriveHostedDoc' comes from
// an index signature, so it must be accessed with
// ['openFileDialogDriveHostedDoc'].
testcase.openFileDialogDriveHostedDoc = async () => {
  chrome.test.assertEq(
      await openFileDialogClickOkButton(
          // @ts-ignore: error TS4111: Property 'testDocument' comes from an
          // index signature, so it must be accessed with ['testDocument'].
          'drive', ENTRIES.testDocument.nameText, true),
      // @ts-ignore: error TS2345: Argument of type 'string' is not assignable
      // to parameter of type 'void'.
      'https://document_alternate_link/Test%20Document');
};

/**
 * Tests opening a hosted doc in the browser, ensuring it correctly navigates to
 * the doc's URL.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDriveEncryptedFile' comes
// from an index signature, so it must be accessed with
// ['openFileDialogDriveEncryptedFile'].
testcase.openFileDialogDriveEncryptedFile = async () => {
  chrome.test.assertEq(
      await openFileDialogClickOkButton(
          // @ts-ignore: error TS4111: Property 'testCSEFile' comes from an
          // index signature, so it must be accessed with ['testCSEFile'].
          'drive', ENTRIES.testCSEFile.nameText, true),
      // @ts-ignore: error TS2345: Argument of type 'string' is not assignable
      // to parameter of type 'void'.
      'https://file_alternate_link/test-encrypted.txt');
};

/**
 * Tests that selecting a hosted doc from a dialog requiring a real file is
 * disabled.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDriveHostedNeedsFile' comes
// from an index signature, so it must be accessed with
// ['openFileDialogDriveHostedNeedsFile'].
testcase.openFileDialogDriveHostedNeedsFile = () => {
  return openFileDialogExpectOkButtonDisabled(
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      'drive', ENTRIES.testDocument.nameText, TEST_DRIVE_FILE);
};

/**
 * Tests that selecting a hosted doc from a dialog requiring a real file is
 * disabled.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogDriveHostedNeedsFile' comes
// from an index signature, so it must be accessed with
// ['saveFileDialogDriveHostedNeedsFile'].
testcase.saveFileDialogDriveHostedNeedsFile = () => {
  return openFileDialogExpectOkButtonDisabled(
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      'drive', ENTRIES.testDocument.nameText, TEST_DRIVE_FILE, 'saveFile');
};

/**
 * Test that an encrypted (via CSE) file will be marked as grey in a dialog
 * requiring a read file.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDriveCSEGrey' comes from an
// index signature, so it must be accessed with ['openFileDialogDriveCSEGrey'].
testcase.openFileDialogDriveCSEGrey = () => {
  // @ts-ignore: error TS4111: Property 'testCSEFile' comes from an index
  // signature, so it must be accessed with ['testCSEFile'].
  return openFileDialogExpectEntryDimmed('drive', ENTRIES.testCSEFile.nameText);
};

/**
 * Tests that selecting an encrypted (via CSE) file from a dialog requiring
 * a real file is disabled.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDriveCSENeedsFile' comes
// from an index signature, so it must be accessed with
// ['openFileDialogDriveCSENeedsFile'].
testcase.openFileDialogDriveCSENeedsFile = () => {
  return openFileDialogExpectOkButtonDisabled(
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      'drive', ENTRIES.testCSEFile.nameText, TEST_DRIVE_FILE);
};

/**
 * Tests opening file dialog on Drive and selecting an office file.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDriveOfficeFile' comes from
// an index signature, so it must be accessed with
// ['openFileDialogDriveOfficeFile'].
testcase.openFileDialogDriveOfficeFile = () => {
  // @ts-ignore: error TS4111: Property 'docxFile' comes from an index
  // signature, so it must be accessed with ['docxFile'].
  return openFileDialogClickOkButton('drive', ENTRIES.docxFile.nameText);
};

/**
 * Tests opening file dialog on Drive and selecting multiple files including an
 * office file.
 */
// @ts-ignore: error TS4111: Property 'openMultiFileDialogDriveOfficeFile' comes
// from an index signature, so it must be accessed with
// ['openMultiFileDialogDriveOfficeFile'].
testcase.openMultiFileDialogDriveOfficeFile = async () => {
  await setUpFileEntrySet('drive');
  await openEntryChoosingWindow({type: 'openFile', acceptsMultiple: true});
  const appId = await waitForDialog();

  // Wait for initial load to finish.
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/My Drive');

  // Sort the file names so we can compare the array directly with the entries
  // returned from pollForChosenEntry() without worrying about order.
  const selectFileNames = [
    ENTRIES.hello.nameText,
    ENTRIES.docxFile.nameText,
  ].sort();

  // Select both files with the dialog.
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${selectFileNames[0]}"]`);
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${selectFileNames[1]}"]`, {ctrl: true});
  await sendTestMessage(
      {name: 'expectFileTask', fileNames: selectFileNames, openType: 'open'});
  const okButton = '.button-panel button.ok:enabled';
  await remoteCall.waitAndClickElement(appId, okButton);

  const chosenEntries =
      // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an 'any'
      // type.
      (await pollForChosenEntry(getCaller())).map(entry => entry.name).sort();
  chrome.test.assertEq(selectFileNames, chosenEntries);
};

/**
 * Tests opening file dialog on Drive and closing it with Cancel button.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogCancelDrive' comes from an
// index signature, so it must be accessed with ['openFileDialogCancelDrive'].
testcase.openFileDialogCancelDrive = () => {
  // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
  // assignable to parameter of type 'string'.
  return openFileDialogClickCancelButton('drive', TEST_DRIVE_FILE);
};

/**
 * Tests opening file dialog on Drive and closing it with ESC key.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogEscapeDrive' comes from an
// index signature, so it must be accessed with ['openFileDialogEscapeDrive'].
testcase.openFileDialogEscapeDrive = () => {
  // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
  // assignable to parameter of type 'string'.
  return openFileDialogSendEscapeKey('drive', TEST_DRIVE_FILE);
};

/**
 * Tests opening file dialog, then closing it with an 'unload' event.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogUnload' comes from an index
// signature, so it must be accessed with ['openFileDialogUnload'].
testcase.openFileDialogUnload = async () => {
  await openEntryChoosingWindow({type: 'openFile'});
  const dialog = await waitForDialog();
  await unloadOpenFileDialog(dialog);
};

/**
 * Tests that the open file dialog's filetype filter does not default to all
 * types.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogDefaultFilter' comes from
// an index signature, so it must be accessed with
// ['openFileDialogDefaultFilter'].
testcase.openFileDialogDefaultFilter = async () => {
  const params = {
    type: 'openFile',
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: true,
  };
  // @ts-ignore: error TS2345: Argument of type '{ type: string; accepts: {
  // extensions: string[]; }[]; acceptsAllTypes: boolean; }' is not assignable
  // to parameter of type 'ChooseEntryOptions'.
  await openEntryChoosingWindow(params);
  const dialog = await waitForDialog();

  // Check: 'JPEG image' should be selected.
  const selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option:checked');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
};

/**
 * Tests that the save file dialog's filetype filter defaults to all types.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogDefaultFilter' comes from
// an index signature, so it must be accessed with
// ['saveFileDialogDefaultFilter'].
testcase.saveFileDialogDefaultFilter = async () => {
  const params = {
    type: 'saveFile',
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: true,
  };
  // @ts-ignore: error TS2345: Argument of type '{ type: string; accepts: {
  // extensions: string[]; }[]; acceptsAllTypes: boolean; }' is not assignable
  // to parameter of type 'ChooseEntryOptions'.
  await openEntryChoosingWindow(params);
  const dialog = await waitForDialog();

  // Check: 'All files' should be selected.
  const selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option:checked');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);
};

/**
 * Tests that the save file dialog's filetype filter can
 * be navigated using the keyboard.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogDefaultFilterKeyNavigation'
// comes from an index signature, so it must be accessed with
// ['saveFileDialogDefaultFilterKeyNavigation'].
testcase.saveFileDialogDefaultFilterKeyNavigation = async () => {
  const params = {
    type: 'saveFile',
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: true,
  };
  // @ts-ignore: error TS2345: Argument of type '{ type: string; accepts: {
  // extensions: string[]; }[]; acceptsAllTypes: boolean; }' is not assignable
  // to parameter of type 'ChooseEntryOptions'.
  await openEntryChoosingWindow(params);
  const dialog = await waitForDialog();

  // Check: 'All files' should be selected.
  let selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: up key causes 'JPEG image' to  be selected.
  const selectControl = 'div.file-type';
  const arrowUpKey = ['ArrowUp', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowUpKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);

  // Check: down key causes 'All files' to be selected.
  const arrowDownKey = ['ArrowDown', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowDownKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: another down key doesn't wrap to the top selection.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowDownKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: left key acts like up when control is closed.
  const arrowLeftKey = ['ArrowLeft', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowLeftKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);

  // Check: right key acts like down when control is closed.
  const arrowRightKey = ['ArrowRight', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowRightKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: Enter key expands the select control.
  const enterKey = ['Enter', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...enterKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: second Enter key collapses the select control.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...enterKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: space key expands the select control.
  const spaceKey = [' ', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: second space key collapses the select control.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: Escape key collapses the select control.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  const escapeKey = ['Escape', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...escapeKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: tab key collapses the select control.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  const tabKey = ['Tab', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...tabKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: tab key collapsing remembers changed selection.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowUpKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...tabKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);

  // Check: Escape key collapsing remembers changed selection.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowDownKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...escapeKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: left arrow does nothing with control expanded.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowLeftKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: right arrow does nothing with control expanded.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowUpKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowRightKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
};

/**
 * Tests that filtering works with { acceptsAllTypes: false } and a single
 * filter. Regression test for https://crbug.com/1097448.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogSingleFilterNoAcceptAll'
// comes from an index signature, so it must be accessed with
// ['saveFileDialogSingleFilterNoAcceptAll'].
testcase.saveFileDialogSingleFilterNoAcceptAll = async () => {
  const params = {
    type: 'saveFile',
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: false,
  };
  // @ts-ignore: error TS2345: Argument of type '{ type: string; accepts: {
  // extensions: string[]; }[]; acceptsAllTypes: boolean; }' is not assignable
  // to parameter of type 'ChooseEntryOptions'.
  await openEntryChoosingWindow(params);
  const dialog = await waitForDialog();

  // Check: 'JPEG image' should be selected.
  const selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option:checked');
  // @ts-ignore: error TS2339: Property 'value' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
};

/**
 * Opens a "Save As" dialog and clicks OK. Helper for the
 * saveFileDialogExtension* tests.
 *
 * @param {!Object} extraParams Extra options to pass to chooseEntry().
 * @param {string} expectName Name for the 'expectFileTask' mock expectation.
 * @return {!Promise<string>} The name of the entry from chooseEntry().
 */
async function showSaveAndConfirmExpecting(extraParams, expectName) {
  const caller = getCaller();

  const params = {
    type: 'saveFile',
    accepts: [{extensions: ['jpg']}],
  };
  // @ts-ignore: error TS2345: Argument of type '{ type: string; accepts: {
  // extensions: string[]; }[]; } & Object' is not assignable to parameter of
  // type 'ChooseEntryOptions'.
  await openEntryChoosingWindow(Object.assign(params, extraParams));
  const dialog = await waitForDialog();

  // Ensure the input field is ready.
  await remoteCall.waitForElement(dialog, '#filename-input-textbox');

  await clickOkButtonExpectName(dialog, expectName, 'saveAs');
  const entry = await pollForChosenEntry(caller);
  // @ts-ignore: error TS2339: Property 'name' does not exist on type
  // 'FileSystemEntry | FileSystemEntry[]'.
  return entry.name;
}

/**
 * Tests that a file extension is not automatically added upon confirmation
 * whilst the "All Files" filter is selected on the "Save As" dialog. Note the
 * saveFileDialogDefaultFilter test above verifies that 'All Files' is actually
 * the default in this setup.
 */
// @ts-ignore: error TS4111: Property
// 'saveFileDialogExtensionNotAddedWithNoFilter' comes from an index signature,
// so it must be accessed with ['saveFileDialogExtensionNotAddedWithNoFilter'].
testcase.saveFileDialogExtensionNotAddedWithNoFilter = async () => {
  // Note these tests use the suggestedName field as a robust way to simulate a
  // user typing into the input field.
  const extraParams = {acceptsAllTypes: true, suggestedName: 'test'};
  const name = await showSaveAndConfirmExpecting(extraParams, 'test');
  chrome.test.assertEq('test', name);
};

/**
 * With no "All Files" option, the JPEG filter should be applied by default, and
 * a ".jpg" extension automatically added on confirm.
 */
// @ts-ignore: error TS4111: Property
// 'saveFileDialogExtensionAddedWithJpegFilter' comes from an index signature,
// so it must be accessed with ['saveFileDialogExtensionAddedWithJpegFilter'].
testcase.saveFileDialogExtensionAddedWithJpegFilter = async () => {
  const extraParams = {acceptsAllTypes: false, suggestedName: 'test'};
  const name = await showSaveAndConfirmExpecting(extraParams, 'test.jpg');
  chrome.test.assertEq('test.jpg', name);
};

/**
 * An extension should only be added if the user didn't provide one, even if it
 * doesn't match the current filter for JPEG files (i.e. /\.(jpg)$/i).
 */
// @ts-ignore: error TS4111: Property
// 'saveFileDialogExtensionNotAddedWhenProvided' comes from an index signature,
// so it must be accessed with ['saveFileDialogExtensionNotAddedWhenProvided'].
testcase.saveFileDialogExtensionNotAddedWhenProvided = async () => {
  const extraParams = {acceptsAllTypes: false, suggestedName: 'foo.png'};
  const name = await showSaveAndConfirmExpecting(extraParams, 'foo.png');
  chrome.test.assertEq('foo.png', name);
};

/**
 * Tests that context menu on File List for file picker dialog.
 * File picker dialog displays fewer menu options than full Files app. For
 * example copy/paste commands are disabled. Right-click on a file/folder should
 * show context menu, whereas right-clicking on the blank parts of file list
 * should NOT display the context menu.
 *
 * crbug.com/917975 crbug.com/983507.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogFileListShowContextMenu'
// comes from an index signature, so it must be accessed with
// ['openFileDialogFileListShowContextMenu'].
testcase.openFileDialogFileListShowContextMenu = async () => {
  // Make sure the file picker will open to Downloads.
  sendBrowserTestCommand({name: 'setLastDownloadDir'}, () => {});

  // Add entries to Downloads.
  await addEntries(['local'], BASIC_LOCAL_ENTRY_SET);

  // Open file picker dialog.
  await openEntryChoosingWindow({type: 'openFile'});
  const appId = await waitForDialog();

  // Wait to finish initial load.
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Wait for files to be displayed.
  const expectedRows = [
    ['Play files', '--', 'Folder'],
    ['Downloads', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      // @ts-ignore: error TS2345: Argument of type '{ ignoreLastModifiedTime:
      // true; }' is not assignable to parameter of type '{ orderCheck: boolean
      // | null | undefined; ignoreFileSize: boolean | null | undefined;
      // ignoreLastModifiedTime: boolean | null | undefined; }'.
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Navigate to Downloads folder.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/My files/Downloads');

  // Right-click "photos" folder to show context menu.
  await remoteCall.waitAndRightClick(appId, '#file-list [file-name="photos"]');

  // Wait until the context menu appears.
  const menuVisible = '#file-context-menu:not([hidden])';
  await remoteCall.waitForElement(appId, menuVisible);

  // Dismiss context menu.
  const escKey = ['Escape', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(appId, menuVisible, ...escKey);
  await remoteCall.waitForElementLost(appId, menuVisible);

  // Right-click inside of #file-list (in an empty space).
  await remoteCall.rightClickFileListBlankSpace(appId);

  // Check that context menu is NOT displayed because there is no visible menu
  // items.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');
};

/**
 * Tests that select all is disabled in the gear menu for an open file dialog.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogSelectAllDisabled' comes
// from an index signature, so it must be accessed with
// ['openFileDialogSelectAllDisabled'].
testcase.openFileDialogSelectAllDisabled = async () => {
  // Open file picker dialog.
  await openEntryChoosingWindow({type: 'openFile'});
  const appId = await waitForDialog();

  // Wait to finish initial load.
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Wait for the gear menu button to appear and click it.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check: #select-all command is shown, but disabled.
  await remoteCall.waitForElement(
      appId,
      '#gear-menu ' +
          'cr-menu-item[command="#select-all"][disabled]:not([hidden])');
};

/**
 * Tests that select all is enabled in the gear menu for an open multiple files
 * dialog. crbug.com/937251
 */
// @ts-ignore: error TS4111: Property 'openMultiFileDialogSelectAllEnabled'
// comes from an index signature, so it must be accessed with
// ['openMultiFileDialogSelectAllEnabled'].
testcase.openMultiFileDialogSelectAllEnabled = async () => {
  // Make sure the file picker will open to Downloads.
  sendBrowserTestCommand({name: 'setLastDownloadDir'}, () => {});

  // Open file picker dialog with support for selecting multiple files.
  await openEntryChoosingWindow({type: 'openFile', acceptsMultiple: true});
  const appId = await waitForDialog();

  // Wait to finish initial load.
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Wait for the gear menu button to appear and click it.
  await remoteCall.waitAndClickElement(appId, '#gear-button');

  // Wait for the gear menu to appear.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Check: #select-all command is shown, but enabled.
  await remoteCall.waitForElement(
      appId,
      '#gear-menu ' +
          'cr-menu-item[command="#select-all"]:not([disabled]):not([hidden])');
};

/**
 * Tests open file dialog on a GuestOS volume. Check that the placeholder is
 * shown in the dialog and that clicking on it mounts the volume. We don't
 * bother actually opening a file since once it's mounted it works like any
 * other local FUSE volume.
 */
// @ts-ignore: error TS4111: Property 'openFileDialogGuestOs' comes from an
// index signature, so it must be accessed with ['openFileDialogGuestOs'].
testcase.openFileDialogGuestOs = async () => {
  // Register a fake GuestOs guest.
  await sendTestMessage({
    name: 'registerMountableGuest',
    displayName: 'Bluejohn',
    canMount: true,
    vmType: 'bruschetta',
  });

  // Open the open file dialog.
  await openEntryChoosingWindow({type: 'openFile'});

  // Wait for the dialog to be fully loaded.
  const appId = await remoteCall.waitForWindow();
  await remoteCall.waitForElement(appId, '#file-list');
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Click the Guest OS placeholder.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.selectPlaceholderItemByType('bruschetta');

  // Wait for the directory scanning to finish to guarantee the FileWatcher call
  // is finished.
  await remoteCall.waitForElement(
      appId, `#list-container[scan-completed="Bluejohn"]`);

  // Wait for the actual volume to appear.
  await directoryTree.waitForItemByType('bruschetta');
};

/**
 * Tests save file dialog on a GuestOS volume. Check that the placeholder is
 * shown in the dialog and that clicking on it mounts the volume. We don't
 * bother actually saving a file since once it's mounted it works like any other
 * local FUSE volume.
 */
// @ts-ignore: error TS4111: Property 'saveFileDialogGuestOs' comes from an
// index signature, so it must be accessed with ['saveFileDialogGuestOs'].
testcase.saveFileDialogGuestOs = async () => {
  // Register a fake GuestOs guest.
  await sendTestMessage({
    name: 'registerMountableGuest',
    displayName: 'Bluejohn',
    canMount: true,
    vmType: 'bruschetta',
  });

  // Open the save file dialog.
  await openEntryChoosingWindow({type: 'saveFile'});

  // Wait for the dialog to be fully loaded.
  const appId = await remoteCall.waitForWindow();
  await remoteCall.waitForElement(appId, '#file-list');
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type '(arg0: Object) => boolean | Object'.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Click the Guest OS placeholder.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.selectPlaceholderItemByType('bruschetta');

  // Wait for the directory scanning to finish to guarantee the FileWatcher call
  // is finished.
  await remoteCall.waitForElement(
      appId, `#list-container[scan-completed="Bluejohn"]`);

  // Wait for the actual volume to appear.
  await directoryTree.waitForItemByType('bruschetta');
};
