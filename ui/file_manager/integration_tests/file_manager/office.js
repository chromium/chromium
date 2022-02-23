// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {FILE_MANAGER_EXTENSIONS_ID, FILE_MANAGER_SWA_APP_ID, FILE_SWA_BASE_URL} from './test_data.js';

/**
 * Waits until a task has been executed, and returns the executed task's
 * descriptor.
 *
 * @param {string} appId Window ID.
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
async function getExecutedTask(appId) {
  const caller = getCaller();

  // Wait until a task has been executed.
  await repeatUntil(async () => {
    const executeTaskCount = await remoteCall.callRemoteTestUtil(
        'staticFakeCounter', appId, ['chrome.fileManagerPrivate.executeTask']);
    if (executeTaskCount == 1) {
      return true;
    }
    // Expect no executedTasks calls.
    chrome.test.assertFalse(!!executeTaskCount);
    return pending(caller, 'Waiting for a task to execute');
  });

  // Arguments for the only execution of executeTask().
  const executeTaskArgs = (await remoteCall.callRemoteTestUtil(
      'staticFakeCalledArgs', appId,
      ['chrome.fileManagerPrivate.executeTask']))[0];

  // The task descriptor is the first argument.
  return executeTaskArgs[0];
}

testcase.openOfficeFile = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.smallDocx.targetPath],
    openType: 'launch'
  });

  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.smallDocx]);

  // Open file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['text.docx']));

  // Check that the Office file's alternate URL has been opened in a browser
  // window.
  await remoteCall.waitForActiveBrowserTabUrl(
      'https://file_alternate_link/text.docx');
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
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['text.docx']));

  // The Web Drive Office task should not be available: another
  // task should have been executed instead (QuickOffice or generic task).
  const taskDescriptor = await getExecutedTask(appId);
  const webDriveOfficeActionId =
      (remoteCall.isSwaMode() ? FILE_SWA_BASE_URL + '?' : '') +
      'open-web-drive-office';
  chrome.test.assertFalse(taskDescriptor.actionId == webDriveOfficeActionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeFromDrive = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.smallDocx]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['text.docx']));

  const filesAppId = remoteCall.isSwaMode() ? FILE_MANAGER_SWA_APP_ID :
                                              FILE_MANAGER_EXTENSIONS_ID;
  const filesTaskType = remoteCall.isSwaMode() ? 'web' : 'app';
  const actionId = (remoteCall.isSwaMode() ? FILE_SWA_BASE_URL + '?' : '') +
      'open-web-drive-office';

  // The Web Drive Office task should be available and executed.
  const expectedDescriptor = {
    appId: filesAppId,
    taskType: filesTaskType,
    actionId: actionId
  };
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(taskDescriptor, expectedDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};
