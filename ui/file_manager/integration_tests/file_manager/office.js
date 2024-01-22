// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {FILE_MANAGER_SWA_APP_ID, FILE_SWA_BASE_URL} from './test_data.js';

/**
 * Returns 'Open in Google Docs' task descriptor.
 *
 * @return {!FileTaskDescriptor}
 */
function openDocWithDriveDescriptor() {
  const filesAppId = FILE_MANAGER_SWA_APP_ID;
  const filesTaskType = 'web';
  const actionId = `${FILE_SWA_BASE_URL}?open-web-drive-office-word`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}

/**
 * Returns 'Open with Excel' task descriptor.
 *
 * @return {!FileTaskDescriptor}
 */
function openExcelWithDriveDescriptor() {
  const filesAppId = FILE_MANAGER_SWA_APP_ID;
  const filesTaskType = 'web';
  const actionId = `${FILE_SWA_BASE_URL}?open-web-drive-office-excel`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}

/**
 * Returns 'Open in PowerPoint' task descriptor.
 *
 * @return {!FileTaskDescriptor}
 */
function openPowerPointWithDriveDescriptor() {
  const filesAppId = FILE_MANAGER_SWA_APP_ID;
  const filesTaskType = 'web';
  const actionId = `${FILE_SWA_BASE_URL}?open-web-drive-office-powerpoint`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}


/**
 * Waits for the expected number of tasks executions, and returns the descriptor
 * of the last executed task.
 *
 * @param {string} appId Window ID.
 * @param {number} expectedCount
 * @return {!Promise<!FileTaskDescriptor>}
 */
async function getExecutedTask(appId, expectedCount = 1) {
  const caller = getCaller();

  // Wait until a task has been executed.
  await repeatUntil(async () => {
    const executeTaskCount = await remoteCall.callRemoteTestUtil(
        'staticFakeCounter', appId, ['chrome.fileManagerPrivate.executeTask']);
    if (executeTaskCount === expectedCount) {
      return true;
    }
    chrome.test.assertTrue(executeTaskCount < expectedCount);
    return pending(caller, 'Waiting for a task to execute');
  });

  // Arguments provided for the last call to executeTask().
  const executeTaskArgs = (await remoteCall.callRemoteTestUtil(
      'staticFakeCalledArgs', appId,
      ['chrome.fileManagerPrivate.executeTask']))[expectedCount - 1];

  // The task descriptor is the first argument.
  return executeTaskArgs[0];
}

// @ts-ignore: error TS4111: Property 'openOfficeWordFile' comes from an index
// signature, so it must be accessed with ['openOfficeWordFile'].
testcase.openOfficeWordFile = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    // @ts-ignore: error TS4111: Property 'smallDocxHosted' comes from an index
    // signature, so it must be accessed with ['smallDocxHosted'].
    fileNames: [ENTRIES.smallDocxHosted.targetPath],
    openType: 'launch',
  });

  const appId = await setupAndWaitUntilReady(
      // @ts-ignore: error TS4111: Property 'smallDocxHosted' comes from an
      // index signature, so it must be accessed with ['smallDocxHosted'].
      RootPath.DRIVE, [], [ENTRIES.smallDocxHosted]);

  // Disable office setup flow so the dialog doesn't open when the file is
  // opened.
  await sendTestMessage({name: 'setOfficeFileHandler'});

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      // @ts-ignore: error TS4111: Property 'smallDocxHosted' comes from an
      // index signature, so it must be accessed with ['smallDocxHosted'].
      'openFile', appId, [ENTRIES.smallDocxHosted.nameText]));

  // Check that the Word file's alternate URL has been opened in a browser
  // window. The query parameter is concatenated to the URL as office files
  // opened from drive have this query parameter added
  // (https://crrev.com/c/3867338).
  await remoteCall.waitForLastOpenedBrowserTabUrl(
      // @ts-ignore: error TS4111: Property 'smallDocxHosted' comes from an
      // index signature, so it must be accessed with ['smallDocxHosted'].
      ENTRIES.smallDocxHosted.alternateUrl.concat('&cros_files=true'));
};

// @ts-ignore: error TS4111: Property 'openOfficeWordFromMyFiles' comes from an
// index signature, so it must be accessed with ['openOfficeWordFromMyFiles'].
testcase.openOfficeWordFromMyFiles = async () => {
  const appId =
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallDocx]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.EMPTY.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['empty']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // The available Office task should be "Upload to Drive".
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// Tests that "Upload to Drive" cannot be enabled if the "Upload Office To
// Cloud" flag is disabled (test setup similar to `openOfficeWordFromMyFiles`).
// @ts-ignore: error TS4111: Property
// 'uploadToDriveRequiresUploadOfficeToCloudEnabled' comes from an index
// signature, so it must be accessed with
// ['uploadToDriveRequiresUploadOfficeToCloudEnabled'].
testcase.uploadToDriveRequiresUploadOfficeToCloudEnabled = async () => {
  const appId =
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallDocx]);
  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.EMPTY.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['empty']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // Since the Upload Office To Cloud flag isn't enabled, the Upload to Drive
  // task should not be available: another task should have been executed
  // instead (QuickOffice or generic task).
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertFalse(
      taskDescriptor.actionId === openDocWithDriveDescriptor().actionId);
  chrome.test.assertFalse(
      taskDescriptor.actionId === openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// @ts-ignore: error TS4111: Property 'openOfficeWordFromDrive' comes from an
// index signature, so it must be accessed with ['openOfficeWordFromDrive'].
testcase.openOfficeWordFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      // @ts-ignore: error TS4111: Property 'smallDocxHosted' comes from an
      // index signature, so it must be accessed with ['smallDocxHosted'].
      RootPath.DRIVE, [], [ENTRIES.smallDocxHosted]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      // @ts-ignore: error TS4111: Property 'smallDocxHosted' comes from an
      // index signature, so it must be accessed with ['smallDocxHosted'].
      'openFile', appId, [ENTRIES.smallDocxHosted.nameText]));

  // The Drive/Docs task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(openDocWithDriveDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// @ts-ignore: error TS4111: Property 'openOfficeExcelFromDrive' comes from an
// index signature, so it must be accessed with ['openOfficeExcelFromDrive'].
testcase.openOfficeExcelFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      // @ts-ignore: error TS4111: Property 'smallXlsxPinned' comes from an
      // index signature, so it must be accessed with ['smallXlsxPinned'].
      RootPath.DRIVE, [], [ENTRIES.smallXlsxPinned]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      // @ts-ignore: error TS4111: Property 'smallXlsxPinned' comes from an
      // index signature, so it must be accessed with ['smallXlsxPinned'].
      'openFile', appId, [ENTRIES.smallXlsxPinned.nameText]));

  // The Web Drive Office Excel task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(openExcelWithDriveDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// @ts-ignore: error TS4111: Property 'openOfficePowerPointFromDrive' comes from
// an index signature, so it must be accessed with
// ['openOfficePowerPointFromDrive'].
testcase.openOfficePowerPointFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      // @ts-ignore: error TS4111: Property 'smallPptxPinned' comes from an
      // index signature, so it must be accessed with ['smallPptxPinned'].
      RootPath.DRIVE, [], [ENTRIES.smallPptxPinned]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      // @ts-ignore: error TS4111: Property 'smallPptxPinned' comes from an
      // index signature, so it must be accessed with ['smallPptxPinned'].
      'openFile', appId, [ENTRIES.smallPptxPinned.nameText]));

  // The Web Drive Office PowerPoint task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(openPowerPointWithDriveDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// @ts-ignore: error TS4111: Property 'openMultipleOfficeWordFromDrive' comes
// from an index signature, so it must be accessed with
// ['openMultipleOfficeWordFromDrive'].
testcase.openMultipleOfficeWordFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [],
      // @ts-ignore: error TS4111: Property 'smallDocxHosted' comes from an
      // index signature, so it must be accessed with ['smallDocxHosted'].
      [ENTRIES.smallDocx, ENTRIES.smallDocxPinned, ENTRIES.smallDocxHosted]);

  const enterKey = ['#file-list', 'Enter', false, false, false];

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Select all the files.
  const ctrlA = ['#file-list', 'a', true, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Check: the file-list should show 3 selected files.
  const caller = getCaller();
  // @ts-ignore: error TS7030: Not all code paths return a value.
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElement(
        appId, '.check-select #files-selected-label');
    if (element.text !== '3 files selected') {
      return pending(
          caller, `Waiting for files to be selected, got: ${element.text}`);
    }
  });

  let taskDescriptor;
  let expectedExecuteTaskCount = 0;
  // @ts-ignore: error TS6133: 'histogramCount' is declared but its value is
  // never read.
  let histogramCount;

  // Wait for the tasks calculation to complete, updating the "Open" button.
  await remoteCall.waitForElement(appId, '#tasks[get-tasks-completed]');

  // Wait until the open button is available.
  await remoteCall.waitForElement(appId, '#tasks');

  // TODO(petermarshall): Check that the Docs task is available but that we fell
  // back to Quick office: one of the selected entries doesn't have a
  // "docs.google.com" alternate URL.

  // Press Enter to execute the task.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  remoteCall.fakeKeyDown(appId, ...enterKey);

  // Check that it's the Docs task.
  expectedExecuteTaskCount++;
  taskDescriptor = await getExecutedTask(appId, expectedExecuteTaskCount);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Unselect the file that doesn't have an alternate URL.
  await remoteCall.waitAndClickElement(
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      appId, `#file-list [file-name="${ENTRIES.smallDocx.nameText}"]`,
      // @ts-ignore: error TS2345: Argument of type '{ ctrl: true; }' is not
      // assignable to parameter of type 'KeyModifiers'.
      {ctrl: true});

  // Wait for the file to be unselected.
  await remoteCall.waitForElement(
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      appId, `[file-name="${ENTRIES.smallDocx.nameText}"]:not([selected])`);

  // Press Enter.
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  remoteCall.fakeKeyDown(appId, ...enterKey);

  // The Drive/Docs task should be available and executed.
  expectedExecuteTaskCount++;
  taskDescriptor = await getExecutedTask(appId, expectedExecuteTaskCount);
  chrome.test.assertEq(openDocWithDriveDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// @ts-ignore: error TS4111: Property 'openOfficeWordFromDriveNotSynced' comes
// from an index signature, so it must be accessed with
// ['openOfficeWordFromDriveNotSynced'].
testcase.openOfficeWordFromDriveNotSynced = async () => {
  const appId =
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.smallDocx]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // The Drive/Docs task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// @ts-ignore: error TS4111: Property 'openOfficeWordFromMyFilesOffline' comes
// from an index signature, so it must be accessed with
// ['openOfficeWordFromMyFilesOffline'].
testcase.openOfficeWordFromMyFilesOffline = async () => {
  const appId =
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallDocx]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.EMPTY.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['empty']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      // @ts-ignore: error TS4111: Property 'smallDocx' comes from an index
      // signature, so it must be accessed with ['smallDocx'].
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // The Drive/Docs task should be executed, but it will fall back to
  // QuickOffice.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// @ts-ignore: error TS4111: Property 'openOfficeWordFromDriveOffline' comes
// from an index signature, so it must be accessed with
// ['openOfficeWordFromDriveOffline'].
testcase.openOfficeWordFromDriveOffline = async () => {
  const appId = await setupAndWaitUntilReady(
      // @ts-ignore: error TS4111: Property 'smallDocxPinned' comes from an
      // index signature, so it must be accessed with ['smallDocxPinned'].
      RootPath.DRIVE, [], [ENTRIES.smallDocxPinned]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      // @ts-ignore: error TS4111: Property 'smallDocxPinned' comes from an
      // index signature, so it must be accessed with ['smallDocxPinned'].
      'openFile', appId, [ENTRIES.smallDocxPinned.nameText]));

  // The Drive/Docs task should be executed, but it will fall back to
  // QuickOffice.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

/** Tests that the educational nudge is displayed when the preference is set. */
// @ts-ignore: error TS4111: Property 'officeShowNudgeGoogleDrive' comes from an
// index signature, so it must be accessed with ['officeShowNudgeGoogleDrive'].
testcase.officeShowNudgeGoogleDrive = async () => {
  // Set the pref emulating that the user has moved a file.
  await sendTestMessage({
    name: 'setPrefOfficeFileMovedToGoogleDrive',
    timestamp: Date.now(),
  });

  // Open the Files app.
  const appId = await setupAndWaitUntilReady(
      // @ts-ignore: error TS4111: Property 'smallDocxPinned' comes from an
      // index signature, so it must be accessed with ['smallDocxPinned'].
      RootPath.DRIVE, [], [ENTRIES.smallDocxPinned]);

  // Check that the nudge and its text is visible.
  await remoteCall.waitNudge(
      appId, 'Recently opened Microsoft files have moved to Google Drive');
};
