// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertDeepEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {createCrostiniForTest} from '../../background/js/mock_crostini.js';
import {DialogType} from '../../common/js/dialog_type.js';
import {metrics} from '../../common/js/metrics.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise} from '../../common/js/test_error_reporting.js';
import {decorate} from '../../common/js/ui.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {DirectoryModel} from './directory_model.js';
import {FakeFileSelectionHandler} from './fake_file_selection_handler.js';
import {FileSelectionHandler} from './file_selection.js';
import {FileTasks} from './file_tasks.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';
import {MetadataUpdateController} from './metadata_update_controller.js';
import {TaskController} from './task_controller.js';
import {Command} from './ui/command.js';
import {FileManagerUI} from './ui/file_manager_ui.js';


/** Mock metrics. */
metrics.recordEnum = function(
    _name: string, _value: any, _valid?: any[]|number) {};

/** Mock chrome APIs.  */
let mockChrome: any;

// Set up test components.
export function setUp() {
  // Mock chrome APIs.
  mockChrome = {
    commandLinePrivate: {
      hasSwitch: function(_name: string, callback: (v: boolean) => void) {
        callback(false);
      },
    },
    runtime: {
      id: 'test-extension-id',
      lastError: null,
    },
  };

  setupFileManagerPrivate();
  installMockChrome(mockChrome);

  // Install <command> elements on the page.
  document.body.innerHTML = [
    '<command id="default-task">',
    '<command id="open-with">',
  ].join('');

  // Initialize Command with the <command>s.
  decorate('command', Command);
}

function createTaskController(fileSelectionHandler: FileSelectionHandler):
    TaskController {
  const taskController = new TaskController(
      DialogType.FULL_PAGE, {
        getLocationInfo: function(_entry: Entry) {
          return VolumeManagerCommon.RootType.DRIVE;
        },
        getDriveConnectionState: function() {
          return 'ONLINE';
        },
        getVolumeInfo: function() {
          return {
            volumeType: VolumeManagerCommon.VolumeType.DRIVE,
          };
        },
      } as unknown as VolumeManager,
      {
        taskMenuButton: document.createElement('button'),
        fileContextMenu: {
          defaultActionMenuItem: document.createElement('div'),
        },
        speakA11yMessage: (_text: string) => {},
      } as unknown as FileManagerUI,
      new MockMetadataModel({}) as unknown as MetadataModel, {
        getCurrentRootType: () => null,
      } as unknown as DirectoryModel,
      fileSelectionHandler, {} as unknown as MetadataUpdateController,
      createCrostiniForTest(), {} as unknown as ProgressCenter);

  return taskController;
}

/**
 * Setup test case fileManagerPrivate.
 */
function setupFileManagerPrivate() {
  mockChrome.fileManagerPrivate = {
    getFileTaskCalledCount_: 0,
    getFileTasks: function(_entries: Entry[], callback: (tasks: any) => void) {
      mockChrome.fileManagerPrivate.getFileTaskCalledCount_++;
      const fileTasks = ([
        /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
          descriptor: {
            appId: 'handler-extension-id',
            taskType: 'file',
            actionId: 'open',
          },
          isDefault: false,
        }),
        /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
          descriptor: {
            appId: 'handler-extension-id',
            taskType: 'file',
            actionId: 'play',
          },
          isDefault: true,
        }),
      ]);
      setTimeout(callback.bind(null, {tasks: fileTasks}), 0);
    },
  };
}

/**
 * Tests that executeEntryTask() runs the expected task.
 */
export function testExecuteEntryTask(callback: () => void) {
  const selectionHandler =
      new FakeFileSelectionHandler() as unknown as FileSelectionHandler;

  const fileSystem = new MockFileSystem('volumeId');
  fileSystem.entries['/test.png'] =
      MockFileEntry.create(fileSystem, '/test.png');
  const taskController = createTaskController(selectionHandler);

  const testEntry = /** @type {FileEntry} */ (fileSystem.entries['/test.png']);
  taskController.executeEntryTask(testEntry);

  reportPromise(
      new Promise<chrome.fileManagerPrivate.FileTaskDescriptor>((resolve) => {
        chrome.fileManagerPrivate.executeTask = resolve;
      }).then((descriptor: chrome.fileManagerPrivate.FileTaskDescriptor) => {
        assert(util.descriptorEqual(
            {appId: 'handler-extension-id', taskType: 'file', actionId: 'play'},
            descriptor));
      }),
      callback);
}

/**
 * Tests that getFileTasks() does not call .fileManagerPrivate.getFileTasks()
 * multiple times when the selected entries are not changed.
 */
export async function testGetFileTasksShouldNotBeCalledMultipleTimes(
    done: () => void) {
  const selectionHandler = new FakeFileSelectionHandler();

  const fileSystem = new MockFileSystem('volumeId');
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
  const taskController =
      createTaskController(selectionHandler as unknown as FileSelectionHandler);

  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 0);

  let tasks = await taskController.getFileTasks();
  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 1);
  assert(util.isSameEntries(tasks.entries, selectionHandler.selection.entries));
  // Make oldSelection.entries !== newSelection.entries
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
  tasks = await taskController.getFileTasks();
  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 1);
  assert(util.isSameEntries(tasks.entries, selectionHandler.selection.entries));

  // Check concurrent calls, should only create one promise and one call to the
  // private API.
  const promise1 = taskController.getFileTasks();
  // Await 0ms to give time to pomise1 to initialize.
  await new Promise(r => setTimeout(r));
  const promise2 = taskController.getFileTasks();
  const [tasks1, tasks2] = await Promise.all([promise1, promise2]);
  assertDeepEquals(
      tasks1.entries, tasks2.entries,
      'both tasks should have test.png as entry');
  assertTrue(tasks1 === tasks2);
  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 1);
  assert(
      util.isSameEntries(tasks1.entries, selectionHandler.selection.entries));

  // Check concurrent calls right after changing the selection.
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/hello.txt')], ['text/plain']);
  const promise3 = taskController.getFileTasks();
  // Await 0ms to give time to pomise3 to initialize.
  await new Promise(r => setTimeout(r));
  const promise4 = taskController.getFileTasks();
  const [tasks3, tasks4] = await Promise.all([promise3, promise4]);
  assertDeepEquals(
      tasks3.entries, tasks4.entries,
      'both tasks should have hello.txt as entry');
  assertTrue(tasks3 === tasks4);
  assert(
      util.isSameEntries(tasks3.entries, selectionHandler.selection.entries));
  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 2);

  done();
}

/**
 * Tests that getFileTasks() should always return the promise whose FileTasks
 * correspond to FileSelectionHandler.selection at the time getFileTasks() is
 * called.
 */
export function testGetFileTasksShouldNotReturnObsoletePromise(
    callback: () => void) {
  const selectionHandler = new FakeFileSelectionHandler();

  const fileSystem = new MockFileSystem('volumeId');
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
  const taskController =
      createTaskController(selectionHandler as unknown as FileSelectionHandler);

  taskController.getFileTasks()
      .then(tasks => {
        assert(util.isSameEntries(
            tasks.entries, selectionHandler.selection.entries));
        selectionHandler.updateSelection(
            [MockFileEntry.create(fileSystem, '/testtest.jpg')],
            ['image/jpeg']);
        return taskController.getFileTasks();
      })
      .then(tasks => {
        assert(util.isSameEntries(
            tasks.entries, selectionHandler.selection.entries));
        callback();
      })
      .catch(error => {
        assertNotReached(error.toString());
      });
}

/**
 * Tests that changing the file selection during a getFileTasks() call causes
 * the getFileTasks() promise to reject.
 */
export function testGetFileTasksShouldNotCacheRejectedPromise(
    callback: () => void) {
  const selectionHandler = new FakeFileSelectionHandler();

  const fileSystem = new MockFileSystem('volumeId');
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
  const taskController =
      createTaskController(selectionHandler as unknown as FileSelectionHandler);

  // Setup the selection handler computeAdditionalCallback to change the file
  // selection during the getFileTasks() call.
  selectionHandler.computeAdditionalCallback = () => {
    selectionHandler.updateSelection(
        [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
  };

  taskController.getFileTasks().then(
      (_tasks: FileTasks) => {
        assertNotReached('Fail: getFileTasks promise should be rejected');
      },
      () => {
        // Clears the selection handler computeAdditionalCallback so that the
        // promise won't be rejected during the getFileTasks() call.
        selectionHandler.computeAdditionalCallback = () => {};

        taskController.getFileTasks().then(
            tasks => {
              assert(util.isSameEntries(
                  tasks.entries, selectionHandler.selection.entries));
              callback();
            },
            () => {
              assertNotReached('Fail: getFileTasks promise was rejected');
            });
      });
}
