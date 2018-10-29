// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome;
var mockCommandLinePrivate;
var metrics;
var onDirectoryChangedListeners;

/**
 * Set string data.
 * @type {Object}
 */
loadTimeData.data = {
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads',
  DRIVE_DIRECTORY_LABEL: 'Google Drive',
  DRIVE_MY_DRIVE_LABEL: 'My Drive',
  DRIVE_TEAM_DRIVES_LABEL: 'Team Drives',
  DRIVE_COMPUTERS_LABEL: 'Computers',
  DRIVE_OFFLINE_COLLECTION_LABEL: 'Offline',
  DRIVE_SHARED_WITH_ME_COLLECTION_LABEL: 'Shared with me',
  REMOVABLE_DIRECTORY_LABEL: 'External Storage',
  ARCHIVE_DIRECTORY_LABEL: 'Archives',
  MY_FILES_ROOT_LABEL: 'My files',
};

function setUp() {
  chrome = {
    fileManagerPrivate: {
      onDirectoryChanged: {
        addListener: function(listener) {
          onDirectoryChangedListeners.push(listener);
        }
      }
    }
  };
  onDirectoryChangedListeners = [];
  mockCommandLinePrivate = new MockCommandLinePrivate();

  metrics = {recordSmallCount: function() {}};

  window.webkitResolveLocalFileSystemURLEntries = {};
  window.webkitResolveLocalFileSystemURL = function(url, callback) {
    callback(webkitResolveLocalFileSystemURLEntries[url]);
  };
}

/**
 * Creates a plain object that can be used as mock for MetadataModel.
 */
function mockMetadataModel() {
  const mock = {
    notifyEntriesChanged: () => {},
    get: function(entries, labels) {
      // Mock a non-shared directory.
      return Promise.resolve([{shared: false}]);
    },
    getCache: function(entries, labels) {
      return [{shared: false}];
    },
  };
  return mock;
}

/**
 * Returns item labels of a directory tree as a list.
 *
 * @param {DirectoryTree} directoryTree A directory tree.
 * @return {Array<string>} List of labels.
 */
function getDirectoryTreeItemLabelsAsAList(directoryTree) {
  var result = [];
  for (var i = 0; i < directoryTree.items.length; i++) {
    var item = directoryTree.items[i];
    result.push(item.label);
  }
  return result;
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
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTree(callback) {
  // Create elements.
  var parentElement = document.createElement('div');
  var directoryTree = document.createElement('div');
  parentElement.appendChild(directoryTree);

  // Create mocks.
  var directoryModel = new MockDirectoryModel();
  var volumeManager = new MockVolumeManager();
  var fileOperationManager = {
    addEventListener: function(name, callback) {}
  };

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');

  DirectoryTree.decorate(directoryTree, directoryModel, volumeManager,
      null, fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  assertEquals(2, directoryTree.items.length);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Under the drive item, there exist 3 entries.
        return driveItem.items.length == 3;
      }).then(function() {
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
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithTeamDrive(callback) {
  // Create elements.
  var parentElement = document.createElement('div');
  var directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();

  parentElement.appendChild(directoryTree);

  // Create mocks.
  var directoryModel = new MockDirectoryModel();
  var volumeManager = new MockVolumeManager();
  var fileOperationManager = {addEventListener: function(name, callback) {}};

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window
      .webkitResolveLocalFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  window.webkitResolveLocalFileSystemURLEntries
      ['filesystem:drive/team_drives/a'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives/a');

  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  assertEquals(2, directoryTree.items.length);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Under the drive item, there exist 4 entries.
        return driveItem.items.length == 4;
      }).then(function() {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(str('DRIVE_TEAM_DRIVES_LABEL'), driveItem.items[1].label);
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
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithEmptyTeamDrive(callback) {
  // Create elements.
  var parentElement = document.createElement('div');
  var directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();

  parentElement.appendChild(directoryTree);

  // Create mocks.
  var directoryModel = new MockDirectoryModel();
  var volumeManager = new MockVolumeManager();
  var fileOperationManager = {addEventListener: function(name, callback) {}};

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window
      .webkitResolveLocalFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  // No directories exist under Team Drives

  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Root entries under Drive volume is generated, Team Drives isn't
        // included because it has no child.
        // See testCreateDirectoryTreeWithTeamDrive for detail.
        return driveItem.items.length == 3;
      }).then(function() {
        var teamDrivesItemFound = false;
        for (var i = 0; i < driveItem.items.length; i++) {
          if (driveItem.items[i].label == str('DRIVE_TEAM_DRIVES_LABEL')) {
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
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithComputers(callback) {
  // Create elements.
  let parentElement = document.createElement('div');
  let directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();

  parentElement.appendChild(directoryTree);

  // Create mocks.
  let directoryModel = new MockDirectoryModel();
  let volumeManager = new MockVolumeManager();
  const fileOperationManager = {addEventListener: function(name, callback) {}};

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  window.webkitResolveLocalFileSystemURLEntries
      ['filesystem:drive/Comuters/My Laptop'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers/My Laptop');

  // Populate the directory tree with the mock filesystem.
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  assertEquals(2, directoryTree.items.length);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Under the drive item, there exist 4 entries.
        return driveItem.items.length == 4;
      }).then(function() {
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
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithEmptyComputers(callback) {
  // Create elements.
  let parentElement = document.createElement('div');
  let directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();

  parentElement.appendChild(directoryTree);

  // Create mocks.
  let directoryModel = new MockDirectoryModel();
  let volumeManager = new MockVolumeManager();
  const fileOperationManager = {addEventListener: function(name, callback) {}};

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  // No directories exist under Team Drives

  // Populate the directory tree with the mock filesystem.
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  const driveItem = directoryTree.items[0];

  // Ensure we do not have a "Computers" item in drive, as it does not contain
  // any children.
  reportPromise(
      waitUntil(function() {
        // Root entries under Drive volume is generated, Computers isn't
        // included because it has no child.
        // See testCreateDirectoryTreeWithComputers for detail.
        return driveItem.items.length == 3;
      }).then(function() {
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
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTreeWithTeamDrivesAndComputers(callback) {
  // Create elements.
  let parentElement = document.createElement('div');
  let directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();

  parentElement.appendChild(directoryTree);

  // Create mocks.
  let directoryModel = new MockDirectoryModel();
  let volumeManager = new MockVolumeManager();
  const fileOperationManager = {addEventListener: function(name, callback) {}};

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window
      .webkitResolveLocalFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  window.webkitResolveLocalFileSystemURLEntries
      ['filesystem:drive/team_drives/a'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives/a');
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  window.webkitResolveLocalFileSystemURLEntries
      ['filesystem:drive/Comuters/My Laptop'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers/My Laptop');

  // Populate the directory tree with the mock filesystem.
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  assertEquals(2, directoryTree.items.length);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  const driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(function() {
        // Under the drive item, there exist 4 entries.
        return driveItem.items.length == 5;
      }).then(function() {
        // There exist 1 my drive entry and 3 fake entries under the drive item.
        assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
        assertEquals(str('DRIVE_TEAM_DRIVES_LABEL'), driveItem.items[1].label);
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
function testUpdateSubElementsFromListSections() {
  // Creates elements.
  const parentElement = document.createElement('div');
  const directoryTree = document.createElement('div');
  parentElement.appendChild(directoryTree);

  // Creates mocks.
  const directoryModel = new MockDirectoryModel();
  const volumeManager = new MockVolumeManager();
  const fileOperationManager = {
    addEventListener: function(name, callback) {}
  };

  const treeModel = new NavigationListModel(
      volumeManager,
      new MockFolderShortcutDataModel([]),
      null, /* recentItem */
      null, /* addNewServicesItem */
      false /* opt_disableMyFilesNavigation */
  );

  const myFilesItem = treeModel.item(0);
  const driveItem = treeModel.item(1);

  assertEquals(NavigationSection.MY_FILES, myFilesItem.section);
  assertEquals(NavigationSection.CLOUD, driveItem.section);

  DirectoryTree.decorate(directoryTree, directoryModel, volumeManager,
      null, fileOperationManager, true);
  directoryTree.dataModel = treeModel;
  directoryTree.updateSubElementsFromList(false);

  // First element should not have section-start attribute, to not display a
  // division line in the first section.
  // My files:
  assertEquals(null, directoryTree.items[0].getAttribute('section-start'));

  // Drive should have section-start, because it's a new section but not the
  // first section.
  assertEquals(
      NavigationSection.CLOUD,
      directoryTree.items[1].getAttribute('section-start'));

  // Regenerate so it re-calculates the 'section-start' without creating the
  // DirectoryItem.
  directoryTree.updateSubElementsFromList(false);
  assertEquals(
      NavigationSection.CLOUD,
      directoryTree.items[1].getAttribute('section-start'));

}

/**
 * Test case for updateSubElementsFromList.
 *
 * Mounts/unmounts removable and archive volumes, and checks these volumes come
 * up to/disappear from the list correctly.
 */
function testUpdateSubElementsFromList() {
  // Creates elements.
  var parentElement = document.createElement('div');
  var directoryTree = document.createElement('div');
  parentElement.appendChild(directoryTree);

  // Creates mocks.
  var directoryModel = new MockDirectoryModel();
  var volumeManager = new MockVolumeManager();
  var fileOperationManager = {
    addEventListener: function(name, callback) {}
  };

  // Sets entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');

  DirectoryTree.decorate(directoryTree, directoryModel, volumeManager,
      null, fileOperationManager, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.updateSubElementsFromList(true);

  // There are 2 volumes, Drive and Downloads, at first.
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabelsAsAList(directoryTree));

  // Mounts a removable volume.
  var removableVolume = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable',
      str('REMOVABLE_DIRECTORY_LABEL'));
  volumeManager.volumeInfoList.add(removableVolume);

  // Asserts that the directoryTree is not updated before the update.
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabelsAsAList(directoryTree));

  // Asserts that a removable directory is added after the update.
  directoryTree.updateSubElementsFromList(false);
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL'),
    str('REMOVABLE_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabelsAsAList(directoryTree));

  // Mounts an archive volume.
  var archiveVolume = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ARCHIVE, 'archive',
      str('ARCHIVE_DIRECTORY_LABEL'));
  volumeManager.volumeInfoList.add(archiveVolume);

  // Asserts that the directoryTree is not updated before the update.
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL'),
    str('REMOVABLE_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabelsAsAList(directoryTree));

  // Asserts that an archive directory is added before the removable directory.
  directoryTree.updateSubElementsFromList(false);
  assertArrayEquals(
      [
        str('DRIVE_DIRECTORY_LABEL'),
        str('DOWNLOADS_DIRECTORY_LABEL'),
        str('REMOVABLE_DIRECTORY_LABEL'),
        str('ARCHIVE_DIRECTORY_LABEL'),
      ],
      getDirectoryTreeItemLabelsAsAList(directoryTree));

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
      getDirectoryTreeItemLabelsAsAList(directoryTree));

  // Asserts that an archive directory is deleted.
  directoryTree.updateSubElementsFromList(false);
  assertArrayEquals([
    str('DRIVE_DIRECTORY_LABEL'),
    str('DOWNLOADS_DIRECTORY_LABEL'),
    str('REMOVABLE_DIRECTORY_LABEL')
  ], getDirectoryTreeItemLabelsAsAList(directoryTree));
}

/**
 * Test adding the first team drive for a user.
 * Team Drives subtree should be shown after the change notification is
 * delivered.
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testAddFirstTeamDrive(callback) {
  // Create elements.
  var parentElement = document.createElement('div');
  var directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();

  parentElement.appendChild(directoryTree);

  // Create mocks.
  var directoryModel = new MockDirectoryModel();
  var volumeManager = new MockVolumeManager();
  var fileOperationManager = {addEventListener: function(name, callback) {}};

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window
      .webkitResolveLocalFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  // No directories exist under Team Drives

  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 3;
      })
          .then(() => {
            window.webkitResolveLocalFileSystemURLEntries
                ['filesystem:drive/team_drives/a'] =
                new MockDirectoryEntry(driveFileSystem, '/team_drives/a');
            let event = {
              entry: window.webkitResolveLocalFileSystemURLEntries
                         ['filesystem:drive/team_drives'],
              eventType: 'changed',
            };
            for (let listener of onDirectoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            return waitUntil(() => {
              for (var i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label ==
                    str('DRIVE_TEAM_DRIVES_LABEL')) {
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
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testRemoveLastTeamDrive(callback) {
  // Create elements.
  var parentElement = document.createElement('div');
  var directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();

  parentElement.appendChild(directoryTree);

  // Create mocks.
  var directoryModel = new MockDirectoryModel();
  var volumeManager = new MockVolumeManager();
  var fileOperationManager = {addEventListener: function(name, callback) {}};

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window
      .webkitResolveLocalFileSystemURLEntries['filesystem:drive/team_drives'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives');
  window.webkitResolveLocalFileSystemURLEntries
      ['filesystem:drive/team_drives/a'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives/a');

  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  var driveItem = directoryTree.items[0];

  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 4;
      })
          .then(() => {
            return new Promise(resolve => {
              window
                  .webkitResolveLocalFileSystemURLEntries
                      ['filesystem:drive/team_drives/a']
                  .remove(resolve);
            });
          })
          .then(() => {
            let event = {
              entry: window.webkitResolveLocalFileSystemURLEntries
                         ['filesystem:drive/team_drives'],
              eventType: 'changed',
            };
            for (let listener of onDirectoryChangedListeners) {
              listener(event);
            }
          })
          .then(() => {
            // Wait team drive grand root to appear.
            return waitUntil(() => {
              for (var i = 0; i < driveItem.items.length; i++) {
                if (driveItem.items[i].label ==
                    str('DRIVE_TEAM_DRIVES_LABEL')) {
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
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testAddFirstComputer(callback) {
  // Create elements.
  let parentElement = document.createElement('div');
  let directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();

  parentElement.appendChild(directoryTree);

  // Create mocks.
  let directoryModel = new MockDirectoryModel();
  let volumeManager = new MockVolumeManager();
  const fileOperationManager = {addEventListener: function(name, callback) {}};

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  // No directories exist under Computers

  // Populate the directory tree with the mock filesystem.
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  let driveItem = directoryTree.items[0];

  // Test that we initially do not have a Computers item under druve, and that
  // adding a filesystem "/Computers/a" results in the Computers item being
  // displayed under drive.
  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 3;
      })
          .then(() => {
            window.webkitResolveLocalFileSystemURLEntries
                ['filesystem:drive/Computers/a'] =
                new MockDirectoryEntry(driveFileSystem, '/Computers/a');
            let event = {
              entry: window.webkitResolveLocalFileSystemURLEntries
                         ['filesystem:drive/Computers'],
              eventType: 'changed',
            };
            for (let listener of onDirectoryChangedListeners) {
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
 * Computerss subtree should be removed after the change notification is
 * delivered.
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testRemoveLastComputer(callback) {
  // Create elements.
  let parentElement = document.createElement('div');
  let directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();

  parentElement.appendChild(directoryTree);

  // Create mocks.
  let directoryModel = new MockDirectoryModel();
  let volumeManager = new MockVolumeManager();
  const fileOperationManager = {addEventListener: function(name, callback) {}};

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/Computers'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers');
  window
      .webkitResolveLocalFileSystemURLEntries['filesystem:drive/Computers/a'] =
      new MockDirectoryEntry(driveFileSystem, '/Computers/a');

  // Populate the directory tree with the mock filesystem.
  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  const driveItem = directoryTree.items[0];

  // Check that removing the local computer "a" results in the entire
  // "Computers" element being removed, as it has no children.
  reportPromise(
      waitUntil(() => {
        return driveItem.items.length == 4;
      })
          .then(() => {
            return new Promise(resolve => {
              window
                  .webkitResolveLocalFileSystemURLEntries
                      ['filesystem:drive/Computers/a']
                  .remove(resolve);
            });
          })
          .then(() => {
            let event = {
              entry: window.webkitResolveLocalFileSystemURLEntries
                         ['filesystem:drive/Computers'],
              eventType: 'changed',
            };
            for (let listener of onDirectoryChangedListeners) {
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
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testInsideMyDriveAndInsideDrive(callback) {
  const parentElement = document.createElement('div');
  const directoryTree = document.createElement('div');
  directoryTree.metadataModel = mockMetadataModel();
  parentElement.appendChild(directoryTree);

  // Create mocks.
  const directoryModel = new MockDirectoryModel();
  const volumeManager = new MockVolumeManager();
  const fileOperationManager = {addEventListener: function(name, callback) {}};

  // Setup My Drive and Downloads and one folder inside each of them.
  const driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  const downloadsFileSystem = volumeManager.volumeInfoList.item(1).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');
  window
      .webkitResolveLocalFileSystemURLEntries['filesystem:drive/root/folder1'] =
      new MockDirectoryEntry(driveFileSystem, '/root/folder1');
  window
      .webkitResolveLocalFileSystemURLEntries['filesystem:downloads/folder1'] =
      new MockDirectoryEntry(downloadsFileSystem, '/folder1');

  DirectoryTree.decorate(
      directoryTree, directoryModel, volumeManager, null, fileOperationManager,
      true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  const driveItem = directoryTree.items[0];
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
