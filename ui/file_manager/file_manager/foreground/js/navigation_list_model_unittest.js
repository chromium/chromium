// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {VolumeInfoImpl} from '../../background/js/volume_info_impl.js';
import {DialogType} from '../../common/js/dialog_type.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {MockCommandLinePrivate} from '../../common/js/mock_chrome.js';
import {MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise, waitUntil} from '../../common/js/test_error_reporting.js';
import {TrashRootEntry} from '../../common/js/trash.js';
import {str, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';

import {AndroidAppListModel} from './android_app_list_model.js';
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
  VolumeInfoImpl.prototype.resolveDisplayRoot = function(successCallback) {
    this.displayRoot_ = this.fileSystem_.root;
    successCallback(this.displayRoot_);
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
      new FakeEntryImpl(
          'linux-files-label', VolumeManagerCommon.RootType.CROSTINI));

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
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(2).label);
  assertEquals(
      'drive', /** @type {!NavigationModelVolumeItem} */
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
  assertEquals('linux-files-label', myFilesEntryList.getUIChildren()[0].name);

  // Trash is displayed as a root when feature is enabled.
  loadTimeData.overrideValues({FILES_TRASH_ENABLED: true});
  model.fakeTrashItem = new NavigationModelFakeItem(
      'trash-label', NavigationModelItemType.TRASH, new TrashRootEntry());
  model.reorderNavigationItems_();
  assertEquals(7, model.length);
  assertEquals(
      'fake-entry://trash', /** @type {!NavigationModelFakeItem} */
      (model.item(4)).entry.toURL());
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
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(1).label);
  const driveItem = /** @type {!NavigationModelVolumeItem} */ (model.item(2));
  assertEquals('drive', driveItem.volumeInfo.volumeId);
  assertFalse(driveItem.disabled);
}

/**
 * Tests model with a disabled Drive volume.
 */
export function testDisabledVolumes() {
  const volumeManager = new MockVolumeManager();
  volumeManager.isDisabled = (volume) => {
    return (volume === VolumeManagerCommon.VolumeType.DRIVE);
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
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(1).label);

  const driveItem = /** @type {!NavigationModelVolumeItem} */ (model.item(2));
  assertEquals('drive', driveItem.volumeInfo.volumeId);
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
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:hoge', '',
      'device/path/1'));

  assertEquals(4, model.length);
  assertEquals(
      '/root/shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).entry.fullPath);
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(1).label);
  assertEquals(
      'drive', /** @type {!NavigationModelVolumeItem} */
      (model.item(2)).volumeInfo.volumeId);
  if (util.isSinglePartitionFormatEnabled()) {
    const drive = model.item(3);
    assertEquals('External Drive', drive.label);
    assertEquals(
        'removable:hoge', /** @type {!NavigationModelFakeItem} */
        (drive).entry.getUIChildren()[0].volumeInfo.volumeId);
  } else {
    assertEquals(
        'removable:hoge', /** @type {!NavigationModelVolumeItem} */
        (model.item(3)).volumeInfo.volumeId);
  }

  // Mount removable volume 'fuga'. Not a partition, so set a different device
  // path to 'hoge'.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:fuga', '',
      'device/path/2'));

  assertEquals(5, model.length);
  assertEquals(
      '/root/shortcut', /** @type {!NavigationModelShortcutItem} */
      (model.item(0)).entry.fullPath);
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(1).label);
  assertEquals(
      'drive', /** @type {!NavigationModelVolumeItem} */
      (model.item(2)).volumeInfo.volumeId);
  if (util.isSinglePartitionFormatEnabled()) {
    const drive1 = model.item(3);
    const drive2 = model.item(4);
    assertEquals('External Drive', drive1.label);
    assertEquals('External Drive', drive2.label);
    assertEquals(
        'removable:hoge', /** @type {!NavigationModelFakeItem} */
        (drive1).entry.getUIChildren()[0].volumeInfo.volumeId);
    assertEquals(
        'removable:fuga', /** @type {!NavigationModelFakeItem} */
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
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(2).label);
  assertEquals(
      'drive', /** @type {!NavigationModelVolumeItem} */
      (model.item(3)).volumeInfo.volumeId);
  if (util.isSinglePartitionFormatEnabled()) {
    assertEquals('External Drive', model.item(4).label);
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
      VolumeManagerCommon.VolumeType.PROVIDED, 'provided:prov1'));
  // Set the device paths of the removable volumes to different strings to
  // test the behaviour of two physically separate external devices.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:hoge', '',
      'device/path/1'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:fuga', '',
      'device/path/2'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ARCHIVE, 'archive:a-rar'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MTP, 'mtp:a-phone'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'provided:prov2'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'android_files:droid'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.SMB, 'smb:file-share'));

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
  //  5.  Drive  - from setup()
  //  6.  smb:file-share
  //  7.  provided:prov1
  //  8.  provided:prov2
  //
  //  9.  removable:hoge
  // 10.  removable:fuga
  // 11.  archive:a-rar  - mounted as archive
  // 12.  mtp:a-phone
  //
  // 13.  android:app1
  // 14.  android:app2

  // Constructor already calls orderAndNestItems_.
  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModelWithApps, DialogType.FULL_PAGE);

  // Check items order and that MTP/Archive/Removable respect the original
  // order.
  assertEquals(14, model.length);
  assertEquals('recent-label', model.item(0).label);

  assertEquals('shortcut', model.item(1).label);
  assertEquals('shortcut2', model.item(2).label);
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(3).label);

  assertEquals(str('DRIVE_DIRECTORY_LABEL'), model.item(4).label);
  assertEquals('smb:file-share', model.item(5).label);
  assertEquals('provided:prov1', model.item(6).label);
  assertEquals('provided:prov2', model.item(7).label);

  if (util.isSinglePartitionFormatEnabled()) {
    assertEquals('External Drive', model.item(8).label);
    assertEquals('External Drive', model.item(9).label);
  } else {
    assertEquals('removable:hoge', model.item(8).label);
    assertEquals('removable:fuga', model.item(9).label);
  }

  assertEquals('archive:a-rar', model.item(10).label);
  assertEquals('mtp:a-phone', model.item(11).label);

  assertEquals('android:app1', model.item(12).label);
  assertEquals('android:app2', model.item(13).label);

  // Check NavigationSection, which defaults to TOP.
  // recent-label.
  assertEquals(NavigationSection.TOP, model.item(0).section);
  // shortcut.
  assertEquals(NavigationSection.TOP, model.item(1).section);
  // shortcut2.
  assertEquals(NavigationSection.TOP, model.item(2).section);

  // My Files.
  assertEquals(NavigationSection.MY_FILES, model.item(3).section);

  // Drive, FSP, and SMB are grouped together.
  // My Drive.
  assertEquals(NavigationSection.CLOUD, model.item(4).section);
  // smb:file-share.
  assertEquals(NavigationSection.CLOUD, model.item(5).section);
  // provided:prov1.
  assertEquals(NavigationSection.CLOUD, model.item(6).section);
  // provided:prov2.
  assertEquals(NavigationSection.CLOUD, model.item(7).section);

  // MTP/Archive/Removable are grouped together.
  // removable:hoge.
  assertEquals(NavigationSection.REMOVABLE, model.item(8).section);
  // removable:fuga.
  assertEquals(NavigationSection.REMOVABLE, model.item(9).section);
  // archive:a-rar.
  assertEquals(NavigationSection.REMOVABLE, model.item(10).section);
  // mtp:a-phone.
  assertEquals(NavigationSection.REMOVABLE, model.item(11).section);

  // android:app1
  assertEquals(NavigationSection.ANDROID_APPS, model.item(12).section);
  // android:app2
  assertEquals(NavigationSection.ANDROID_APPS, model.item(13).section);

  const myFilesModel = model.item(3);
  // Re-order again: cast to allow calling this private model function.
  /** @type {!Object} */ (model).orderAndNestItems_();
  // Check if My Files is still in the same position.
  assertEquals(NavigationSection.MY_FILES, model.item(3).section);
  // Check if My Files model is still the same instance, because DirectoryTree
  // expects it to be the same instance to be able to find it on the tree.
  assertEquals(myFilesModel, model.item(3));
}

/**
 * Tests model with My files enabled.
 */
export function testMyFilesVolumeEnabled(callback) {
  const volumeManager = new MockVolumeManager();
  // Item 1 of the volume info list should have Downloads volume type.
  assertEquals(
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      volumeManager.volumeInfoList.item(1).volumeType);
  // Create a downloads folder inside the item.
  const downloadsVolume = volumeManager.volumeInfoList.item(1);
  /** @type {!MockFileSystem} */ (downloadsVolume.fileSystem).populate([
    '/Downloads/',
  ]);

  const shortcutListModel = new MockFolderShortcutDataModel([]);
  const recentItem = null;

  // Create Android 'Play files' volume.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'android_files:droid'));

  const crostiniFakeItem = new NavigationModelFakeItem(
      'linux-files-label', NavigationModelItemType.CROSTINI,
      new FakeEntryImpl(
          'linux-files-label', VolumeManagerCommon.RootType.CROSTINI));

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
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(0).label);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), model.item(1).label);

  // Android and Crostini are displayed within My files. And there is no
  // Downloads volume inside it. Downloads should be a normal folder inside
  // the My files volume.
  const myFilesItem = /** @type NavigationModelFakeItem */ (model.item(0));
  const myFilesEntryList = /** @type {!EntryList} */ (myFilesItem.entry);
  assertEquals(2, myFilesEntryList.getUIChildren().length);
  assertEquals('android_files:droid', myFilesEntryList.getUIChildren()[0].name);
  assertEquals('linux-files-label', myFilesEntryList.getUIChildren()[1].name);

  const reader = myFilesEntryList.createReader();
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
        assertEquals(foundEntries[0].name, 'Downloads');
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
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      volumeManager.volumeInfoList.item(1).volumeType);
  // Create a downloads folder inside the item.
  const downloadsVolume = volumeManager.volumeInfoList.item(1);
  /** @type {!MockFileSystem} */ (downloadsVolume.fileSystem).populate([
    '/Downloads/',
  ]);

  const shortcutListModel = new MockFolderShortcutDataModel([]);
  const recentItem = null;

  // Create Android 'Play files' volume and set as disabled.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'android_files:droid'));
  volumeManager.isDisabled = (volume) => {
    return (volume === VolumeManagerCommon.VolumeType.ANDROID_FILES);
  };

  // Create Crostini 'Linux files' volume. It should be enabled by default.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.CROSTINI, 'crostini'));

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
  assertEquals(str('MY_FILES_ROOT_LABEL'), model.item(0).label);
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
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition1',
      'partition1', 'device/path/1'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition2',
      'partition2', 'device/path/1'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition3',
      'partition3', 'device/path/1'));

  const model = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);

  // Check that the common root shows 3 partitions.
  let groupedUsbs = /** @type NavigationModelFakeItem */ (model.item(2));
  assertEquals('External Drive', groupedUsbs.label);
  assertEquals(3, groupedUsbs.entry.getUIChildren().length);

  // Add a 4th partition, which triggers NavigationListModel to recalculate.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition4',
      'partition4', 'device/path/1'));

  // Check that the common root shows 4 partitions.
  groupedUsbs = /** @type NavigationModelFakeItem */ (model.item(2));
  assertEquals('External Drive', groupedUsbs.label);
  assertEquals(4, groupedUsbs.entry.getUIChildren().length);

  // Remove the 4th partition, which triggers NavigationListModel to
  // recalculate.
  volumeManager.volumeInfoList.remove('removable:partition4');

  // Check that the common root shows 3 partitions.
  groupedUsbs = /** @type NavigationModelFakeItem */ (model.item(2));
  assertEquals('External Drive', groupedUsbs.label);
  assertEquals(3, groupedUsbs.entry.getUIChildren().length);

  // Add an extra copy of partition3, which replaces the existing partition3
  // and triggers NavigationListModel to recalculate.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition3',
      'partition3', 'device/path/1'));

  // Check that partition3 is not duplicated.
  groupedUsbs = /** @type NavigationModelFakeItem */ (model.item(2));
  assertEquals('External Drive', groupedUsbs.label);
  assertEquals(3, groupedUsbs.entry.getUIChildren().length);
}
