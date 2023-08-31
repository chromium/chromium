// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {FakeEntryImpl, GuestOsPlaceholder, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FileData, State} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {convertEntryToFileData} from '../ducks/all_entries.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore, waitDeepEquals} from '../for_tests.js';
import {getEmptyState} from '../store.js';

import {addUiEntry, removeUiEntry} from './ui_entries.js';
import {convertVolumeInfoAndMetadataToVolume} from './volumes.js';

export function setUp() {
  // sortEntries() from addUiEntry() reducer requires volumeManager and
  // directoryModel on window.
  setUpFileManagerOnWindow();
}

/** Generate MyFiles entry with real volume entry. */
function createMyFilesDataWithVolumeEntry():
    {fileData: FileData, volumeInfo: VolumeInfo} {
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const fileData = convertEntryToFileData(new VolumeEntry(downloadsVolumeInfo));
  return {fileData, volumeInfo: downloadsVolumeInfo};
}

/** Tests a normal UI entry can be added correctly. */
export async function testAddUiEntry(done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  // Dispatch an action to add a UI entry.
  const uiEntry =
      new FakeEntryImpl('Ui entry', VolumeManagerCommon.RootType.RECENT);
  store.dispatch(addUiEntry({entry: uiEntry}));

  // Expect the newly added entry is in the store.
  const want: Partial<State> = {
    allEntries: {
      [uiEntry.toURL()]: convertEntryToFileData(uiEntry),
    },
    uiEntries: [uiEntry.toURL()],
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      uiEntries: state.uiEntries,
                                    }));

  done();
}

/** Tests that a duplicate UI entry won't be added. */
export async function testAddDuplicateUiEntry(done: () => void) {
  const initialState = getEmptyState();
  // Add one UI entry in the store.
  const uiEntry =
      new FakeEntryImpl('Ui entry', VolumeManagerCommon.RootType.RECENT);
  initialState.uiEntries.push(uiEntry.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to add an already existed UI entry.
  store.dispatch(addUiEntry({entry: uiEntry}));

  // Expect nothing changes in the store.
  const want: State['uiEntries'] = [uiEntry.toURL()];
  await waitDeepEquals(store, want, (state) => state.uiEntries);

  done();
}

/**
 * Tests that adding UI entry for MyFiles will reset the children filed of
 * MyFiles entry.
 */
export async function testAddUiEntryForMyFiles(done: () => void) {
  const initialState = getEmptyState();
  // Setup MyFiles entry in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  initialState.allEntries[fileData.entry.toURL()] = fileData;
  initialState.volumes[volumeInfo.volumeId] = myFilesVolume;
  // Add children to the MyFiles entry.
  const childEntry = new GuestOsPlaceholder(
      'Play files', 0, chrome.fileManagerPrivate.VmType.ARCVM);
  initialState.allEntries[childEntry.toURL()] =
      convertEntryToFileData(childEntry);
  myFilesEntry.addEntry(childEntry);
  fileData.children.push(childEntry.toURL());
  initialState.uiEntries.push(childEntry.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to add a new UI entry which belongs to MyFiles.
  const uiEntry =
      new FakeEntryImpl('Linux files', VolumeManagerCommon.RootType.CROSTINI);
  store.dispatch(addUiEntry({entry: uiEntry}));

  // Expect 2 ui entries in the store.
  const want: Partial<State> = {
    allEntries: {
      [myFilesEntry.toURL()]: {
        ...fileData,
        children: [
          // Children are in sorted order.
          uiEntry.toURL(),
          childEntry.toURL(),
        ],
      },
      [childEntry.toURL()]: convertEntryToFileData(childEntry),
      [uiEntry.toURL()]: convertEntryToFileData(uiEntry),
    },
    // No sorting order, order is based on the push order.
    uiEntries: [childEntry.toURL(), uiEntry.toURL()],
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      uiEntries: state.uiEntries,
                                    }));

  // Check the UI entry is added to MyFiles entry.
  assertEquals(2, myFilesEntry.getUIChildren().length);
  assertEquals(uiEntry, myFilesEntry.getUIChildren()[1]);

  done();
}

/**
 * Tests that UI entry won't be added to MyFiles if it's already existed.
 */
export async function testAddDuplicateUiEntryForMyFiles(done: () => void) {
  const initialState = getEmptyState();
  const uiEntry = new GuestOsPlaceholder(
      'Play files', 0, chrome.fileManagerPrivate.VmType.ARCVM);
  // Setup MyFiles entry and add the new ui entry in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  initialState.allEntries[fileData.entry.toURL()] = fileData;
  initialState.volumes[volumeInfo.volumeId] = myFilesVolume;
  myFilesEntry.addEntry(uiEntry);
  fileData.children.push(uiEntry.toURL());
  initialState.uiEntries.push(uiEntry.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to add an already existed UI entry.
  store.dispatch(addUiEntry({entry: uiEntry}));

  // Expect no changes in the store.
  await waitDeepEquals(store, initialState, (state) => state);

  // Check the UI entry is not being added to MyFiles entry again.
  assertEquals(1, myFilesEntry.getUIChildren().length);
  assertEquals(uiEntry, myFilesEntry.getUIChildren()[0]);

  done();
}

/**
 * Tests that UI entry won't be added to MyFiles if the corresponding volume
 * is already existed.
 */
export async function testAddDuplicateUiEntryForMyFilesWhenVolumeExists(
    done: () => void) {
  const initialState = getEmptyState();
  // Placeholder UI entry and the volume entry it represents have the same
  // label.
  const label = 'Play files';
  // Setup MyFiles entry and add the volume entry in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  initialState.allEntries[fileData.entry.toURL()] = fileData;
  initialState.volumes[volumeInfo.volumeId] = myFilesVolume;
  const playFilesVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, label);
  const playFilesVolumeEntry = new VolumeEntry(playFilesVolumeInfo);
  myFilesEntry.addEntry(playFilesVolumeEntry);
  fileData.children.push(playFilesVolumeEntry.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to add UI entry.
  const uiEntry =
      new GuestOsPlaceholder(label, 0, chrome.fileManagerPrivate.VmType.ARCVM);
  store.dispatch(addUiEntry({entry: uiEntry}));

  // Expect the UI entry is not being added to the store.
  await waitDeepEquals(store, [], (state) => state.uiEntries);

  // Check the UI entry is not being added to MyFiles entry again.
  assertEquals(1, myFilesEntry.getUIChildren().length);
  assertEquals(playFilesVolumeEntry, myFilesEntry.getUIChildren()[0]);

  done();
}

/**
 * Tests that UI entry will be disabled if the corresponding volume
 * type is disabled in the volume manager.
 */
export async function testAddUiEntryWithDisabledVolumeType(done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  // Dispatch an action to add UI entry.
  const {volumeManager} = window.fileManager;
  // Disable Android files volume type.
  volumeManager.isDisabled = (volumeType) => {
    return volumeType === VolumeManagerCommon.VolumeType.ANDROID_FILES;
  };
  const uiEntry = new GuestOsPlaceholder(
      'Play files', 0, chrome.fileManagerPrivate.VmType.ARCVM);
  store.dispatch(addUiEntry({entry: uiEntry}));

  // Expect the UI entry is being disabled.
  await waitUntil(() => uiEntry.disabled === true);

  done();
}

/** Tests that UI entry can be removed from store correctly. */
export async function testRemoveUiEntry(done: () => void) {
  const initialState = getEmptyState();
  const uiEntry =
      new FakeEntryImpl('Ui entry', VolumeManagerCommon.RootType.RECENT);
  // Setup the UI entry in both uiEntries and allEntries in the store.
  initialState.allEntries[uiEntry.toURL()] = convertEntryToFileData(uiEntry);
  initialState.uiEntries.push(uiEntry.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to remove the UI entry.
  store.dispatch(removeUiEntry({key: uiEntry.toURL()}));

  // Expect the UI entry has been removed.
  await waitDeepEquals(store, [], (state) => state.uiEntries);

  done();
}

/** Tests that removing non-existed UI entry won't do anything. */
export async function testRemoveNonExistedUiEntry(done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  // Dispatch an action to remove a non-existed UI entry.
  const uiEntry =
      new FakeEntryImpl('Ui entry', VolumeManagerCommon.RootType.TRASH);
  store.dispatch(removeUiEntry({key: uiEntry.toURL()}));

  // Expect nothing changes in the store.
  await waitDeepEquals(store, initialState, (state) => state);

  done();
}

/**
 * Tests removing UI entry from MyFiles will reset the children field of
 * MyFiles entry.
 */
export async function testRemoveUiEntryFromMyFiles(done: () => void) {
  const initialState = getEmptyState();
  const uiEntry =
      new FakeEntryImpl('Linux files', VolumeManagerCommon.RootType.CROSTINI);
  // Setup MyFiles entry and add the ui entry in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  initialState.allEntries[fileData.entry.toURL()] = fileData;
  initialState.volumes[volumeInfo.volumeId] = myFilesVolume;
  myFilesEntry.addEntry(uiEntry);
  fileData.children.push(uiEntry.toURL());
  initialState.allEntries[uiEntry.toURL()] = convertEntryToFileData(uiEntry);
  initialState.uiEntries.push(uiEntry.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to
  store.dispatch(removeUiEntry({key: uiEntry.toURL()}));

  // Expect the entry has been removed from MyFiles.
  const want: Partial<State> = {
    allEntries: {
      [myFilesEntry.toURL()]: {
        ...convertEntryToFileData(myFilesEntry),
        children: [],
      },
      [uiEntry.toURL()]: convertEntryToFileData(uiEntry),
    },
    uiEntries: [],
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      uiEntries: state.uiEntries,
                                    }));

  // Check the UI entry has also been removed from MyFiles entry.
  assertEquals(0, myFilesEntry.getUIChildren().length);

  done();
}
