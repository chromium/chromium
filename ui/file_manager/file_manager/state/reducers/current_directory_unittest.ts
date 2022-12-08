// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {DialogType} from '../../common/js/dialog_type.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Crostini} from '../../externs/background/crostini.js';
import {CurrentDirectory, PropStatus} from '../../externs/ts/state.js';
import {FileSelectionHandler} from '../../foreground/js/file_selection.js';
import {MetadataModel} from '../../foreground/js/metadata/metadata_model.js';
import {MockMetadataModel} from '../../foreground/js/metadata/mock_metadata.js';
import {TaskController} from '../../foreground/js/task_controller.js';
import {changeDirectory, updateSelection} from '../actions/current_directory.js';
import {assertStateEquals} from '../for_tests.js';
import {getEmptyState, getStore, Store} from '../store.js';

let fileSystem: MockFileSystem;

export function setUp() {
  // changeDirectory() reducer uses the VolumeManager.
  const volumeManager = new MockVolumeManager();
  window.fileManager = {
    volumeManager: volumeManager,
    metadataModel: new MockMetadataModel({}) as unknown as MetadataModel,
    crostini: {} as unknown as Crostini,
    selectionHandler: {} as unknown as FileSelectionHandler,
    taskController: {} as unknown as TaskController,
    dialogType: DialogType.FULL_PAGE,
  };

  fileSystem = volumeManager.getCurrentProfileVolumeInfo(
                                'downloads')!.fileSystem as MockFileSystem;
  fileSystem.populate([
    '/dir-1/',
    '/dir-2/sub-dir/',
    '/dir-2/file.txt',
    '/dir-3/',
  ]);
}

function setupStore(): Store {
  const store = getStore();
  store.init(getEmptyState());
  return store;
}

function cd(store: Store, directory: DirectoryEntry) {
  store.dispatch(changeDirectory(
      {to: directory, toKey: directory.toURL(), status: PropStatus.SUCCESS}));
}
function changeSelection(store: Store, entries: Entry[]) {
  store.dispatch(updateSelection({
    selectedKeys: entries.map(e => e.toURL()),
    entries,
  }));
}

export function testChangeDirectoryFromEmpty() {
  const store = setupStore();
  const dir1 = fileSystem.entries['/dir-1'];
  // The current directory starts empty.
  assertTrue(store.getState().currentDirectory?.key === undefined);

  // Change Directory happens in 2 steps.
  // First step, start the directory change.
  store.dispatch(changeDirectory({toKey: dir1.toURL()}));
  const want: CurrentDirectory = {
    key: dir1.toURL(),
    status: PropStatus.STARTED,
    rootType: undefined,
    pathComponents: [],
    selection: {
      keys: [],
      dirCount: 0,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: undefined,
      fileTasks: {
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        tasks: [],
        status: PropStatus.SUCCESS,
      },
    },
  };
  assertStateEquals(want, store.getState().currentDirectory);

  // Finish the directory change.
  store.dispatch(changeDirectory(
      {toKey: dir1.toURL(), to: dir1, status: PropStatus.SUCCESS}));

  want.status = PropStatus.SUCCESS;
  want.key = dir1.toURL();
  want.rootType = VolumeManagerCommon.RootType.DOWNLOADS;
  want.pathComponents = [
    {name: 'Downloads', label: 'Downloads', key: fileSystem.root.toURL()},
    {name: dir1.name, label: dir1.name, key: dir1.toURL()},
  ];
  assertStateEquals(want, store.getState().currentDirectory);
}

export function testChangeDirectoryTwice() {
  const store = setupStore();
  const dir2 = fileSystem.entries['/dir-2'];
  const subDir = fileSystem.entries['/dir-2/sub-dir'];
  const dir1 = fileSystem.entries['/dir-1'];
  cd(store, dir2);
  changeSelection(store, [subDir]);
  cd(store, dir1);
  const want: CurrentDirectory = {
    key: dir1.toURL(),
    status: PropStatus.SUCCESS,
    rootType: VolumeManagerCommon.RootType.DOWNLOADS,
    pathComponents: [
      {name: 'Downloads', label: 'Downloads', key: fileSystem.root.toURL()},
      {name: dir1.name, label: dir1.name, key: dir1.toURL()},
    ],
    selection: {
      keys: [],
      dirCount: 0,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: undefined,
      fileTasks: {
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        tasks: [],
        status: PropStatus.SUCCESS,
      },
    },
  };

  assertStateEquals(want, store.getState().currentDirectory);
}


export function testChangeSelection() {
  const store = setupStore();
  const dir2 = fileSystem.entries['/dir-2'];
  const subDir = fileSystem.entries['/dir-2/sub-dir'];
  const file = fileSystem.entries['/dir-2/file.txt'];
  cd(store, dir2);
  changeSelection(store, [subDir]);

  const want: CurrentDirectory = {
    key: dir2.toURL(),
    status: PropStatus.SUCCESS,
    rootType: VolumeManagerCommon.RootType.DOWNLOADS,
    pathComponents: [
      {name: 'Downloads', label: 'Downloads', key: fileSystem.root.toURL()},
      {name: dir2.name, label: dir2.name, key: dir2.toURL()},
    ],
    selection: {
      keys: [subDir.toURL()],
      dirCount: 1,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: undefined,
      fileTasks: {
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        tasks: [],
        status: PropStatus.STARTED,
      },
    },
  };
  assertStateEquals(want, store.getState().currentDirectory);

  // Change the selection for a completely different one:
  changeSelection(store, [file]);
  want.selection.keys = [file.toURL()];
  want.selection.dirCount = 0;
  want.selection.fileCount = 1;
  assertStateEquals(want, store.getState().currentDirectory);

  // Append to the selection.
  changeSelection(store, [file, subDir]);
  want.selection.keys = [file.toURL(), subDir.toURL()];
  want.selection.dirCount = 1;
  want.selection.fileCount = 1;
  assertStateEquals(want, store.getState().currentDirectory);
}
