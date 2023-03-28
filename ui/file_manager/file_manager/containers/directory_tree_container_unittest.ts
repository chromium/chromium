// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {MockVolumeManager} from '../background/js/mock_volume_manager.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from '../common/js/files_app_entry_types.js';
import {metrics} from '../common/js/metrics.js';
import {installMockChrome} from '../common/js/mock_chrome.js';
import {MockFileSystem} from '../common/js/mock_entry.js';
import {waitUntil} from '../common/js/test_error_reporting.js';
import {waitForElementUpdate} from '../common/js/unittest_util.js';
import {VolumeManagerCommon} from '../common/js/volume_manager_types.js';
import {State} from '../externs/ts/state.js';
import {MetadataItem} from '../foreground/js/metadata/metadata_item.js';
import {MetadataModel} from '../foreground/js/metadata/metadata_model.js';
import {MockMetadataModel} from '../foreground/js/metadata/mock_metadata.js';
import {addVolume, removeVolume} from '../state/actions/volumes.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore} from '../state/for_tests.js';
import {convertEntryToFileData} from '../state/reducers/all_entries.js';
import {convertVolumeInfoAndMetadataToVolume, driveRootEntryListKey} from '../state/reducers/volumes.js';
import {getEmptyState, getEntry, getFileData, getStore} from '../state/store.js';
import {XfTree} from '../widgets/xf_tree.js';

import {DirectoryTreeContainer} from './directory_tree_container.js';

/** The directory tree container instance */
let directoryTreeContainer: DirectoryTreeContainer;

/** To store all the directory changed listener. */
let directoryChangedListeners: Array<(event: any) => void>;

export function setUp() {
  // DirectoryModel and VolumeManager are required for DirectoryTreeContainer.
  setUpFileManagerOnWindow();
  // Mock file manager private API.
  directoryChangedListeners = [];
  const mockChrome = {
    fileManagerPrivate: {
      onDirectoryChanged: {
        addListener: (listener: (event: any) => void) => {
          directoryChangedListeners.push(listener);
        },
      },
    },
  };
  installMockChrome(mockChrome);
  // Initialize directory tree container.
  const {directoryModel, volumeManager} = window.fileManager;
  document.body.setAttribute('theme', 'refresh23');
  directoryTreeContainer = new DirectoryTreeContainer(
      document.body, directoryModel, volumeManager, createMockMetadataModel());
}

export function tearDown() {
  // Unsubscribe from the store.
  if (directoryTreeContainer) {
    getStore().unsubscribe(directoryTreeContainer);
  }
  document.body.innerHTML = '';
}

/**
 * Mock metrics.
 */
metrics.recordEnum = function() {};
metrics.recordSmallCount = function() {};
metrics.startInterval = function() {};
metrics.recordInterval = function() {};

/**
 * Returns a mock MetadataModel.
 */
function createMockMetadataModel(): MetadataModel {
  const metadataModel = new MockMetadataModel({}) as unknown as MetadataModel;
  metadataModel.notifyEntriesChanged = () => {};
  // get and getCache mock a non-shared directory.
  metadataModel.get = () => Promise.resolve([{shared: false} as MetadataItem]);
  metadataModel.getCache = () => [{shared: false} as MetadataItem];
  return metadataModel;
}

/**
 * Returns the directory tree item labels.
 */
function getDirectoryTreeItemLabels(directoryTree: XfTree): string[] {
  const labels = [];
  for (const item of directoryTree.items) {
    labels.push(item.label);
  }
  return labels;
}

/**
 * Add MyFiles and Drive data to the store.
 * Note: it will modify the state passed in place.
 */
async function addMyFilesAndDriveToStore(state: State):
    Promise<{myFilesFs: MockFileSystem, driveFs: MockFileSystem}> {
  const {volumeManager} = window.fileManager;
  // Prepare data for MyFiles.
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const downloadsEntry = new VolumeEntry(downloadsVolumeInfo);
  state.allEntries[downloadsEntry.toURL()] =
      convertEntryToFileData(downloadsEntry);
  state.volumes[downloadsVolumeInfo.volumeId] =
      convertVolumeInfoAndMetadataToVolume(
          downloadsVolumeInfo, createFakeVolumeMetadata(downloadsVolumeInfo));
  // Prepare data for Drive.
  // - Fake drive root.
  const driveRootEntryList = new EntryList(
      'Google Drive', VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT);
  state.allEntries[driveRootEntryListKey] =
      convertEntryToFileData(driveRootEntryList);
  state.uiEntries.push(driveRootEntryListKey);
  // - My Drive
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE)!;
  await driveVolumeInfo.resolveDisplayRoot();
  const driveEntry = new VolumeEntry(driveVolumeInfo);
  state.allEntries[driveEntry.toURL()] = convertEntryToFileData(driveEntry);
  state.volumes[driveVolumeInfo.volumeId] =
      convertVolumeInfoAndMetadataToVolume(
          driveVolumeInfo, createFakeVolumeMetadata(driveVolumeInfo));
  driveRootEntryList.addEntry(driveEntry);
  // - Shared with me
  const sharedWithMeEntry =
      driveVolumeInfo
          .fakeEntries[VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME];
  state.allEntries[sharedWithMeEntry.toURL()] =
      convertEntryToFileData(sharedWithMeEntry);
  state.uiEntries.push(sharedWithMeEntry.toURL());
  driveRootEntryList.addEntry(sharedWithMeEntry);
  // - Offline
  const offlineEntry =
      driveVolumeInfo.fakeEntries[VolumeManagerCommon.RootType.DRIVE_OFFLINE];
  state.allEntries[offlineEntry.toURL()] = convertEntryToFileData(offlineEntry);
  state.uiEntries.push(offlineEntry.toURL());
  driveRootEntryList.addEntry(offlineEntry);

  return {
    myFilesFs: downloadsVolumeInfo.fileSystem as MockFileSystem,
    driveFs: driveVolumeInfo.fileSystem as MockFileSystem,
  };
}

/**
 * Add Team drives data with optional children to the store.
 * Note: it will modify the state passed in place.
 */
function addTeamDrivesToStore(state: State, childEntries: string[] = []) {
  const {volumeManager} = window.fileManager;
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE)!;
  const teamDrivesEntry = driveVolumeInfo.sharedDriveDisplayRoot;
  state.allEntries[teamDrivesEntry.toURL()] =
      convertEntryToFileData(teamDrivesEntry);
  const driveRootEntryList =
      getEntry(state, driveRootEntryListKey) as EntryList;
  const existingChildren = driveRootEntryList.getUIChildren();
  existingChildren.splice(1, 0, teamDrivesEntry);

  if (childEntries.length > 0) {
    const driveFs = driveVolumeInfo.fileSystem as MockFileSystem;
    driveFs.populate(childEntries);
  }
}

/**
 * Add Computers data with optional children to the store.
 * Note: it will modify the state passed in place.
 */
function addComputerDrivesToStore(state: State, childEntries: string[] = []) {
  const {volumeManager} = window.fileManager;
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE)!;
  const computersEntry = driveVolumeInfo.computersDisplayRoot;
  state.allEntries[computersEntry.toURL()] =
      convertEntryToFileData(computersEntry);
  const driveRootEntryList =
      getEntry(state, driveRootEntryListKey) as EntryList;
  const existingChildren = driveRootEntryList.getUIChildren();
  // Computers entry should be inserted after Team drives entry if it exists.
  const insertIndex = existingChildren[1] instanceof FakeEntryImpl ? 1 : 2;
  existingChildren.splice(insertIndex, 0, computersEntry);

  if (childEntries.length > 0) {
    const driveFs = driveVolumeInfo.fileSystem as MockFileSystem;
    driveFs.populate(childEntries);
  }
}

/**
 * Add Android app navigation data to the store.
 * Note: it will modify the state passed in place.
 */
function addAndroidAppToStore(state: State) {
  state.androidApps['com.test.app1'] = {
    name: 'App 1',
    packageName: 'com.test.app1',
    activityName: 'Activity1',
    iconSet: {icon16x16Url: 'url1', icon32x32Url: 'url2'},
  };
}

/**
 * Test case for typical creation of directory tree.
 * This test expects that the following tree is built.
 *
 * MyFiles
 * Google Drive
 * - My Drive
 * - Shared with me
 * - Offline
 */
export async function testCreateDirectoryTree(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const myFilesItem = directoryTree.items[0]!;
  const driveItem = directoryTree.items[1]!;

  assertEquals('Downloads', myFilesItem.label);
  assertEquals('Google Drive', driveItem.label);

  await waitUntil(() => {
    // Under the drive item, there exist 3 entries.
    return driveItem.items.length == 3;
  });
  // There exist 1 my drive entry and 2 fake entries under the drive item.
  assertEquals('My Drive', driveItem.items[0]!.label);
  assertEquals('Shared with me', driveItem.items[1]!.label);
  assertEquals('Offline', driveItem.items[2]!.label);

  done();
}

/**
 * Test case for creating tree with Team Drives.
 * This test expects that the following tree is built.
 *
 * MyFiles
 * Google Drive
 * - My Drive
 * - Team Drives (only if there is a child team drive).
 * - Shared with me
 * - Offline
 */
export async function testCreateDirectoryTreeWithTeamDrive(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Add Team Drives.
  addTeamDrivesToStore(initialState, ['/team_drives/a/']);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const myFilesItem = directoryTree.items[0]!;
  const driveItem = directoryTree.items[1]!;

  assertEquals('Downloads', myFilesItem.label);
  assertEquals('Google Drive', driveItem.label);

  await waitUntil(() => {
    // Under the drive item, there exist 4 entries.
    return driveItem.items.length == 4;
  });
  // There exist 1 my drive entry and 3 fake entries under the drive item.
  assertEquals('My Drive', driveItem.items[0]!.label);
  assertEquals('Shared drives', driveItem.items[1]!.label);
  assertEquals('Shared with me', driveItem.items[2]!.label);
  assertEquals('Offline', driveItem.items[3]!.label);

  done();
}

/**
 * Test case for creating tree with empty Team Drives.
 * The Team Drives subtree should be removed if the user has no team drives.
 */
export async function testCreateDirectoryTreeWithEmptyTeamDrive(
    done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // No directories exist under Team Drives.
  addTeamDrivesToStore(initialState);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const myFilesItem = directoryTree.items[0]!;
  const driveItem = directoryTree.items[1]!;

  assertEquals('Downloads', myFilesItem.label);
  assertEquals('Google Drive', driveItem.label);

  await waitUntil(() => {
    // Root entries under Drive volume is generated, Team Drives isn't
    // included because it has no child.
    // See testCreateDirectoryTreeWithTeamDrive for detail.
    return driveItem.items.length == 3;
  });
  let teamDrivesItemFound = false;
  for (let i = 0; i < driveItem.items.length; i++) {
    if (driveItem.items[i]!.label == 'Shared drives') {
      teamDrivesItemFound = true;
      break;
    }
  }
  assertFalse(teamDrivesItemFound, 'Team Drives should NOT be generated');

  done();
}

/**
 * Test case for creating tree with Computers.
 * This test expects that the following tree is built.
 *
 * MyFiles
 * Google Drive
 * - My Drive
 * - Computers (only if there is a child computer).
 * - Shared with me
 * - Offline
 */
export async function testCreateDirectoryTreeWithComputers(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Add Computers.
  addComputerDrivesToStore(initialState, ['/Computers/My Laptop/']);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const myFilesItem = directoryTree.items[0]!;
  const driveItem = directoryTree.items[1]!;

  assertEquals('Downloads', myFilesItem.label);
  assertEquals('Google Drive', driveItem.label);

  await waitUntil(() => {
    // Under the drive item, there exist 4 entries.
    return driveItem.items.length == 4;
  });
  // There exist 1 my drive entry and 3 fake entries under the drive item.
  assertEquals('My Drive', driveItem.items[0]!.label);
  assertEquals('Computers', driveItem.items[1]!.label);
  assertEquals('Shared with me', driveItem.items[2]!.label);
  assertEquals('Offline', driveItem.items[3]!.label);

  done();
}

/**
 * Test case for creating tree with empty Computers.
 * The Computers subtree should be removed if the user has no computers.
 */
export async function testCreateDirectoryTreeWithEmptyComputers(
    done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // No directories exist under Computers.
  addComputerDrivesToStore(initialState);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const myFilesItem = directoryTree.items[0]!;
  const driveItem = directoryTree.items[1]!;

  assertEquals('Downloads', myFilesItem.label);
  assertEquals('Google Drive', driveItem.label);

  await waitUntil(() => {
    // Root entries under Drive volume is generated, Computers isn't
    // included because it has no child.
    // See testCreateDirectoryTreeWithComputers for detail.
    return driveItem.items.length == 3;
  });
  let computersItemFound = false;
  for (let i = 0; i < driveItem.items.length; i++) {
    if (driveItem.items[i]!.label == 'Computers') {
      computersItemFound = true;
      break;
    }
  }
  assertFalse(computersItemFound, 'Computers should NOT be generated');

  done();
}

/**
 * Test case for creating tree with Team Drives & Computers.
 * This test expects that the following tree is built.
 *
 * MyFiles
 * Google Drive
 * - My Drive
 * - Team Drives
 * - Computers
 * - Shared with me
 * - Offline
 */
export async function testCreateDirectoryTreeWithTeamDrivesAndComputers(
    done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Add Team drives.
  addTeamDrivesToStore(initialState, ['/team_drives/a/']);
  // Add Computers.
  addComputerDrivesToStore(initialState, ['/Computers/My Laptop/']);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const myFilesItem = directoryTree.items[0]!;
  const driveItem = directoryTree.items[1]!;

  assertEquals('Downloads', myFilesItem.label);
  assertEquals('Google Drive', driveItem.label);

  await waitUntil(() => {
    // Under the drive item, there exist 5 entries.
    return driveItem.items.length == 5;
  });
  // There exist 1 my drive entry and 4 fake entries under the drive item.
  assertEquals('My Drive', driveItem.items[0]!.label);
  assertEquals('Shared drives', driveItem.items[1]!.label);
  assertEquals('Computers', driveItem.items[2]!.label);
  assertEquals('Shared with me', driveItem.items[3]!.label);
  assertEquals('Offline', driveItem.items[4]!.label);

  done();
}

/**
 * Test case for setting separator property.
 *
 * 'separator' property is used to display a line divider between
 * "sections" in the directory tree.
 *
 * This test expects that the following tree is built.
 *
 * MyFiles
 * Google Drive
 * Android app 1
 */
export async function testSeparatorInNavigationSections(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Add Android apps.
  addAndroidAppToStore(initialState);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles, Drive and Android app should be listed.
  await waitUntil(() => directoryTree.items.length === 3);
  const myFilesItem = directoryTree.items[0]!;
  const driveItem = directoryTree.items[1]!;
  const androidAppItem = directoryTree.items[2]!;

  // First element should not have separator property, to not display a
  // division line in the first section.
  // Downloads:
  assertFalse(myFilesItem.separator);

  // Drive should have separator, because it's a new section but not the
  // first section.
  assertTrue(driveItem.separator);

  // Android app should have separator, because it's a new section but not the
  // first section.
  assertTrue(androidAppItem.separator);

  done();
}

/**
 * Test case for directory update with volume changes.
 *
 * Mounts/unmounts removable and archive volumes, and checks these volumes come
 * up to/disappear from the list correctly.
 */
export async function testDirectoryTreeUpdateWithVolumeChanges(
    done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Store initialization will notify DirectoryTree.
  const store = setupStore(initialState);

  // There are 2 volumes, MyFiles and Drive, at first.
  await waitUntil(() => directoryTree.items.length === 2);
  assertArrayEquals(
      [
        'Downloads',
        'Google Drive',
      ],
      getDirectoryTreeItemLabels(directoryTree));

  // Mounts a removable volume.
  const {volumeManager} = window.fileManager;
  const removableVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable', 'Removable 1');
  volumeManager.volumeInfoList.add(removableVolumeInfo);
  store.dispatch(addVolume({
    volumeInfo: removableVolumeInfo,
    volumeMetadata: createFakeVolumeMetadata(removableVolumeInfo),
  }));

  // Asserts that a removable directory is added after the update.
  await waitUntil(() => directoryTree.items.length === 3);
  assertArrayEquals(
      [
        'Downloads',
        'Google Drive',
        'Removable 1',
      ],
      getDirectoryTreeItemLabels(directoryTree));

  // Mounts an archive volume.
  const archiveVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ARCHIVE, 'archive', 'Archive 1');
  volumeManager.volumeInfoList.add(archiveVolumeInfo);
  store.dispatch(addVolume({
    volumeInfo: archiveVolumeInfo,
    volumeMetadata: createFakeVolumeMetadata(archiveVolumeInfo),
  }));

  // Asserts that an archive directory is added before the removable
  await waitUntil(() => directoryTree.items.length === 4);
  assertArrayEquals(
      [
        'Downloads',
        'Google Drive',
        'Removable 1',
        'Archive 1',
      ],
      getDirectoryTreeItemLabels(directoryTree));

  // Deletes an archive directory.
  store.dispatch(removeVolume({volumeId: archiveVolumeInfo.volumeId}));

  // Asserts that an archive directory is deleted.
  await waitUntil(() => directoryTree.items.length === 3);
  assertArrayEquals(
      [
        'Downloads',
        'Google Drive',
        'Removable 1',
      ],
      getDirectoryTreeItemLabels(directoryTree));

  done();
}

/**
 * Test case for directory tree with disabled Android files.
 *
 * If some of the volumes under MyFiles are disabled, this should be reflected
 * in the directory model as well: the children shouldn't be loaded, and
 * clicking on the item or the expand icon shouldn't do anything.
 */
export async function testDirectoryTreeWithAndroidDisabled(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Store initialization will notify DirectoryTree.
  const store = setupStore(initialState);
  await waitUntil(() => directoryTree.items.length === 2);

  // Create Android 'Play files' volume and set as disabled.
  const {volumeManager} = window.fileManager;
  const androidVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'android_files:droid',
      'Android files');
  volumeManager.volumeInfoList.add(androidVolumeInfo);
  volumeManager.isDisabled = (volumeType) => {
    return volumeType === VolumeManagerCommon.VolumeType.ANDROID_FILES;
  };
  store.dispatch(addVolume({
    volumeInfo: androidVolumeInfo,
    volumeMetadata: createFakeVolumeMetadata(androidVolumeInfo),
  }));

  // Asserts that MyFiles should have 1 child: Android files.
  const myFilesItem = directoryTree.items[0]!;
  await waitUntil(() => myFilesItem.items.length === 1);
  const androidItem = myFilesItem.items[0]!;
  assertEquals('Android files', androidItem.label);
  assertTrue(androidItem.disabled);

  // Clicking on the item shouldn't select it.
  androidItem.click();
  await waitForElementUpdate(androidItem);
  assertFalse(androidItem.selected);

  // The item shouldn't be expanded.
  assertFalse(androidItem.expanded);
  // Clicking on the expand icon shouldn't expand.
  const expandIcon =
      androidItem.shadowRoot!.querySelector<HTMLSpanElement>('.expand-icon')!;
  expandIcon.click();
  await waitForElementUpdate(androidItem);
  assertFalse(androidItem.expanded);

  done();
}

/**
 * Test case for directory tree with disabled removable volume.
 *
 * If removable volumes are disabled, they should be allowed to mount/unmount,
 * but cannot be selected or expanded.
 */
export async function testDirectoryTreeWithRemovableDisabled(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Store initialization will notify DirectoryTree.
  const store = setupStore(initialState);
  await waitUntil(() => directoryTree.items.length === 2);

  // Create removable volume and set as disabled.
  const {volumeManager} = window.fileManager;
  const removableVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable', 'Removable');
  volumeManager.volumeInfoList.add(removableVolumeInfo);
  volumeManager.isDisabled = (volumeType) => {
    return volumeType === VolumeManagerCommon.VolumeType.REMOVABLE;
  };
  store.dispatch(addVolume({
    volumeInfo: removableVolumeInfo,
    volumeMetadata: createFakeVolumeMetadata(removableVolumeInfo),
  }));

  // Asserts that a removable directory is added after the update.
  await waitUntil(() => directoryTree.items.length === 3);
  const removableItem = directoryTree.items[2]!;
  assertEquals('Removable', removableItem.label);
  assertTrue(removableItem.disabled);

  // Clicking on the item shouldn't select it.
  removableItem.click();
  await waitForElementUpdate(removableItem);
  assertFalse(removableItem.selected);

  // The item shouldn't be expanded.
  assertFalse(removableItem.expanded);
  // Clicking on the expand icon shouldn't expand.
  const expandIcon =
      removableItem.shadowRoot!.querySelector<HTMLSpanElement>('.expand-icon')!;
  expandIcon.click();
  await waitForElementUpdate(removableItem);
  assertFalse(removableItem.expanded);

  // Unmount the removable and assert that it's removed from the directory.
  store.dispatch(removeVolume({volumeId: removableVolumeInfo.volumeId}));
  await waitUntil(() => directoryTree.items.length === 2);

  done();
}

/**
 * Test a disabled drive volume.
 *
 * If drive is disabled, this should be reflected in the directory model as
 * well: the children shouldn't be loaded, and clicking on the item or the
 * expand icon shouldn't do anything.
 */
export async function testDirectoryTreeWithDriveDisabled(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  const {myFilesFs, driveFs} = await addMyFilesAndDriveToStore(initialState);
  // Add sub folders for them.
  myFilesFs.populate(['/folder1/']);
  driveFs.populate(['/root/folder1/']);
  const driveRootEntryFileData =
      getFileData(initialState, driveRootEntryListKey)!;
  // Disable the drive root entry.
  driveRootEntryFileData.disabled = true;
  (driveRootEntryFileData.entry as EntryList).disabled = true;
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const myFilesItem = directoryTree.items[0]!;
  const driveItem = directoryTree.items[1]!;

  await waitUntil(() => {
    // Under the drive item, there exist 0 entries. In MyFiles should
    // exist 1 entry folder1.
    return driveItem.items.length === 0 && myFilesItem.items.length === 1;
  });

  assertEquals('Google Drive', driveItem.label);
  assertTrue(driveItem.disabled);

  // Clicking on the item shouldn't select it.
  driveItem.click();
  await waitForElementUpdate(driveItem);
  assertFalse(driveItem.selected);

  // The item shouldn't be expanded.
  const treeItemElement = driveItem.shadowRoot!.querySelector('li')!;
  const expandIconElement =
      driveItem.shadowRoot!.querySelector<HTMLSpanElement>('.expand-icon')!;
  let isExpanded = treeItemElement.getAttribute('aria-expanded') || 'false';
  assertEquals('false', isExpanded);
  // Clicking on the expand icon shouldn't expand.
  expandIconElement.click();
  await waitForElementUpdate(driveItem);
  isExpanded = treeItemElement.getAttribute('aria-expanded') || 'false';
  assertEquals('false', isExpanded);

  done();
}

/**
 * Test adding the first team drive for a user.
 * Team Drives subtree should be shown after the change notification is
 * delivered.
 */
export async function testAddFirstTeamDrive(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  const {driveFs} = await addMyFilesAndDriveToStore(initialState);
  // No directories exist under Team Drives.
  addTeamDrivesToStore(initialState);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const driveItem = directoryTree.items[1]!;

  // Drive should include My Drive, Shared with me, Offline.
  await waitUntil(() => {
    return driveItem.items.length === 3;
  });

  // Add one folder to the Team drives.
  driveFs.populate(['/team_drives/a/']);
  const event = {
    entry: driveFs.entries['/team_drives'],
    eventType: 'changed',
  };
  for (const listener of directoryChangedListeners) {
    listener(event);
  }
  await waitUntil(() => {
    return driveItem.items.length === 4 &&
        driveItem.items[1]!.label === 'Shared drives';
  });

  done();
}

/**
 * Test removing the last team drive for a user.
 * Team Drives subtree should be removed after the change notification is
 * delivered.
 */
export async function testRemoveLastTeamDrive(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  const {driveFs} = await addMyFilesAndDriveToStore(initialState);
  // Add one entry under Team Drives.
  addTeamDrivesToStore(initialState, ['/team_drives/a/']);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const driveItem = directoryTree.items[1]!;

  // Drive should include My Drive, Team drives, Shared with me, Offline.
  await waitUntil(() => {
    return driveItem.items.length == 4;
  });

  // Remove the only child from Team drives.
  await new Promise(resolve => {
    driveFs.entries['/team_drives/a'].remove(resolve);
  });

  const event = {
    entry: driveFs.entries['/team_drives'],
    eventType: 'changed',
  };
  for (const listener of directoryChangedListeners) {
    listener(event);
  }
  // Wait Team drives grand root to disappear.
  await waitUntil(() => {
    for (let i = 0; i < driveItem.items.length; i++) {
      if (driveItem.items[i]!.label === 'Shared drives') {
        return false;
      }
    }
    return true;
  });

  done();
}

/**
 * Test adding the first computer for a user.
 * Computers subtree should be shown after the change notification is
 * delivered.
 */
export async function testAddFirstComputer(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  const {driveFs} = await addMyFilesAndDriveToStore(initialState);
  // No directories exist under Computers.
  addComputerDrivesToStore(initialState);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const driveItem = directoryTree.items[1]!;

  // Drive should include My Drive, Shared with me, Offline.
  await waitUntil(() => {
    return driveItem.items.length === 3;
  });

  // Test that we initially do not have a Computers item under Drive, and that
  // adding a filesystem "/Computers/My Laptop" results in the Computers item
  // being displayed under Drive.
  driveFs.populate(['/Computers/My laptop/']);
  const event = {
    entry: driveFs.entries['/Computers'],
    eventType: 'changed',
  };
  for (const listener of directoryChangedListeners) {
    listener(event);
  }
  await waitUntil(() => {
    return driveItem.items.length === 4 &&
        driveItem.items[1]!.label === 'Computers';
  });

  done();
}

/**
 * Test removing the last computer for a user.
 * Computers subtree should be removed after the change notification is
 * delivered.
 */
export async function testRemoveLastComputer(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  const {driveFs} = await addMyFilesAndDriveToStore(initialState);
  // Add one entry under Computers.
  addComputerDrivesToStore(initialState, ['/Computers/My Laptop/']);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const driveItem = directoryTree.items[1]!;

  // Drive should include My Drive, Computers, Shared with me, Offline.
  await waitUntil(() => {
    return driveItem.items.length == 4;
  });

  // Check that removing the local computer "My Laptop" results in the entire
  // "Computers" element being removed, as it has no children.
  await new Promise(resolve => {
    driveFs.entries['/Computers/My Laptop'].remove(resolve);
  });

  const event = {
    entry: driveFs.entries['/Computers'],
    eventType: 'changed',
  };
  for (const listener of directoryChangedListeners) {
    listener(event);
  }
  // Wait Computers grand root to disappear.
  await waitUntil(() => {
    for (let i = 0; i < driveItem.items.length; i++) {
      if (driveItem.items[i]!.label === 'Computers') {
        return false;
      }
    }
    return true;
  });

  done();
}

/**
 * Test adding FSPs.
 * Sub directories should be fetched for FSPs, but not for the Smb FSP.
 */
export async function testAddProviders(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Store initialization will notify DirectoryTree.
  const store = setupStore(initialState);

  const volumeManager = window.fileManager.volumeManager as MockVolumeManager;
  // Add a volume representing a non-Smb provider to the mock filesystem.
  const nonSmbProviderVolumeInfo = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'not_smb', 'NOT_SMB_LABEL');
  volumeManager.volumeInfoList.add(nonSmbProviderVolumeInfo);
  // Add a sub directory to the non-Smb provider.
  const providerFs =
      assert(volumeManager.volumeInfoList.item(2).fileSystem) as MockFileSystem;
  providerFs.populate(['/non_smb_child/']);
  store.dispatch(addVolume({
    volumeInfo: nonSmbProviderVolumeInfo,
    volumeMetadata: createFakeVolumeMetadata(nonSmbProviderVolumeInfo),
  }));

  // Add a volume representing an Smb provider to the mock filesystem.
  const smbProviderVolumeInfo = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'smb', 'SMB_LABEL', '@smb');
  volumeManager.volumeInfoList.add(smbProviderVolumeInfo);
  // Add a sub directory to the Smb provider.
  const smbProviderFs =
      assert(volumeManager.volumeInfoList.item(3).fileSystem) as MockFileSystem;
  smbProviderFs.populate(['/smb_child/']);
  store.dispatch(addVolume({
    volumeInfo: smbProviderVolumeInfo,
    volumeMetadata: createFakeVolumeMetadata(smbProviderVolumeInfo),
  }));

  // Add a volume representing an smbfs share to the mock filesystem.
  const smbShareVolumeInfo = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.SMB, 'smbfs', 'SMBFS_LABEL');
  volumeManager.volumeInfoList.add(smbShareVolumeInfo);
  // Add a sub directory to the smbfs.
  const smbFs =
      assert(volumeManager.volumeInfoList.item(4).fileSystem) as MockFileSystem;
  smbFs.populate(['/smbfs_child/']);
  store.dispatch(addVolume({
    volumeInfo: smbShareVolumeInfo,
    volumeMetadata: createFakeVolumeMetadata(smbShareVolumeInfo),
  }));


  // At top level, MyFiles and Drive, 3 volumes should be listed.
  await waitUntil(() => directoryTree.items.length === 5);

  assertEquals('Downloads', directoryTree.items[0]!.label);
  assertEquals('Google Drive', directoryTree.items[1]!.label);
  assertEquals('SMBFS_LABEL', directoryTree.items[2]!.label);
  assertEquals('NOT_SMB_LABEL', directoryTree.items[3]!.label);
  assertEquals('SMB_LABEL', directoryTree.items[4]!.label);

  const smbFsItem = directoryTree.items[2]!;
  const providerItem = directoryTree.items[3]!;
  const smbItem = directoryTree.items[4]!;
  await waitUntil(() => {
    // Under providerItem there should be 1 entry, 'non_smb_child'.
    return providerItem.items.length === 1;
  });
  assertEquals('non_smb_child', providerItem.items[0]!.label);
  // Ensure there are no entries under smbItem.
  assertEquals(0, smbItem.items.length);
  assertEquals(0, smbFsItem.items.length);

  done();
}

/**
 * Test sub directories are not fetched for SMB, until the directory is
 * clicked.
 */
export async function testSmbNotFetchedUntilClick(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  await addMyFilesAndDriveToStore(initialState);
  // Store initialization will notify DirectoryTree.
  const store = setupStore(initialState);

  const volumeManager = window.fileManager.volumeManager as MockVolumeManager;
  // Add a volume representing a smb provider to the mock filesystem.
  const smbProviderVolumeInfo = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'smb', 'SMB_LABEL', '@smb');
  volumeManager.volumeInfoList.add(smbProviderVolumeInfo);
  // Add a sub directory to the smb provider.
  const smbProviderFs =
      assert(volumeManager.volumeInfoList.item(2).fileSystem) as MockFileSystem;
  smbProviderFs.populate(['/smb_child/']);
  store.dispatch(addVolume({
    volumeInfo: smbProviderVolumeInfo,
    volumeMetadata: createFakeVolumeMetadata(smbProviderVolumeInfo),
  }));

  // At top level, MyFiles and Drive, smb provider volume should be listed.
  await waitUntil(() => directoryTree.items.length === 3);
  assertEquals('Downloads', directoryTree.items[0]!.label);
  assertEquals('Google Drive', directoryTree.items[1]!.label);
  assertEquals('SMB_LABEL', directoryTree.items[2]!.label);

  // Expect the SMB share has no children.
  const smbItem = directoryTree.items[2]!;
  assertEquals(0, smbItem.items.length);

  // Click on the SMB volume.
  smbItem.click();

  await waitUntil(() => {
    // Wait until the SMB share item has been updated with its sub
    // directories.
    return smbItem.items.length == 1;
  });
  assertEquals('smb_child', smbItem.items[0]!.label);

  done();
}

/** Test aria-expanded attribute for directory tree item. */
export async function testAriaExpanded(done: () => void) {
  const initialState = getEmptyState();
  const directoryTree = directoryTreeContainer.tree;

  // Add MyFiles and Drive to the store.
  const {myFilesFs, driveFs} = await addMyFilesAndDriveToStore(initialState);
  // Add sub folders for them.
  myFilesFs.populate(['/folder1/']);
  driveFs.populate(['/root/folder1']);
  // Store initialization will notify DirectoryTree.
  setupStore(initialState);

  // At top level, MyFiles and Drive should be listed.
  await waitUntil(() => directoryTree.items.length === 2);
  const myFilesItem = directoryTree.items[0]!;
  const driveItem = directoryTree.items[1]!;

  await waitUntil(() => {
    // Under the drive item, there exist 3 entries. In MyFiles should
    // exist 1 entry folder1.
    return driveItem.items.length === 3 && myFilesItem.items.length === 1;
  });

  // MyFiles will be expanded by default.
  assertTrue(myFilesItem.expanded);
  const treeItemElement = myFilesItem.shadowRoot!.querySelector('li')!;
  const expandIconElement =
      myFilesItem.shadowRoot!.querySelector<HTMLSpanElement>('.expand-icon')!;
  const ariaExpanded = treeItemElement.getAttribute('aria-expanded');
  assertTrue(ariaExpanded === 'true');
  // .tree-children should have role="group" otherwise Chromevox doesn't
  // speak the depth level properly.
  const treeChildrenElement =
      myFilesItem.shadowRoot!.querySelector<HTMLUListElement>('.tree-children')!
      ;
  assertEquals('group', treeChildrenElement.getAttribute('role'));
  // Click to collapse MyFiles.
  expandIconElement.click();
  await waitForElementUpdate(myFilesItem);
  assertFalse(myFilesItem.expanded);

  // After clicking on expand-icon, aria-expanded should be set to false.
  assertEquals('false', treeItemElement.getAttribute('aria-expanded'));

  done();
}
