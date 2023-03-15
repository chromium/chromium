// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../background/js/mock_volume_manager.js';
import {DialogType} from '../common/js/dialog_type.js';
import {VolumeManagerCommon} from '../common/js/volume_manager_types.js';
import {Crostini} from '../externs/background/crostini.js';
import {FilesAppDirEntry} from '../externs/files_app_entry_interfaces.js';
import {FileData, FileKey, PropStatus, State, Volume} from '../externs/ts/state.js';
import {constants} from '../foreground/js/constants.js';
import {FileSelectionHandler} from '../foreground/js/file_selection.js';
import {MetadataItem} from '../foreground/js/metadata/metadata_item.js';
import {MetadataModel} from '../foreground/js/metadata/metadata_model.js';
import {MockMetadataModel} from '../foreground/js/metadata/mock_metadata.js';
import {createFakeDirectoryModel} from '../foreground/js/mock_directory_model.js';
import {TaskController} from '../foreground/js/task_controller.js';

import {EntryMetadata, updateMetadata} from './actions/all_entries.js';
import {changeDirectory, updateDirectoryContent, updateSelection} from './actions/current_directory.js';
import {getEmptyState, getStore, StateSelector, Store, waitForState} from './store.js';

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
  store.dispatch(updateDirectoryContent({entries}));
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
      reject(new Error(`waitDeepEquals timed out waiting for \n${want}`));
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
      console.log(error.stack);
      console.error(error);
      throw error;
    }
  });

  await Promise.race([checker, timeout]);
}

/** Setup store and initialize it with empty state. */
export function setupStore(): Store {
  const store = getStore();
  store.init(getEmptyState());
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
    crostini: {} as unknown as Crostini,
    selectionHandler: {} as unknown as FileSelectionHandler,
    taskController: {} as unknown as TaskController,
    dialogType: DialogType.FULL_PAGE,
    directoryModel: createFakeDirectoryModel(),
  };
}

/**
 * Create a fake FileData with partial information. Only the fields listed are
 * required, other fields are optional.
 */
export function createFakeFileData(
    partialFileData: Pick<FileData, 'entry'|'label'|'type'>&
    Partial<Omit<FileData, 'entry'|'label'|'type'>>,
    ): FileData {
  const defaultFileData = {
    icon: constants.ICON_TYPES.FOLDER,
    volumeType: null,
    isDirectory: true,
    metadata: {} as MetadataItem,
    isRootEntry: false,
    isEjectable: false,
    shouldDelayLoadingChildren: false,
    children: [],
    expanded: false,
  };
  return {
    ...defaultFileData,
    ...partialFileData,
  };
}

/** Create a fake VolumeMetadata. */
export function createFakeVolumeMetadata(
    partialMetadata:
        Pick<chrome.fileManagerPrivate.VolumeMetadata, 'volumeId'|'volumeType'>&
    Partial<Omit<
        chrome.fileManagerPrivate.VolumeMetadata, 'volumeId'|'volumeType'>>,
    ): chrome.fileManagerPrivate.VolumeMetadata {
  const defaultMetadata = {
    profile: {
      displayName: 'foobar@chromium.org',
      isCurrentProfile: true,
      profileId: '',
    },
    configurable: false,
    watchable: true,
    source: VolumeManagerCommon.Source.SYSTEM,
    volumeLabel: undefined,
    fileSystemId: undefined,
    providerId: undefined,
    sourcePath: undefined,
    deviceType: undefined,
    devicePath: undefined,
    isParentDevice: undefined,
    isReadOnly: false,
    isReadOnlyRemovableDevice: false,
    hasMedia: false,
    mountCondition: undefined,
    mountContext: undefined,
    diskFileSystemType: undefined,
    iconSet: {icon16x16Url: '', icon32x32Url: ''},
    driveLabel: '',
    remoteMountPath: undefined,
    hidden: false,
    vmType: undefined,
  };
  return {
    ...defaultMetadata,
    ...partialMetadata,
  };
}

/**
 * Create a fake Volume. Only the fields listed are required, other fields are
 * optional.
 */
export function createFakeVolume(
    partialVolume: Pick<Volume, 'volumeId'|'volumeType'|'rootKey'|'label'>&
    Partial<Omit<Volume, 'volumeId'|'volumeType'|'rootKey'|'label'>>): Volume {
  const defaultVolume = {
    status: PropStatus.SUCCESS,
    source: VolumeManagerCommon.Source.SYSTEM,
    error: undefined,
    deviceType: undefined,
    devicePath: undefined,
    isReadOnly: false,
    isReadOnlyRemovableDevice: false,
    providerId: undefined,
    configurable: false,
    watchable: true,
    diskFileSystemType: '',
    iconSet: {icon16x16Url: '', icon32x32Url: ''},
    driveLabel: '',
    vmType: undefined,
    isDisabled: false,
    prefixKey: undefined,
  };
  return {
    ...defaultVolume,
    ...partialVolume,
  };
}
