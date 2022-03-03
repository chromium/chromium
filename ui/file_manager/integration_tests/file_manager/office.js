// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {FILE_MANAGER_EXTENSIONS_ID, FILE_MANAGER_SWA_APP_ID, FILE_SWA_BASE_URL} from './test_data.js';

/**
 * Returns Web Drive Office's task descriptor.
 *
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
function webDriveOfficeDescriptor() {
  const filesAppId = remoteCall.isSwaMode() ? FILE_MANAGER_SWA_APP_ID :
                                              FILE_MANAGER_EXTENSIONS_ID;
  const filesTaskType = remoteCall.isSwaMode() ? 'web' : 'app';
  const actionIdPrefix = remoteCall.isSwaMode() ? FILE_SWA_BASE_URL + '?' : '';
  const actionId = `${actionIdPrefix}open-web-drive-office`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}

/**
 * Waits for the expected number of tasks executions, and returns the descriptor
 * of the last executed task.
 *
 * @param {string} appId Window ID.
 * @param {number} expectedCount
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
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

testcase.openOfficeFile = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.smallDocxHosted.targetPath],
    openType: 'launch'
  });

  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxHosted]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocxHosted.nameText]));

  // Check that the Office file's alternate URL has been opened in a browser
  // window.
  await remoteCall.waitForActiveBrowserTabUrl(
      ENTRIES.smallDocxHosted.alternateUrl);
};

testcase.openOfficeFromMyFiles = async () => {
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

  // The Web Drive Office task should not be available: another
  // task should have been executed instead (QuickOffice or generic task).
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == webDriveOfficeDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxHosted]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocxHosted.nameText]));

  // The Web Drive Office task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(webDriveOfficeDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openMultipleOfficeFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, [], [
    ENTRIES.smallDocx, ENTRIES.smallDocxHosted, ENTRIES.smallXlsxPinned,
    ENTRIES.smallPptxPinned
  ]);

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

  // Check: the file-list should show 4 selected files.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElement(
        appId, '.check-select #files-selected-label');
    if (element.text !== '4 files selected') {
      return pending(
          caller, `Waiting for files to be selected, got: ${element.text}`);
    }
  });

  let taskDescriptor;
  let expectedExecuteTaskCount = 0;

  // Wait for the tasks calculation to complete, updating the "Open" button.
  await remoteCall.waitForElement(appId, '#tasks[get-tasks-completed]');

  // Check whether the open button is available.
  const openButton = await remoteCall.waitForElement(appId, '#tasks');

  // Check that the Web Drive Office task is not available: one of the
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
        taskDescriptor.actionId == webDriveOfficeDescriptor().actionId);
  }

  // Unselect the file that doesn't have an alternate URL.
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${ENTRIES.smallDocx.nameText}"]`,
      {ctrl: true});

  // Wait for the file to be unselected.
  await remoteCall.waitForElement(
      appId, `[file-name="${ENTRIES.smallDocx.nameText}"]:not([selected])`);

  // Press Enter.
  remoteCall.fakeKeyDown(appId, ...enterKey);

  // The Web Drive Office task should be available and executed.
  expectedExecuteTaskCount++;
  taskDescriptor = await getExecutedTask(appId, expectedExecuteTaskCount);
  chrome.test.assertEq(webDriveOfficeDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeFromDriveNotSynced = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.smallDocx]);

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
      taskDescriptor.actionId == webDriveOfficeDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeFromDriveOffline = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxPinned]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocxPinned.nameText]));

  // When offline, the Web Drive Office task should not be available: another
  // task should have been executed instead (QuickOffice or generic task).
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == webDriveOfficeDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};
