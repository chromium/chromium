// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TestEntryInfo} from '../test_util.js';
import {addEntries, ENTRIES, getCaller, openEntryChoosingWindow, pending, pollForChosenEntry, repeatUntil, sendBrowserTestCommand, sendTestMessage} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_LOCAL_ENTRY_SET} from './test_data.js';

/**
 * Sends a key event to an open file dialog, after selecting the file |name|
 * entry in the file list.
 *
 * @param name File name shown in the dialog.
 * @param key Key detail for fakeKeyDown event.
 * @param dialog ID of the file dialog window.
 * @return Promise to be fulfilled on success.
 */
async function sendOpenFileDialogKey(
    name: string, key: readonly[string, string, boolean, boolean, boolean],
    dialog: string): Promise<void> {
  await remoteCall.waitUntilSelected(dialog, name);
  await remoteCall.callRemoteTestUtil('fakeKeyDown', dialog, key);
}

/**
 * Clicks a button in the open file dialog, after selecting the file |name|
 * entry in the file list and checking that |button| exists.
 *
 * @param name File name shown in the dialog.
 * @param button Selector of the dialog button.
 * @param dialog ID of the file dialog window.
 * @return Promise to be fulfilled on success.
 */
async function clickOpenFileDialogButton(
    name: string, button: string, dialog: string): Promise<void> {
  await remoteCall.waitUntilSelected(dialog, name);
  await remoteCall.waitForElement(dialog, button);
  const event = [button, 'click'];
  await remoteCall.callRemoteTestUtil('fakeEvent', dialog, event);
}

/**
 * Sends an unload event to an open file dialog (after it is drawn) causing the
 * dialog to shut-down and close.
 *
 * @param dialog ID of the file dialog window.
 * @param element Element to query for drawing.
 * @return Promise to be fulfilled on success.
 */
async function unloadOpenFileDialog(
    dialog: string,
    element: string = '.button-panel button.ok'): Promise<void> {
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
 * @param volume Name of the volume.
 * @return Promise to resolve({Array<TestEntryInfo>}) on success, the Array
 *     being the basic file entry set of the |volume|.
 */
async function setUpFileEntrySet(volume: string): Promise<TestEntryInfo[]> {
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
 * Adds the basic file entry sets then opens the file dialog on the volume. Once
 * file |name| is shown, select it and click the Ok button.
 *
 * @param volume Volume name for  remoteCall.openAndWaitForClosingDialog.
 * @param name File name to select in the dialog.
 * @param useBrowserOpen Whether to launch the dialog from the browser.
 * @return Promise to be fulfilled on success.
 */
async function openFileDialogClickOkButton(
    volume: string, name: string,
    useBrowserOpen: boolean = false): Promise<unknown> {
  const okButton = '.button-panel button.ok:enabled';
  await sendTestMessage(
      {name: 'expectFileTask', fileNames: [name], openType: 'open'});
  const closer = clickOpenFileDialogButton.bind(null, name, okButton);

  const entrySet = await setUpFileEntrySet(volume);
  const result = await remoteCall.openAndWaitForClosingDialog(
                     {type: 'openFile'}, volume, entrySet, closer,
                     useBrowserOpen) as Entry;
  // If the file is opened via the filesystem API, check the name matches.
  // Otherwise, the caller is responsible for verifying the returned URL.
  if (!useBrowserOpen) {
    chrome.test.assertEq(name, result.name);
  }

  return result;
}

/**
 * Clicks the OK button in the provided dialog, expecting the provided `name` to
 * be passed into the `OnFilesImpl()` observer in the C++ test harness.
 *
 * @param appId App window Id.
 * @param name The (single) filename passed to the EXPECT_CALL when verifying
 *     the mocked OnFilesOpenedImpl().
 * @param openType Type of the dialog ('open' or 'saveAs').
 */
async function clickOkButtonExpectName(
    appId: string, name: string, openType: string) {
  await sendTestMessage({name: 'expectFileTask', fileNames: [name], openType});

  const okButton = '.button-panel button.ok:enabled';
  await remoteCall.waitAndClickElement(appId, okButton);
}

/**
 * Adds the basic file entry sets then opens the save file dialog on the volume.
 * Once file |name| is shown, select it and click the Ok button, again clicking
 * Ok in the confirmation dialog.
 *
 * @param volume Volume name for  remoteCall.openAndWaitForClosingDialog.
 * @param name File name to select in the dialog.
 * @return Promise to be fulfilled on success.
 */
async function saveFileDialogClickOkButton(
    volume: string, name: string): Promise<void> {
  const caller = getCaller();

  const closer = async (appId: string) => {
    await remoteCall.waitUntilSelected(appId, name);
    await repeatUntil(async () => {
      const element =
          await remoteCall.waitForElement(appId, '#filename-input-textbox');
      if (element.value !== name) {
        return pending(caller, 'Text field not updated');
      }
      return;
    });

    await clickOkButtonExpectName(appId, name, 'saveAs');

    const confirmOkButton = '.files-confirm-dialog .cr-dialog-ok';
    await remoteCall.waitForElement(appId, confirmOkButton);
    await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, [confirmOkButton, 'click']);
  };

  const entrySet = await setUpFileEntrySet(volume);
  const result =
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'saveFile'}, volume, entrySet, closer, false) as Entry;
  chrome.test.assertEq(name, result.name);
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume. Once
 * file |name| is shown, select it and verify that the Ok button is disabled.
 *
 * @param volume Volume name for  remoteCall.openAndWaitForClosingDialog.
 * @param name File name to select in the dialog where the OK button should be
 *     disabled
 * @param enabledName File name to select where the OK button should be enabled,
 *     used to ensure that switching to |name| results in the OK button becoming
 *     disabled.
 * @param type The dialog type to open.
 * @return Promise to be fulfilled on success.
 */
async function openFileDialogExpectOkButtonDisabled(
    volume: string, name: string, enabledName: string,
    type: 'openFile'|'saveFile' = 'openFile'): Promise<void> {
  const okButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';
  const closer = async (dialog: string) => {
    await remoteCall.waitUntilSelected(dialog, enabledName);
    await remoteCall.waitForElement(dialog, okButton);
    await remoteCall.waitUntilSelected(dialog, name);
    await remoteCall.waitForElement(dialog, disabledOkButton);
    clickOpenFileDialogButton(name, cancelButton, dialog);
  };

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      await remoteCall.openAndWaitForClosingDialog(
          {type}, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume. Once
 * file |name| is shown, verifies that it's dimmed according to added classes.
 *
 * @param volume Volume name for  remoteCall.openAndWaitForClosingDialog.
 * @param name File name to check for being dimmed in the dialog.
 * @return Promise to be fulfilled on success.
 */
async function openFileDialogExpectEntryDimmed(
    volume: string, name: string): Promise<void> {
  const type = 'openFile';
  const cancelButton = '.button-panel button.cancel';
  const fileEntry = `#file-list [file-name="${name}"]`;
  const closer = async (dialog: string) => {
    const element = await remoteCall.waitForElement(dialog, fileEntry);
    let dimmed = false;
    for (const className of element.attributes['class']!.split(' ')) {
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
      await remoteCall.openAndWaitForClosingDialog(
          {type}, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume. Once
 * file |name| is shown, select it and click the Cancel button.
 *
 * @param volume Volume name for  remoteCall.openAndWaitForClosingDialog.
 * @param name File name to select in the dialog.
 * @return Promise to be fulfilled on success.
 */
async function openFileDialogClickCancelButton(
    volume: string, name: string): Promise<void> {
  const cancelButton = '.button-panel button.cancel';
  const closer = clickOpenFileDialogButton.bind(null, name, cancelButton);

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'openFile'}, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume. Once
 * file |name| is shown, select it and send an Escape key.
 *
 * @param volume Volume name for  remoteCall.openAndWaitForClosingDialog.
 * @param name File name to select in the dialog.
 * @return Promise to be fulfilled on success.
 */
async function openFileDialogSendEscapeKey(
    volume: string, name: string): Promise<void> {
  const escapeKey = ['#file-list', 'Escape', false, false, false] as const;
  const closer = sendOpenFileDialogKey.bind(null, name, escapeKey);

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      await remoteCall.openAndWaitForClosingDialog(
          {type: 'openFile'}, volume, entrySet, closer));
}

/**
 * Tests for display:none status of feedback panels in Files app.
 *
 * @param type Type of dialog to open.
 */
async function checkFeedbackDisplayHidden(type: 'openFile'|'saveFile') {
  // Open dialog of the specified 'type'.
  await openEntryChoosingWindow({type});
  const appId = await remoteCall.waitForDialog();

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
  // Check the display style of the feedback panels container.
  const element = await remoteCall.waitForElementStyles(
      appId, ['.files-feedback-panels'], ['display']);
  // Check that CSS display style is 'none'.
  chrome.test.assertTrue(element.styles!['display'] === 'none');
}

/**
 * Test file present in Downloads.
 */
function getTestFileName(): string {
  // Type TestEntryInfo's targetPath can be undefined, but the first item
  // from BASIC_LOCAL_ENTRY_SET has value, we need to do type casting here.
  return BASIC_LOCAL_ENTRY_SET[0]!.targetPath;
}

/**
 * Tests opening file dialog on Downloads and closing it with Ok button.
 */
export async function openFileDialogDownloads() {
  return openFileDialogClickOkButton('downloads', getTestFileName());
}

/**
 * Tests opening file dialog sets aria-multiselect true on grid and list.
 */
export async function openFileDialogAriaMultipleSelect() {
  // Open File dialog.
  await openEntryChoosingWindow({type: 'openFile'});
  const appId = await remoteCall.waitForDialog();

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: <list> has aria-multiselect set to true.
  const list = 'list#file-list[aria-multiselectable=true]';
  await remoteCall.waitForElement(appId, list);

  // Check: <grid> has aria-multiselect set to true.
  const grid = 'grid#file-list[aria-multiselectable=true]';
  await remoteCall.waitForElement(appId, grid);
}

/**
 * Tests opening save file dialog sets aria-multiselect false on grid and list.
 */
export async function saveFileDialogAriaSingleSelect() {
  // Open Save as dialog.
  await openEntryChoosingWindow({type: 'saveFile'});
  const appId = await remoteCall.waitForDialog();

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: <list> has aria-multiselect set to false.
  const list = 'list#file-list[aria-multiselectable=false]';
  await remoteCall.waitForElement(appId, list);

  // Check: <grid> has aria-multiselect set to false.
  const grid = 'grid#file-list[aria-multiselectable=false]';
  await remoteCall.waitForElement(appId, grid);
}

/**
 * Tests opening save file dialog on Downloads and closing it with Ok button.
 */
export async function saveFileDialogDownloads() {
  return saveFileDialogClickOkButton('downloads', getTestFileName());
}

/**
 * Tests opening save file dialog on Downloads and using New Folder button.
 */
export async function saveFileDialogDownloadsNewFolderButton() {
  // Open Save as dialog.
  await openEntryChoosingWindow({type: 'saveFile'});
  const appId = await remoteCall.waitForDialog();

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: New Folder button should be enabled and click on it.
  const query = '#new-folder-button:not([disabled])';
  await remoteCall.waitAndClickElement(appId, query);

  // Wait for the new folder with input to appear, assume the rest of the
  // process works (covered by other tests).
  const textInput = '#file-list .table-row[renaming] input.rename';
  await remoteCall.waitForElement(appId, textInput);
}

/**
 * Tests opening file dialog on Downloads and closing it with Cancel button.
 */
export async function openFileDialogCancelDownloads() {
  return openFileDialogClickCancelButton('downloads', getTestFileName());
}

/**
 * Tests opening file dialog on Downloads and closing it with ESC key.
 */
export async function openFileDialogEscapeDownloads() {
  return openFileDialogSendEscapeKey('downloads', getTestFileName());
}

/**
 * Tests the feedback panels are hidden when using an open file dialog.
 */
export async function openFileDialogPanelsDisabled() {
  return checkFeedbackDisplayHidden('openFile');
}

/**
 * Tests the feedback panels are hidden when using a save file dialog.
 */
export async function saveFileDialogPanelsDisabled() {
  return checkFeedbackDisplayHidden('saveFile');
}

/**
 * Test file present in Drive only.
 */
const TEST_DRIVE_FILE = ENTRIES.hello.targetPath;

/**
 * Test file present in Drive only.
 */
const TEST_DRIVE_PINNED_FILE = ENTRIES.pinned.targetPath;

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
export async function openFileDialogDrive() {
  return openFileDialogClickOkButton('drive', TEST_DRIVE_FILE);
}

/**
 * Tests save file dialog on Drive and closing it with Ok button.
 */
export async function saveFileDialogDrive() {
  return saveFileDialogClickOkButton('drive', TEST_DRIVE_FILE);
}

/**
 * Tests that an unpinned file cannot be selected in file open dialogs while
 * offline.
 */
export async function openFileDialogDriveOffline() {
  return openFileDialogExpectOkButtonDisabled(
      'drive', TEST_DRIVE_FILE, TEST_DRIVE_PINNED_FILE);
}

/**
 * Tests that an unpinned file cannot be selected in save file dialogs while
 * offline.
 */
export async function saveFileDialogDriveOffline() {
  return openFileDialogExpectOkButtonDisabled(
      'drive', TEST_DRIVE_FILE, TEST_DRIVE_PINNED_FILE, 'saveFile');
}

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
export async function openFileDialogDriveOfflinePinned() {
  return openFileDialogClickOkButton('drive', TEST_DRIVE_PINNED_FILE);
}

/**
 * Tests save file dialog on Drive and closing it with Ok button.
 */
export async function saveFileDialogDriveOfflinePinned() {
  return saveFileDialogClickOkButton('drive', TEST_DRIVE_PINNED_FILE);
}

/**
 * Tests opening a file from Drive in the browser, ensuring it correctly opens
 * the file URL.
 */
export async function openFileDialogDriveFromBrowser() {
  const url = new URL(
      await openFileDialogClickOkButton('drive', TEST_DRIVE_FILE, true) as
      string);

  chrome.test.assertEq(url.protocol, 'file:');
  chrome.test.assertTrue(
      url.pathname.endsWith(`/root/${TEST_DRIVE_FILE}`), url.pathname);
}

/**
 * Tests opening a hosted doc in the browser, ensuring it correctly navigates to
 * the doc's URL.
 */
export async function openFileDialogDriveHostedDoc() {
  chrome.test.assertEq(
      await openFileDialogClickOkButton(
          'drive', ENTRIES.testDocument.nameText, true),
      'https://document_alternate_link/Test%20Document');
}

/**
 * Tests opening a hosted doc in the browser, ensuring it correctly navigates to
 * the doc's URL.
 */
export async function openFileDialogDriveEncryptedFile() {
  chrome.test.assertEq(
      await openFileDialogClickOkButton(
          'drive', ENTRIES.testCSEFile.nameText, true),
      'https://file_alternate_link/test-encrypted.txt');
}

/**
 * Tests that selecting a hosted doc from a dialog requiring a real file is
 * disabled.
 */
export async function openFileDialogDriveHostedNeedsFile() {
  return openFileDialogExpectOkButtonDisabled(
      'drive', ENTRIES.testDocument.nameText, TEST_DRIVE_FILE);
}

/**
 * Tests that selecting a hosted doc from a dialog requiring a real file is
 * disabled.
 */
export async function saveFileDialogDriveHostedNeedsFile() {
  return openFileDialogExpectOkButtonDisabled(
      'drive', ENTRIES.testDocument.nameText, TEST_DRIVE_FILE, 'saveFile');
}

/**
 * Test that an encrypted (via CSE) file will be marked as grey in a dialog
 * requiring a read file.
 */
export async function openFileDialogDriveCSEGrey() {
  return openFileDialogExpectEntryDimmed('drive', ENTRIES.testCSEFile.nameText);
}

/**
 * Tests that selecting an encrypted (via CSE) file from a dialog requiring a
 * real file is disabled.
 */
export async function openFileDialogDriveCSENeedsFile() {
  return openFileDialogExpectOkButtonDisabled(
      'drive', ENTRIES.testCSEFile.nameText, TEST_DRIVE_FILE);
}

/**
 * Tests opening file dialog on Drive and selecting an office file.
 */
export async function openFileDialogDriveOfficeFile() {
  return openFileDialogClickOkButton('drive', ENTRIES.docxFile.nameText);
}

/**
 * Tests opening file dialog on Drive and selecting multiple files including an
 * office file.
 */
export async function openMultiFileDialogDriveOfficeFile() {
  await setUpFileEntrySet('drive');
  await openEntryChoosingWindow({type: 'openFile', acceptsMultiple: true});
  const appId = await remoteCall.waitForDialog();

  // Wait for initial load to finish.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My Drive');

  // Sort the file names so we can compare the array directly with the entries
  // returned from pollForChosenEntry() without worrying about
  // order.
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

  const chosenEntries = ((await pollForChosenEntry(getCaller())) as Entry[])
                            .map(entry => entry.name)
                            .sort();
  chrome.test.assertEq(selectFileNames, chosenEntries);
}

/**
 * Tests opening file dialog on Drive and closing it with Cancel button.
 */
export async function openFileDialogCancelDrive() {
  return openFileDialogClickCancelButton('drive', TEST_DRIVE_FILE);
}

/**
 * Tests opening file dialog on Drive and closing it with ESC key.
 */
export async function openFileDialogEscapeDrive() {
  return openFileDialogSendEscapeKey('drive', TEST_DRIVE_FILE);
}

/**
 * Tests opening file dialog, then closing it with an 'unload' event.
 */
export async function openFileDialogUnload() {
  await openEntryChoosingWindow({type: 'openFile'});
  const dialog = await remoteCall.waitForDialog();
  await unloadOpenFileDialog(dialog);
}

/**
 * Tests that the open file dialog's filetype filter does not default to all
 * types.
 */
export async function openFileDialogDefaultFilter() {
  const params = {
    type: 'openFile' as const,
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: true,
  };
  await openEntryChoosingWindow(params);
  const dialog = await remoteCall.waitForDialog();

  // Check: 'JPEG image' should be selected.
  const selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option:checked');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
}

/**
 * Tests that the save file dialog's filetype filter defaults to all types.
 */
export async function saveFileDialogDefaultFilter() {
  const params = {
    type: 'saveFile' as const,
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: true,
  };
  await openEntryChoosingWindow(params);
  const dialog = await remoteCall.waitForDialog();

  // Check: 'All files' should be selected.
  const selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option:checked');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);
}

/**
 * Tests that the save file dialog's filetype filter can be navigated using the
 * keyboard.
 */
export async function saveFileDialogDefaultFilterKeyNavigation() {
  const params = {
    type: 'saveFile' as const,
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: true,
  };
  await openEntryChoosingWindow(params);
  const dialog = await remoteCall.waitForDialog();

  // Check: 'All files' should be selected.
  let selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: up key causes 'JPEG image' to  be selected.
  const selectControl = 'div.file-type';
  const arrowUpKey = ['ArrowUp', false, false, false] as const;
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowUpKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);

  // Check: down key causes 'All files' to be selected.
  const arrowDownKey = ['ArrowDown', false, false, false] as const;
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowDownKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: another down key doesn't wrap to the top selection.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowDownKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: left key acts like up when control is closed.
  const arrowLeftKey = ['ArrowLeft', false, false, false] as const;
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowLeftKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);

  // Check: right key acts like down when control is closed.
  const arrowRightKey = ['ArrowRight', false, false, false] as const;
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowRightKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: Enter key expands the select control.
  const enterKey = ['Enter', false, false, false] as const;
  await remoteCall.fakeKeyDown(dialog, selectControl, ...enterKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: second Enter key collapses the select control.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...enterKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: space key expands the select control.
  const spaceKey = [' ', false, false, false] as const;
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: second space key collapses the select control.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: Escape key collapses the select control.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  const escapeKey = ['Escape', false, false, false] as const;
  await remoteCall.fakeKeyDown(dialog, selectControl, ...escapeKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: tab key collapses the select control.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  const tabKey = ['Tab', false, false, false] as const;
  await remoteCall.fakeKeyDown(dialog, selectControl, ...tabKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');

  // Check: tab key collapsing remembers changed selection.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowUpKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
  await remoteCall.fakeKeyDown(dialog, selectControl, ...tabKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);

  // Check: Escape key collapsing remembers changed selection.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowDownKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);
  await remoteCall.fakeKeyDown(dialog, selectControl, ...escapeKey);
  await remoteCall.waitForElementLost(
      dialog, '.file-type div.options[expanded=expanded]');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: left arrow does nothing with control expanded.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...spaceKey);
  await remoteCall.waitForElement(
      dialog, '.file-type div.options[expanded=expanded]');
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowLeftKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);

  // Check: right arrow does nothing with control expanded.
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowUpKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
  await remoteCall.fakeKeyDown(dialog, selectControl, ...arrowRightKey);
  selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option.selected');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
}

/**
 * Tests that filtering works with { acceptsAllTypes: false } and a single
 * filter. Regression test for https://crbug.com/1097448.
 */
export async function saveFileDialogSingleFilterNoAcceptAll() {
  const params = {
    type: 'saveFile' as const,
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: false,
  };
  await openEntryChoosingWindow(params);
  const dialog = await remoteCall.waitForDialog();

  // Check: 'JPEG image' should be selected.
  const selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option:checked');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
}

/**
 * Opens a "Save As" dialog and clicks OK. Helper for the
 * saveFileDialogExtension* tests.
 *
 * @param extraParams Extra options to pass to chooseEntry().
 * @param expectName Name for the 'expectFileTask' mock expectation.
 * @return The name of the entry from chooseEntry().
 */
async function showSaveAndConfirmExpecting(
    extraParams: chrome.fileSystem.ChooseEntryOptions,
    expectName: string): Promise<string> {
  const caller = getCaller();

  const params = {
    type: 'saveFile' as const,
    accepts: [{extensions: ['jpg']}],
  };
  await openEntryChoosingWindow(Object.assign(params, extraParams));
  const dialog = await remoteCall.waitForDialog();

  // Ensure the input field is ready.
  await remoteCall.waitForElement(dialog, '#filename-input-textbox');

  await clickOkButtonExpectName(dialog, expectName, 'saveAs');
  const entry = await pollForChosenEntry(caller) as Entry;
  return entry.name;
}

/**
 * Tests that a file extension is not automatically added upon confirmation
 * whilst the "All Files" filter is selected on the "Save As" dialog. Note the
 * saveFileDialogDefaultFilter test above verifies that 'All Files' is actually
 * the default in this setup.
 */
export async function saveFileDialogExtensionNotAddedWithNoFilter() {
  // Note these tests use the suggestedName field as a robust way to simulate a
  // user typing into the input field.
  const extraParams = {acceptsAllTypes: true, suggestedName: 'test'};
  const name = await showSaveAndConfirmExpecting(extraParams, 'test');
  chrome.test.assertEq('test', name);
}

/**
 * With no "All Files" option, the JPEG filter should be applied by default, and
 * a ".jpg" extension automatically added on confirm.
 */
export async function saveFileDialogExtensionAddedWithJpegFilter() {
  const extraParams = {acceptsAllTypes: false, suggestedName: 'test'};
  const name = await showSaveAndConfirmExpecting(extraParams, 'test.jpg');
  chrome.test.assertEq('test.jpg', name);
}

/**
 * An extension should only be added if the user didn't provide one, even if it
 * doesn't match the current filter for JPEG files (i.e. /\.(jpg)$/i).
 */
export async function saveFileDialogExtensionNotAddedWhenProvided() {
  const extraParams = {acceptsAllTypes: false, suggestedName: 'foo.png'};
  const name = await showSaveAndConfirmExpecting(extraParams, 'foo.png');
  chrome.test.assertEq('foo.png', name);
}

/**
 * Tests that context menu on File List for file picker dialog.
 * File picker dialog displays fewer menu options than full Files app. For
 * example copy/paste commands are disabled. Right-click on a file/folder should
 * show context menu, whereas right-clicking on the blank parts of file list
 * should NOT display the context menu.
 *
 * crbug.com/917975 crbug.com/983507.
 */
export async function openFileDialogFileListShowContextMenu() {
  // Make sure the file picker will open to Downloads.
  sendBrowserTestCommand({name: 'setLastDownloadDir'});

  // Add entries to Downloads.
  await addEntries(['local'], BASIC_LOCAL_ENTRY_SET);

  // Open file picker dialog.
  await openEntryChoosingWindow({type: 'openFile'});
  const appId = await remoteCall.waitForDialog();

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Wait for files to be displayed.
  const expectedRows = [
    ['Play files', '--', 'Folder'],
    ['Downloads', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Navigate to Downloads folder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Downloads');

  // Right-click "photos" folder to show context menu.
  await remoteCall.waitAndRightClick(appId, '#file-list [file-name="photos"]');

  // Wait until the context menu appears.
  const menuVisible = '#file-context-menu:not([hidden])';
  await remoteCall.waitForElement(appId, menuVisible);

  // Dismiss context menu.
  const escKey = ['Escape', false, false, false] as const;
  await remoteCall.fakeKeyDown(appId, menuVisible, ...escKey);
  await remoteCall.waitForElementLost(appId, menuVisible);

  // Right-click inside of #file-list (in an empty space).
  await remoteCall.rightClickFileListBlankSpace(appId);

  // Check that context menu is NOT displayed because there is no visible menu
  // items.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');
}

/**
 * Tests that select all is disabled in the gear menu for an open file dialog.
 */
export async function openFileDialogSelectAllDisabled() {
  // Open file picker dialog.
  await openEntryChoosingWindow({type: 'openFile'});
  const appId = await remoteCall.waitForDialog();

  // Wait to finish initial load.
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
}

/**
 * Tests that select all is enabled in the gear menu for an open multiple files
 * dialog. crbug.com/937251
 */
export async function openMultiFileDialogSelectAllEnabled() {
  // Make sure the file picker will open to Downloads.
  sendBrowserTestCommand({name: 'setLastDownloadDir'});

  // Open file picker dialog with support for selecting multiple files.
  await openEntryChoosingWindow({type: 'openFile', acceptsMultiple: true});
  const appId = await remoteCall.waitForDialog();

  // Wait to finish initial load.
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
}

/**
 * Tests open file dialog on a GuestOS volume. Check that the placeholder is
 * shown in the dialog and that clicking on it mounts the volume. We don't
 * bother actually opening a file since once it's mounted it works like any
 * other local FUSE volume.
 */
export async function openFileDialogGuestOs() {
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
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Click the Guest OS placeholder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectPlaceholderItemByType('bruschetta');

  // Wait for the directory scanning to finish to guarantee the FileWatcher call
  // is finished.
  await remoteCall.waitForElement(
      appId, `#list-container[scan-completed="Bluejohn"]`);

  // Wait for the actual volume to appear.
  await directoryTree.waitForItemByType('bruschetta');
}

/**
 * Tests save file dialog on a GuestOS volume. Check that the placeholder is
 * shown in the dialog and that clicking on it mounts the volume. We don't
 * bother actually saving a file since once it's mounted it works like any other
 * local FUSE volume.
 */
export async function saveFileDialogGuestOs() {
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
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Click the Guest OS placeholder.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectPlaceholderItemByType('bruschetta');

  // Wait for the directory scanning to finish to guarantee the FileWatcher call
  // is finished.
  await remoteCall.waitForElement(
      appId, `#list-container[scan-completed="Bluejohn"]`);

  // Wait for the actual volume to appear.
  await directoryTree.waitForItemByType('bruschetta');
}
