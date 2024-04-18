// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {Crostini} from '../background/js/crostini.js';
import {MockVolumeManager} from '../background/js/mock_volume_manager.js';
import type {VolumeInfo} from '../background/js/volume_info.js';
import type {FilesAppDirEntry} from '../common/js/files_app_entry_types.js';
import type {DirectoryTreeNamingController} from '../foreground/js/directory_tree_naming_controller.js';
import {FakeFileSelectionHandler} from '../foreground/js/fake_file_selection_handler.js';
import type {MetadataModel} from '../foreground/js/metadata/metadata_model.js';
import {MockMetadataModel} from '../foreground/js/metadata/mock_metadata.js';
import {createFakeDirectoryModel} from '../foreground/js/mock_directory_model.js';
import type {TaskController} from '../foreground/js/task_controller.js';
import type {FileManagerUI} from '../foreground/js/ui/file_manager_ui.js';

import {type EntryMetadata, updateMetadata} from './ducks/all_entries.js';
import {changeDirectory, updateDirectoryContent, updateSelection} from './ducks/current_directory.js';
import {DialogType, type FileKey, PropStatus, type State} from './state.js';
import {getEmptyState, getStore, type StateSelector, type Store, waitForState} from './store.js';

/**
 * Compares 2 State objects and fails with nicely formatted message when it
 * fails.
 */
export function assertStateEquals(want: any, got: any) {
  assertDeepEquals(
      want, got,
      `\nWANT:\n${JSON.stringify(want, null, 2)}\nGOT:\n${
          JSON.stringify(got, null, 2)}\n\n`);
}

/**
 * Returns the `allEntries` size of the passed State.
 */
export function allEntriesSize(state: State): number {
  return Object.keys(state.allEntries).length;
}

/**
 * Compares the current state's allEntries field to the expected list. Fails
 * with a nicely formatted message if there's a mismatch.
 */
export function assertAllEntriesEqual(store: Store, want: FileKey[]) {
  const got = Object.keys(store.getState().allEntries).sort();
  assertDeepEquals(
      want, got,
      `\nWANT:\n${JSON.stringify(want, null, 2)}\nGOT:\n${
          JSON.stringify(got, null, 2)}\n\n`);
}

/** Change the directory in the store. */
export function cd(store: Store, directory: DirectoryEntry|FilesAppDirEntry) {
  store.dispatch(changeDirectory(
      {to: directory, toKey: directory.toURL(), status: PropStatus.SUCCESS}));
}

/** Updates the selection in the store. */
export function changeSelection(store: Store, entries: Entry[]) {
  store.dispatch(updateSelection({
    selectedKeys: entries.map(e => e.toURL()),
    entries,
  }));
}

/** Updates the metadata in the store. */
export function updMetadata(store: Store, metadata: EntryMetadata[]) {
  store.dispatch(updateMetadata({metadata}));
}

/** Updates the directory content in the store. */
export function updateContent(store: Store, entries: Entry[]) {
  store.dispatch(updateDirectoryContent({entries, status: PropStatus.SUCCESS}));
}

/**
 * Store state might include objects (e.g. Entry type) which can not stringified
 * by JSON, here we implement a custom "replacer" to handle that.
 */
function jsonStringifyStoreState(state: any): string {
  return JSON.stringify(state, (key, value) => {
    // Currently only the key with "entry" (inside `FileData`) can't be
    // stringified, we just return its URL.
    if (key === 'entry') {
      return value.toURL();
    }
    return value;
  }, 2);
}

/**
 * Waits for a part of the Store to be in the expected state.
 *
 * Waits a maximum of 10 seconds, since in the unittest the Store manipulation
 * has all async APIs mocked.
 *
 * Usage:
 * let want: StoreSomething = {somePartOfStore: 'desired state'};
 * store.dispatch(someActionsProducer(...));
 * await waitDeepEquals(store, want, (state) => state.something);
 */
export async function waitDeepEquals(
    store: Store, want: any, stateSelection: StateSelector) {
  let got: any;
  const timeout = new Promise((_, reject) => {
    setTimeout(() => {
      reject(new Error(`waitDeepEquals timed out.\nWANT:\n${
          jsonStringifyStoreState(
              want)}\nGOT:\n${jsonStringifyStoreState(got)}`));
    }, 10000);
  });

  const checker = waitForState(store, (state) => {
    try {
      got = stateSelection(state);
      assertDeepEquals(want, got);
      return true;
    } catch (error: any) {
      if (error.constructor?.name === 'AssertionError') {
        return false;
      }
      throw error;
    }
  });

  await Promise.race([checker, timeout]);
}

/** Setup store and initialize it with empty state. */
export function setupStore(initialState: State = getEmptyState()): Store {
  const store = getStore();
  store.init(initialState);
  return store;
}

/**
 * Setup fileManager dependencies on window object.
 */
export function setUpFileManagerOnWindow() {
  const volumeManager = new MockVolumeManager();
  // Keys are defined in file_manager.d.ts
  window.fileManager = {
    volumeManager: volumeManager,
    metadataModel: new MockMetadataModel({}) as unknown as MetadataModel,
    ui: {} as unknown as FileManagerUI,
    crostini: {} as unknown as Crostini,
    selectionHandler: new FakeFileSelectionHandler(),
    taskController: {} as unknown as TaskController,
    dialogType: DialogType.FULL_PAGE,
    directoryModel: createFakeDirectoryModel(),
    fileFilter: {
      filter() {
        return true;
      },
    },
    directoryTreeNamingController: {} as unknown as
        DirectoryTreeNamingController,
    getLastVisitedUrl() {
      return '';
    },
    getTranslatedString(_id: string) {
      return '';
    },
    onUnloadForTest() {},
  };
}

/**
 * Create a fake VolumeMetadata with VolumeInfo, VolumeInfo can be created by
 * MockVolumeManager.createMockVolumeInfo.
 */
export function createFakeVolumeMetadata(
    volumeInfo: VolumeInfo,
    ): chrome.fileManagerPrivate.VolumeMetadata {
  return {
    volumeId: volumeInfo.volumeId,
    volumeType: volumeInfo.volumeType as chrome.fileManagerPrivate.VolumeType,
    profile: {
      ...volumeInfo.profile,
      profileId: '',
    },
    configurable: volumeInfo.configurable,
    watchable: volumeInfo.watchable,
    source: volumeInfo.source as chrome.fileManagerPrivate.Source,
    volumeLabel: volumeInfo.label,
    fileSystemId: undefined,
    providerId: volumeInfo.providerId,
    sourcePath: undefined,
    deviceType: volumeInfo.deviceType as chrome.fileManagerPrivate.DeviceType,
    devicePath: volumeInfo.devicePath,
    isParentDevice: undefined,
    isReadOnly: volumeInfo.isReadOnly,
    isReadOnlyRemovableDevice: volumeInfo.isReadOnlyRemovableDevice,
    hasMedia: false,
    mountCondition: undefined,
    mountContext: undefined,
    diskFileSystemType: volumeInfo.diskFileSystemType,
    iconSet: volumeInfo.iconSet,
    driveLabel: volumeInfo.driveLabel,
    remoteMountPath: volumeInfo.remoteMountPath,
    hidden: false,
    vmType: volumeInfo.vmType,
  };
}
