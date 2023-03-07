// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {FakeEntryImpl, GuestOsPlaceholder, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryType, FileData, State} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {addUiEntry as addUiEntryAction, removeUiEntry as removeUiEntryAction} from '../actions/ui_entries.js';
import {createFakeFileData, createFakeVolume, setUpFileManagerOnWindow} from '../for_tests.js';
import {getEmptyState} from '../store.js';

import {addUiEntry, removeUiEntry} from './ui_entries.js';

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
  const fileData = createFakeFileData({
    entry: new VolumeEntry(downloadsVolumeInfo),
    volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
    label: 'My files',
    type: EntryType.VOLUME_ROOT,
  });
  return {fileData, volumeInfo: downloadsVolumeInfo};
}

/** Tests a normal ui entry can be added correctly. */
export function testAddUiEntry() {
  const currentState: State = getEmptyState();
  const uiEntry =
      new FakeEntryImpl('Ui entry', VolumeManagerCommon.RootType.RECENT);
  const newState = addUiEntry(currentState, addUiEntryAction({entry: uiEntry}));
  assertEquals(1, newState.uiEntries.length);
  assertEquals(uiEntry.toURL(), newState.uiEntries[0]);
}

/** Tests that a duplicate ui entry won't be added. */
export function testAddDuplicateUiEntry() {
  const currentState: State = getEmptyState();
  const uiEntry =
      new FakeEntryImpl('Ui entry', VolumeManagerCommon.RootType.RECENT);
  currentState.uiEntries.push(uiEntry.toURL());
  const newState = addUiEntry(currentState, addUiEntryAction({entry: uiEntry}));
  assertEquals(1, newState.uiEntries.length);
  assertEquals(uiEntry.toURL(), newState.uiEntries[0]);
  // The reference of uiEntries won't change.
  assertEquals(currentState.uiEntries, newState.uiEntries);
}

/**
 * Tests that adding ui entry for MyFiles will reset the children filed of
 * MyFiles entry.
 */
export function testAddUiEntryForMyFiles() {
  const currentState: State = getEmptyState();
  // Setup MyFiles entry in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = createFakeVolume({
    volumeType: volumeInfo.volumeType,
    volumeId: volumeInfo.volumeId,
    label: volumeInfo.label,
    rootKey: volumeInfo.displayRoot.toURL(),
  });
  currentState.allEntries[fileData.entry.toURL()] = fileData;
  currentState.volumes[volumeInfo.volumeId] = myFilesVolume;
  // Add children to the MyFiles entry.
  const childEntry = new GuestOsPlaceholder(
      'Play files', 0, chrome.fileManagerPrivate.VmType.ARCVM);
  currentState.allEntries[childEntry.toURL()] = createFakeFileData({
    entry: childEntry,
    label: 'Play files',
    type: EntryType.PLACEHOLDER,
  });
  myFilesEntry.addEntry(childEntry);
  fileData.children.push(childEntry.toURL());

  const uiEntry =
      new FakeEntryImpl('Linux files', VolumeManagerCommon.RootType.CROSTINI);
  // Before calling addUiEntry(), the new uiEntry should already be in store,
  // this is handled by cacheEntries(), we should emulate the process here. This
  // is required for sortEntries().
  currentState.allEntries[uiEntry.toURL()] = createFakeFileData({
    entry: uiEntry,
    label: 'Linux files',
    type: EntryType.PLACEHOLDER,
  });
  const newState = addUiEntry(currentState, addUiEntryAction({entry: uiEntry}));
  assertEquals(1, newState.uiEntries.length);
  assertEquals(uiEntry.toURL(), newState.uiEntries[0]);
  // Check the ui entry is added to MyFiles entry.
  assertEquals(2, myFilesEntry.getUIChildren().length);
  assertEquals(uiEntry, myFilesEntry.getUIChildren()[1]);
  // Check the children of MyFiles entry gets updated and resorted.
  assertArrayEquals(
      [
        uiEntry.toURL(),
        childEntry.toURL(),
      ],
      newState.allEntries[myFilesEntry.toURL()].children);
}

/**
 * Tests that ui entry won't be added to MyFiles if it's already existed.
 */
export function testAddDuplicateUiEntryForMyFiles() {
  const currentState: State = getEmptyState();
  const uiEntry = new GuestOsPlaceholder(
      'Play files', 0, chrome.fileManagerPrivate.VmType.ARCVM);
  // Setup MyFiles entry and add the new ui entry in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = createFakeVolume({
    volumeType: volumeInfo.volumeType,
    volumeId: volumeInfo.volumeId,
    label: volumeInfo.label,
    rootKey: volumeInfo.displayRoot.toURL(),
  });
  currentState.allEntries[fileData.entry.toURL()] = fileData;
  currentState.volumes[volumeInfo.volumeId] = myFilesVolume;
  myFilesEntry.addEntry(uiEntry);

  const newState = addUiEntry(currentState, addUiEntryAction({entry: uiEntry}));
  assertEquals(1, newState.uiEntries.length);
  assertEquals(uiEntry.toURL(), newState.uiEntries[0]);
  // Check the ui entry is not being added to MyFiles entry again.
  assertEquals(1, myFilesEntry.getUIChildren().length);
  assertEquals(uiEntry, myFilesEntry.getUIChildren()[0]);
  // Check the children of MyFiles has not been touched.
  assertEquals(
      newState.allEntries[myFilesEntry.toURL()].children,
      currentState.allEntries[myFilesEntry.toURL()].children);
}

/**
 * Tests that ui entry won't be added to MyFiles if the corresponding volume
 * is already existed.
 */
export function testAddDuplicateUiEntryForMyFilesWhenVolumeExists() {
  const currentState: State = getEmptyState();
  // Placeholder ui entry and the volume entry it represents have the same
  // label.
  const label = 'Play files';
  const uiEntry =
      new GuestOsPlaceholder(label, 0, chrome.fileManagerPrivate.VmType.ARCVM);
  // Setup MyFiles entry and add the new volume entry in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = createFakeVolume({
    volumeType: volumeInfo.volumeType,
    volumeId: volumeInfo.volumeId,
    label: volumeInfo.label,
    rootKey: volumeInfo.displayRoot.toURL(),
  });
  currentState.allEntries[fileData.entry.toURL()] = fileData;
  currentState.volumes[volumeInfo.volumeId] = myFilesVolume;
  const playFilesVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, label);
  const playFilesVolumeEntry = new VolumeEntry(playFilesVolumeInfo);
  myFilesEntry.addEntry(playFilesVolumeEntry);

  const newState = addUiEntry(currentState, addUiEntryAction({entry: uiEntry}));
  // Check the ui entry has not been touched.
  assertEquals(currentState.uiEntries, newState.uiEntries);
  // Check the ui entry is not being added to MyFiles entry again.
  assertEquals(1, myFilesEntry.getUIChildren().length);
  assertEquals(playFilesVolumeEntry, myFilesEntry.getUIChildren()[0]);
  // Check the children of MyFiles has not been touched.
  assertEquals(
      newState.allEntries[myFilesEntry.toURL()].children,
      currentState.allEntries[myFilesEntry.toURL()].children);
}

/** Tests that ui entry can be removed from store correctly. */
export function testRemoveUiEntry() {
  const currentState: State = getEmptyState();
  const uiEntry =
      new FakeEntryImpl('Ui entry', VolumeManagerCommon.RootType.RECENT);
  // Setup the ui entry in both uiEntries and allEntries in the store.
  currentState.allEntries[uiEntry.toURL()] = createFakeFileData({
    entry: uiEntry,
    label: 'Ui entry',
    type: EntryType.PLACEHOLDER,
  });
  currentState.uiEntries.push(uiEntry.toURL());

  const newState =
      removeUiEntry(currentState, removeUiEntryAction({key: uiEntry.toURL()}));
  assertEquals(0, newState.uiEntries.length);
}

/** Tests that removing non-existed ui entry won't do anything. */
export function testRemoveNonExistedUiEntry() {
  const currentState: State = getEmptyState();
  const uiEntry =
      new FakeEntryImpl('Ui entry', VolumeManagerCommon.RootType.TRASH);
  const newState =
      removeUiEntry(currentState, removeUiEntryAction({key: uiEntry.toURL()}));
  // Check that uiEntries won't be touched.
  assertEquals(newState.uiEntries, currentState.uiEntries);
}

/**
 * Tests removing ui entry from MyFiles will reset the children field of
 * MyFiles entry.
 */
export function testRemoveUiEntryFromMyFiles() {
  const currentState: State = getEmptyState();
  const uiEntry =
      new FakeEntryImpl('Linux files', VolumeManagerCommon.RootType.CROSTINI);
  // Setup MyFiles entry and add the ui entry in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = createFakeVolume({
    volumeType: volumeInfo.volumeType,
    volumeId: volumeInfo.volumeId,
    label: volumeInfo.label,
    rootKey: volumeInfo.displayRoot.toURL(),
  });
  currentState.allEntries[fileData.entry.toURL()] = fileData;
  currentState.volumes[volumeInfo.volumeId] = myFilesVolume;
  myFilesEntry.addEntry(uiEntry);
  fileData.children.push(uiEntry.toURL());
  currentState.allEntries[uiEntry.toURL()] = createFakeFileData({
    entry: uiEntry,
    volumeType: VolumeManagerCommon.VolumeType.CROSTINI,
    label: 'Linux files',
    type: EntryType.PLACEHOLDER,
  });
  currentState.uiEntries.push(uiEntry.toURL());

  const newState =
      removeUiEntry(currentState, removeUiEntryAction({key: uiEntry.toURL()}));
  assertEquals(0, newState.uiEntries.length);
  // Check the ui entry has also been removed from MyFiles entry.
  assertEquals(0, myFilesEntry.getUIChildren().length);
  assertEquals(0, newState.allEntries[myFilesEntry.toURL()].children.length);
}
