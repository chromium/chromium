// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {fakeMyFilesVolumeId} from '../../background/js/mock_volume_manager.js';
import type {MockFileSystem} from '../../common/js/mock_entry.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {FakeFileSelectionHandler} from '../../foreground/js/fake_file_selection_handler.js';
import {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import {type CurrentDirectory, type FileTasks, FileTaskType, PropStatus} from '../../state/state.js';
import {clearCachedEntries} from '../ducks/all_entries.js';
import {fetchFileTasks} from '../ducks/current_directory.js';
import {allEntriesSize, assertAllEntriesEqual, assertStateEquals, setUpFileManagerOnWindow, setupStore, updateContent, updMetadata, waitDeepEquals} from '../for_tests.js';
import {getFilesData, type Store} from '../store.js';

import {changeDirectory, updateSelection} from './current_directory.js';


let fileSystem: MockFileSystem;

export function setUp() {
  // changeDirectory() reducer uses the VolumeManager.
  setUpFileManagerOnWindow();
  window.fileManager.selectionHandler = new FakeFileSelectionHandler();

  fileSystem = window.fileManager.volumeManager
                   .getCurrentProfileVolumeInfo(
                       VolumeType.DOWNLOADS)!.fileSystem as MockFileSystem;
  fileSystem.populate([
    '/dir-1/',
    '/dir-2/sub-dir/',
    '/dir-2/file.txt',
    '/dir-3/',
  ]);
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
  const dir1 = fileSystem.entries['/dir-1'] as DirectoryEntry;
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
    content: {
      keys: [],
      status: PropStatus.SUCCESS,
    },
    selection: {
      keys: [],
      dirCount: 0,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: 0,
      fileTasks: {
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        tasks: [],
        status: PropStatus.SUCCESS,
      },
    },
    hasDlpDisabledFiles: false,
  };
  assertStateEquals(want, store.getState().currentDirectory);

  // Finish the directory change.
  store.dispatch(changeDirectory(
      {toKey: dir1.toURL(), to: dir1, status: PropStatus.SUCCESS}));

  want.status = PropStatus.SUCCESS;
  want.key = dir1.toURL();
  want.rootType = RootType.DOWNLOADS;
  want.pathComponents = [
    {name: 'Downloads', label: 'Downloads', key: fileSystem.root.toURL()},
    {name: dir1.name, label: dir1.name, key: dir1.toURL()},
  ];
  assertStateEquals(want, store.getState().currentDirectory);
}

export function testChangeDirectoryTwice() {
  const store = setupStore();
  const dir2 = fileSystem.entries['/dir-2'] as DirectoryEntry;
  const subDir = fileSystem.entries['/dir-2/sub-dir'] as DirectoryEntry;
  const dir1 = fileSystem.entries['/dir-1'] as DirectoryEntry;
  cd(store, dir2);
  updateContent(store, [subDir]);
  changeSelection(store, [subDir]);
  cd(store, dir1);
  const want: CurrentDirectory = {
    key: dir1.toURL(),
    status: PropStatus.SUCCESS,
    rootType: RootType.DOWNLOADS,
    pathComponents: [
      {name: 'Downloads', label: 'Downloads', key: fileSystem.root.toURL()},
      {name: dir1.name, label: dir1.name, key: dir1.toURL()},
    ],
    content: {
      keys: [],
      status: PropStatus.SUCCESS,
    },
    selection: {
      keys: [],
      dirCount: 0,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: 0,
      fileTasks: {
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        tasks: [],
        status: PropStatus.SUCCESS,
      },
    },
    hasDlpDisabledFiles: false,
  };

  assertStateEquals(want, store.getState().currentDirectory);
}

export function testChangeSelection() {
  const store = setupStore();
  const dir2 = fileSystem.entries['/dir-2'] as DirectoryEntry;
  const subDir = fileSystem.entries['/dir-2/sub-dir'] as DirectoryEntry;
  const file = fileSystem.entries['/dir-2/file.txt'] as DirectoryEntry;
  cd(store, dir2);
  updateContent(store, [subDir, file]);
  changeSelection(store, [subDir]);

  const want: CurrentDirectory = {
    key: dir2.toURL(),
    status: PropStatus.SUCCESS,
    rootType: RootType.DOWNLOADS,
    pathComponents: [
      {name: 'Downloads', label: 'Downloads', key: fileSystem.root.toURL()},
      {name: dir2.name, label: dir2.name, key: dir2.toURL()},
    ],
    content: {
      keys: [subDir.toURL(), file.toURL()],
      status: PropStatus.SUCCESS,
    },
    selection: {
      keys: [subDir.toURL()],
      dirCount: 1,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: 1,
      fileTasks: {
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        tasks: [],
        status: PropStatus.STARTED,
      },
    },
    hasDlpDisabledFiles: false,
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
  want.selection.offlineCachedCount = 2;
  assertStateEquals(want, store.getState().currentDirectory);
}

export function testChangeDirectoryContent() {
  const store = setupStore();
  const dir2 = fileSystem.entries['/dir-2'] as DirectoryEntry;
  const subDir = fileSystem.entries['/dir-2/sub-dir'] as DirectoryEntry;
  const file = fileSystem.entries['/dir-2/file.txt']!;
  cd(store, dir2);

  const want: CurrentDirectory = {
    key: dir2.toURL(),
    status: PropStatus.SUCCESS,
    rootType: RootType.DOWNLOADS,
    pathComponents: [
      {name: 'Downloads', label: 'Downloads', key: fileSystem.root.toURL()},
      {name: dir2.name, label: dir2.name, key: dir2.toURL()},
    ],
    content: {
      keys: [],
      status: PropStatus.SUCCESS,
    },
    selection: {
      keys: [],
      dirCount: 0,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: 0,
      fileTasks: {
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        tasks: [],
        status: PropStatus.SUCCESS,
      },
    },
    hasDlpDisabledFiles: false,
  };
  assertStateEquals(want, store.getState().currentDirectory);
  assertEquals(
      1, allEntriesSize(store.getState()), 'only dir-2 should be cached');
  assertAllEntriesEqual(store, [`filesystem:${fakeMyFilesVolumeId}/dir-2`]);

  // Send the content update:
  updateContent(store, [subDir]);
  want.content.keys = [subDir.toURL()];
  assertStateEquals(want, store.getState().currentDirectory);
  assertEquals(
      2, allEntriesSize(store.getState()),
      'dir-2 and dir-2/sub-dir should be cached');
  assertAllEntriesEqual(store, [
    `filesystem:${fakeMyFilesVolumeId}/dir-2`,
    `filesystem:${fakeMyFilesVolumeId}/dir-2/sub-dir`,
  ]);

  // Send another content update - it should replace the original:
  updateContent(store, [file]);
  want.content.keys = [file.toURL()];
  assertStateEquals(want, store.getState().currentDirectory);
  assertEquals(
      3, allEntriesSize(store.getState()),
      'dir-2, dir-2/sub-dir and dir-2/file should be cached');
  assertAllEntriesEqual(store, [
    `filesystem:${fakeMyFilesVolumeId}/dir-2`,
    `filesystem:${fakeMyFilesVolumeId}/dir-2/file.txt`,
    `filesystem:${fakeMyFilesVolumeId}/dir-2/sub-dir`,
  ]);

  // Clear cached entries: only dir2 and file should be kept.
  store.dispatch(clearCachedEntries());
  assertEquals(
      2, allEntriesSize(store.getState()),
      'only dir-2 and dir-2/file should still be cached');
  assertAllEntriesEqual(store, [
    `filesystem:${fakeMyFilesVolumeId}/dir-2`,
    `filesystem:${fakeMyFilesVolumeId}/dir-2/file.txt`,
  ]);
}

export function testComputeHasDlpDisabledFiles() {
  const store = setupStore();
  const dir2 = fileSystem.entries['/dir-2'] as DirectoryEntry;
  const subDir = fileSystem.entries['/dir-2/sub-dir'] as DirectoryEntry;
  const file = fileSystem.entries['/dir-2/file.txt']!;
  cd(store, dir2);
  updateContent(store, [subDir, file]);

  const want: CurrentDirectory = {
    key: dir2.toURL(),
    status: PropStatus.SUCCESS,
    rootType: RootType.DOWNLOADS,
    pathComponents: [
      {name: 'Downloads', label: 'Downloads', key: fileSystem.root.toURL()},
      {name: dir2.name, label: dir2.name, key: dir2.toURL()},
    ],
    content: {
      keys: [subDir.toURL(), file.toURL()],
      status: PropStatus.SUCCESS,
    },
    selection: {
      keys: [],
      dirCount: 0,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: 0,
      fileTasks: {
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        tasks: [],
        status: PropStatus.SUCCESS,
      },
    },
    hasDlpDisabledFiles: false,
  };
  assertStateEquals(want, store.getState().currentDirectory);

  // Send the metadata update:
  const metadata: MetadataItem = new MetadataItem();
  metadata.isRestrictedForDestination = true;
  updMetadata(store, [{entry: file, metadata}]);
  want.hasDlpDisabledFiles = true;
  assertStateEquals(want, store.getState().currentDirectory);

  // Send a content update and "remove" the disabled file:
  updateContent(store, [subDir]);
  want.content.keys = [subDir.toURL()];
  want.hasDlpDisabledFiles = false;
  assertStateEquals(want, store.getState().currentDirectory);
}

function mockGetFileTasks(tasks: chrome.fileManagerPrivate.FileTask[]) {
  const mocked =
      (_entries: Entry[], _sourceUrls: string[],
       callback: (resultingTasks: chrome.fileManagerPrivate.ResultingTasks) =>
           void) => {
        setTimeout(callback, 0, {tasks});
      };
  chrome.fileManagerPrivate.getFileTasks = mocked;
}

const fakeFileTasks: chrome.fileManagerPrivate.FileTask = {
  descriptor: {
    appId: 'handler-extension-id1',
    taskType: 'app',
    actionId: 'any',
  },
  isDefault: false,
  isGenericFileHandler: false,
  title: 'app 1',
  iconUrl: undefined,
  isDlpBlocked: false,
};

export async function testFetchTasks(done: () => void) {
  const store = setupStore();
  const dir2 = fileSystem.entries['/dir-2'] as DirectoryEntry;
  const subDir = fileSystem.entries['/dir-2/sub-dir'] as DirectoryEntry;
  const file = fileSystem.entries['/dir-2/file.txt']!;
  cd(store, dir2);
  changeSelection(store, [subDir, file]);

  const filesData = getFilesData(store.getState(), [file.toURL()]);
  const want: FileTasks = {
    policyDefaultHandlerStatus: undefined,
    defaultTask: undefined,
    tasks: [],
    status: PropStatus.SUCCESS,
  };

  // Mock returning 0 tasks, returns SUCCESS and empty tasks.
  mockGetFileTasks([]);
  store.dispatch(fetchFileTasks(filesData));
  await waitDeepEquals(store, want, (state) => {
    return state.currentDirectory?.selection.fileTasks;
  });

  // Mock the private API results with one task which is the default task.
  mockGetFileTasks([fakeFileTasks]);
  want.tasks = [
    {
      iconType: '',
      descriptor: {
        appId: 'handler-extension-id1',
        taskType: FileTaskType.APP,
        actionId: 'any',
      },
      isDefault: false,
      isGenericFileHandler: false,
      title: 'app 1',
      iconUrl: undefined,
      isDlpBlocked: false,
    },
  ];
  want.defaultTask = {...want.tasks[0]!};
  store.dispatch(fetchFileTasks(filesData));
  await waitDeepEquals(
      store, want, (state) => state.currentDirectory?.selection.fileTasks);

  // Mock the API task as genericFileHandler, so it shouldn't be a default task.
  const genericTask = {
    ...fakeFileTasks,
    isGenericFileHandler: true,
  };
  mockGetFileTasks([genericTask]);
  want.tasks[0]!.isGenericFileHandler = true;
  want.defaultTask = undefined;
  store.dispatch(fetchFileTasks(filesData));
  await waitDeepEquals(
      store, want, (state) => state.currentDirectory?.selection.fileTasks);

  done();
}
