// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Fake task.
 *
 * @param {boolean} isDefault Whether the task is default or not.
 * @param {string} taskId Task ID.
 * @param {string} title Title of the task.
 * @param {boolean=} opt_isGenericFileHandler Whether the task is a generic
 *     file handler.
 * @constructor
 */
function FakeTask(isDefault, taskId, title, opt_isGenericFileHandler) {
  this.driveApp = false;
  this.iconUrl = 'chrome://theme/IDR_DEFAULT_FAVICON';  // Dummy icon
  this.isDefault = isDefault;
  this.taskId = taskId;
  this.title = title;
  this.isGenericFileHandler = opt_isGenericFileHandler || false;
  Object.freeze(this);
}

/**
 * Fake tasks for a local volume.
 *
 * @type {Array<FakeTask>}
 * @const
 */
const DOWNLOADS_FAKE_TASKS = [
  new FakeTask(true, 'dummytaskid|open-with', 'DummyTask1'),
  new FakeTask(false, 'dummytaskid-2|open-with', 'DummyTask2')
];

/**
 * Fake tasks for a local volume opening in browser.
 *
 * @type {Array<FakeTask>}
 * @const
 */
const DOWNLOADS_FAKE_TEXT = [
  new FakeTask(true, FILE_MANAGER_EXTENSIONS_ID + '|file|view-in-browser'),
];

/**
 * Fake tasks for a PDF file opening in browser.
 *
 * @type {Array<FakeTask>}
 * @const
 */
const DOWNLOADS_FAKE_PDF = [
  new FakeTask(true, FILE_MANAGER_EXTENSIONS_ID + '|file|view-as-pdf'),
];

/**
 * Fake tasks for a drive volume.
 *
 * @type {Array<FakeTask>}
 * @const
 */
const DRIVE_FAKE_TASKS = [
  new FakeTask(true, 'dummytaskid|drive|open-with', 'DummyTask1'),
  new FakeTask(false, 'dummytaskid-2|drive|open-with', 'DummyTask2')
];

/**
 * Sets up task tests.
 *
 * @param {string} rootPath Root path.
 * @param {Array<FakeTask>} fakeTasks Fake tasks.
 */
async function setupTaskTest(rootPath, fakeTasks) {
  const appId = await setupAndWaitUntilReady(rootPath);
  await remoteCall.callRemoteTestUtil('overrideTasks', appId, [fakeTasks]);
  return appId;
}

/**
 * Tests executing the default task when there is only one task.
 *
 * @param {string} appId Window ID.
 * @param {string} expectedTaskId Task ID expected to execute.
 */
async function executeDefaultTask(appId, expectedTaskId) {
  // Select file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']));

  // Double-click the file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId,
      ['#file-list li.table-row[selected] .filename-label span']));

  // Wait until the task is executed.
  await remoteCall.waitUntilTaskExecutes(appId, expectedTaskId);
}

/**
 * Tests to specify default task via the default task dialog.
 *
 * @param {string} appId Window ID.
 * @param {string} expectedTaskId Task ID to be expected to newly specify as
 *     default.
 * @return {Promise} Promise to be fulfilled/rejected depends on the test
 *     result.
 */
async function defaultTaskDialog(appId, expectedTaskId) {
  // Prepare expected labels.
  const expectedLabels = [
    'DummyTask1 (default)',
    'DummyTask2',
  ];

  // Select file.
  await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']);

  // Click the change default menu.
  await remoteCall.waitForElement(appId, '#tasks[multiple]');
  await remoteCall.waitForElement(appId, '#tasks-menu .change-default');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId,
      ['#tasks', 'select', {item: {type: 'ChangeDefaultTask'}}]));

  const caller = getCaller();

  // Wait for the list of menu item is added as expected.
  await repeatUntil(async () => {
    // Obtains menu items.
    const items = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId,
        ['#default-task-dialog #default-tasks-list li']);

    // Compare the contents of items.
    const actualLabels = items.map((item) => item.text);
    if (chrome.test.checkDeepEq(expectedLabels, actualLabels)) {
      return true;
    }
    return pending(
        caller, 'Tasks do not match, expected: %j, actual: %j.', expectedLabels,
        actualLabels);
  });

  // Click the non default item.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeEvent', appId, [
        '#default-task-dialog #default-tasks-list li:nth-of-type(2)',
        'mousedown', {bubbles: true, button: 0}
      ]));
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeEvent', appId, [
        '#default-task-dialog #default-tasks-list li:nth-of-type(2)', 'click',
        {bubbles: true}
      ]));

  // Wait for the dialog hidden, and the task is executed.
  await remoteCall.waitForElementLost(appId, '#default-task-dialog', null);

  // Execute the new default task. Click on "Open ▼" button.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ['#tasks']);

  // Wait for dropdown menu to show.
  await remoteCall.waitForElement(
      appId, '#tasks-menu:not([hidden]) cr-menu-item');

  // Click on first menu item.
  remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#tasks-menu:not([hidden]) cr-menu-item:nth-child(1)']);

  // Wait dropdown menu to hide.
  chrome.test.assertTrue(
      !!await remoteCall.waitForElement(appId, '#tasks-menu[hidden]'));

  // Check the executed tasks.
  await remoteCall.waitUntilTaskExecutes(appId, expectedTaskId);
}

testcase.executeDefaultTaskDrive = async () => {
  const appId = await setupTaskTest(RootPath.DRIVE, DRIVE_FAKE_TASKS);
  await executeDefaultTask(appId, 'dummytaskid|drive|open-with');
};

testcase.executeDefaultTaskDownloads = async () => {
  const appId = await setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_TASKS);
  await executeDefaultTask(appId, 'dummytaskid|open-with');
};

testcase.defaultTaskForTextPlain = async () => {
  const appId = await setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_TEXT);
  await executeDefaultTask(
      appId, FILE_MANAGER_EXTENSIONS_ID + '|file|view-in-browser');
};

testcase.defaultTaskForPdf = async () => {
  const appId = await setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_PDF);
  await executeDefaultTask(
      appId, FILE_MANAGER_EXTENSIONS_ID + '|file|view-as-pdf');
};

testcase.defaultTaskDialogDrive = async () => {
  const appId = await setupTaskTest(RootPath.DRIVE, DRIVE_FAKE_TASKS);
  await defaultTaskDialog(appId, 'dummytaskid-2|drive|open-with');
};

testcase.defaultTaskDialogDownloads = async () => {
  const appId = await setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_TASKS);
  await defaultTaskDialog(appId, 'dummytaskid-2|open-with');
};

testcase.genericTaskIsNotExecuted = async () => {
  const tasks = [new FakeTask(
      false, 'dummytaskid|open-with', 'DummyTask1',
      true /* isGenericFileHandler */)];

  // When default task is not set, executeDefaultInternal_ in file_tasks.js
  // tries to show it in a browser tab. By checking the view-in-browser task is
  // executed, we check that default task is not set in this situation.
  //
  // See: src/ui/file_manager/file_manager/foreground/js/file_tasks.js&l=404
  const appId = await setupTaskTest(RootPath.DOWNLOADS, tasks);
  await executeDefaultTask(
      appId, FILE_MANAGER_EXTENSIONS_ID + '|file|view-in-browser');
};

testcase.genericTaskAndNonGenericTask = async () => {
  const tasks = [
    new FakeTask(
        false, 'dummytaskid|open-with', 'DummyTask1',
        true /* isGenericFileHandler */),
    new FakeTask(
        false, 'dummytaskid-2|open-with', 'DummyTask2',
        false /* isGenericFileHandler */),
    new FakeTask(
        false, 'dummytaskid-3|open-with', 'DummyTask3',
        true /* isGenericFileHandler */)
  ];

  const appId = await setupTaskTest(RootPath.DOWNLOADS, tasks);
  await executeDefaultTask(appId, 'dummytaskid-2|open-with');
};
