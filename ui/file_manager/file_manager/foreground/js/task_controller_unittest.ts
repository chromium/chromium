// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertDeepEquals, assertEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {createCrostiniForTest} from '../../background/js/mock_crostini.js';
import type {ProgressCenter} from '../../background/js/progress_center.js';
import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {crInjectTypeAndInit} from '../../common/js/cr_ui.js';
import {queryDecoratedElement} from '../../common/js/dom_utils.js';
import {isSameEntries} from '../../common/js/entry_utils.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import type {MockFileSystem} from '../../common/js/mock_entry.js';
import {MockFileEntry} from '../../common/js/mock_entry.js';
import {descriptorEqual} from '../../common/js/util.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {changeDirectory} from '../../state/ducks/current_directory.js';
import {setUpFileManagerOnWindow} from '../../state/for_tests.js';
import {PropStatus, type State} from '../../state/state.js';
import {getEmptyState, getStore, waitForState} from '../../state/store.js';

import type {DirectoryModel} from './directory_model.js';
import type {FileSelectionHandler} from './file_selection.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';
import type {MetadataUpdateController} from './metadata_update_controller.js';
import {TaskController} from './task_controller.js';
import {ComboButton} from './ui/combobutton.js';
import {Command} from './ui/command.js';
import type {FileManagerUI} from './ui/file_manager_ui.js';
import {FilesMenuItem} from './ui/files_menu.js';

/** Mock chrome APIs.  */
let mockChrome: any;

/** VolumeInfo for Downloads volume */
let downloads: VolumeInfo;

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
  document.body.innerHTML = getTrustedHTML`
<command id="default-task">
<command id="open-with">
<cr-menu id="tasks-menu">
  <cr-menu-item id="default-task-menu-item" command="#default-task">
  </cr-menu-item>
</cr-menu>
<cr-button id="tasks" menu="#tasks-menu"> Open </cr-button>
`;

  // Initialize Command with the <command>s.
  for (const command of document.querySelectorAll<Command>('command')) {
    crInjectTypeAndInit(command, Command);
  }

  setUpFileManagerOnWindow();
  const volumeManager = window.fileManager.volumeManager;

  downloads = volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;

  const store = getStore();
  store.init(getEmptyState());

  store.dispatch(changeDirectory({
    to: downloads.displayRoot,
    toKey: downloads.displayRoot.toURL(),
    status: PropStatus.SUCCESS,
  }));
}

function createTaskController(fileSelectionHandler: FileSelectionHandler):
    TaskController {
  const taskController = new TaskController(
      {
        getLocationInfo: function(_entry: Entry) {
          return RootType.DRIVE;
        },
        getDriveConnectionState: function() {
          return 'ONLINE';
        },
        getVolumeInfo: function() {
          return {
            volumeType: VolumeType.DRIVE,
          };
        },
      } as unknown as VolumeManager,
      {
        taskMenuButton: queryDecoratedElement('#tasks', ComboButton),
        defaultTaskMenuItem:
            queryDecoratedElement('#default-task-menu-item', FilesMenuItem),
        speakA11yMessage: (_text: string) => {},
        listContainer: document.createElement('div'),
        tasksSeparator: document.createElement('hr'),
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
    getFileTaskCalledEntries_: [],
    getFileTasks: function(
        entries: Entry[], _sourceUrls: string[],
        callback: (tasks: any) => void) {
      mockChrome.fileManagerPrivate.getFileTaskCalledCount_++;
      mockChrome.fileManagerPrivate.getFileTaskCalledEntries_.push(entries);
      const fileTasks = ([
        {
          descriptor: {
            appId: 'handler-extension-id',
            taskType: 'file',
            actionId: 'open',
          },
          isDefault: false,
        },
        {
          descriptor: {
            appId: 'handler-extension-id',
            taskType: 'file',
            actionId: 'play',
          },
          isDefault: true,
        },
      ]);
      setTimeout(callback.bind(null, {tasks: fileTasks}), 0);
    },
  };
}

/**
 * Tests that executeEntryTask() runs the expected task.
 */
export async function testExecuteEntryTask() {
  const selectionHandler = window.fileManager.selectionHandler;

  const fileSystem = downloads.fileSystem as MockFileSystem;
  fileSystem.entries['/test.png'] =
      MockFileEntry.create(fileSystem, '/test.png');
  const taskController = createTaskController(selectionHandler);

  const testEntry = fileSystem.entries['/test.png'] as Entry;
  taskController.executeEntryTask(testEntry);

  const descriptor =
      await new Promise<chrome.fileManagerPrivate.FileTaskDescriptor>(
          (resolve) => {
            chrome.fileManagerPrivate.executeTask = resolve;
          });

  assert(descriptorEqual(
      {appId: 'handler-extension-id', taskType: 'file', actionId: 'play'},
      descriptor));
}

/**
 * Tests that getFileTasks() does not call .fileManagerPrivate.getFileTasks()
 * multiple times when the selected entries are not changed.
 */
export async function testGetFileTasksShouldNotBeCalledMultipleTimes() {
  const selectionHandler = window.fileManager.selectionHandler;
  const store = getStore();
  const taskController =
      createTaskController(selectionHandler as unknown as FileSelectionHandler);

  const fileSystem = downloads.fileSystem as MockFileSystem;
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png'], store);

  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 0);

  let tasks = await taskController.getFileTasks();
  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 1);
  assert(isSameEntries(tasks.entries, selectionHandler.selection.entries));
  // NOTE: It updates to the same file.
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png'], store);
  tasks = await taskController.getFileTasks();
  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 2);
  assert(isSameEntries(tasks.entries, selectionHandler.selection.entries));

  // The update above generates a new selection, even though it's updating to
  // the same file, this causes a new private API call.
  // The Store ActionsProducer debounces multiple concurrent calls for the same
  // file so in practice this shouldn't be a problem.
  const promise1 = taskController.getFileTasks();
  // Await 0ms to give time to promise1 to initialize.
  await new Promise(r => setTimeout(r));
  const promise2 = taskController.getFileTasks();
  const [tasks1, tasks2] = await Promise.all([promise1, promise2]);
  assertDeepEquals(
      tasks1.entries, tasks2.entries,
      'both tasks should have test.png as entry');
  assertTrue(tasks1 === tasks2);
  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 2);
  assert(isSameEntries(tasks1.entries, selectionHandler.selection.entries));

  // Check concurrent calls right after changing the selection.
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/hello.txt')], ['text/plain'], store);
  const promise3 = taskController.getFileTasks();
  // Await 0ms to give time to promise3 to initialize.
  await new Promise(r => setTimeout(r));
  const promise4 = taskController.getFileTasks();
  const [tasks3, tasks4] = await Promise.all([promise3, promise4]);
  assertDeepEquals(
      tasks3.entries, tasks4.entries,
      'both tasks should have hello.txt as entry');
  assert(isSameEntries(tasks3.entries, selectionHandler.selection.entries));
  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 3);
}


/**
 * Tests the file tasks in the store are updated each time the selected entries
 * are changed, including when there are no selected entries.
 */
export async function testFileTasksUpdatedAfterSelectionChange() {
  const selectionHandler = window.fileManager.selectionHandler;
  const store = getStore();
  const fileSystem = downloads.fileSystem as MockFileSystem;

  // Check no file tasks initially in the store.
  await waitForState(
      store,
      (st: State) =>
          st.currentDirectory?.selection.fileTasks.tasks !== undefined &&
          st.currentDirectory?.selection.fileTasks.tasks.length === 0);

  // Select entry.
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png'], store);
  // Check file tasks in store.
  await waitForState(
      store,
      (st: State) =>
          st.currentDirectory?.selection.fileTasks.tasks !== undefined &&
          st.currentDirectory?.selection.fileTasks.tasks.length > 0);

  // Select blank.
  selectionHandler.updateSelection([], [], store);
  // Check no file tasks in the store.
  await waitForState(
      store,
      (st: State) =>
          st.currentDirectory?.selection.fileTasks.tasks !== undefined &&
          st.currentDirectory?.selection.fileTasks.tasks.length === 0);
}

/**
 * Tests that getFileTasks() should always return the promise whose FileTasks
 * correspond to FileSelectionHandler.selection at the time getFileTasks() is
 * called.
 */
export async function testGetFileTasksShouldNotReturnObsoletePromise() {
  const selectionHandler = window.fileManager.selectionHandler;
  const store = getStore();
  const fileSystem = downloads.fileSystem as MockFileSystem;
  const taskController =
      createTaskController(selectionHandler as unknown as FileSelectionHandler);
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png'], store);

  let tasks = await taskController.getFileTasks();
  assert(isSameEntries(tasks.entries, selectionHandler.selection.entries));
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/testtest.jpg')], ['image/jpeg'],
      store);
  try {
    tasks = await taskController.getFileTasks();
    assert(isSameEntries(tasks.entries, selectionHandler.selection.entries));
  } catch (error) {
    assertNotReached(error!.toString());
  }
}

/**
 * Tests that changing the file selection during a getFileTasks() call causes
 * the getFileTasks() promise to reject.
 */
export async function testGetFileTasksShouldNotCacheRejectedPromise() {
  const selectionHandler = window.fileManager.selectionHandler;
  const store = getStore();
  const fileSystem = downloads.fileSystem as MockFileSystem;
  const taskController =
      createTaskController(selectionHandler as unknown as FileSelectionHandler);

  // Setup the selection handler computeAdditionalCallback to change the file
  // selection during the getFileTasks() call.
  let selectionUpdated = false;
  selectionHandler.computeAdditionalCallback = () => {
    // Update the selection.
    selectionHandler.updateSelection(
        [MockFileEntry.create(fileSystem, '/other-file.png')], ['image/png'],
        store);
    selectionUpdated = true;
  };

  // Set the initial selection.
  selectionHandler.updateSelection(
      [MockFileEntry.create(fileSystem, '/test.png')], ['image/png'], store);

  const tasks = await taskController.getFileTasks();
  assertTrue(selectionUpdated, 'selection should update');

  // Clears the selection handler computeAdditionalCallback so that the
  // promise won't be rejected during the getFileTasks() call.
  selectionHandler.computeAdditionalCallback = () => {};
  const callsToApi = mockChrome.fileManagerPrivate.getFileTaskCalledCount_;

  // Calling getFileTasks() in the same selection should not call the
  // private API.
  const tasks2 = await taskController.getFileTasks();
  assert(isSameEntries(tasks.entries, tasks2.entries));
  assert(isSameEntries(tasks2.entries, selectionHandler.selection.entries));

  // No more calls to the private API.
  assertEquals(
      callsToApi, mockChrome.fileManagerPrivate.getFileTaskCalledCount_);
  assertEquals(
      0,
      mockChrome.fileManagerPrivate.getFileTaskCalledEntries_
          .filter(
              (entries: Entry[]) =>
                  entries.filter((e: Entry) => e.name === '/test.png').length)
          .length,
      'Should have NO calls to private API for the initial selection');
}
