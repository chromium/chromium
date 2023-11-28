// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {fakeDriveVolumeId, MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {VolumeInfoImpl} from '../../background/js/volume_info_impl.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {isSinglePartitionFormatEnabled} from '../../common/js/flags.js';
import {MockCommandLinePrivate} from '../../common/js/mock_chrome.js';
import {MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise, waitUntil} from '../../common/js/test_error_reporting.js';
import {str} from '../../common/js/translations.js';
import {TrashRootEntry} from '../../common/js/trash.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {DialogType} from '../../externs/ts/state.js';

import {AndroidAppListModel} from './android_app_list_model.js';
import {constants} from './constants.js';
import {DirectoryModel} from './directory_model.js';
import {createFakeAndroidAppListModel} from './fake_android_app_list_model.js';
import {createFakeDirectoryModel} from './mock_directory_model.js';
import {MockFolderShortcutDataModel} from './mock_folder_shortcut_data_model.js';
import {NavigationListModel, NavigationModelAndroidAppItem, NavigationModelFakeItem, NavigationModelItemType, NavigationModelShortcutItem, NavigationModelVolumeItem, NavigationSection} from './navigation_list_model.js';

/**
 * Mock Recent fake entry.
 * @type {!FilesAppEntry}
 */
const recentFakeEntry =
    /** @type {!FilesAppEntry} */ ({toURL: () => 'fake-entry://recent'});

/**
 * Directory model.
 * @type {!DirectoryModel}
 */
let directoryModel;

/**
 * AndroidAppList model.
 * @type {!AndroidAppListModel}
 */
let androidAppListModel;

/**
 * Drive file system.
 * @type {!MockFileSystem}
 */
let drive;

/**
 * Removable volume file system.
 * @type {!MockFileSystem}
 */
let hoge;

// Setup the test components.
export function setUp() {
  // Mock chrome APIs.
  new MockCommandLinePrivate();

  // Override VolumeInfo.prototype.resolveDisplayRoot to be sync.
  // @ts-ignore: error TS7006: Parameter 'successCallback' implicitly has an
  // 'any' type.
  VolumeInfoImpl.prototype.resolveDisplayRoot = function(successCallback) {
    // @ts-ignore: error TS2341: Property 'fileSystem_' is private and only
    // accessible within class 'VolumeInfoImpl'.
    this.displayRoot_ = this.fileSystem_.root;
    // @ts-ignore: error TS2341: Property 'displayRoot_' is private and only
    // accessible within class 'VolumeInfoImpl'.
    successCallback(this.displayRoot_);
    // @ts-ignore: error TS2341: Property 'fileSystem_' is private and only
    // accessible within class 'VolumeInfoImpl'.
    return Promise.resolve(this.fileSystem_.root);
  };

  // Create mock components.
  directoryModel = createFakeDirectoryModel();
  androidAppListModel = createFakeAndroidAppListModel([]);
  drive = new MockFileSystem('drive');
  hoge = new MockFileSystem('removable:hoge');
}

/**
 * Tests model.
 */
export function testModel() {
  const volumeManager = new MockVolumeManager();

  const shortcutListModel = new MockFolderShortcutDataModel(
      [MockFileEntry.create(drive, '/root/shortcut')]);
  const recentItem = new NavigationModelFakeItem(
      'recent-label', NavigationModelItemType.RECENT, recentFakeEntry);

  const crostiniFakeItem = new NavigationModelFakeItem(
      'linux-files-label', NavigationModelItemType.CROSTINI,
      new FakeEntryImpl('linux-files-label', RootType.CROSTINI));

  const androidAppListModelWithApps =
      createFakeAndroidAppListModel(['android:app1', 'android:app2']);

  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModelWithApps, DialogType.FULL_PAGE);
  model.linuxFilesItem = crostiniFakeItem;

  // Expect 6 items.
  assertEquals(6, model.length);
  assertEquals(
      'fake-entry://recent', /** @type {!NavigationModelFakeItem} */
      (model.item(0)).entry.toURL());
  assertEquals(
      '/root/shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(1)).entry.fullPath);
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(2)?.label);
  assertEquals(
      fakeDriveVolumeId, /** @type {!NavigationModelVolumeItem} */
      (model.item(3)).volumeInfo.volumeId);
  assertEquals(
      'android:app1', /** @type {!NavigationModelAndroidAppItem} */
      (model.item(4)).label);
  assertEquals(
      'android:app2', /** @type {!NavigationModelAndroidAppItem} */
      (model.item(5)).label);

  // Downloads and Crostini are displayed within My files.
  const myFilesItem = /** @type NavigationModelFakeItem */ (model.item(2));
  const myFilesEntryList = /** @type {!EntryList} */ (myFilesItem.entry);
  assertEquals(1, myFilesEntryList.getUIChildren().length);
  assertEquals('linux-files-label', myFilesEntryList.getUIChildren()[0]?.name);

  // Trash is displayed as a root when feature is enabled and should be the last
  // item in the model.
  loadTimeData.overrideValues({FILES_TRASH_ENABLED: true});
  model.fakeTrashItem = new NavigationModelFakeItem(
      'trash-label', NavigationModelItemType.TRASH, new TrashRootEntry());
  model.refreshNavigationItems();
  assertEquals(7, model.length);
  assertEquals(
      'fake-entry://trash', /** @type {!NavigationModelFakeItem} */
      (model.item(6)).entry.toURL());
}

/**
 * Tests model with no Recents, Linux files, Play files.
 */
export function testNoRecentOrLinuxFiles() {
  const volumeManager = new MockVolumeManager();

  const shortcutListModel = new MockFolderShortcutDataModel(
      [MockFileEntry.create(drive, '/root/shortcut')]);
  const recentItem = null;

  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);

  assertEquals(3, model.length);
  assertEquals(
      '/root/shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).entry.fullPath);
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(1)?.label);
  const driveItem = /** @type {!NavigationModelVolumeItem} */ (model.item(2));
  assertEquals(fakeDriveVolumeId, driveItem.volumeInfo.volumeId);
  assertFalse(driveItem.disabled);
}

/**
 * Tests model with a disabled Drive volume.
 */
export function testDisabledVolumes() {
  const volumeManager = new MockVolumeManager();
  volumeManager.isDisabled = (volume) => {
    return (volume === VolumeType.DRIVE);
  };

  const shortcutListModel = new MockFolderShortcutDataModel(
      [MockFileEntry.create(drive, '/root/shortcut')]);
  const recentItem = null;

  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);

  assertEquals(3, model.length);
  assertEquals(
      '/root/shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).entry.fullPath);
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(1)?.label);

  const driveItem = /** @type {!NavigationModelVolumeItem} */ (model.item(2));
  assertEquals(fakeDriveVolumeId, driveItem.volumeInfo.volumeId);
  assertTrue(driveItem.disabled);
}

/**
 * Tests adding and removing shortcuts.
 */
export function testAddAndRemoveShortcuts() {
  const volumeManager = new MockVolumeManager();

  const shortcutListModel = new MockFolderShortcutDataModel(
      [MockFileEntry.create(drive, '/root/shortcut')]);
  const recentItem = null;

  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);

  assertEquals(3, model.length);

  // Add a shortcut at the tail, shortcuts are sorted by their label.
  const addShortcut = MockFileEntry.create(drive, '/root/shortcut2');
  shortcutListModel.splice(1, 0, addShortcut);

  assertEquals(4, model.length);
  assertEquals(
      'shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).label);
  assertEquals(
      'shortcut2', /** @type {!NavigationModelShortcutItem} */
      (model.item(1)).label);

  // Add a shortcut at the head.
  const headShortcut = MockFileEntry.create(drive, '/root/head');
  shortcutListModel.splice(0, 0, headShortcut);

  assertEquals(5, model.length);
  assertEquals(
      'head', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).label);
  assertEquals(
      'shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(1)).label);
  assertEquals(
      'shortcut2', /** @type {!NavigationModelShortcutItem} */
      (model.item(2)).label);

  // Remove the last shortcut.
  shortcutListModel.splice(2, 1);

  assertEquals(4, model.length);
  assertEquals(
      'head', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).label);
  assertEquals(
      'shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(1)).label);

  // Remove the first shortcut.
  shortcutListModel.splice(0, 1);

  assertEquals(3, model.length);
  assertEquals(
      'shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).label);
}

/**
 * Tests testAddAndRemoveVolumes test with SinglePartitionFormat flag is on.
 */
export function testAddAndRemoveVolumesWhenSinglePartitionOn() {
  loadTimeData.overrideValues({FILES_SINGLE_PARTITION_FORMAT_ENABLED: true});
  testAddAndRemoveVolumes();
}

/**
 * Tests adding and removing volumes.
 */
export function testAddAndRemoveVolumes() {
  const volumeManager = new MockVolumeManager();

  const shortcutListModel = new MockFolderShortcutDataModel(
      [MockFileEntry.create(drive, '/root/shortcut')]);
  const recentItem = null;

  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);

  assertEquals(3, model.length);

  // Mount removable volume 'hoge'.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:hoge', '', 'device/path/1'));

  assertEquals(4, model.length);
  assertEquals(
      '/root/shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).entry.fullPath);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(1).label);
  assertEquals(
      fakeDriveVolumeId, /** @type {!NavigationModelVolumeItem} */
      (model.item(2)).volumeInfo.volumeId);
  if (isSinglePartitionFormatEnabled()) {
    const drive = model.item(3);
    // @ts-ignore: error TS18048: 'drive' is possibly 'undefined'.
    assertEquals('External Drive', drive.label);
    assertEquals(
        'removable:hoge', /** @type {!NavigationModelFakeItem} */
        // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on
        // type 'FilesAppEntry'.
        (drive).entry.getUIChildren()[0].volumeInfo.volumeId);
  } else {
    assertEquals(
        'removable:hoge', /** @type {!NavigationModelVolumeItem} */
        (model.item(3)).volumeInfo.volumeId);
  }

  // Mount removable volume 'fuga'. Not a partition, so set a different device
  // path to 'hoge'.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:fuga', '', 'device/path/2'));

  assertEquals(5, model.length);
  assertEquals(
      '/root/shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).entry.fullPath);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(1).label);
  assertEquals(
      fakeDriveVolumeId, /** @type {!NavigationModelVolumeItem} */
      (model.item(2)).volumeInfo.volumeId);
  if (isSinglePartitionFormatEnabled()) {
    const drive1 = model.item(3);
    const drive2 = model.item(4);
    // @ts-ignore: error TS18048: 'drive1' is possibly 'undefined'.
    assertEquals('External Drive', drive1.label);
    // @ts-ignore: error TS18048: 'drive2' is possibly 'undefined'.
    assertEquals('External Drive', drive2.label);
    assertEquals(
        'removable:hoge', /** @type {!NavigationModelFakeItem} */
        // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on
        // type 'FilesAppEntry'.
        (drive1).entry.getUIChildren()[0].volumeInfo.volumeId);
    assertEquals(
        'removable:fuga', /** @type {!NavigationModelFakeItem} */
        // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on
        // type 'FilesAppEntry'.
        (drive2).entry.getUIChildren()[0].volumeInfo.volumeId);
  } else {
    assertEquals(
        'removable:hoge', /** @type {!NavigationModelVolumeItem} */
        (model.item(3)).volumeInfo.volumeId);
    assertEquals(
        'removable:fuga', /** @type {!NavigationModelVolumeItem} */
        (model.item(4)).volumeInfo.volumeId);
  }

  // Create a shortcut on the 'hoge' volume.
  shortcutListModel.splice(1, 0, MockFileEntry.create(hoge, '/shortcut2'));

  assertEquals(6, model.length);
  assertEquals(
      '/root/shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).entry.fullPath);
  assertEquals(
      '/shortcut2', /** @type {!NavigationModelShortcutItem} */
      (model.item(1)).entry.fullPath);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(2).label);
  assertEquals(
      fakeDriveVolumeId, /** @type {!NavigationModelVolumeItem} */
      (model.item(3)).volumeInfo.volumeId);
  if (isSinglePartitionFormatEnabled()) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    assertEquals('External Drive', model.item(4).label);
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    assertEquals('External Drive', model.item(5).label);
  } else {
    assertEquals(
        'removable:hoge', /** @type {!NavigationModelVolumeItem} */
        (model.item(4)).volumeInfo.volumeId);
    assertEquals(
        'removable:fuga', /** @type {!NavigationModelVolumeItem} */
        (model.item(5)).volumeInfo.volumeId);
  }
}

/**
 * Tests testOrderAndNestItems test with SinglePartitionFormat flag is on.
 */
export function testOrderAndNestItemsWhenSinglePartitionOn() {
  loadTimeData.overrideValues({FILES_SINGLE_PARTITION_FORMAT_ENABLED: true});
  testOrderAndNestItems();
}

/**
 * Tests that orderAndNestItems_ function does the following:
 * 1. produces the expected order of volumes.
 * 2. manages NavigationSection for the relevant volumes.
 * 3. keeps MTP/Archive/Removable volumes on the original order.
 */
export function testOrderAndNestItems() {
  const volumeManager = new MockVolumeManager();

  const shortcutListModel = new MockFolderShortcutDataModel([
    MockFileEntry.create(drive, '/root/shortcut'),
    MockFileEntry.create(drive, '/root/shortcut2'),
  ]);

  const recentItem = new NavigationModelFakeItem(
      'recent-label', NavigationModelItemType.RECENT, recentFakeEntry);

  // Create different volumes.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.PROVIDED, 'provided:prov1'));
  // Set the device paths of the removable volumes to different strings to
  // test the behaviour of two physically separate external devices.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:hoge', '', 'device/path/1'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:fuga', '', 'device/path/2'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.ARCHIVE, 'archive:a-rar'));
  volumeManager.volumeInfoList.add(
      MockVolumeManager.createMockVolumeInfo(VolumeType.MTP, 'mtp:a-phone'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.PROVIDED, 'provided:prov2'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.ANDROID_FILES, 'android_files:droid'));
  volumeManager.volumeInfoList.add(
      MockVolumeManager.createMockVolumeInfo(VolumeType.SMB, 'smb:file-share'));
  // Add ODFS.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.PROVIDED, 'provided:odfs', '', '',
      constants.ODFS_EXTENSION_ID));

  const androidAppListModelWithApps =
      createFakeAndroidAppListModel(['android:app1', 'android:app2']);

  // Navigation items built above:
  //  1.  fake-entry://recent
  //  2.  /root/shortcut
  //  3.  /root/shortcut2
  //  4.  My files
  //        -> Downloads
  //        -> Play files
  //        -> Linux files
  //  5.  Google Drive  - from setup()
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

  // Constructor already calls orderAndNestItems_.
  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModelWithApps, DialogType.FULL_PAGE);

  // Check items order and that MTP/Archive/Removable respect the original
  // order.
  assertEquals(15, model.length);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('recent-label', model.item(0).label);

  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('shortcut', model.item(1).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('shortcut2', model.item(2).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(3).label);

  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), model.item(4).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('provided:odfs', model.item(5).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('smb:file-share', model.item(6).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('provided:prov1', model.item(7).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('provided:prov2', model.item(8).label);

  if (isSinglePartitionFormatEnabled()) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    assertEquals('External Drive', model.item(9).label);
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    assertEquals('External Drive', model.item(10).label);
  } else {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    assertEquals('removable:hoge', model.item(9).label);
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    assertEquals('removable:fuga', model.item(10).label);
  }

  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('archive:a-rar', model.item(11).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('mtp:a-phone', model.item(12).label);

  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('android:app1', model.item(13).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('android:app2', model.item(14).label);

  // Check NavigationSection, which defaults to TOP.
  // recent-label.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.TOP, model.item(0).section);
  // shortcut.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.TOP, model.item(1).section);
  // shortcut2.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.TOP, model.item(2).section);

  // My Files.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.MY_FILES, model.item(3).section);

  // My Drive.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.GOOGLE_DRIVE, model.item(4).section);

  // ODFS.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.ODFS, model.item(5).section);

  // SMB and other FSP are grouped together.
  // smb:file-share.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.CLOUD, model.item(6).section);
  // provided:prov1.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.CLOUD, model.item(7).section);
  // provided:prov2.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.CLOUD, model.item(8).section);

  // MTP/Archive/Removable are grouped together.
  // removable:hoge.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.REMOVABLE, model.item(9).section);
  // removable:fuga.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.REMOVABLE, model.item(10).section);
  // archive:a-rar.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.REMOVABLE, model.item(11).section);
  // mtp:a-phone.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.REMOVABLE, model.item(12).section);

  // android:app1
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.ANDROID_APPS, model.item(13).section);
  // android:app2
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.ANDROID_APPS, model.item(14).section);

  const myFilesModel = model.item(3);
  // Re-order again: cast to allow calling this private model function.
  // @ts-ignore: error TS2339: Property 'orderAndNestItems_' does not exist on
  // type 'Object'.
  /** @type {!Object} */ (model).orderAndNestItems_();
  // Check if My Files is still in the same position.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(NavigationSection.MY_FILES, model.item(3).section);
  // Check if My Files model is still the same instance, because DirectoryTree
  // expects it to be the same instance to be able to find it on the tree.
  assertEquals(myFilesModel, model.item(3));
}

/**
 * Tests model with My files enabled.
 */
/** @param {()=>void} callback */
export function testMyFilesVolumeEnabled(callback) {
  const volumeManager = new MockVolumeManager();
  // Item 1 of the volume info list should have Downloads volume type.
  assertEquals(
      VolumeType.DOWNLOADS, volumeManager.volumeInfoList.item(1).volumeType);
  // Create a downloads folder inside the item.
  const downloadsVolume = volumeManager.volumeInfoList.item(1);
  /** @type {!MockFileSystem} */ (downloadsVolume.fileSystem).populate([
    '/Downloads/',
  ]);

  const shortcutListModel = new MockFolderShortcutDataModel([]);
  const recentItem = null;

  // Create Android 'Play files' volume.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.ANDROID_FILES, 'android_files:droid'));

  const crostiniFakeItem = new NavigationModelFakeItem(
      'linux-files-label', NavigationModelItemType.CROSTINI,
      new FakeEntryImpl('linux-files-label', RootType.CROSTINI));

  // Navigation items built above:
  //  1. My files
  //       -> Play files
  //       -> Linux files
  //  2. Drive  - added by default by MockVolumeManager.

  // Constructor already calls orderAndNestItems_.
  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);
  model.linuxFilesItem = crostiniFakeItem;

  assertEquals(2, model.length);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(0).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), model.item(1).label);

  // Android and Crostini are displayed within My files. And there is no
  // Downloads volume inside it. Downloads should be a normal folder inside
  // the My files volume.
  const myFilesItem = /** @type NavigationModelFakeItem */ (model.item(0));
  const myFilesEntryList = /** @type {!EntryList} */ (myFilesItem.entry);
  assertEquals(2, myFilesEntryList.getUIChildren().length);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('android_files:droid', myFilesEntryList.getUIChildren()[0].name);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('linux-files-label', myFilesEntryList.getUIChildren()[1].name);

  const reader = myFilesEntryList.createReader();
  // @ts-ignore: error TS7034: Variable 'foundEntries' implicitly has type
  // 'any[]' in some locations where its type cannot be determined.
  const foundEntries = [];
  reader.readEntries((entries) => {
    for (const entry of entries) {
      foundEntries.push(entry);
    }
  });

  reportPromise(
      waitUntil(() => {
        // Wait for Downloads folder to be read from My files volume.
        return foundEntries.length >= 1;
      }).then(() => {
        // @ts-ignore: error TS7005: Variable 'foundEntries' implicitly has an
        // 'any[]' type.
        assertEquals(foundEntries[0].name, 'Downloads');
        // @ts-ignore: error TS7005: Variable 'foundEntries' implicitly has an
        // 'any[]' type.
        assertTrue(foundEntries[0].isDirectory);
      }),
      callback);
}

/**
 * Tests that Android and Crostini, which are displayed within MyFiles, are set
 * to disabled/enabled correctly.
 */
export function testMyFilesSubdirectoriesCanBeDisabled() {
  const volumeManager = new MockVolumeManager();
  // Item 1 of the volume info list should have Downloads volume type.
  assertEquals(
      VolumeType.DOWNLOADS, volumeManager.volumeInfoList.item(1).volumeType);
  // Create a downloads folder inside the item.
  const downloadsVolume = volumeManager.volumeInfoList.item(1);
  /** @type {!MockFileSystem} */ (downloadsVolume.fileSystem).populate([
    '/Downloads/',
  ]);

  const shortcutListModel = new MockFolderShortcutDataModel([]);
  const recentItem = null;

  // Create Android 'Play files' volume and set as disabled.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.ANDROID_FILES, 'android_files:droid'));
  volumeManager.isDisabled = (volume) => {
    return (volume === VolumeType.ANDROID_FILES);
  };

  // Create Crostini 'Linux files' volume. It should be enabled by default.
  volumeManager.volumeInfoList.add(
      MockVolumeManager.createMockVolumeInfo(VolumeType.CROSTINI, 'crostini'));

  // Navigation items built above:
  //  1. My files
  //       -> Play files
  //       -> Linux files
  //  2. Drive  - added by default by MockVolumeManager.

  // Constructor already calls orderAndNestItems_.
  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);

  assertEquals(2, model.length);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(0).label);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), model.item(1).label);

  // Android is displayed within My files, and should be disabled.
  const myFilesItem = /** @type {!NavigationModelFakeItem} */ (model.item(0));
  const myFilesEntryList = /** @type {!EntryList} */ (myFilesItem.entry);
  assertEquals(2, myFilesEntryList.getUIChildren().length);
  const androidItem =
      /** @type {!VolumeEntry} */ (myFilesEntryList.getUIChildren()[0]);
  const crostiniItem =
      /** @type {!VolumeEntry} */ (myFilesEntryList.getUIChildren()[1]);
  assertEquals('android_files:droid', androidItem.name);
  assertTrue(androidItem.disabled);
  assertEquals('crostini', crostiniItem.name);
  assertFalse(crostiniItem.disabled);
}

/**
 * Tests that adding a new partition to the same grouped USB will add the
 * partition to the grouping.
 */
export function testMultipleUsbPartitionsGrouping() {
  const shortcutListModel = new MockFolderShortcutDataModel([]);
  const recentItem = null;
  const volumeManager = new MockVolumeManager();

  // Use same device path so the partitions are grouped.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition1', 'partition1',
      'device/path/1'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition2', 'partition2',
      'device/path/1'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition3', 'partition3',
      'device/path/1'));

  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);

  // Check that the common root shows 3 partitions.
  let groupedUsbs = /** @type NavigationModelFakeItem */ (model.item(2));
  assertEquals('External Drive', groupedUsbs.label);
  // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on type
  // 'FilesAppEntry'.
  assertEquals(3, groupedUsbs.entry.getUIChildren().length);

  // Add a 4th partition, which triggers NavigationListModel to recalculate.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition4', 'partition4',
      'device/path/1'));

  // Check that the common root shows 4 partitions.
  groupedUsbs = /** @type NavigationModelFakeItem */ (model.item(2));
  assertEquals('External Drive', groupedUsbs.label);
  // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on type
  // 'FilesAppEntry'.
  assertEquals(4, groupedUsbs.entry.getUIChildren().length);

  // Remove the 4th partition, which triggers NavigationListModel to
  // recalculate.
  volumeManager.volumeInfoList.remove('removable:partition4');

  // Check that the common root shows 3 partitions.
  groupedUsbs = /** @type NavigationModelFakeItem */ (model.item(2));
  assertEquals('External Drive', groupedUsbs.label);
  // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on type
  // 'FilesAppEntry'.
  assertEquals(3, groupedUsbs.entry.getUIChildren().length);

  // Add an extra copy of partition3, which replaces the existing partition3
  // and triggers NavigationListModel to recalculate.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition3', 'partition3',
      'device/path/1'));

  // Check that partition3 is not duplicated.
  groupedUsbs = /** @type NavigationModelFakeItem */ (model.item(2));
  assertEquals('External Drive', groupedUsbs.label);
  // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on type
  // 'FilesAppEntry'.
  assertEquals(3, groupedUsbs.entry.getUIChildren().length);
}
