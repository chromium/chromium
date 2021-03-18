// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {Command} from 'chrome://resources/js/cr/ui/command.m.js';
import {assertEquals, assertNotReached} from 'chrome://test/chai_assert.js';

import {createCrostiniForTest} from '../../background/js/mock_crostini.m.js';
import {metrics} from '../../common/js/metrics.m.js';
import {installMockChrome} from '../../common/js/mock_chrome.m.js';
import {MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.m.js';
import {reportPromise} from '../../common/js/test_error_reporting.m.js';
import {util} from '../../common/js/util.m.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.m.js';
import {ProgressCenter} from '../../externs/background/progress_center.m.js';
import {VolumeManager} from '../../externs/volume_manager.m.js';

import {DialogType} from './dialog_type.m.js';
import {DirectoryModel} from './directory_model.m.js';
import {FakeFileSelectionHandler} from './fake_file_selection_handler.m.js';
import {FileSelectionHandler} from './file_selection.m.js';
import {MockMetadataModel} from './metadata/mock_metadata.m.js';
import {MetadataUpdateController} from './metadata_update_controller.m.js';
import {NamingController} from './naming_controller.m.js';
import {TaskController} from './task_controller.m.js';
import {FileManagerUI} from './ui/file_manager_ui.m.js';

/**
 * Mock metrics.
 * @param {string} name
 * @param {*} value
 * @param {Array<*>|number=} valid
 */
metrics.recordEnum = function(name, value, valid) {};

/**
 * Mock chrome APIs.
 * @type {!Object}
 */
let mockChrome;

// Set up test components.
export function setUp() {
  // Mock LoadTimeData strings.
  window.loadTimeData.getBoolean = key => false;
  window.loadTimeData.getString = id => id;

  // Mock chrome APIs.
  mockChrome = {
    commandLinePrivate: {
      hasSwitch: function(name, callback) {
        callback(false);
      },
    },
    runtime: {
      id: 'test-extension-id',
      lastError: null,
    },
    storage: {
      onChanged: {
        addListener: function(callback) {},
      },
      local: {
        get: function(key, callback) {
          callback({});
        },
        set: function(value) {},
      },
    },
  };

  setupFileManagerPrivate();
  installMockChrome(mockChrome);

  // Install <command> elements on the page.
  document.body.innerHTML = [
    '<command id="default-task">',
    '<command id="open-with">',
    '<command id="more-actions">',
    '<command id="show-submenu">',
  ].join('');

  // Initialize Command with the <command>s.
  decorate('command', Command);
}

/**
 * Returns a task controller.
 * @param {!FileSelectionHandler} fileSelectionHandler
 * @return {!TaskController}
 */
function createTaskController(fileSelectionHandler) {
  const taskController = new TaskController(
      DialogType.FULL_PAGE,
      /** @type {!VolumeManager} */ ({
        getLocationInfo: function(entry) {
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
      }),
      /** @type {!FileManagerUI} */ ({
        taskMenuButton: document.createElement('button'),
        shareMenuButton: {
          menu: document.createElement('div'),
        },
        fileContextMenu: {
          defaultActionMenuItem: document.createElement('div'),
        },
        shareSubMenu: document.createElement('div'),
        speakA11yMessage: text => {},
      }),
      new MockMetadataModel({}),
      /** @type {!DirectoryModel} */ ({
        getCurrentRootType: () => null,
      }),
      fileSelectionHandler,
      /** @type {!MetadataUpdateController} */ ({}),
      /** @type {!NamingController} */ ({}), createCrostiniForTest(),
      /** @type {!ProgressCenter} */ ({}));

  return taskController;
}

/**
 * Setup test case fileManagerPrivate.
 */
function setupFileManagerPrivate() {
  mockChrome.fileManagerPrivate = {
    DriveConnectionStateType: {
      ONLINE: 'ONLINE',
      OFFLINE: 'OFFLINE',
      METERED: 'METERED',
    },
    DriveOfflineReason: {
      NOT_READY: 'NOT_READY',
      NO_NETWORK: 'NO_NETWORK',
      NO_SERVICE: 'NO_SERVICE',
    },
    getFileTaskCalledCount_: 0,
    getFileTasks: function(entries, callback) {
      mockChrome.fileManagerPrivate.getFileTaskCalledCount_++;
      const fileTasks = ([
        /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
          taskId: 'handler-extension-id|file|open',
          isDefault: false,
        }),
        /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
          taskId: 'handler-extension-id|file|play',
          isDefault: true,
        }),
      ]);
      setTimeout(callback.bind(null, fileTasks), 0);
    },
    onAppsUpdated: {
      addListener: function() {},
    },
  };
}

/**
 * Tests that executeEntryTask() runs the expected task.
 */
export function testExecuteEntryTask(callback) {
  const selectionHandler = new FakeFileSelectionHandler();

  const fileSystem = new MockFileSystem('volumeId');
  fileSystem.entries['/test.png'] =
      MockFileEntry.create(fileSystem, '/test.png');
  const taskController = createTaskController(selectionHandler);

  const testEntry = /** @type {FileEntry} */ (fileSystem.entries['/test.png']);
  taskController.executeEntryTask(testEntry);

  reportPromise(
      new Promise(resolve => {
        mockChrome.fileManagerPrivate.executeTask = resolve;
      }).then(taskId => {
        assertEquals('handler-extension-id|file|play', taskId);
      }),
      callback);
}

/**
 * Tests that getFileTasks() does not call .fileManagerPrivate.getFileTasks()
 * multiple times when the selected entries are not changed.
 */
export function testGetFileTasksShouldNotBeCalledMultipleTimes(callback) {
  const selectionHandler = new FakeFileSelectionHandler();

  const fileSystem = new MockFileSystem('volumeId');
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
  const taskController = createTaskController(selectionHandler);

  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 0);

  taskController.getFileTasks()
      .then(tasks => {
        assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 1);
        assert(util.isSameEntries(
            tasks.entries, selectionHandler.selection.entries));
        // Make oldSelection.entries !== newSelection.entries
        selectionHandler.updateSelection(
            [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
        return taskController.getFileTasks();
      })
      .then(tasks => {
        assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 1);
        assert(util.isSameEntries(
            tasks.entries, selectionHandler.selection.entries));
        callback();
      })
      .catch(error => {
        assertNotReached(error.toString());
        callback();
      });
}

/**
 * Tests that getFileTasks() should always return the promise whose FileTasks
 * correspond to FileSelectionHandler.selection at the time getFileTasks() is
 * called.
 */
export function testGetFileTasksShouldNotReturnObsoletePromise(callback) {
  const selectionHandler = new FakeFileSelectionHandler();

  const fileSystem = new MockFileSystem('volumeId');
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
  const taskController = createTaskController(selectionHandler);

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
        callback();
      });
}

/**
 * Tests that changing the file selection during a getFileTasks() call causes
 * the getFileTasks() promise to reject.
 */
export function testGetFileTasksShouldNotCacheRejectedPromise(callback) {
  const selectionHandler = new FakeFileSelectionHandler();

  const fileSystem = new MockFileSystem('volumeId');
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
  const taskController = createTaskController(selectionHandler);

  // Setup the selection handler computeAdditionalCallback to change the file
  // selection during the getFileTasks() call.
  selectionHandler.computeAdditionalCallback = () => {
    selectionHandler.updateSelection(
        [MockFileEntry.create(fileSystem, '/test.png')], ['image/png']);
  };

  taskController.getFileTasks().then(
      tasks => {
        assertNotReached('Fail: getFileTasks promise should be rejected');
        callback();
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
              callback();
            });
      });
}
