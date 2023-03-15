// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {TrashRootEntry} from '../../common/js/trash.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryType, FileData, NavigationSection, NavigationType, Volume} from '../../externs/ts/state.js';
import {refreshNavigationRoots as refreshNavigationRootsAction, updateNavigationEntry as updateNavigationEntryAction} from '../actions/navigation.js';
import {createFakeFileData, createFakeVolume, setUpFileManagerOnWindow} from '../for_tests.js';
import {getEmptyState} from '../store.js';

import {refreshNavigationRoots, updateNavigationEntry} from './navigation.js';
import {driveRootEntryListKey, myFilesEntryListKey, recentRootKey, trashRootKey} from './volumes.js';

export function setUp() {
  setUpFileManagerOnWindow();
}

/** Create FileData for recent entry. */
function createRecentFileData(): FileData {
  const recentEntry = new FakeEntryImpl(
      'Recent', VolumeManagerCommon.RootType.RECENT,
      chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE,
      chrome.fileManagerPrivate.FileCategory.ALL);
  return createFakeFileData({
    entry: recentEntry,
    label: 'Recent',
    type: EntryType.RECENT,
  });
}

/** Create FileData for shortcut entry. */
function createShortcutEntryFileData(
    fileSystemName: string, entryName: string, label: string): FileData {
  const fakeFs = new MockFileSystem(fileSystemName);
  const shortcutEntry = MockFileEntry.create(fakeFs, `/root/${entryName}`);
  return createFakeFileData({
    entry: shortcutEntry,
    label,
    type: EntryType.FS_API,
  });
}

/** Create FileData for MyFiles entry. */
function createMyFilesEntryFileData(): {fileData: FileData, volume: Volume} {
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const myFilesEntry = new VolumeEntry(downloadsVolumeInfo);
  const fileData = createFakeFileData({
    entry: myFilesEntry,
    volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
    label: 'My files',
    type: EntryType.VOLUME_ROOT,
  });
  const volume = createFakeVolume({
    volumeId: downloadsVolumeInfo.volumeId,
    volumeType: fileData.volumeType!,
    label: fileData.label,
    rootKey: fileData.entry.toURL(),
  });
  return {fileData, volume};
}

/** Create FileData for drive root entry. */
function createDriveRootEntryFileData(): FileData {
  const driveEntry = new EntryList(
      'Google Drive', VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT);
  return createFakeFileData({
    entry: driveEntry,
    label: 'Google Drive',
    type: EntryType.ENTRY_LIST,
  });
}

/** Create FileData for trash entry. */
function createTrashEntryFileData(): FileData {
  const trashEntry = new TrashRootEntry();
  return createFakeFileData({
    entry: trashEntry,
    label: 'Bin',
    type: EntryType.TRASH,
  });
}

/** Create android apps. */
function createAndroidApps(): [
  chrome.fileManagerPrivate.AndroidApp, chrome.fileManagerPrivate.AndroidApp
] {
  return [
    {
      name: 'App 1',
      packageName: 'com.test.app1',
      activityName: 'Activity1',
      iconSet: {icon16x16Url: 'url1', icon32x32Url: 'url2'},
    },
    {
      name: 'App 2',
      packageName: 'com.test.app2',
      activityName: 'Activity2',
      iconSet: {icon16x16Url: 'url3', icon32x32Url: 'url4'},
    },
  ];
}

/** Create file data and volume data for volume. */
function createVolumeFileData(
    volumeType: VolumeManagerCommon.VolumeType, volumeId: string,
    label: string = '',
    devicePath: string = ''): {fileData: FileData, volume: Volume} {
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      volumeType, volumeId, label, devicePath);
  const {volumeManager} = window.fileManager;
  volumeManager.volumeInfoList.add(volumeInfo);
  const volumeEntry = new VolumeEntry(volumeInfo);
  const fileData = createFakeFileData({
    entry: volumeEntry,
    label,
    type: EntryType.VOLUME_ROOT,
  });
  const volume = createFakeVolume({
    volumeId: volumeInfo.volumeId,
    volumeType: volumeInfo.volumeType,
    label,
    rootKey: volumeEntry.toURL(),
    devicePath,
  });
  return {fileData, volume};
}

/**
 * Tests that navigation roots with all different types:
 * 1. produces the expected order of volumes.
 * 2. manages NavigationSection for the relevant volumes.
 * 3. keeps MTP/Archive/Removable volumes on the original order.
 */
export function testNavigationRoots() {
  const currentState = getEmptyState();
  // Put recent entry in the store.
  const recentEntryFileData = createRecentFileData();
  currentState.allEntries[recentRootKey] = recentEntryFileData;
  // Put 2 shortcut entries in the store.
  const shortcutEntryFileData1 =
      createShortcutEntryFileData('drive', 'shortcut1', 'Shortcut 1');
  currentState.allEntries[shortcutEntryFileData1.entry.toURL()] =
      shortcutEntryFileData1;
  currentState.folderShortcuts.push(shortcutEntryFileData1.entry.toURL());
  const shortcutEntryFileData2 =
      createShortcutEntryFileData('drive', 'shortcut2', 'Shortcut 2');
  currentState.allEntries[shortcutEntryFileData2.entry.toURL()] =
      shortcutEntryFileData2;
  currentState.folderShortcuts.push(shortcutEntryFileData2.entry.toURL());
  // Put MyFiles entry in the store.
  const myFilesVolume = createMyFilesEntryFileData();
  currentState.allEntries[myFilesVolume.fileData.entry.toURL()] =
      myFilesVolume.fileData;
  currentState.volumes[myFilesVolume.volume.volumeId] = myFilesVolume.volume;
  // Put drive entry in the store.
  const driveRootEntryFileData = createDriveRootEntryFileData();
  currentState.allEntries[driveRootEntryListKey] = driveRootEntryFileData;
  // Put trash entry in the store.
  const trashEntryFileData = createTrashEntryFileData();
  currentState.allEntries[trashRootKey] = trashEntryFileData;
  // Put the android apps in the store.
  const androidAppsData = createAndroidApps();
  currentState.androidApps[androidAppsData[0].packageName] = androidAppsData[0];
  currentState.androidApps[androidAppsData[1].packageName] = androidAppsData[1];

  // Create different volumes.
  const providerVolume1 = createVolumeFileData(
      VolumeManagerCommon.VolumeType.PROVIDED, 'provided:prov1');
  currentState.allEntries[providerVolume1.fileData.entry.toURL()] =
      providerVolume1.fileData;
  currentState.volumes[providerVolume1.volume.volumeId] =
      providerVolume1.volume;

  // Set the device paths of the removable volumes to different strings to
  // test the behavior of two physically separate external devices.
  const hogeVolume = createVolumeFileData(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:hoge', 'Hoge',
      'device/path/1');
  currentState.allEntries[hogeVolume.fileData.entry.toURL()] =
      hogeVolume.fileData;
  currentState.volumes[hogeVolume.volume.volumeId] = hogeVolume.volume;

  const fugaVolume = createVolumeFileData(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:fuga', 'Fuga',
      'device/path/2');
  currentState.allEntries[fugaVolume.fileData.entry.toURL()] =
      fugaVolume.fileData;
  currentState.volumes[fugaVolume.volume.volumeId] = fugaVolume.volume;

  const archiveVolume = createVolumeFileData(
      VolumeManagerCommon.VolumeType.ARCHIVE, 'archive:a-rar');
  currentState.allEntries[archiveVolume.fileData.entry.toURL()] =
      archiveVolume.fileData;
  currentState.volumes[archiveVolume.volume.volumeId] = archiveVolume.volume;

  const mtpVolume =
      createVolumeFileData(VolumeManagerCommon.VolumeType.MTP, 'mtp:a-phone');
  currentState.allEntries[mtpVolume.fileData.entry.toURL()] =
      mtpVolume.fileData;
  currentState.volumes[mtpVolume.volume.volumeId] = mtpVolume.volume;

  const providerVolume2 = createVolumeFileData(
      VolumeManagerCommon.VolumeType.PROVIDED, 'provided:prov2');
  currentState.allEntries[providerVolume2.fileData.entry.toURL()] =
      providerVolume2.fileData;
  currentState.volumes[providerVolume2.volume.volumeId] =
      providerVolume2.volume;

  const androidFilesVolume = createVolumeFileData(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'android_files:droid');
  androidFilesVolume.volume.prefixKey = myFilesVolume.fileData.entry.toURL();
  currentState.allEntries[androidFilesVolume.fileData.entry.toURL()] =
      androidFilesVolume.fileData;
  currentState.volumes[androidFilesVolume.volume.volumeId] =
      androidFilesVolume.volume;

  const smbVolume = createVolumeFileData(
      VolumeManagerCommon.VolumeType.SMB, 'smb:file-share');
  currentState.allEntries[smbVolume.fileData.entry.toURL()] =
      smbVolume.fileData;
  currentState.volumes[smbVolume.volume.volumeId] = smbVolume.volume;

  const newState =
      refreshNavigationRoots(currentState, refreshNavigationRootsAction());
  // Navigation roots built above:
  //  1.  fake-entry://recent
  //  2.  /root/shortcut1
  //  3.  /root/shortcut2
  //  4.  My files
  //      * Android files - won't be included as root because it's inside
  //      MyFiles.
  //  5.  Drive
  //  6.  Trash
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

  // Check items order and that MTP/Archive/Removable respect the original
  // order.
  const {roots} = newState.navigation;
  assertEquals(15, roots.length);

  // recent.
  assertEquals(recentEntryFileData.entry.toURL(), roots[0]!.key);
  assertEquals(NavigationSection.TOP, roots[0]!.section);
  assertEquals(false, roots[0]!.separator);
  assertEquals(NavigationType.RECENT, roots[0]!.type);
  // shortcut1.
  assertEquals(shortcutEntryFileData1.entry.toURL(), roots[1]!.key);
  assertEquals(NavigationSection.TOP, roots[1]!.section);
  assertEquals(false, roots[1]!.separator);
  assertEquals(NavigationType.SHORTCUT, roots[1]!.type);
  // shortcut2.
  assertEquals(shortcutEntryFileData2.entry.toURL(), roots[2]!.key);
  assertEquals(NavigationSection.TOP, roots[2]!.section);
  assertEquals(false, roots[2]!.separator);
  assertEquals(NavigationType.SHORTCUT, roots[2]!.type);

  // My Files.
  assertEquals(myFilesVolume.fileData.entry.toURL(), roots[3]!.key);
  assertEquals(NavigationSection.MY_FILES, roots[3]!.section);
  assertEquals(true, roots[3]!.separator);
  assertEquals(NavigationType.VOLUME, roots[3]!.type);

  // Drive.
  assertEquals(driveRootEntryFileData.entry.toURL(), roots[4]!.key);
  assertEquals(NavigationSection.CLOUD, roots[4]!.section);
  assertEquals(true, roots[4]!.separator);
  assertEquals(NavigationType.DRIVE, roots[4]!.type);

  // Trash.
  assertEquals(trashEntryFileData.entry.toURL(), roots[5]!.key);
  assertEquals(NavigationSection.TRASH, roots[5]!.section);
  assertEquals(true, roots[5]!.separator);
  assertEquals(NavigationType.TRASH, roots[5]!.type);

  // FSP, and SMB are grouped together.
  // smb:file-share.
  assertEquals(smbVolume.fileData.entry.toURL(), roots[6]!.key);
  assertEquals(NavigationSection.CLOUD, roots[6]!.section);
  assertEquals(true, roots[6]!.separator);
  assertEquals(NavigationType.VOLUME, roots[6]!.type);
  // provided:prov1.
  assertEquals(providerVolume1.fileData.entry.toURL(), roots[7]!.key);
  assertEquals(NavigationSection.CLOUD, roots[7]!.section);
  assertEquals(false, roots[7]!.separator);
  assertEquals(NavigationType.VOLUME, roots[7]!.type);
  // provided:prov2.
  assertEquals(providerVolume2.fileData.entry.toURL(), roots[8]!.key);
  assertEquals(NavigationSection.CLOUD, roots[8]!.section);
  assertEquals(false, roots[8]!.separator);
  assertEquals(NavigationType.VOLUME, roots[8]!.type);

  // MTP/Archive/Removable are grouped together.
  // removable:hoge.
  assertEquals(hogeVolume.fileData.entry.toURL(), roots[9]!.key);
  assertEquals(NavigationSection.REMOVABLE, roots[9]!.section);
  assertEquals(true, roots[9]!.separator);
  assertEquals(NavigationType.VOLUME, roots[9]!.type);
  // removable:fuga.
  assertEquals(fugaVolume.fileData.entry.toURL(), roots[10]!.key);
  assertEquals(NavigationSection.REMOVABLE, roots[10]!.section);
  assertEquals(false, roots[10]!.separator);
  assertEquals(NavigationType.VOLUME, roots[10]!.type);
  // archive:a-rar.
  assertEquals(archiveVolume.fileData.entry.toURL(), roots[11]!.key);
  assertEquals(NavigationSection.REMOVABLE, roots[11]!.section);
  assertEquals(false, roots[11]!.separator);
  assertEquals(NavigationType.VOLUME, roots[11]!.type);
  // mtp:a-phone.
  assertEquals(mtpVolume.fileData.entry.toURL(), roots[12]!.key);
  assertEquals(NavigationSection.REMOVABLE, roots[12]!.section);
  assertEquals(false, roots[12]!.separator);
  assertEquals(NavigationType.VOLUME, roots[12]!.type);

  // android:app1
  assertEquals(androidAppsData[0].packageName, roots[13]!.key);
  assertEquals(NavigationSection.ANDROID_APPS, roots[13]!.section);
  assertEquals(true, roots[13]!.separator);
  assertEquals(NavigationType.ANDROID_APPS, roots[13]!.type);
  // android:app2
  assertEquals(androidAppsData[1].packageName, roots[14]!.key);
  assertEquals(NavigationSection.ANDROID_APPS, roots[14]!.section);
  assertEquals(false, roots[14]!.separator);
  assertEquals(NavigationType.ANDROID_APPS, roots[14]!.type);
}

/**
 * Tests navigation roots with no Recents.
 */
export function testNavigationRootsWithoutRecents() {
  const currentState = getEmptyState();
  // Put shortcut entry in the store.
  const shortcutEntryFileData =
      createShortcutEntryFileData('drive', 'shortcut', 'Shortcut');
  currentState.allEntries[shortcutEntryFileData.entry.toURL()] =
      shortcutEntryFileData;
  currentState.folderShortcuts.push(shortcutEntryFileData.entry.toURL());
  // Put MyFiles entry in the store.
  const myFilesVolume = createMyFilesEntryFileData();
  currentState.allEntries[myFilesVolume.fileData.entry.toURL()] =
      myFilesVolume.fileData;
  currentState.volumes[myFilesVolume.volume.volumeId] = myFilesVolume.volume;

  const newState =
      refreshNavigationRoots(currentState, refreshNavigationRootsAction());

  // Expect 2 navigation roots.
  const {roots} = newState.navigation;
  assertEquals(2, roots.length);
  assertDeepEquals(
      {
        key: shortcutEntryFileData.entry.toURL(),
        section: NavigationSection.TOP,
        separator: false,
        type: NavigationType.SHORTCUT,
      },
      roots[0]);
  assertEquals(myFilesVolume.fileData.entry.toURL(), roots[1]!.key);
}

/**
 * Tests navigation roots with fake MyFiles.
 */
export function testNavigationRootsWithFakeMyFiles() {
  const currentState = getEmptyState();
  // Put recent entry in the store.
  const recentEntryFileData = createRecentFileData();
  currentState.allEntries[recentRootKey] = recentEntryFileData;
  // Put MyFiles entry in the store.
  const myFilesEntryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);
  currentState.allEntries[myFilesEntryList.toURL()] = createFakeFileData({
    entry: myFilesEntryList,
    label: 'My files',
    type: EntryType.ENTRY_LIST,
  });

  const newState =
      refreshNavigationRoots(currentState, refreshNavigationRootsAction());

  // Expect 2 navigation roots.
  const {roots} = newState.navigation;
  assertEquals(2, roots.length);
  // The type of MyFiles navigation root should be ENTRY_LIST.
  assertDeepEquals(
      {
        key: myFilesEntryList.toURL(),
        section: NavigationSection.MY_FILES,
        separator: true,
        type: NavigationType.ENTRY_LIST,
      },
      roots[1]);
}

/**
 * Tests navigation roots with volumes.
 */
export function testNavigationRootsWithVolumes() {
  const currentState = getEmptyState();
  // Put recent entry in the store.
  const recentEntryFileData = createRecentFileData();
  currentState.allEntries[recentRootKey] = recentEntryFileData;
  // Put MyFiles entry in the store.
  const myFilesVolume = createMyFilesEntryFileData();
  currentState.allEntries[myFilesVolume.fileData.entry.toURL()] =
      myFilesVolume.fileData;
  currentState.volumes[myFilesVolume.volume.volumeId] = myFilesVolume.volume;
  // Put drive entry in the store.
  const driveRootEntryFileData = createDriveRootEntryFileData();
  currentState.allEntries[driveRootEntryListKey] = driveRootEntryFileData;

  // Put removable volume 'hoge' in the store.
  const hogeVolume = createVolumeFileData(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:hoge', 'Hoge',
      'device/path/1');
  currentState.allEntries[hogeVolume.fileData.entry.toURL()] =
      hogeVolume.fileData;
  currentState.volumes[hogeVolume.volume.volumeId] = hogeVolume.volume;

  // Create a shortcut for the 'hoge' volume in the store.
  const hogeShortcutEntryFileData = createShortcutEntryFileData(
      hogeVolume.volume.volumeId, 'shortcut-hoge', 'Hoge shortcut');
  currentState.allEntries[hogeShortcutEntryFileData.entry.toURL()] =
      hogeShortcutEntryFileData;
  currentState.folderShortcuts.push(hogeShortcutEntryFileData.entry.toURL());

  // Put removable volume 'fuga' in the store. Not a partition, so set a
  // different device path to 'hoge'.
  const fugaVolume = createVolumeFileData(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:fuga', 'Fuga',
      'device/path/2');
  currentState.allEntries[fugaVolume.fileData.entry.toURL()] =
      fugaVolume.fileData;
  currentState.volumes[fugaVolume.volume.volumeId] = fugaVolume.volume;

  const newState =
      refreshNavigationRoots(currentState, refreshNavigationRootsAction());

  // Expect 6 navigation roots.
  const {roots} = newState.navigation;
  assertEquals(6, roots.length);
  assertEquals(recentEntryFileData.entry.toURL(), roots[0]!.key);
  assertDeepEquals(
      {
        key: hogeShortcutEntryFileData.entry.toURL(),
        section: NavigationSection.TOP,
        separator: false,
        type: NavigationType.SHORTCUT,
      },
      roots[1]);
  assertEquals(myFilesVolume.fileData.entry.toURL(), roots[2]!.key);
  assertEquals(driveRootEntryFileData.entry.toURL(), roots[3]!.key);
  assertDeepEquals(
      {
        key: hogeVolume.fileData.entry.toURL(),
        section: NavigationSection.REMOVABLE,
        separator: true,
        type: NavigationType.VOLUME,
      },
      roots[4]);
  assertDeepEquals(
      {
        key: fugaVolume.fileData.entry.toURL(),
        section: NavigationSection.REMOVABLE,
        separator: false,
        type: NavigationType.VOLUME,
      },
      roots[5]);
}

/**
 * Tests that for multiple partition volumes, only the parent entry will be
 * added to the navigation roots.
 */
export function testMultipleUsbPartitionsGrouping() {
  const currentState = getEmptyState();

  // Add parent entry list to the store.
  const devicePath = 'device/path/1';
  const parentEntry = new EntryList(
      'Partition wrap', VolumeManagerCommon.RootType.REMOVABLE, devicePath);
  currentState.allEntries[parentEntry.toURL()] = createFakeFileData({
    entry: parentEntry,
    label: 'Partition wrap',
    type: EntryType.ENTRY_LIST,
  });
  // Create 3 volumes with the same device path so the partitions are grouped.
  const partitionVolume1 = createVolumeFileData(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition1',
      'partition1', devicePath);
  partitionVolume1.volume.prefixKey = parentEntry.toURL();
  currentState.allEntries[partitionVolume1.fileData.entry.toURL()] =
      partitionVolume1.fileData;
  currentState.volumes[partitionVolume1.volume.volumeId] =
      partitionVolume1.volume;
  const partitionVolume2 = createVolumeFileData(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition2',
      'partition2', devicePath);
  currentState.allEntries[partitionVolume2.fileData.entry.toURL()] =
      partitionVolume2.fileData;
  currentState.volumes[partitionVolume2.volume.volumeId] =
      partitionVolume2.volume;
  partitionVolume2.volume.prefixKey = parentEntry.toURL();
  const partitionVolume3 = createVolumeFileData(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition3',
      'partition3', devicePath);
  currentState.allEntries[partitionVolume3.fileData.entry.toURL()] =
      partitionVolume3.fileData;
  currentState.volumes[partitionVolume3.volume.volumeId] =
      partitionVolume3.volume;
  partitionVolume3.volume.prefixKey = parentEntry.toURL();

  const newState =
      refreshNavigationRoots(currentState, refreshNavigationRootsAction());

  // Expect only the parent entry and MyFiles being added to the navigation
  // roots.
  const {roots} = newState.navigation;
  assertEquals(2, roots.length);
  assertEquals(myFilesEntryListKey, roots[0]!.key);
  assertDeepEquals(
      {
        key: parentEntry.toURL(),
        section: NavigationSection.REMOVABLE,
        separator: true,
        type: NavigationType.VOLUME,
      },
      roots[1]);
}

/**
 * Tests that the volumes filtered by the volume manager won't be shown in the
 * navigation roots.
 */
export function testNavigationRootsWithFilteredVolume() {
  const currentState = getEmptyState();
  // Put 2 volumes in the store.
  const volume1 = createVolumeFileData(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable1');
  currentState.allEntries[volume1.fileData.entry.toURL()] = volume1.fileData;
  currentState.volumes[volume1.volume.volumeId] = volume1.volume;
  // Volume2 is not added to VolumeManager's volumeInfoList.
  const volumeInfo2 = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable2');
  const volumeEntry2 = new VolumeEntry(volumeInfo2);
  currentState.allEntries[volumeEntry2.toURL()] = createFakeFileData({
    entry: volumeEntry2,
    label: volumeInfo2.label,
    type: EntryType.VOLUME_ROOT,
  });
  currentState.volumes[volumeInfo2.volumeId] = createFakeVolume({
    volumeId: volumeInfo2.volumeId,
    volumeType: volumeInfo2.volumeType,
    label: volumeInfo2.label,
    rootKey: volumeEntry2.toURL(),
  });

  const newState =
      refreshNavigationRoots(currentState, refreshNavigationRootsAction());

  // Expect only volume1 and MyFiles in the navigation roots.
  const {roots} = newState.navigation;
  assertEquals(2, roots.length);
  assertEquals(myFilesEntryListKey, roots[0]!.key);
  assertEquals(volume1.fileData.entry.toURL(), roots[1]!.key);
}

/** Tests that navigation entry can be updated correctly. */
export function testUpdateNavigationEntry() {
  const currentState = getEmptyState();
  // Add MyFiles entry to the store.
  const myFilesVolume = createMyFilesEntryFileData();
  const myFilesEntryKey = myFilesVolume.fileData.entry.toURL();
  currentState.allEntries[myFilesEntryKey] = myFilesVolume.fileData;

  assertFalse(currentState.allEntries[myFilesEntryKey].expanded);
  const newState = updateNavigationEntry(
      currentState,
      updateNavigationEntryAction({key: myFilesEntryKey, expanded: true}));
  assertTrue(newState.allEntries[myFilesEntryKey].expanded);
}

/** Tests that navigation entry won't be updated without valid file data. */
export function testUpdateNavigationEntryWithoutValidFileData() {
  const currentState = getEmptyState();

  const newState = updateNavigationEntry(
      currentState,
      updateNavigationEntryAction({key: 'not-exist-key', expanded: true}));
  // Check state won't be touched.
  assertEquals(currentState, newState);
}
