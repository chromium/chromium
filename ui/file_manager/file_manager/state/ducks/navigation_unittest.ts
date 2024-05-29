// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {TrashRootEntry} from '../../common/js/trash.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {ICON_TYPES, ODFS_EXTENSION_ID} from '../../foreground/js/constants.js';
import {type AndroidApp, type FileData, type MaterializedView, type NavigationRoot, NavigationSection, NavigationType, type State, type Volume} from '../../state/state.js';
import {convertEntryToFileData} from '../ducks/all_entries.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore, waitDeepEquals} from '../for_tests.js';
import {getEmptyState, getFileData} from '../store.js';

import {updateMaterializedViews} from './materialized_views.js';
import {refreshNavigationRoots} from './navigation.js';
import {convertVolumeInfoAndMetadataToVolume, driveRootEntryListKey, myFilesEntryListKey, recentRootKey, trashRootKey} from './volumes.js';

export function setUp() {
  setUpFileManagerOnWindow();
}

/** Create FileData for recent entry. */
function createRecentFileData(): FileData {
  const recentEntry = new FakeEntryImpl(
      'Recent', RootType.RECENT,
      chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE,
      chrome.fileManagerPrivate.FileCategory.ALL);
  return convertEntryToFileData(recentEntry);
}

/** Create FileData for shortcut entry. */
function createShortcutEntryFileData(
    fileSystemName: string, entryName: string, label: string): FileData {
  const fakeFs = new MockFileSystem(fileSystemName);
  const shortcutEntry = MockFileEntry.create(fakeFs, `/root/${entryName}`);
  return {
    ...convertEntryToFileData(shortcutEntry),
    label,
  };
}

/** Create FileData for MyFiles entry. */
function createMyFilesEntryFileData(): {fileData: FileData, volume: Volume} {
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;
  const myFilesEntry = new VolumeEntry(downloadsVolumeInfo);
  const fileData = convertEntryToFileData(myFilesEntry);
  const volume = convertVolumeInfoAndMetadataToVolume(
      downloadsVolumeInfo, createFakeVolumeMetadata(downloadsVolumeInfo));
  return {fileData, volume};
}

/** Create FileData for drive root entry. */
function createDriveRootEntryListFileData(): FileData {
  const driveRootEntryList =
      new EntryList('Google Drive', RootType.DRIVE_FAKE_ROOT);
  return convertEntryToFileData(driveRootEntryList);
}

/** Create FileData for trash entry. */
function createTrashEntryFileData(): FileData {
  const trashEntry = new TrashRootEntry();
  return convertEntryToFileData(trashEntry);
}

/** Create android apps. */
function createAndroidApps(): [AndroidApp, AndroidApp] {
  return [
    {
      name: 'App 1',
      packageName: 'com.test.app1',
      activityName: 'Activity1',
      iconSet: {icon16x16Url: 'url1', icon32x32Url: 'url2'},
      icon: {icon16x16Url: 'url1', icon32x32Url: 'url2'},
    },
    {
      name: 'App 2',
      packageName: 'com.test.app2',
      activityName: 'Activity2',
      iconSet: {icon16x16Url: '', icon32x32Url: ''},
      icon: ICON_TYPES.GENERIC,
    },
  ];
}

/** Create file data and volume data for volume. */
function createVolumeFileData(
    volumeType: VolumeType, volumeId: string, label: string = '',
    devicePath: string = '',
    providerId: string = ''): {fileData: FileData, volume: Volume} {
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      volumeType, volumeId, label, devicePath, providerId);
  const {volumeManager} = window.fileManager;
  volumeManager.volumeInfoList.add(volumeInfo);
  const volumeEntry = new VolumeEntry(volumeInfo);
  const fileData = convertEntryToFileData(volumeEntry);
  const volume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  return {fileData, volume};
}

/**
 * Tests that navigation roots with all different types:
 * 1. produces the expected order of volumes.
 * 2. manages NavigationSection for the relevant volumes.
 * 3. keeps MTP/Archive/Removable volumes on the original order.
 */
export async function testNavigationRoots(done: () => void) {
  const initialState = getEmptyState();
  // Put recent entry in the store.
  const recentEntryFileData = createRecentFileData();
  initialState.allEntries[recentRootKey] = recentEntryFileData;
  // Put 2 shortcut entries in the store.
  const shortcutEntryFileData1 =
      createShortcutEntryFileData('drive', 'shortcut1', 'Shortcut 1');
  initialState.allEntries[shortcutEntryFileData1.key] = shortcutEntryFileData1;
  initialState.folderShortcuts.push(shortcutEntryFileData1.key);
  const shortcutEntryFileData2 =
      createShortcutEntryFileData('drive', 'shortcut2', 'Shortcut 2');
  initialState.allEntries[shortcutEntryFileData2.key] = shortcutEntryFileData2;
  initialState.folderShortcuts.push(shortcutEntryFileData2.key);
  // Put MyFiles entry in the store.
  const myFilesVolume = createMyFilesEntryFileData();
  initialState.allEntries[myFilesVolume.fileData.key] = myFilesVolume.fileData;
  initialState.volumes[myFilesVolume.volume.volumeId] = myFilesVolume.volume;
  // Put drive entry in the store.
  const driveRootEntryFileData = createDriveRootEntryListFileData();
  initialState.allEntries[driveRootEntryListKey] = driveRootEntryFileData;
  initialState.uiEntries.push(driveRootEntryListKey);
  // Put trash entry in the store.
  const trashEntryFileData = createTrashEntryFileData();
  initialState.allEntries[trashRootKey] = trashEntryFileData;
  initialState.uiEntries.push(trashRootKey);
  // Put the android apps in the store.
  const androidAppsData = createAndroidApps();
  initialState.androidApps[androidAppsData[0].packageName] = androidAppsData[0];
  initialState.androidApps[androidAppsData[1].packageName] = androidAppsData[1];

  // Create different volumes.
  const providerVolume1 =
      createVolumeFileData(VolumeType.PROVIDED, 'provided:prov1');
  initialState.allEntries[providerVolume1.fileData.key] =
      providerVolume1.fileData;
  initialState.volumes[providerVolume1.volume.volumeId] =
      providerVolume1.volume;

  // Set the device paths of the removable volumes to different strings to
  // test the behavior of two physically separate external devices.
  const hogeVolume = createVolumeFileData(
      VolumeType.REMOVABLE, 'removable:hoge', 'Hoge', 'device/path/1');
  initialState.allEntries[hogeVolume.fileData.key] = hogeVolume.fileData;
  initialState.volumes[hogeVolume.volume.volumeId] = hogeVolume.volume;

  const fugaVolume = createVolumeFileData(
      VolumeType.REMOVABLE, 'removable:fuga', 'Fuga', 'device/path/2');
  initialState.allEntries[fugaVolume.fileData.key] = fugaVolume.fileData;
  initialState.volumes[fugaVolume.volume.volumeId] = fugaVolume.volume;

  const archiveVolume =
      createVolumeFileData(VolumeType.ARCHIVE, 'archive:a-rar');
  initialState.allEntries[archiveVolume.fileData.key] = archiveVolume.fileData;
  initialState.volumes[archiveVolume.volume.volumeId] = archiveVolume.volume;

  const mtpVolume = createVolumeFileData(VolumeType.MTP, 'mtp:a-phone');
  initialState.allEntries[mtpVolume.fileData.key] = mtpVolume.fileData;
  initialState.volumes[mtpVolume.volume.volumeId] = mtpVolume.volume;

  const providerVolume2 =
      createVolumeFileData(VolumeType.PROVIDED, 'provided:prov2');
  initialState.allEntries[providerVolume2.fileData.key] =
      providerVolume2.fileData;
  initialState.volumes[providerVolume2.volume.volumeId] =
      providerVolume2.volume;

  const androidFilesVolume =
      createVolumeFileData(VolumeType.ANDROID_FILES, 'android_files:droid');
  androidFilesVolume.volume.prefixKey = myFilesVolume.fileData.key;
  initialState.allEntries[androidFilesVolume.fileData.key] =
      androidFilesVolume.fileData;
  initialState.volumes[androidFilesVolume.volume.volumeId] =
      androidFilesVolume.volume;

  const smbVolume = createVolumeFileData(VolumeType.SMB, 'smb:file-share');
  initialState.allEntries[smbVolume.fileData.key] = smbVolume.fileData;
  initialState.volumes[smbVolume.volume.volumeId] = smbVolume.volume;

  const odfsVolume = createVolumeFileData(
      VolumeType.PROVIDED, 'provided:odfs', '', '', ODFS_EXTENSION_ID);
  initialState.allEntries[odfsVolume.fileData.key] = odfsVolume.fileData;
  initialState.volumes[odfsVolume.volume.volumeId] = odfsVolume.volume;

  const store = setupStore(initialState);

  // Dispatch an action to refresh navigation roots.
  store.dispatch(refreshNavigationRoots());

  // Expect navigation roots being built in the store:
  //  1.  fake-entry://recent
  //  2.  /root/shortcut1
  //  3.  /root/shortcut2
  //  4.  My files
  //      * Android files - won't be included as root because it's inside
  //      MyFiles.
  //  5.  Google Drive
  //  6.  ODFS
  //  7.  smb:file-share
  //  8.  provided:prov1
  //  9.  provided:prov2
  //
  // 10.  removable:hoge
  // 11.  removable:fuga
  // 12.  archive:a-rar  - mounted as archive
  // 13.  mtp:a-phone
  //
  // 14.  android:app1
  // 15.  android:app2
  //
  // 16.  Trash

  // Check items order and that MTP/Archive/Removable respect the original
  // order.
  const want: State['navigation']['roots'] = [
    // recent.
    {
      key: recentEntryFileData.key,
      section: NavigationSection.TOP,
      separator: false,
      type: NavigationType.RECENT,
    },
    // shortcut1.
    {
      key: shortcutEntryFileData1.key,
      section: NavigationSection.TOP,
      separator: false,
      type: NavigationType.SHORTCUT,
    },
    // shortcut2.
    {
      key: shortcutEntryFileData2.key,
      section: NavigationSection.TOP,
      separator: false,
      type: NavigationType.SHORTCUT,
    },
    // My Files.
    {
      key: myFilesVolume.fileData.key,
      section: NavigationSection.MY_FILES,
      separator: true,
      type: NavigationType.VOLUME,
    },
    // Drive.
    {
      key: driveRootEntryFileData.key,
      section: NavigationSection.GOOGLE_DRIVE,
      separator: true,
      type: NavigationType.DRIVE,
    },
    // ODFS
    {
      key: odfsVolume.fileData.key,
      section: NavigationSection.ODFS,
      separator: true,
      type: NavigationType.VOLUME,
    },
    // Other FSP, and SMB are grouped together.
    // smb:file-share.
    {
      key: smbVolume.fileData.key,
      section: NavigationSection.CLOUD,
      separator: true,
      type: NavigationType.VOLUME,
    },
    // provided:prov1.
    {
      key: providerVolume1.fileData.key,
      section: NavigationSection.CLOUD,
      separator: false,
      type: NavigationType.VOLUME,
    },
    // provided:prov2.
    {
      key: providerVolume2.fileData.key,
      section: NavigationSection.CLOUD,
      separator: false,
      type: NavigationType.VOLUME,
    },
    // MTP/Archive/Removable are grouped together.
    // removable:hoge.
    {
      key: hogeVolume.fileData.key,
      section: NavigationSection.REMOVABLE,
      separator: true,
      type: NavigationType.VOLUME,
    },
    // removable:fuga.
    {
      key: fugaVolume.fileData.key,
      section: NavigationSection.REMOVABLE,
      separator: false,
      type: NavigationType.VOLUME,
    },
    // archive:a-rar.
    {
      key: archiveVolume.fileData.key,
      section: NavigationSection.REMOVABLE,
      separator: false,
      type: NavigationType.VOLUME,
    },
    // mtp:a-phone.
    {
      key: mtpVolume.fileData.key,
      section: NavigationSection.REMOVABLE,
      separator: false,
      type: NavigationType.VOLUME,
    },
    // android:app1.
    {
      key: androidAppsData[0].packageName,
      section: NavigationSection.ANDROID_APPS,
      separator: true,
      type: NavigationType.ANDROID_APPS,
    },
    // android:app2.
    {
      key: androidAppsData[1].packageName,
      section: NavigationSection.ANDROID_APPS,
      separator: false,
      type: NavigationType.ANDROID_APPS,
    },
    // Trash.
    {
      key: trashEntryFileData.key,
      section: NavigationSection.TRASH,
      separator: true,
      type: NavigationType.TRASH,
    },
  ];
  await waitDeepEquals(store, want, (state) => state.navigation.roots);

  done();
}

/**
 * Tests navigation roots with no Recents.
 */
export async function testNavigationRootsWithoutRecents(done: () => void) {
  const initialState = getEmptyState();
  // Put shortcut entry in the store.
  const shortcutEntryFileData =
      createShortcutEntryFileData('drive', 'shortcut', 'Shortcut');
  initialState.allEntries[shortcutEntryFileData.key] = shortcutEntryFileData;
  initialState.folderShortcuts.push(shortcutEntryFileData.key);
  // Put MyFiles entry in the store.
  const myFilesVolume = createMyFilesEntryFileData();
  initialState.allEntries[myFilesVolume.fileData.key] = myFilesVolume.fileData;
  initialState.volumes[myFilesVolume.volume.volumeId] = myFilesVolume.volume;

  const store = setupStore(initialState);

  // Dispatch an action to refresh navigation roots.
  store.dispatch(refreshNavigationRoots());

  // Expect 2 navigation roots.
  const want: State['navigation']['roots'] = [
    // shortcut.
    {
      key: shortcutEntryFileData.key,
      section: NavigationSection.TOP,
      separator: false,
      type: NavigationType.SHORTCUT,
    },
    // My Files volume.
    {
      key: myFilesVolume.fileData.key,
      section: NavigationSection.MY_FILES,
      separator: true,
      type: NavigationType.VOLUME,
    },
  ];
  await waitDeepEquals(store, want, (state) => state.navigation.roots);

  done();
}

/**
 * Tests navigation roots with fake MyFiles.
 */
export async function testNavigationRootsWithFakeMyFiles(done: () => void) {
  const initialState = getEmptyState();
  // Put recent entry in the store.
  const recentEntryFileData = createRecentFileData();
  initialState.allEntries[recentRootKey] = recentEntryFileData;
  // Put MyFiles entry in the store.
  const myFilesEntryList = new EntryList('My files', RootType.MY_FILES);
  initialState.allEntries[myFilesEntryList.toURL()] =
      convertEntryToFileData(myFilesEntryList);

  const store = setupStore(initialState);

  // Dispatch an action to refresh navigation roots.
  store.dispatch(refreshNavigationRoots());

  // Expect 2 navigation roots.
  const want: State['navigation']['roots'] = [
    // recent.
    {
      key: recentEntryFileData.key,
      section: NavigationSection.TOP,
      separator: false,
      type: NavigationType.RECENT,
    },
    // My Files entry list.
    {
      key: myFilesEntryList.toURL(),
      section: NavigationSection.MY_FILES,
      separator: true,
      type: NavigationType.ENTRY_LIST,
    },
  ];
  await waitDeepEquals(store, want, (state) => state.navigation.roots);

  done();
}

/**
 * Tests navigation roots with volumes.
 */
export async function testNavigationRootsWithVolumes(done: () => void) {
  const initialState = getEmptyState();
  // Put recent entry in the store.
  const recentEntryFileData = createRecentFileData();
  initialState.allEntries[recentRootKey] = recentEntryFileData;
  // Put MyFiles entry in the store.
  const myFilesVolume = createMyFilesEntryFileData();
  initialState.allEntries[myFilesVolume.fileData.key] = myFilesVolume.fileData;
  initialState.volumes[myFilesVolume.volume.volumeId] = myFilesVolume.volume;
  // Put drive entry in the store.
  const driveRootEntryFileData = createDriveRootEntryListFileData();
  initialState.allEntries[driveRootEntryListKey] = driveRootEntryFileData;
  initialState.uiEntries.push(driveRootEntryListKey);

  // Put removable volume 'hoge' in the store.
  const hogeVolume = createVolumeFileData(
      VolumeType.REMOVABLE, 'removable:hoge', 'Hoge', 'device/path/1');
  initialState.allEntries[hogeVolume.fileData.key] = hogeVolume.fileData;
  initialState.volumes[hogeVolume.volume.volumeId] = hogeVolume.volume;

  // Create a shortcut for the 'hoge' volume in the store.
  const hogeShortcutEntryFileData = createShortcutEntryFileData(
      hogeVolume.volume.volumeId, 'shortcut-hoge', 'Hoge shortcut');
  initialState.allEntries[hogeShortcutEntryFileData.key] =
      hogeShortcutEntryFileData;
  initialState.folderShortcuts.push(hogeShortcutEntryFileData.key);

  // Put removable volume 'fuga' in the store. Not a partition, so set a
  // different device path to 'hoge'.
  const fugaVolume = createVolumeFileData(
      VolumeType.REMOVABLE, 'removable:fuga', 'Fuga', 'device/path/2');
  initialState.allEntries[fugaVolume.fileData.key] = fugaVolume.fileData;
  initialState.volumes[fugaVolume.volume.volumeId] = fugaVolume.volume;

  const store = setupStore(initialState);

  // Dispatch an action to refresh navigation roots.
  store.dispatch(refreshNavigationRoots());

  // Expect 6 navigation roots.
  const want: State['navigation']['roots'] = [
    // recent.
    {
      key: recentEntryFileData.key,
      section: NavigationSection.TOP,
      separator: false,
      type: NavigationType.RECENT,
    },
    // hoge shortcut.
    {
      key: hogeShortcutEntryFileData.key,
      section: NavigationSection.TOP,
      separator: false,
      type: NavigationType.SHORTCUT,
    },
    // My Files.
    {
      key: myFilesVolume.fileData.key,
      section: NavigationSection.MY_FILES,
      separator: true,
      type: NavigationType.VOLUME,
    },
    // Drive.
    {
      key: driveRootEntryFileData.key,
      section: NavigationSection.GOOGLE_DRIVE,
      separator: true,
      type: NavigationType.DRIVE,
    },
    // hoge volume.
    {
      key: hogeVolume.fileData.key,
      section: NavigationSection.REMOVABLE,
      separator: true,
      type: NavigationType.VOLUME,
    },
    // fuga volume.
    {
      key: fugaVolume.fileData.key,
      section: NavigationSection.REMOVABLE,
      separator: false,
      type: NavigationType.VOLUME,
    },
  ];
  await waitDeepEquals(store, want, (state) => state.navigation.roots);

  done();
}

/**
 * Tests that for multiple partition volumes, only the parent entry will be
 * added to the navigation roots.
 */
export async function testMultipleUsbPartitionsGrouping(done: () => void) {
  const initialState = getEmptyState();

  // Add parent entry list to the store.
  const devicePath = 'device/path/1';
  const parentEntry =
      new EntryList('Partition wrap', RootType.REMOVABLE, devicePath);
  initialState.allEntries[parentEntry.toURL()] =
      convertEntryToFileData(parentEntry);
  // Create 3 volumes with the same device path so the partitions are grouped.
  const partitionVolume1 = createVolumeFileData(
      VolumeType.REMOVABLE, 'removable:partition1', 'partition1', devicePath);
  partitionVolume1.volume.prefixKey = parentEntry.toURL();
  initialState.allEntries[partitionVolume1.fileData.key] =
      partitionVolume1.fileData;
  initialState.volumes[partitionVolume1.volume.volumeId] =
      partitionVolume1.volume;
  const partitionVolume2 = createVolumeFileData(
      VolumeType.REMOVABLE, 'removable:partition2', 'partition2', devicePath);
  initialState.allEntries[partitionVolume2.fileData.key] =
      partitionVolume2.fileData;
  initialState.volumes[partitionVolume2.volume.volumeId] =
      partitionVolume2.volume;
  partitionVolume2.volume.prefixKey = parentEntry.toURL();
  const partitionVolume3 = createVolumeFileData(
      VolumeType.REMOVABLE, 'removable:partition3', 'partition3', devicePath);
  initialState.allEntries[partitionVolume3.fileData.key] =
      partitionVolume3.fileData;
  initialState.volumes[partitionVolume3.volume.volumeId] =
      partitionVolume3.volume;
  partitionVolume3.volume.prefixKey = parentEntry.toURL();

  const store = setupStore(initialState);

  // Dispatch an action to refresh navigation roots.
  store.dispatch(refreshNavigationRoots());

  // Expect only the parent entry and MyFiles being added to the navigation
  // roots.
  const want: State['navigation']['roots'] = [
    // My Files entry list.
    {
      key: myFilesEntryListKey,
      section: NavigationSection.MY_FILES,
      separator: false,
      type: NavigationType.ENTRY_LIST,
    },
    // parent entry for all removable partitions.
    {
      key: parentEntry.toURL(),
      section: NavigationSection.REMOVABLE,
      separator: true,
      type: NavigationType.VOLUME,
    },
  ];
  await waitDeepEquals(store, want, (state) => state.navigation.roots);

  done();
}

/**
 * Tests that the volumes filtered by the volume manager won't be shown in the
 * navigation roots.
 */
export async function testNavigationRootsWithFilteredVolume(done: () => void) {
  const initialState = getEmptyState();
  // Put volume1 in the store.
  const volume1 = createVolumeFileData(VolumeType.REMOVABLE, 'removable1');
  initialState.allEntries[volume1.fileData.key] = volume1.fileData;
  initialState.volumes[volume1.volume.volumeId] = volume1.volume;
  // Put volume2 in the store.
  const volumeInfo2 = MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable2');
  const volumeEntry2 = new VolumeEntry(volumeInfo2);
  initialState.allEntries[volumeEntry2.toURL()] =
      convertEntryToFileData(volumeEntry2);
  initialState.volumes[volumeInfo2.volumeId] =
      convertVolumeInfoAndMetadataToVolume(
          volumeInfo2, createFakeVolumeMetadata(volumeInfo2));
  // Mark volume2 as not allowed volume.
  const {volumeManager} = window.fileManager;
  volumeManager.isAllowedVolume = (volumeInfo) => volumeInfo !== volumeInfo2;

  const store = setupStore(initialState);

  // Dispatch an action to refresh navigation roots.
  store.dispatch(refreshNavigationRoots());

  // Expect only volume1 and MyFiles in the navigation roots.
  const want: State['navigation']['roots'] = [
    // My Files entry list.
    {
      key: myFilesEntryListKey,
      section: NavigationSection.MY_FILES,
      separator: false,
      type: NavigationType.ENTRY_LIST,
    },
    // volume1.
    {
      key: volume1.fileData.key,
      section: NavigationSection.REMOVABLE,
      separator: true,
      type: NavigationType.VOLUME,
    },
  ];
  await waitDeepEquals(store, want, (state) => state.navigation.roots);

  done();
}

/**
 * Tests that materialized views add navigation roots.
 */
export async function testNavigationRootsWithMaterializedViews(
    done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  // Roots and materialized views start empty.
  assertDeepEquals(store.getState().materializedViews, []);
  assertDeepEquals(store.getState().navigation.roots, []);

  // Dispatch an action to refresh navigation roots.
  store.dispatch(refreshNavigationRoots());

  // Everything is still empty.
  assertDeepEquals(
      store.getState().materializedViews, [], 'expected empty views');
  // It contains My files by default.
  assertEquals(store.getState().navigation.roots.length, 1);
  assertEquals(
      getFileData(store.getState(), store.getState().navigation.roots[0]!.key)
          ?.label,
      'My files');
  const myFilesRoot = store.getState().navigation.roots[0]!;

  // Add a view.
  store.dispatch(updateMaterializedViews({
    materializedViews: [
      {
        viewId: 1,
        name: 'test view',
      },
    ],
  }));

  const viewKey = 'materialized-view://1/';
  const wantView: MaterializedView = {
    id: '1',
    key: viewKey,
    label: 'test view',
    icon: ICON_TYPES.STAR,
    isRoot: true,
  };
  await waitDeepEquals(store, [wantView], (state) => state.materializedViews);

  // Refresh to pick up the new view.
  store.dispatch(refreshNavigationRoots());
  const wantViewRoot: NavigationRoot = {
    key: viewKey,
    section: NavigationSection.TOP,
    separator: false,
    type: NavigationType.MATERIALIZED_VIEW,
  };
  await waitDeepEquals(
      store, [wantViewRoot, myFilesRoot], (state) => state.navigation.roots);

  done();
}
