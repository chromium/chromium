// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Sends a key event to an open file dialog, after selecting the file |name|
 * entry in the file list.
 *
 * @param {!string} name File name shown in the dialog.
 * @param {!Array} key Key detail for fakeKeyDown event.
 * @param {!string} dialog ID of the file dialog window.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function sendOpenFileDialogKey(name, key, dialog) {
  await remoteCall.callRemoteTestUtil('selectFile', dialog, [name]);
  await remoteCall.callRemoteTestUtil('fakeKeyDown', dialog, key);
}

/**
 * Clicks a button in the open file dialog, after selecting the file |name|
 * entry in the file list and checking that |button| exists.
 *
 * @param {!string} name File name shown in the dialog.
 * @param {!string} button Selector of the dialog button.
 * @param {!string} dialog ID of the file dialog window.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function clickOpenFileDialogButton(name, button, dialog) {
  await remoteCall.callRemoteTestUtil('selectFile', dialog, [name]);
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
 * @return {!Promise} Promise to be fulfilled on success.
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
 * @return {!Promise} Promise to resolve({Array<TestEntryInfo>}) on success,
 *    the Array being the basic file entry set of the |volume|.
 */
async function setUpFileEntrySet(volume) {
  let localEntryPromise = addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
  let driveEntryPromise = addEntries(
      ['drive'], [ENTRIES.hello, ENTRIES.pinned, ENTRIES.testDocument]);

  await Promise.all([localEntryPromise, driveEntryPromise]);
  if (volume == 'drive') {
    return [ENTRIES.hello, ENTRIES.pinned, ENTRIES.testDocument];
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
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function openFileDialogClickOkButton(
    volume, name, useBrowserOpen = false) {
  const okButton = '.button-panel button.ok:enabled';
  await sendTestMessage(
      {name: 'expectFileTask', fileNames: [name], openType: 'open'});
  let closer = clickOpenFileDialogButton.bind(null, name, okButton);

  const entrySet = await setUpFileEntrySet(volume);
  const result = await openAndWaitForClosingDialog(
      {type: 'openFile'}, volume, entrySet, closer, useBrowserOpen);
  // If the file is opened via the filesystem API, check the name matches.
  // Otherwise, the caller is responsible for verifying the returned URL.
  if (!useBrowserOpen) {
    chrome.test.assertEq(name, result.name);
  }
  return result;
}

/**
 * Adds the basic file entry sets then opens the save file dialog on the volume.
 * Once file |name| is shown, select it and click the Ok button, again clicking
 * Ok in the confirmation dialog.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function saveFileDialogClickOkButton(volume, name) {
  const caller = getCaller();

  await sendTestMessage(
      {name: 'expectFileTask', fileNames: [name], openType: 'saveAs'});

  let closer = async (appId) => {
    const okButton = '.button-panel button.ok:enabled';

    await remoteCall.callRemoteTestUtil('selectFile', appId, [name]);
    await repeatUntil(async () => {
      const element =
          await remoteCall.waitForElement(appId, '#filename-input-textbox');
      if (element.value !== name) {
        return pending(caller, 'Text field not updated');
      }
    });

    await remoteCall.waitForElement(appId, okButton);
    const event = [okButton, 'click'];
    await remoteCall.callRemoteTestUtil('fakeEvent', appId, event);

    const confirmOkButton = '.files-confirm-dialog .cr-dialog-ok';
    await remoteCall.waitForElement(appId, confirmOkButton);
    await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, [confirmOkButton, 'click']);
  };

  const entrySet = await setUpFileEntrySet(volume);
  const result = await openAndWaitForClosingDialog(
      {type: 'saveFile'}, volume, entrySet, closer, false);
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
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function openFileDialogExpectOkButtonDisabled(
    volume, name, enabledName, type = 'openFile') {
  const okButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';
  let closer = async (dialog) => {
    await remoteCall.callRemoteTestUtil('selectFile', dialog, [enabledName]);
    await remoteCall.waitForElement(dialog, okButton);
    await remoteCall.callRemoteTestUtil('selectFile', dialog, [name]);
    await remoteCall.waitForElement(dialog, disabledOkButton);
    clickOpenFileDialogButton(name, cancelButton, dialog);
  };

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog({type}, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and click the Cancel button.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function openFileDialogClickCancelButton(volume, name) {
  const type = {type: 'openFile'};

  const cancelButton = '.button-panel button.cancel';
  let closer = clickOpenFileDialogButton.bind(null, name, cancelButton);

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(type, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and send an Escape key.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function openFileDialogSendEscapeKey(volume, name) {
  const type = {type: 'openFile'};

  const escapeKey = ['#file-list', 'Escape', false, false, false];
  let closer = sendOpenFileDialogKey.bind(null, name, escapeKey);

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(type, volume, entrySet, closer));
}

/**
 * Tests for display:none status of feedback panels in Files app.
 *
 * @param {string} type Type of dialog to open.
 */
async function checkFeedbackDisplayHidden(type) {
  // Open dialog of the specified 'type'.
  chrome.fileSystem.chooseEntry({type: type}, (entry) => {});
  const appId = await remoteCall.waitForWindow('dialog#');

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
  // Check the display style of the feedback panels container.
  const element = await remoteCall.waitForElementStyles(
      appId, ['.files-feedback-panels'], ['display']);
  // Check that CSS display style is 'none'.
  chrome.test.assertTrue(element.styles['display'] === 'none');
}

/**
 * Test file present in Downloads.
 * @{!string}
 */
const TEST_LOCAL_FILE = BASIC_LOCAL_ENTRY_SET[0].targetPath;

/**
 * Tests opening file dialog on Downloads and closing it with Ok button.
 */
testcase.openFileDialogDownloads = () => {
  return openFileDialogClickOkButton('downloads', TEST_LOCAL_FILE);
};

/**
 * Tests opening save file dialog on Downloads and closing it with Ok button.
 */
testcase.saveFileDialogDownloads = () => {
  return saveFileDialogClickOkButton('downloads', TEST_LOCAL_FILE);
};

/**
 * Tests opening save file dialog on Downloads and using New Folder button.
 */
testcase.saveFileDialogDownloadsNewFolderButton = async () => {
  // Open Save as dialog.
  chrome.fileSystem.chooseEntry({type: 'saveFile'}, (entry) => {});
  const appId = await remoteCall.waitForWindow('dialog#');

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: New Folder button should be enabled and click on it.
  const query = '#new-folder-button:not([disabled])';
  const newFolderButton = await remoteCall.waitAndClickElement(appId, query);

  // Wait for the new folder with input to appear, assume the rest of the
  // process works (covered by other tests).
  const textInput = '#file-list .table-row[renaming] input.rename';
  await remoteCall.waitForElement(appId, textInput);
};

/**
 * Tests opening file dialog on Downloads and closing it with Cancel button.
 */
testcase.openFileDialogCancelDownloads = () => {
  return openFileDialogClickCancelButton('downloads', TEST_LOCAL_FILE);
};

/**
 * Tests opening file dialog on Downloads and closing it with ESC key.
 */
testcase.openFileDialogEscapeDownloads = () => {
  return openFileDialogSendEscapeKey('downloads', TEST_LOCAL_FILE);
};

/**
 * Tests the feedback panels are hidden when using an open file dialog.
 */
testcase.openFileDialogPanelsDisabled = () => {
  return checkFeedbackDisplayHidden('openFile');
};

/**
 * Tests the feedback panels are hidden when using a save file dialog.
 */
testcase.saveFileDialogPanelsDisabled = () => {
  return checkFeedbackDisplayHidden('saveFile');
};

/**
 * Test file present in Drive only.
 * @{!string}
 */
const TEST_DRIVE_FILE = ENTRIES.hello.targetPath;

/**
 * Test file present in Drive only.
 * @{!string}
 */
const TEST_DRIVE_PINNED_FILE = ENTRIES.pinned.targetPath;

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
testcase.openFileDialogDrive = () => {
  return openFileDialogClickOkButton('drive', TEST_DRIVE_FILE);
};

/**
 * Tests save file dialog on Drive and closing it with Ok button.
 */
testcase.saveFileDialogDrive = () => {
  return saveFileDialogClickOkButton('drive', TEST_DRIVE_FILE);
};

/**
 * Tests that an unpinned file cannot be selected in file open dialogs while
 * offline.
 */
testcase.openFileDialogDriveOffline = () => {
  return openFileDialogExpectOkButtonDisabled(
      'drive', TEST_DRIVE_FILE, TEST_DRIVE_PINNED_FILE);
};

/**
 * Tests that an unpinned file cannot be selected in save file dialogs while
 * offline.
 */
testcase.saveFileDialogDriveOffline = () => {
  return openFileDialogExpectOkButtonDisabled(
      'drive', TEST_DRIVE_FILE, TEST_DRIVE_PINNED_FILE, 'saveFile');
};

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
testcase.openFileDialogDriveOfflinePinned = () => {
  return openFileDialogClickOkButton('drive', TEST_DRIVE_PINNED_FILE);
};

/**
 * Tests save file dialog on Drive and closing it with Ok button.
 */
testcase.saveFileDialogDriveOfflinePinned = () => {
  return saveFileDialogClickOkButton('drive', TEST_DRIVE_PINNED_FILE);
};

/**
 * Tests opening a file from Drive in the browser, ensuring it correctly
 * opens the file URL.
 */
testcase.openFileDialogDriveFromBrowser = async () => {
  const url = new URL(
      await openFileDialogClickOkButton('drive', TEST_DRIVE_FILE, true));

  chrome.test.assertEq(url.protocol, 'file:');
  chrome.test.assertTrue(
      url.pathname.endsWith(`/root/${TEST_DRIVE_FILE}`), url.pathname);
};

/**
 * Tests opening a hosted doc in the browser, ensuring it correctly navigates to
 * the doc's URL.
 */
testcase.openFileDialogDriveHostedDoc = async () => {
  chrome.test.assertEq(
      await openFileDialogClickOkButton(
          'drive', ENTRIES.testDocument.nameText, true),
      'https://document_alternate_link/Test%20Document');
};

/**
 * Tests that selecting a hosted doc from a dialog requiring a real file is
 * disabled.
 */
testcase.openFileDialogDriveHostedNeedsFile = () => {
  return openFileDialogExpectOkButtonDisabled(
      'drive', ENTRIES.testDocument.nameText, TEST_DRIVE_FILE);
};

/**
 * Tests that selecting a hosted doc from a dialog requiring a real file is
 * disabled.
 */
testcase.saveFileDialogDriveHostedNeedsFile = () => {
  return openFileDialogExpectOkButtonDisabled(
      'drive', ENTRIES.testDocument.nameText, TEST_DRIVE_FILE, 'saveFile');
};

/**
 * Tests opening file dialog on Drive and closing it with Cancel button.
 */
testcase.openFileDialogCancelDrive = () => {
  return openFileDialogClickCancelButton('drive', TEST_DRIVE_FILE);
};

/**
 * Tests opening file dialog on Drive and closing it with ESC key.
 */
testcase.openFileDialogEscapeDrive = () => {
  return openFileDialogSendEscapeKey('drive', TEST_DRIVE_FILE);
};

/**
 * Tests opening file dialog, then closing it with an 'unload' event.
 */
testcase.openFileDialogUnload = async () => {
  chrome.fileSystem.chooseEntry({type: 'openFile'}, (entry) => {});
  const dialog = await remoteCall.waitForWindow('dialog#');
  await unloadOpenFileDialog(dialog);
};

/**
 * Tests that the open file dialog's filetype filter does not default to all
 * types.
 */
testcase.openFileDialogDefaultFilter = async () => {
  const params = {
    type: 'openFile',
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: true,
  };
  chrome.fileSystem.chooseEntry(params, (entry) => {});
  const dialog = await remoteCall.waitForWindow('dialog#');

  // Check: 'JPEG image' should be selected.
  const selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option:checked');
  chrome.test.assertEq('1', selectedFilter.value);
  chrome.test.assertEq('JPEG image', selectedFilter.text);
};

/**
 * Tests that the save file dialog's filetype filter defaults to all types.
 */
testcase.saveFileDialogDefaultFilter = async () => {
  const params = {
    type: 'saveFile',
    accepts: [{extensions: ['jpg']}],
    acceptsAllTypes: true,
  };
  chrome.fileSystem.chooseEntry(params, (entry) => {});
  const dialog = await remoteCall.waitForWindow('dialog#');

  // Check: 'All files' should be selected.
  const selectedFilter =
      await remoteCall.waitForElement(dialog, '.file-type option:checked');
  chrome.test.assertEq('0', selectedFilter.value);
  chrome.test.assertEq('All files', selectedFilter.text);
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
testcase.openFileDialogFileListShowContextMenu = async () => {
  // Add entries to Downloads.
  await addEntries(['local'], BASIC_LOCAL_ENTRY_SET);

  // Open file picker dialog.
  chrome.fileSystem.chooseEntry({type: 'openFile'}, (entry) => {});
  const appId = await remoteCall.waitForWindow('dialog#');

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
  await remoteCall.navigateWithDirectoryTree(appId, '/Downloads', 'My files');

  // Right-click "photos" folder to show context menu.
  await remoteCall.waitAndRightClick(appId, '#file-list [file-name="photos"]');

  // Wait until the context menu appears.
  const menuVisible = '#file-context-menu:not([hidden])';
  await remoteCall.waitForElement(appId, menuVisible);

  // Dismiss context menu.
  const escKey = ['Escape', false, false, false];
  await remoteCall.fakeKeyDown(appId, menuVisible, ...escKey);
  await remoteCall.waitForElementLost(appId, menuVisible);

  // Right-click 100px inside of #file-list (in an empty space).
  const offsetBottom = -100;
  const offsetRight = -100;
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'rightClickOffset', appId, ['#file-list', offsetBottom, offsetRight]),
      'right click failed');

  // Check that context menu is NOT displayed because there is no visible menu
  // items.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');
};
