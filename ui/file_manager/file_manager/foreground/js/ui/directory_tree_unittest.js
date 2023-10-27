// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../../background/js/mock_volume_manager.js';
import {DialogType} from '../../../common/js/dialog_type.js';
import {EntryList} from '../../../common/js/files_app_entry_types.js';
import {installMockChrome, MockCommandLinePrivate} from '../../../common/js/mock_chrome.js';
import {MockDirectoryEntry} from '../../../common/js/mock_entry.js';
import {reportPromise, waitUntil} from '../../../common/js/test_error_reporting.js';
import {str} from '../../../common/js/translations.js';
import {VolumeManagerCommon} from '../../../common/js/volume_manager_types.js';
import {FileOperationManager} from '../../../externs/background/file_operation_manager.js';
import {DirectoryModel} from '../directory_model.js';
import {createFakeAndroidAppListModel} from '../fake_android_app_list_model.js';
import {MetadataModel} from '../metadata/metadata_model.js';
import {createFakeDirectoryModel} from '../mock_directory_model.js';
import {MockFolderShortcutDataModel} from '../mock_folder_shortcut_data_model.js';
import {MockNavigationListModel} from '../mock_navigation_list_model.js';
import {NavigationListModel, NavigationModelFakeItem, NavigationModelItemType, NavigationSection} from '../navigation_list_model.js';

import {DirectoryTree, EntryListItem, SubDirectoryItem} from './directory_tree.js';

/** @type {!MockVolumeManager} */
let volumeManager;

/** @type {!DirectoryModel} */
let directoryModel;

/** @type {!MetadataModel} */
let metadataModel;

/** @type {!FileOperationManager} */
let fileOperationManager;

// @ts-ignore: error TS2314: Generic type 'Array<T>' requires 1 type
// argument(s).
/** @type {!Array} */
let directoryChangedListeners;

/** @type {!Object<string,!MockDirectoryEntry>} */
let fakeFileSystemURLEntries;

/** @type {!FileSystem} */
let driveFileSystem;


/**
 * Mock Chrome APIs
 * @type {!Object}
 */
let mockChrome;

// Set up test components.
export function setUp() {
  loadTimeData.overrideValues({
    UNIFIED_MEDIA_VIEW_ENABLED: false,
    GUEST_OS: false,
  });

  // Setup mock chrome APIs.
  directoryChangedListeners = [];
  mockChrome = {
    fileManagerPrivate: {
      onDirectoryChanged: {
        // @ts-ignore: error TS7006: Parameter 'listener' implicitly has an
        // 'any' type.
        addListener: (listener) => {
          directoryChangedListeners.push(listener);
        },
      },
    },
  };
  installMockChrome(mockChrome);
  new MockCommandLinePrivate();

  // Setup mock components.
  volumeManager = new MockVolumeManager();
  directoryModel = createFakeDirectoryModel();
  metadataModel = /** @type {!MetadataModel} */ ({});
  // @ts-ignore: error TS2352: Conversion of type '{ addEventListener: (name:
  // any, callback: any) => void; }' to type 'FileOperationManager' may be a
  // mistake because neither type sufficiently overlaps with the other. If this
  // was intentional, convert the expression to 'unknown' first.
  fileOperationManager = /** @type {!FileOperationManager} */ ({
    // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
    // type.
    addEventListener: (name, callback) => {},
  });

  // Setup fake file system URL resolver.
  fakeFileSystemURLEntries = {};
  window.webkitResolveLocalFileSystemURL = (url, callback) => {
    // @ts-ignore: error TS2345: Argument of type 'MockDirectoryEntry |
    // undefined' is not assignable to parameter of type 'FileSystemEntry'.
    callback(fakeFileSystemURLEntries[url]);
  };

  driveFileSystem = assert(volumeManager.volumeInfoList.item(0).fileSystem);
  // @ts-ignore: error TS2740: Type 'FileSystemDirectoryEntry' is missing the
  // following properties from type 'MockDirectoryEntry': clone, getAllChildren,
  // getEntry_, metadata, and 2 more.
  fakeFileSystemURLEntries['filesystem:drive/root'] =
      MockDirectoryEntry.create(driveFileSystem, '/root');
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/Computers'] =
      MockDirectoryEntry.create(driveFileSystem, '/Computers');
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/team_drives'] =
      MockDirectoryEntry.create(driveFileSystem, '/team_drives');
}

/**
 * Creates the DOM element structure of the directory tree and returns the
 * directory tree DOM element from within that structure.
 * @return {!HTMLElement}
 */
function createElements() {
  const parent = document.createElement('div');
  const tree = document.createElement('div');
  parent.appendChild(tree);
  return /** @type {!HTMLElement} */ (tree);
}

/**
 * Returns a mock MetadataModel.
 * @return {!MetadataModel}
 */
function createMockMetadataModel() {
  // @ts-ignore: error TS2352: Conversion of type '{ notifyEntriesChanged: () =>
  // void; get: (entries: FileSystemEntry[], labels: string[]) => Promise<{
  // shared: false; }[]>; getCache: (entries: FileSystemEntry[], labels:
  // string[]) => { ...; }[]; }' to type 'MetadataModel' may be a mistake
  // because neither type sufficiently overlaps with the other. If this was
  // intentional, convert the expression to 'unknown' first.
  return /** @type {!MetadataModel} */ ({
    notifyEntriesChanged: () => {},
    // get and getCache mock a non-shared directory.
    // @ts-ignore: error TS6133: 'labels' is declared but its value is never
    // read.
    get: (entries, labels) => {
      return Promise.resolve([{shared: false}]);
    },
    // @ts-ignore: error TS6133: 'labels' is declared but its value is never
    // read.
    getCache: (entries, labels) => {
      return [{shared: false}];
    },
  });
}

/**
 * Returns the directory tree item labels.
 *
 * @param {!DirectoryTree} directoryTree The directory tree object.
 * @return {!Array<string>} Array of label strings.
 */
function getDirectoryTreeItemLabels(directoryTree) {
  const labels = [];
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'DirectoryTree'.
  for (const item of directoryTree.items) {
    labels.push(item.label);
  }
  return labels;
}

/**
 * Test case for typical creation of directory tree.
 * This test expects that the following tree is built.
 *
 * Google Drive
 * - My Drive
 * - Shared with me
 * - Offline
 * Downloads
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testCreateDirectoryTree(callback) {
  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(2, directoryTree.items.length);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        // Under the drive item, there exist 3 entries.
        return driveItem.items.length == 3;
      }).then(() => {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(
            str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
            driveItem.items[1].label);
        assertEquals(
            str('DRIVE_OFFLINE_COLLECTION_LABEL'), driveItem.items[2].label);
      }),
      callback);
}

/**
 * Test case for creating tree with Team Drives.
 * This test expects that the following tree is built.
 *
 * Google Drive
 * - My Drive
 * - Team Drives (only if there is a child team drive).
 * - Shared with me
 * - Offline
 * Downloads
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testCreateDirectoryTreeWithTeamDrive(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/team_drives/a'] =
      MockDirectoryEntry.create(driveFileSystem, '/team_drives/a');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(2, directoryTree.items.length);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        // Under the drive item, there exist 4 entries.
        return driveItem.items.length == 4;
      }).then(() => {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(
            str('DRIVE_SHARED_DRIVES_LABEL'), driveItem.items[1].label);
        assertEquals(
            str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
            driveItem.items[2].label);
        assertEquals(
            str('DRIVE_OFFLINE_COLLECTION_LABEL'), driveItem.items[3].label);
      }),
      callback);
}

/**
 * Test case for creating tree with empty Team Drives.
 * The Team Drives subtree should be removed if the user has no team drives.
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testCreateDirectoryTreeWithEmptyTeamDrive(callback) {
  // No directories exist under Team Drives

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        // Root entries under Drive volume is generated, Team Drives isn't
        // included because it has no child.
        // See testCreateDirectoryTreeWithTeamDrive for detail.
        return driveItem.items.length == 3;
      }).then(() => {
        let teamDrivesItemFound = false;
        for (let i = 0; i < driveItem.items.length; i++) {
          if (driveItem.items[i].label == str('DRIVE_SHARED_DRIVES_LABEL')) {
            teamDrivesItemFound = true;
            break;
          }
        }
        assertFalse(teamDrivesItemFound, 'Team Drives should NOT be generated');
      }),
      callback);
}

/**
 * Test case for creating tree with Computers.
 * This test expects that the following tree is built.
 *
 * Google Drive
 * - My Drive
 * - Computers (only if there is a child computer).
 * - Shared with me
 * - Offline
 * Downloads
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testCreateDirectoryTreeWithComputers(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/Comuters/My Laptop'] =
      MockDirectoryEntry.create(driveFileSystem, '/Computers/My Laptop');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(2, directoryTree.items.length);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        // Under the drive item, there exist 4 entries.
        return driveItem.items.length == 4;
      }).then(() => {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(str('DRIVE_COMPUTERS_LABEL'), driveItem.items[1].label);
        assertEquals(
            str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
            driveItem.items[2].label);
        assertEquals(
            str('DRIVE_OFFLINE_COLLECTION_LABEL'), driveItem.items[3].label);
      }),
      callback);
}

/**
 * Test case for creating tree with empty Computers.
 * The Computers subtree should be removed if the user has no computers.
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testCreateDirectoryTreeWithEmptyComputers(callback) {
  // No directories exist under Team Drives

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  // Ensure we do not have a "Computers" item in drive, as it does not contain
  // any children.
  reportPromise(
      waitUntil(() => {
        // Root entries under Drive volume is generated, Computers isn't
        // included because it has no child.
        // See testCreateDirectoryTreeWithComputers for detail.
        return driveItem.items.length == 3;
      }).then(() => {
        let teamDrivesItemFound = false;
        for (let i = 0; i < driveItem.items.length; i++) {
          if (driveItem.items[i].label == str('DRIVE_COMPUTERS_LABEL')) {
            teamDrivesItemFound = true;
            break;
          }
        }
        assertFalse(teamDrivesItemFound, 'Computers should NOT be generated');
      }),
      callback);
}

/**
 * Test case for creating tree with Team Drives & Computers.
 * This test expects that the following tree is built.
 *
 * Google Drive
 * - My Drive
 * - Team Drives
 * - Computers
 * - Shared with me
 * - Offline
 * Downloads
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testCreateDirectoryTreeWithTeamDrivesAndComputers(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/team_drives/a'] =
      MockDirectoryEntry.create(driveFileSystem, '/team_drives/a');
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/Comuters/My Laptop'] =
      MockDirectoryEntry.create(driveFileSystem, '/Computers/My Laptop');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(2, directoryTree.items.length);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        // Under the drive item, there exist 4 entries.
        return driveItem.items.length == 5;
      }).then(() => {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(
            str('DRIVE_SHARED_DRIVES_LABEL'), driveItem.items[1].label);
        assertEquals(str('DRIVE_COMPUTERS_LABEL'), driveItem.items[2].label);
        assertEquals(
            str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
            driveItem.items[3].label);
        assertEquals(
            str('DRIVE_OFFLINE_COLLECTION_LABEL'), driveItem.items[4].label);
      }),
      callback);
}

/**
 * Test case for updateSubElementsFromList setting section-start attribute.
 *
 * 'section-start' attribute is used to display a line divider between
 * "sections" in the directory tree. This is calculated in NavigationListModel.
 */
export function testUpdateSubElementsFromListSections() {
  const recentItem = null;
  const shortcutListModel = new MockFolderShortcutDataModel([]);
  const androidAppListModel = createFakeAndroidAppListModel(['android:app1']);
  const treeModel = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);
  const myFilesItem = treeModel.item(0);
  const driveItem = treeModel.item(1);
  const androidAppItem = treeModel.item(2);

  // @ts-ignore: error TS18048: 'myFilesItem' is possibly 'undefined'.
  assertEquals(NavigationSection.MY_FILES, myFilesItem.section);
  // @ts-ignore: error TS18048: 'driveItem' is possibly 'undefined'.
  assertEquals(NavigationSection.GOOGLE_DRIVE, driveItem.section);
  // @ts-ignore: error TS18048: 'androidAppItem' is possibly 'undefined'.
  assertEquals(NavigationSection.ANDROID_APPS, androidAppItem.section);

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  const mockMetadata = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, mockMetadata,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = treeModel;

  // Coerce to DirectoryTree type and update the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(false);

  // First element should not have section-start attribute, to not display a
  // division line in the first section.
  // My files:
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(null, directoryTree.items[0].getAttribute('section-start'));

  // Drive should have section-start, because it's a new section but not the
  // first section.
  assertEquals(
      NavigationSection.GOOGLE_DRIVE,
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'HTMLElement'.
      directoryTree.items[1].getAttribute('section-start'));

  // Regenerate so it re-calculates the 'section-start' without creating the
  // DirectoryItem.
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(false);
  assertEquals(
      NavigationSection.GOOGLE_DRIVE,
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'HTMLElement'.
      directoryTree.items[1].getAttribute('section-start'));
}

/**
 * Test case for updateSubElementsFromList.
 *
 * Mounts/unmounts removable and archive volumes, and checks these volumes come
 * up to/disappear from the list correctly.
 */
export function testUpdateSubElementsFromList() {
  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and update the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(true);

  // There are 2 volumes, Drive and Downloads, at first.
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
      ],
      // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
      // assignable to parameter of type 'DirectoryTree'.
      getDirectoryTreeItemLabels(directoryTree));

  // Mounts a removable volume.
  const removableVolume = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable',
      str('REMOVABLE_DIRECTORY_LABEL'));
  volumeManager.volumeInfoList.add(removableVolume);

  // Asserts that the directoryTree is not updated before the update.
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
      ],
      // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
      // assignable to parameter of type 'DirectoryTree'.
      getDirectoryTreeItemLabels(directoryTree));

  // Asserts that a removable directory is added after the update.
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(false);
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
        str('REMOVABLE_DIRECTORY_LABEL'),
      ],
      // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
      // assignable to parameter of type 'DirectoryTree'.
      getDirectoryTreeItemLabels(directoryTree));

  // Mounts an archive volume.
  const archiveVolume = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ARCHIVE, 'archive',
      str('ARCHIVE_DIRECTORY_LABEL'));
  volumeManager.volumeInfoList.add(archiveVolume);

  // Asserts that the directoryTree is not updated before the update.
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
        str('REMOVABLE_DIRECTORY_LABEL'),
      ],
      // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
      // assignable to parameter of type 'DirectoryTree'.
      getDirectoryTreeItemLabels(directoryTree));

  // Asserts that an archive directory is added before the removable directory.
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(false);
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
        str('REMOVABLE_DIRECTORY_LABEL'),
        str('ARCHIVE_DIRECTORY_LABEL'),
      ],
      // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
      // assignable to parameter of type 'DirectoryTree'.
      getDirectoryTreeItemLabels(directoryTree));

  // Deletes an archive directory.
  volumeManager.volumeInfoList.remove('archive');

  // Asserts that the directoryTree is not updated before the update.
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
        str('REMOVABLE_DIRECTORY_LABEL'),
        str('ARCHIVE_DIRECTORY_LABEL'),
      ],
      // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
      // assignable to parameter of type 'DirectoryTree'.
      getDirectoryTreeItemLabels(directoryTree));

  // Asserts that an archive directory is deleted.
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(false);
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
        str('REMOVABLE_DIRECTORY_LABEL'),
      ],
      // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
      // assignable to parameter of type 'DirectoryTree'.
      getDirectoryTreeItemLabels(directoryTree));
}

/**
 * Test case for updateSubElementsFromList.
 *
 * If some of the volumes under MyFiles are disabled, this should be reflected
 * in the directory model as well: the children shouldn't be loaded, and
 * clicking on the item or the expand icon shouldn't do anything.
 */
// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export async function testUpdateSubElementsAndroidDisabled(done) {
  const recentItem = null;
  const shortcutListModel = new MockFolderShortcutDataModel([]);
  const androidAppListModel = createFakeAndroidAppListModel([]);

  // Create Android 'Play files' volume and set as disabled.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'android_files:droid'));
  volumeManager.isDisabled = (volume) => {
    return (volume === VolumeManagerCommon.VolumeType.ANDROID_FILES);
  };

  const treeModel = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  const mockMetadata = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, mockMetadata,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = treeModel;

  // Coerce to DirectoryTree type and update the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(true);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const myFilesItem = directoryTree.items[0];
  await waitUntil(() => myFilesItem.items.length === 1);
  const androidItem = /** @type {!SubDirectoryItem} */ (myFilesItem.items[0]);
  assertEquals('android_files:droid', androidItem.label);
  assertTrue(androidItem.disabled);
  assertFalse(androidItem.hasChildren);

  // Clicking on the item shouldn't select it.
  androidItem.click();
  await new Promise(resolve => requestAnimationFrame(resolve));
  // @ts-ignore: error TS2345: Argument of type 'boolean | undefined' is not
  // assignable to parameter of type 'boolean'.
  assertFalse(androidItem.selected);

  // The item shouldn't be expanded.
  let isExpanded = androidItem.getAttribute('aria-expanded') || 'false';
  assertEquals('false', isExpanded);
  // Clicking on the expand icon shouldn't expand.
  // @ts-ignore: error TS2339: Property 'click' does not exist on type
  // 'Element'.
  androidItem.querySelector('.expand-icon').click();
  await new Promise(resolve => requestAnimationFrame(resolve));
  isExpanded = androidItem.getAttribute('aria-expanded') || 'false';
  assertEquals('false', isExpanded);

  done();
}

/**
 * Test case for updateSubElementsFromList.
 *
 * If removable volumes are disabled, they should be allowed to mount/unmount,
 * but cannot be selected or expanded.
 */
// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export async function testUpdateSubElementsRemovableDisabled(done) {
  const recentItem = null;
  const shortcutListModel = new MockFolderShortcutDataModel([]);
  const androidAppListModel = createFakeAndroidAppListModel([]);

  // Set removable volumes as disabled.
  volumeManager.isDisabled = (volume) => {
    return (volume === VolumeManagerCommon.VolumeType.REMOVABLE);
  };

  const treeModel = new NavigationListModel(
      volumeManager, shortcutListModel.asFolderShortcutsDataModel(), recentItem,
      directoryModel, androidAppListModel, DialogType.FULL_PAGE);

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  const mockMetadata = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, mockMetadata,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = treeModel;

  // Coerce to DirectoryTree type and update the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(true);

  // There are 2 volumes at first.
  // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not assignable
  // to parameter of type 'DirectoryTree'.
  assertEquals(2, getDirectoryTreeItemLabels(directoryTree).length);

  // Mount a removable volume.
  const removableVolume = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable',
      str('REMOVABLE_DIRECTORY_LABEL'));
  volumeManager.volumeInfoList.add(removableVolume);

  // Asserts that a removable directory is added after the update.
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(false);
  // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not assignable
  // to parameter of type 'DirectoryTree'.
  assertEquals(3, getDirectoryTreeItemLabels(directoryTree).length);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const removableItem = directoryTree.items[2];
  assertEquals(str('REMOVABLE_DIRECTORY_LABEL'), removableItem.label);
  assertTrue(removableItem.disabled);
  assertFalse(removableItem.hasChildren);
  // Clicking on the item shouldn't select it.
  removableItem.click();
  await new Promise(resolve => requestAnimationFrame(resolve));
  assertFalse(removableItem.selected);
  // The item shouldn't be expanded.
  let isExpanded = removableItem.getAttribute('aria-expanded') || 'false';
  assertEquals('false', isExpanded);
  // Clicking on the expand icon shouldn't expand.
  removableItem.querySelector('.expand-icon').click();
  await new Promise(resolve => requestAnimationFrame(resolve));
  isExpanded = removableItem.getAttribute('aria-expanded') || 'false';
  assertEquals('false', isExpanded);

  // Unmount the removable and assert that it's removed from the directory tree.
  volumeManager.volumeInfoList.remove('removable');
  // @ts-ignore: error TS2339: Property 'updateSubElementsFromList' does not
  // exist on type 'HTMLElement'.
  directoryTree.updateSubElementsFromList(false);
  // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not assignable
  // to parameter of type 'DirectoryTree'.
  assertEquals(2, getDirectoryTreeItemLabels(directoryTree).length);

  done();
}

/**
 * Test adding the first team drive for a user.
 * Team Drives subtree should be shown after the change notification is
 * delivered.
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testAddFirstTeamDrive(callback) {
  // No directories exist under Team Drives

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 3;
      })
          .then(() => {
            // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not
            // assignable to type 'MockDirectoryEntry'.
            fakeFileSystemURLEntries['filesystem:drive/team_drives/a'] =
                MockDirectoryEntry.create(driveFileSystem, '/team_drives/a');
            const event = {
              entry: fakeFileSystemURLEntries['filesystem:drive/team_drives'],
              eventType: 'changed',
            };
            for (const listener of directoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            return waitUntil(() => {
              for (let i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label ==
                    str('DRIVE_SHARED_DRIVES_LABEL')) {
                  return !driveItem.items[i].hidden;
                }
              }
              return false;
            });
          }),
      callback);
}

/**
 * Test removing the last team drive for a user.
 * Team Drives subtree should be removed after the change notification is
 * delivered.
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testRemoveLastTeamDrive(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/team_drives/a'] =
      MockDirectoryEntry.create(driveFileSystem, '/team_drives/a');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 4;
      })
          .then(() => {
            return new Promise(resolve => {
              // @ts-ignore: error TS2532: Object is possibly 'undefined'.
              fakeFileSystemURLEntries['filesystem:drive/team_drives/a'].remove(
                  // @ts-ignore: error TS2345: Argument of type '(value: any) =>
                  // void' is not assignable to parameter of type '() => any'.
                  resolve);
            });
          })
          .then(() => {
            const event = {
              entry: fakeFileSystemURLEntries['filesystem:drive/team_drives'],
              eventType: 'changed',
            };
            for (const listener of directoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            // Wait team drive grand root to appear.
            return waitUntil(() => {
              for (let i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label ==
                    str('DRIVE_SHARED_DRIVES_LABEL')) {
                  return false;
                }
              }
              return true;
            });
          }),
      callback);
}

/**
 * Test adding the first computer for a user.
 * Computers subtree should be shown after the change notification is
 * delivered.
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testAddFirstComputer(callback) {
  // No directories exist under Computers

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  // Test that we initially do not have a Computers item under Drive, and that
  // adding a filesystem "/Computers/a" results in the Computers item being
  // displayed under Drive.

  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 3;
      })
          .then(() => {
            // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not
            // assignable to type 'MockDirectoryEntry'.
            fakeFileSystemURLEntries['filesystem:drive/Computers/a'] =
                MockDirectoryEntry.create(driveFileSystem, '/Computers/a');
            const event = {
              entry: fakeFileSystemURLEntries['filesystem:drive/Computers'],
              eventType: 'changed',
            };
            for (const listener of directoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            return waitUntil(() => {
              for (let i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label == str('DRIVE_COMPUTERS_LABEL')) {
                  return !driveItem.items[i].hidden;
                }
              }
              return false;
            });
          }),
      callback);
}

/**
 * Test removing the last computer for a user.
 * Computers subtree should be removed after the change notification is
 * delivered.
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testRemoveLastComputer(callback) {
  // Setup entries returned by fakeFileSystemURLResults.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/Computers/a'] =
      MockDirectoryEntry.create(driveFileSystem, '/Computers/a');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];

  // Check that removing the local computer "a" results in the entire
  // "Computers" element being removed, as it has no children.
  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 4;
      })
          .then(() => {
            return new Promise(resolve => {
              // @ts-ignore: error TS2532: Object is possibly 'undefined'.
              fakeFileSystemURLEntries['filesystem:drive/Computers/a'].remove(
                  // @ts-ignore: error TS2345: Argument of type '(value: any) =>
                  // void' is not assignable to parameter of type '() => any'.
                  resolve);
            });
          })
          .then(() => {
            const event = {
              entry: fakeFileSystemURLEntries['filesystem:drive/Computers'],
              eventType: 'changed',
            };
            for (const listener of directoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            // Wait team drive grand root to appear.
            return waitUntil(() => {
              for (let i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label == str('DRIVE_COMPUTERS_LABEL')) {
                  return false;
                }
              }
              return true;
            });
          }),
      callback);
}

/**
 * Test DirectoryItem.insideMyDrive property, which should return true when
 * inside My Drive and any of its sub-directories; Should return false for
 * everything else, including within Team Drive.
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testInsideMyDriveAndInsideDrive(callback) {
  // Setup My Drive and Downloads and one folder inside each of them.
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/root/folder1'] =
      MockDirectoryEntry.create(driveFileSystem, '/root/folder1');
  const downloadsFileSystem = volumeManager.volumeInfoList.item(1).fileSystem;
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:downloads/folder1'] =
      MockDirectoryEntry.create(downloadsFileSystem, '/folder1');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  const mockMetadata = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, mockMetadata,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const downloadsItem = directoryTree.items[1];

  reportPromise(
      waitUntil(() => {
        // Under the drive item, there exist 3 entries. In Downloads should
        // exist 1 entry folder1.
        return driveItem.items.length === 3 && downloadsItem.items.length === 1;
      }).then(() => {
        // insideMyDrive
        assertTrue(driveItem.insideMyDrive, 'Drive root');
        assertTrue(driveItem.items[0].insideMyDrive, 'My Drive root');
        assertFalse(driveItem.items[1].insideMyDrive, 'Team Drives root');
        assertFalse(driveItem.items[2].insideMyDrive, 'Offline root');
        assertFalse(downloadsItem.insideMyDrive, 'Downloads root');
        assertFalse(downloadsItem.items[0].insideMyDrive, 'Downloads/folder1');
        // insideDrive
        assertTrue(driveItem.insideDrive, 'Drive root');
        assertTrue(driveItem.items[0].insideDrive, 'My Drive root');
        assertTrue(driveItem.items[1].insideDrive, 'Team Drives root');
        assertTrue(driveItem.items[2].insideDrive, 'Offline root');
        assertFalse(downloadsItem.insideDrive, 'Downloads root');
        assertFalse(downloadsItem.items[0].insideDrive, 'Downloads/folder1');
      }),
      callback);
}

/**
 * Test adding FSPs.
 * Sub directories should be fetched for FSPs, but not for the Smb FSP.
 *
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testAddProviders(callback) {
  // Add a volume representing a non-Smb provider to the mock filesystem.
  volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'not_smb', 'NOT_SMB_LABEL');

  // Add a sub directory to the non-Smb provider.
  const provider = assert(volumeManager.volumeInfoList.item(2).fileSystem);
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:not_smb/child'] =
      MockDirectoryEntry.create(provider, '/child');

  // Add a volume representing an Smb provider to the mock filesystem.
  volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'smb', 'SMB_LABEL', '@smb');

  // Add a sub directory to the Smb provider.
  const smbProvider = assert(volumeManager.volumeInfoList.item(3).fileSystem);
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:smb/child'] =
      MockDirectoryEntry.create(smbProvider, '/smb_child');

  // Add a volume representing an smbfs share to the mock filesystem.
  volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.SMB, 'smbfs', 'SMBFS_LABEL');

  // Add a sub directory to the Smb provider.
  const smbfs = assert(volumeManager.volumeInfoList.item(4).fileSystem);
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:smbfs/child'] =
      MockDirectoryEntry.create(smbfs, '/smbfs_child');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  const metadataModel = createMockMetadataModel();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = metadataModel;
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(5, directoryTree.items.length);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals('NOT_SMB_LABEL', directoryTree.items[2].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals('SMB_LABEL', directoryTree.items[3].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals('SMBFS_LABEL', directoryTree.items[4].label);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const providerItem = directoryTree.items[2];
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const smbItem = directoryTree.items[3];
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const smbfsItem = directoryTree.items[4];
  reportPromise(
      waitUntil(() => {
        // Under providerItem there should be 1 entry, 'child'. Ensure there are
        // no entries under smbItem.
        return providerItem.items.length === 1;
      }).then(() => {
        assertEquals('child', providerItem.items[0].label);
        assertEquals(0, smbItem.items.length);
        assertEquals(0, smbfsItem.items.length);
      }),
      callback);
}

/**
 * Test sub directories are not fetched for SMB, until the directory is
 * clicked.
 * @param {!function(boolean):void} callback A callback function which is called
 *     with test result.
 */
export function testSmbNotFetchedUntilClick(callback) {
  // Add a volume representing an Smb provider to the mock filesystem.
  volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'smb', 'SMB_LABEL', '@smb');

  // Add a sub directory to the Smb provider.
  const smbProvider = assert(volumeManager.volumeInfoList.item(2).fileSystem);
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:smb/child'] =
      MockDirectoryEntry.create(smbProvider, '/smb_child');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  const metadataModel = createMockMetadataModel();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = metadataModel;
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);
  // Draw the tree again, as this triggers a different code path which tries to
  // refresh the sub directory list.
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(3, directoryTree.items.length);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  assertEquals('SMB_LABEL', directoryTree.items[2].label);

  // Expect the SMB share has no children.
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const smbItem = directoryTree.items[2];
  assertEquals(0, smbItem.items.length);

  // Click on the SMB volume.
  smbItem.click();

  reportPromise(
      waitUntil(() => {
        // Wait until the SMB share item has been updated with its sub
        // directories.
        return smbItem.items.length == 1;
      }).then(() => {
        assertEquals('smb_child', smbItem.items[0].label);
      }),
      callback);
}

/** Test EntryListItem.sortEntries doesn't fail sorting empty array. */
export function testEntryListItemSortEntriesEmpty() {
  const rootType = VolumeManagerCommon.RootType.MY_FILES;
  const entryList = new EntryList(str('MY_FILES_ROOT_LABEL'), rootType);
  const modelItem = new NavigationModelFakeItem(
      entryList.label, NavigationModelItemType.ENTRY_LIST, entryList);

  const metadataModel = createMockMetadataModel();
  const directoryTree = /** @type {!DirectoryTree} */ (createElements());
  // @ts-ignore: error TS2540: Cannot assign to 'metadataModel' because it is a
  // read-only property.
  directoryTree.metadataModel = metadataModel;
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, metadataModel,
      fileOperationManager, true);
  // @ts-ignore: error TS2740: Type 'MockNavigationListModel' is missing the
  // following properties from type 'NavigationListModel': shortcutListModel_,
  // recentModelItem_, directoryModel_, androidAppListModel_, and 21 more.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  const entryListItem = new EntryListItem(rootType, modelItem, directoryTree);

  assertEquals(0, entryListItem.sortEntries([]).length);
}


/** Test EntryListItem.sortEntries doesn't fail sorting empty array. */
// @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any' type.
export function testAriaExpanded(callback) {
  // Setup My Drive and Downloads and one folder inside each of them.
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/root/folder1'] =
      MockDirectoryEntry.create(driveFileSystem, '/root/folder1');
  const downloadsFileSystem = volumeManager.volumeInfoList.item(1).fileSystem;
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:downloads/folder1'] =
      MockDirectoryEntry.create(downloadsFileSystem, '/folder1');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  const mockMetadata = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, mockMetadata,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const downloadsItem = directoryTree.items[1];
  reportPromise(
      waitUntil(() => {
        if (!downloadsItem.expanded) {
          // While downloads isn't expanded aria-expanded should be also false.
          const ariaExpanded = downloadsItem.getAttribute('aria-expanded');
          assertTrue(ariaExpanded === 'false' || ariaExpanded === null);
          // Click has to be async to wait Downloads to reads its children.
          downloadsItem.querySelector('.expand-icon').click();
        }
        // After clicking on expand-icon, aria-expanded should be set to true.
        return downloadsItem.getAttribute('aria-expanded') === 'true';
      }).then(() => {
        // .tree-children should have role="group" otherwise Chromevox doesn't
        // speak the depth level properly.
        assertEquals(
            'group',
            downloadsItem.querySelector('.tree-children').getAttribute('role'));
      }),
      callback);
}

/**
 * Test a disabled drive volume.
 *
 * If drive is disabled, this should be reflected in the directory model as
 * well: the children shouldn't be loaded, and clicking on the item or the
 * expand icon shouldn't do anything.
 */
// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export async function testDriveDisabled(done) {
  // Setup My Drive and Downloads and one folder inside each of them.
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:drive/root/folder1'] =
      MockDirectoryEntry.create(driveFileSystem, '/root/folder1');
  const downloadsFileSystem = volumeManager.volumeInfoList.item(1).fileSystem;
  // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry' is not assignable
  // to type 'MockDirectoryEntry'.
  fakeFileSystemURLEntries['filesystem:downloads/folder1'] =
      MockDirectoryEntry.create(downloadsFileSystem, '/folder1');

  // Populate the directory tree with the mock filesystem.
  let directoryTree = createElements();
  // @ts-ignore: error TS2339: Property 'metadataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.metadataModel = createMockMetadataModel();
  const mockMetadata = createMockMetadataModel();
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, mockMetadata,
      fileOperationManager, true);
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'HTMLElement'.
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);

  // Coerce to DirectoryTree type and draw the tree.
  directoryTree = /** @type {!DirectoryTree} */ (directoryTree);
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  // Set the drive to be disabled. We do it here since it's already added when
  // the MockVolumeManager is created.
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'HTMLElement'.
  const driveItem = directoryTree.items[0];
  driveItem.disabled = true;
  // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
  // 'HTMLElement'.
  directoryTree.redraw(true);

  assertEquals(str('DRIVE_DIRECTORY_LABEL'), driveItem.label);
  assertTrue(driveItem.disabled);
  assertFalse(driveItem.hasChildren);

  // Clicking on the item shouldn't select it.
  driveItem.click();
  await new Promise(resolve => requestAnimationFrame(resolve));
  assertFalse(driveItem.selected);

  // The item shouldn't be expanded.
  let isExpanded = driveItem.getAttribute('aria-expanded') || 'false';
  assertEquals('false', isExpanded);
  // Clicking on the expand icon shouldn't expand.
  driveItem.querySelector('.expand-icon').click();
  await new Promise(resolve => requestAnimationFrame(resolve));
  isExpanded = driveItem.getAttribute('aria-expanded') || 'false';
  assertEquals('false', isExpanded);

  done();
}
