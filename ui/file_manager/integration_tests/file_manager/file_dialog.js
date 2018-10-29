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
function sendOpenFileDialogKey(name, key, dialog) {
  return remoteCall.callRemoteTestUtil('selectFile', dialog, [name])
      .then(function sendDialogKeyEvent() {
        return remoteCall.callRemoteTestUtil('fakeKeyDown', dialog, key);
      });
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
function clickOpenFileDialogButton(name, button, dialog) {
  return remoteCall.callRemoteTestUtil('selectFile', dialog, [name])
      .then(function awaitDialogButton() {
        return remoteCall.waitForElement(dialog, button);
      })
      .then(function sendButtonClickEvent() {
        const event = [button, 'click'];
        return remoteCall.callRemoteTestUtil('fakeEvent', dialog, event);
      });
}

/**
 * Sends an unload event to an open file dialog (after it is drawn) causing
 * the dialog to shut-down and close.
 *
 * @param {!string} dialog ID of the file dialog window.
 * @param {string} element Element to query for drawing.
 * @return {!Promise} Promise to be fulfilled on success.
 */
function unloadOpenFileDialog(dialog, element = '.button-panel button.ok') {
  return remoteCall.waitForElement(dialog, element).then(() => {
    return remoteCall.callRemoteTestUtil('unload', dialog, [])
        .then(function getDialogErrorCount() {
          return remoteCall.callRemoteTestUtil('getErrorCount', dialog, []);
        })
        .then(function expectNoDialogErrors(errorCount) {
          chrome.test.assertEq(0, errorCount);
        });
  });
}

/**
 * Adds basic file entry sets for both 'local' and 'drive', and returns the
 * entry set of the given |volume|.
 *
 * @param {!string} volume Name of the volume.
 * @return {!Promise} Promise to resolve({Array<TestEntryInfo>}) on success,
 *    the Array being the basic file entry set of the |volume|.
 */
function createFileEntryPromise(volume) {
  let localEntryPromise = addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
  let driveEntryPromise =
      addEntries(['drive'], [ENTRIES.hello, ENTRIES.pinned]);

  return Promise.all([localEntryPromise, driveEntryPromise])
      .then(function returnVolumeEntrySet() {
        if (volume == 'drive')
          return [ENTRIES.hello, ENTRIES.pinned];
        return BASIC_LOCAL_ENTRY_SET;
      });
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and click the Ok button.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
function openFileDialogClickOkButton(volume, name) {
  const type = {type: 'openFile'};

  const okButton = '.button-panel button.ok:enabled';
  let closer = clickOpenFileDialogButton.bind(null, name, okButton);

  return createFileEntryPromise(volume)
      .then(function openFileDialog(entrySet) {
        return openAndWaitForClosingDialog(type, volume, entrySet, closer);
      })
      .then(function expectDialogFileEntry(result) {
        chrome.test.assertEq(name, result.name);
      });
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and verify that the Ok button is
 * disabled.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
function openFileDialogExpectOkButtonDisabled(volume, name, pinnedName) {
  const type = {type: 'openFile'};

  const okButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';
  let closer = dialog => {
    return remoteCall.callRemoteTestUtil('selectFile', dialog, [pinnedName])
        .then(function awaitDialogButton() {
          return remoteCall.waitForElement(dialog, okButton);
        })
        .then(() => remoteCall.callRemoteTestUtil('selectFile', dialog, [name]))
        .then(function awaitDialogButton() {
          return remoteCall.waitForElement(dialog, disabledOkButton);
        })
        .then(() => {
          clickOpenFileDialogButton(name, cancelButton, dialog);
        });
  };

  return createFileEntryPromise(volume)
      .then(function openFileDialog(entrySet) {
        return openAndWaitForClosingDialog(type, volume, entrySet, closer);
      })
      .then(function expectDialogFileEntry(result) {
        chrome.test.assertEq(undefined, result);
      });
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and click the Cancel button.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
function openFileDialogClickCancelButton(volume, name) {
  const type = {type: 'openFile'};

  const cancelButton = '.button-panel button.cancel';
  let closer = clickOpenFileDialogButton.bind(null, name, cancelButton);

  return createFileEntryPromise(volume)
      .then(function openFileDialog(entrySet) {
        return openAndWaitForClosingDialog(type, volume, entrySet, closer);
      })
      .then(function expectDialogCancelled(result) {
        chrome.test.assertEq(undefined, result);
      });
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and send an Escape key.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
function openFileDialogSendEscapeKey(volume, name) {
  const type = {type: 'openFile'};

  const escapeKey = ['#file-list', 'Escape', false, false, false];
  let closer = sendOpenFileDialogKey.bind(null, name, escapeKey);

  return createFileEntryPromise(volume)
      .then(function openFileDialog(entrySet) {
        return openAndWaitForClosingDialog(type, volume, entrySet, closer);
      })
      .then(function expectDialogCancelled(result) {
        chrome.test.assertEq(undefined, result);
      });
}

/**
 * Test file present in Downloads.
 * @{!string}
 */
const TEST_LOCAL_FILE = BASIC_LOCAL_ENTRY_SET[0].targetPath;

/**
 * Tests opening file dialog on Downloads and closing it with Ok button.
 */
testcase.openFileDialogDownloads = function() {
  testPromise(openFileDialogClickOkButton('downloads', TEST_LOCAL_FILE));
};

/**
 * Tests opening file dialog on Downloads and closing it with Cancel button.
 */
testcase.openFileDialogCancelDownloads = function() {
  testPromise(openFileDialogClickCancelButton('downloads', TEST_LOCAL_FILE));
};

/**
 * Tests opening file dialog on Downloads and closing it with ESC key.
 */
testcase.openFileDialogEscapeDownloads = function() {
  testPromise(openFileDialogSendEscapeKey('downloads', TEST_LOCAL_FILE));
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
testcase.openFileDialogDrive = function() {
  testPromise(openFileDialogClickOkButton('drive', TEST_DRIVE_FILE));
};

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
testcase.openFileDialogDriveOffline = function() {
  testPromise(openFileDialogExpectOkButtonDisabled(
      'drive', TEST_DRIVE_FILE, TEST_DRIVE_PINNED_FILE));
};

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
testcase.openFileDialogDriveOfflinePinned = function() {
  testPromise(openFileDialogClickOkButton('drive', TEST_DRIVE_PINNED_FILE));
};

/**
 * Tests opening file dialog on Drive and closing it with Cancel button.
 */
testcase.openFileDialogCancelDrive = function() {
  testPromise(openFileDialogClickCancelButton('drive', TEST_DRIVE_FILE));
};

/**
 * Tests opening file dialog on Drive and closing it with ESC key.
 */
testcase.openFileDialogEscapeDrive = function() {
  testPromise(openFileDialogSendEscapeKey('drive', TEST_DRIVE_FILE));
};

/**
 * Tests opening file dialog, then closing it with an 'unload' event.
 */
testcase.openFileDialogUnload = function() {
  chrome.fileSystem.chooseEntry({type: 'openFile'}, (entry) => {});

  testPromise(remoteCall.waitForWindow('dialog#').then((dialog) => {
    return unloadOpenFileDialog(dialog);
  }));
};
