// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, getHistogramCount, pending, repeatUntil, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {FILE_MANAGER_EXTENSIONS_ID, FILE_MANAGER_SWA_APP_ID, FILE_SWA_BASE_URL} from './test_data.js';

/**
 * The name of the UMA to track the results of trying to enable Web Drive Office
 * for MS Office files.
 * @const {string}
 */
const WebDriveOfficeTaskResultHistogramName =
    'FileBrowser.OfficeFiles.WebDriveOffice';

/**
 * The UMA's enumeration values (must be consistent with
 * WebDriveOfficeTaskResult in tools/metrics/histograms/enums.xml).
 * @enum {number}
 */
const WebDriveOfficeTaskResultHistogramValues = {
  AVAILABLE: 0,
  FLAG_DISABLED: 1,
  OFFLINE: 2,
  NOT_ON_DRIVE: 3,
  DRIVE_ERROR: 4,
  DRIVE_METADATA_ERROR: 5,
  INVALID_ALTERNATE_URL: 6,
  DRIVE_ALTERNATE_URL: 7,
  UNEXPECTED_ALTERNATE_URL: 8,
};

/**
 * The name of the UMA to track the handlers used to open MS Office files.
 * @const {string}
 */
const OfficeFileHandlersHistogramBaseName =
    'FileBrowser.OfficeFiles.FileHandler';

/**
 * Office file handlers UMA values (must be consistent with OfficeFileHandler in
 * tools/metrics/histograms/enums.xml).
 * @const @enum {number}
 */
const OfficeFileHandlersHistogramValues = {
  OTHER: 0,
  WEB_DRIVE_OFFICE: 1,
  QUICK_OFFICE: 2,
};

/**
 * Returns Web Drive Office Word's task descriptor.
 *
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
function webDriveOfficeWordDescriptor() {
  const filesAppId = remoteCall.isSwaMode() ? FILE_MANAGER_SWA_APP_ID :
                                              FILE_MANAGER_EXTENSIONS_ID;
  const filesTaskType = remoteCall.isSwaMode() ? 'web' : 'app';
  const actionIdPrefix = remoteCall.isSwaMode() ? FILE_SWA_BASE_URL + '?' : '';
  const actionId = `${actionIdPrefix}open-web-drive-office-word`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}

/**
 * Returns Web Drive Office Excel's task descriptor.
 *
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
function webDriveOfficeExcelDescriptor() {
  const filesAppId = remoteCall.isSwaMode() ? FILE_MANAGER_SWA_APP_ID :
                                              FILE_MANAGER_EXTENSIONS_ID;
  const filesTaskType = remoteCall.isSwaMode() ? 'web' : 'app';
  const actionIdPrefix = remoteCall.isSwaMode() ? FILE_SWA_BASE_URL + '?' : '';
  const actionId = `${actionIdPrefix}open-web-drive-office-excel`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}

/**
 * Returns Web Drive Office PowerPoint's task descriptor.
 *
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
function webDriveOfficePowerPointDescriptor() {
  const filesAppId = remoteCall.isSwaMode() ? FILE_MANAGER_SWA_APP_ID :
                                              FILE_MANAGER_EXTENSIONS_ID;
  const filesTaskType = remoteCall.isSwaMode() ? 'web' : 'app';
  const actionIdPrefix = remoteCall.isSwaMode() ? FILE_SWA_BASE_URL + '?' : '';
  const actionId = `${actionIdPrefix}open-web-drive-office-powerpoint`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}

/**
 * Returns Upload to Drive task descriptor.
 *
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
function uploadOfficeToDriveDescriptor() {
  const filesAppId = remoteCall.isSwaMode() ? FILE_MANAGER_SWA_APP_ID :
                                              FILE_MANAGER_EXTENSIONS_ID;
  const filesTaskType = remoteCall.isSwaMode() ? 'web' : 'app';
  const actionIdPrefix = remoteCall.isSwaMode() ? FILE_SWA_BASE_URL + '?' : '';
  const actionId = `${actionIdPrefix}upload-office-to-drive`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}


/**
 * Waits for the expected number of tasks executions, and returns the descriptor
 * of the last executed task.
 *
 * @param {string} appId Window ID.
 * @param {number} expectedCount
 * @return {!Promise<!chrome.fileManagerPrivate.FileTaskDescriptor>}
 */
async function getExecutedTask(appId, expectedCount = 1) {
  const caller = getCaller();

  // Wait until a task has been executed.
  await repeatUntil(async () => {
    const executeTaskCount = await remoteCall.callRemoteTestUtil(
        'staticFakeCounter', appId, ['chrome.fileManagerPrivate.executeTask']);
    if (executeTaskCount == expectedCount) {
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

testcase.openOfficeWordFile = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.smallDocxHosted.targetPath],
    openType: 'launch',
  });

  let histogramCount = await getHistogramCount(
      OfficeFileHandlersHistogramBaseName + '.Drive.Online',
      OfficeFileHandlersHistogramValues.WEB_DRIVE_OFFICE);
  chrome.test.assertEq(
      0, histogramCount,
      'Unexpected UMA metric value for Office file handlers');

  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxHosted]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocxHosted.nameText]));

  // Check that the Word file's alternate URL has been opened in a browser
  // window.
  await remoteCall.waitForLastOpenedBrowserTabUrl(
      ENTRIES.smallDocxHosted.alternateUrl);

  // Assert that a UMA sample has been reported for executing Web Drive Office
  // from Drive and Online.
  histogramCount = await getHistogramCount(
      OfficeFileHandlersHistogramBaseName + '.Drive.Online',
      OfficeFileHandlersHistogramValues.WEB_DRIVE_OFFICE);
  chrome.test.assertEq(
      1, histogramCount,
      'Unexpected UMA metric value for Office file handlers');
};

testcase.openOfficeWordFromMyFiles = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallDocx]);

  let histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.NOT_ON_DRIVE);
  chrome.test.assertEq(
      0, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.EMPTY.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['empty']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // The available Office task should be "Upload to Drive".
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertTrue(
      taskDescriptor.actionId == uploadOfficeToDriveDescriptor().actionId);

  // Assert, for Web Drive Office, that a UMA sample has been reported for
  // getting tasks for an Office file that is not on Drive.
  histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.NOT_ON_DRIVE);
  chrome.test.assertEq(
      1, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// Tests that "Upload to Drive" cannot be enabled if the "Web Drive Office" flag
// is disabled (test setup similar to `openOfficeWordFromMyFiles`).
testcase.uploadToDriveRequiresWebDriveOfficeEnabled = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallDocx]);
  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.EMPTY.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['empty']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // Since the Web Drive Office flag isn't enabled, the Upload to Drive task
  // should not be available: another task should have been executed instead
  // (QuickOffice or generic task).
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == uploadOfficeToDriveDescriptor().actionId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == webDriveOfficeWordDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeWordFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxHosted]);

  let histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.AVAILABLE);
  chrome.test.assertEq(
      0, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocxHosted.nameText]));

  // The Web Drive Office Word task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(webDriveOfficeWordDescriptor(), taskDescriptor);

  // Assert that a UMA sample has been reported for selecting an Office file on
  // Drive.
  histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.AVAILABLE);
  chrome.test.assertEq(
      1, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeExcelFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallXlsxPinned]);

  let histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.AVAILABLE);
  chrome.test.assertEq(
      0, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallXlsxPinned.nameText]));

  // The Web Drive Office Excel task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(webDriveOfficeExcelDescriptor(), taskDescriptor);

  // Assert that a UMA sample has been reported for selecting an Office file on
  // Drive.
  histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.AVAILABLE);
  chrome.test.assertEq(
      1, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficePowerPointFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallPptxPinned]);

  let histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.AVAILABLE);
  chrome.test.assertEq(
      0, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallPptxPinned.nameText]));

  // The Web Drive Office PowerPoint task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(webDriveOfficePowerPointDescriptor(), taskDescriptor);

  // Assert that a UMA sample has been reported for selecting an Office file on
  // Drive.
  histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.AVAILABLE);
  chrome.test.assertEq(
      1, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openMultipleOfficeWordFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [],
      [ENTRIES.smallDocxHosted, ENTRIES.smallDocxPinned, ENTRIES.smallDocx]);

  const enterKey = ['#file-list', 'Enter', false, false, false];

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Select all the files.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Check: the file-list should show 3 selected files.
  const caller = getCaller();
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
  let histogramCount;

  // Wait for the tasks calculation to complete, updating the "Open" button.
  await remoteCall.waitForElement(appId, '#tasks[get-tasks-completed]');

  // Check whether the open button is available.
  const openButton = await remoteCall.waitForElement(appId, '#tasks');

  // Check that the Web Drive Office Word task is not available: one of the
  // selected entries doesn't have a "docs.google.com" alternate URL. The "Open"
  // button will be hidden if there is no other task to execute the selected
  // office files, this happens to non-branded bots because QuickOffice is only
  // available for branded builds.
  if (!openButton.hidden) {
    // Press Enter to execute it.
    remoteCall.fakeKeyDown(appId, ...enterKey);

    // check that it's not the Web Drive Office task.
    expectedExecuteTaskCount++;
    taskDescriptor = await getExecutedTask(appId, expectedExecuteTaskCount);
    chrome.test.assertFalse(
        taskDescriptor.actionId == webDriveOfficeWordDescriptor().actionId);

    // If Web Drive Office is not available, Quick Office is the default
    // handler. Assert that a UMA sample has been reported for executing Quick
    // Office from Drive and Online.
    histogramCount = await getHistogramCount(
        OfficeFileHandlersHistogramBaseName + '.Drive.Online',
        OfficeFileHandlersHistogramValues.QUICK_OFFICE);
    chrome.test.assertEq(
        1, histogramCount,
        'Unexpected UMA metric value for Office file handlers');
  }

  // Assert that a UMA sample has been reported for selecting an Office file on
  // Drive that doesn't have a Web Drive editing alternate URL yet.
  histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.DRIVE_ALTERNATE_URL);
  chrome.test.assertEq(
      1, histogramCount, 'Unexpected UMA metric value for Web Drive Office');
  histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.AVAILABLE);
  chrome.test.assertEq(
      0, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Unselect the file that doesn't have an alternate URL.
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${ENTRIES.smallDocx.nameText}"]`,
      {ctrl: true});

  // Wait for the file to be unselected.
  await remoteCall.waitForElement(
      appId, `[file-name="${ENTRIES.smallDocx.nameText}"]:not([selected])`);

  // Press Enter.
  remoteCall.fakeKeyDown(appId, ...enterKey);

  // The Web Drive Office Word task should be available and executed.
  expectedExecuteTaskCount++;
  taskDescriptor = await getExecutedTask(appId, expectedExecuteTaskCount);
  chrome.test.assertEq(webDriveOfficeWordDescriptor(), taskDescriptor);

  // Assert that a UMA sample has been reported for selecting synchronized
  // Office files on Drive.
  histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.AVAILABLE);
  chrome.test.assertEq(
      1, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Assert that a UMA sample has been reported for executing Web Drive Office
  // from Drive and Online.
  histogramCount = await getHistogramCount(
      OfficeFileHandlersHistogramBaseName + '.Drive.Online',
      OfficeFileHandlersHistogramValues.WEB_DRIVE_OFFICE);
  chrome.test.assertEq(
      1, histogramCount,
      'Unexpected UMA metric value for Office file handlers');

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeWordFromDriveNotSynced = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.smallDocx]);

  let histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.DRIVE_ALTERNATE_URL);
  chrome.test.assertEq(
      0, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // When the office file doesn't have an expected alternate URL
  // (docs.google.com host), the Web Drive Office task should not be available:
  // another task should have been executed instead (QuickOffice or generic
  // task).
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == webDriveOfficeWordDescriptor().actionId);

  // Assert that a UMA sample has been reported for selecting an Office file on
  // Drive that doesn't have a Web Drive editing alternate URL.
  histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.DRIVE_ALTERNATE_URL);
  chrome.test.assertEq(
      1, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeWordFromMyFilesOffline = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallDocx]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.EMPTY.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['empty']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // The Upload to Drive task should not be available: another
  // task should have been executed instead (QuickOffice or generic task).
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == uploadOfficeToDriveDescriptor().actionId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == webDriveOfficeWordDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeWordFromDriveOffline = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxPinned]);

  let histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.OFFLINE);
  chrome.test.assertEq(
      0, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocxPinned.nameText]));

  // When offline, the Web Drive Office Word task should not be available:
  // another task should have been executed instead (QuickOffice or generic
  // task).
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == webDriveOfficeWordDescriptor().actionId);

  // Assert that a UMA sample has been reported for selecting an Office file on
  // Drive while offline.
  histogramCount = await getHistogramCount(
      WebDriveOfficeTaskResultHistogramName,
      WebDriveOfficeTaskResultHistogramValues.OFFLINE);
  chrome.test.assertEq(
      1, histogramCount, 'Unexpected UMA metric value for Web Drive Office');

  if (taskDescriptor.actionId == 'qo_documents') {
    // On branded builds, if Web Drive Office is not available, Quick Office is
    // the default handler. Assert that a UMA sample has been reported for
    // executing Quick Office from Drive and Offline.
    histogramCount = await getHistogramCount(
        OfficeFileHandlersHistogramBaseName + '.Drive.Offline',
        OfficeFileHandlersHistogramValues.QUICK_OFFICE);
    chrome.test.assertEq(
        1, histogramCount,
        'Unexpected UMA metric value for Office file handlers');
  }

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};
