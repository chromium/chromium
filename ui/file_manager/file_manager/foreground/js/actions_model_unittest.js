// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {metrics} from '../../common/js/metrics.js';
import {installMockChrome, MockCommandLinePrivate} from '../../common/js/mock_chrome.js';
import {MockDirectoryEntry, MockFileEntry} from '../../common/js/mock_entry.js';
import {reportPromise} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

import {ActionsModel} from './actions_model.js';
import {FolderShortcutsDataModel} from './folder_shortcuts_data_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';
import {ActionModelUI} from './ui/action_model_ui.js';
import {FilesAlertDialog} from './ui/files_alert_dialog.js';
import {ListContainer} from './ui/list_container.js';

/**
 * @type {!MockVolumeManager}
 */
let volumeManager;

/**
 * @type {!FileSystem}
 */
let driveFileSystem;

/**
 * @type {!FileSystem}
 */
let providedFileSystem;


/**
 * @returns {!FolderShortcutsDataModel}
 */
function createFakeFolderShortcutsDataModel() {
  class FakeFolderShortcutsModel extends EventTarget {
    constructor() {
      super();
      this.has = false;
    }

    exists() {
      return this.has;
    }

    add(entry) {
      this.has = true;
      return 0;
    }

    remove(entry) {
      this.has = false;
      return 0;
    }
  }

  const model = /** @type {!Object} */ (new FakeFolderShortcutsModel());
  return /** @type {!FolderShortcutsDataModel} */ (model);
}

/**
 * @type {!FolderShortcutsDataModel}
 */
let shortcutsModel;

/** @implements ActionModelUI */
class MockUI {
  constructor() {
    this.listContainer = /** @type {!ListContainer} */ ({
      currentView: {
        updateListItemsMetadata: function() {},
      },
    });

    this.alertDialog = /** @type {!FilesAlertDialog} */ ({
      showHtml: function() {},
    });
  }
}

/**
 * @type {!MockUI}
 */
let ui;

export function setUp() {
  // Mock Chrome APIs.
  const mockChrome = {
    runtime: {
      lastError: null,
    },
  };
  installMockChrome(mockChrome);
  new MockCommandLinePrivate();

  /**
   * Mock metrics.recordBoolean.
   * @param {string} name Short metric name.
   * @param {boolean} value The value to be recorded.
   */
  metrics.recordBoolean = (name, value) => {
    metrics.calls[name]++;
  };

  metrics.calls = {
    DrivePinSuccess: 0,
    DriveHostedFilePinSuccess: 0,
  };

  // Setup Drive file system.
  volumeManager = new MockVolumeManager();
  let type = VolumeManagerCommon.VolumeType.DRIVE;
  driveFileSystem =
      assert(volumeManager.getCurrentProfileVolumeInfo(type).fileSystem);

  // Setup Provided file system.
  type = VolumeManagerCommon.VolumeType.PROVIDED;
  volumeManager.createVolumeInfo(type, 'provided', 'Provided');
  providedFileSystem =
      assert(volumeManager.getCurrentProfileVolumeInfo(type).fileSystem);

  // Create mock action model components.
  shortcutsModel = createFakeFolderShortcutsDataModel();
  ui = new MockUI();
}

/**
 * Tests that the correct actions are available for a Google Drive directory.
 */
export function testDriveDirectoryEntry(callback) {
  driveFileSystem.entries['/test'] =
      MockDirectoryEntry.create(driveFileSystem, '/test');

  const metadataModel = new MockMetadataModel({
    canShare: true,
  });

  let model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, ui,
      [driveFileSystem.entries['/test']]);

  let invalidated = 0;
  model.addEventListener('invalidated', () => {
    invalidated++;
  });

  return reportPromise(
      model.initialize()
          .then(() => {
            const actions = model.getActions();
            assertEquals(5, Object.keys(actions).length);

            // 'Share' should be disabled in offline mode.
            const shareAction = actions[ActionsModel.CommonActionId.SHARE];
            assertTrue(!!shareAction);
            volumeManager.driveConnectionState = {
              type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
            };
            assertFalse(shareAction.canExecute());

            // 'Manage in Drive' should be disabled in offline mode.
            const manageInDriveAction =
                actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
            assertTrue(!!manageInDriveAction);
            assertFalse(manageInDriveAction.canExecute());

            // 'Create Shortcut' should be enabled, until it's executed, then
            // disabled.
            const createFolderShortcutAction =
                actions[ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT];
            assertTrue(!!createFolderShortcutAction);
            assertTrue(createFolderShortcutAction.canExecute());
            createFolderShortcutAction.execute();
            assertFalse(createFolderShortcutAction.canExecute());
            assertEquals(1, invalidated);

            // The model is invalidated, as list of actions have changed.
            // Recreated the model and check that the actions are updated.
            model = new ActionsModel(
                volumeManager, metadataModel, shortcutsModel, ui,
                [driveFileSystem.entries['/test']]);
            model.addEventListener('invalidated', () => {
              invalidated++;
            });
            return model.initialize();
          })
          .then(() => {
            const actions = model.getActions();
            assertEquals(6, Object.keys(actions).length);
            assertTrue(!!actions[ActionsModel.CommonActionId.SHARE]);
            assertTrue(
                !!actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE]);
            assertTrue(!!actions[ActionsModel.InternalActionId
                                     .REMOVE_FOLDER_SHORTCUT]);

            // 'Create shortcut' should be disabled.
            const createFolderShortcutAction =
                actions[ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT];
            assertTrue(!!createFolderShortcutAction);
            assertFalse(createFolderShortcutAction.canExecute());
            assertEquals(1, invalidated);
          }),
      callback);
}

/**
 * Tests that the correct actions are available for a Google Drive file.
 */
export function testDriveFileEntry(callback) {
  driveFileSystem.entries['/test.txt'] =
      MockFileEntry.create(driveFileSystem, '/test.txt');

  const metadataModel = new MockMetadataModel({
    hosted: false,
    pinned: false,
    canPin: true,
  });

  let model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, ui,
      [driveFileSystem.entries['/test.txt']]);
  let invalidated = 0;

  return reportPromise(
      model.initialize()
          .then(() => {
            const actions = model.getActions();
            assertEquals(3, Object.keys(actions).length);
            assertTrue(!!actions[ActionsModel.CommonActionId.SHARE]);

            // 'Save for Offline' should be enabled.
            const saveForOfflineAction =
                actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
            assertTrue(!!saveForOfflineAction);
            assertTrue(saveForOfflineAction.canExecute());

            // 'Manage in Drive' should be enabled.
            const manageInDriveAction =
                actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
            assertTrue(!!manageInDriveAction);
            assertTrue(manageInDriveAction.canExecute());

            chrome.fileManagerPrivate.pinDriveFile = (entry, pin, callback) => {
              metadataModel.properties.pinned = true;
              assertEquals(driveFileSystem.entries['/test.txt'], entry);
              assertTrue(pin);
              callback();
            };

            // For pinning, invalidating is done asynchronously, so we need to
            // wait for it with a promise.
            return new Promise((fulfill, reject) => {
              model.addEventListener('invalidated', () => {
                invalidated++;
                fulfill();
              });
              saveForOfflineAction.execute();
            });
          })
          .then(() => {
            assertTrue(metadataModel.properties.pinned);
            assertEquals(1, invalidated);

            assertEquals(1, metrics.calls['DrivePinSuccess']);
            assertEquals(0, metrics.calls['DriveHostedFilePinSuccess']);

            // The model is invalidated, as list of actions have changed.
            // Recreated the model and check that the actions are updated.
            model = new ActionsModel(
                volumeManager, metadataModel, shortcutsModel, ui,
                [driveFileSystem.entries['/test.txt']]);
            return model.initialize();
          })
          .then(() => {
            const actions = model.getActions();
            assertEquals(3, Object.keys(actions).length);
            assertTrue(!!actions[ActionsModel.CommonActionId.SHARE]);

            // 'Offline not Necessary' should be enabled.
            const offlineNotNecessaryAction =
                actions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY];
            assertTrue(!!offlineNotNecessaryAction);
            assertTrue(offlineNotNecessaryAction.canExecute());

            // 'Manage in Drive' should be enabled.
            const manageInDriveAction =
                actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
            assertTrue(!!manageInDriveAction);
            assertTrue(manageInDriveAction.canExecute());

            chrome.fileManagerPrivate.pinDriveFile = (entry, pin, callback) => {
              metadataModel.properties.pinned = false;
              assertEquals(driveFileSystem.entries['/test.txt'], entry);
              assertFalse(pin);
              callback();
            };

            return new Promise((fulfill, reject) => {
              model.addEventListener('invalidated', () => {
                invalidated++;
                fulfill();
              });
              offlineNotNecessaryAction.execute();
            });
          })
          .then(() => {
            assertFalse(metadataModel.properties.pinned);
            assertEquals(2, invalidated);
          }),
      callback);
}

/**
 * Tests that the correct actions are available for a Google Drive hosted file.
 */
export function testDriveHostedFileEntry(callback) {
  const testDocument = MockFileEntry.create(driveFileSystem, '/test.gdoc');
  const testFile = MockFileEntry.create(driveFileSystem, '/test.txt');
  driveFileSystem.entries['/test.gdoc'] = testDocument;
  driveFileSystem.entries['/test.txt'] = testFile;

  const metadataModel = new MockMetadataModel({});
  metadataModel.set(testDocument, {hosted: true, pinned: false, canPin: true});
  metadataModel.set(testFile, {hosted: false, pinned: false, canPin: true});
  volumeManager.driveConnectionState = {
    type: chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
    hasCellularNetworkAccess: false,
    canPinHostedFiles: true,
  };
  chrome.fileManagerPrivate.pinDriveFile = (entry, pin, callback) => {
    const metadata = metadataModel.getCache([entry])[0];
    metadata.pinned = pin;
    metadataModel.set(entry, metadata);
    callback();
  };

  let model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, ui, [testDocument]);

  return reportPromise(
      model.initialize()
          .then(() => {
            const actions = model.getActions();

            // 'Save for Offline' should be enabled.
            const saveForOfflineAction =
                actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
            assertTrue(!!saveForOfflineAction);
            assertTrue(saveForOfflineAction.canExecute());

            // Check the actions for multiple selection.
            model = new ActionsModel(
                volumeManager, metadataModel, shortcutsModel, ui,
                [testDocument, testFile]);
            return model.initialize();
          })
          .then(() => {
            const actions = model.getActions();

            // 'Offline not Necessary' should not be enabled.
            assertFalse(actions.hasOwnProperty(
                ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY));

            // 'Save for Offline' should be enabled.
            const saveForOfflineAction =
                actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
            assertTrue(!!saveForOfflineAction);
            assertTrue(saveForOfflineAction.canExecute());

            // For pinning, invalidating is done asynchronously, so we need to
            // wait for it with a promise.
            return new Promise((fulfill, reject) => {
              model.addEventListener('invalidated', () => {
                fulfill();
              });
              saveForOfflineAction.execute();
            });
          })
          .then(() => {
            assertTrue(!!metadataModel.getCache([testDocument])[0].pinned);
            assertTrue(!!metadataModel.getCache([testFile])[0].pinned);

            assertEquals(2, metrics.calls['DrivePinSuccess']);
            assertEquals(1, metrics.calls['DriveHostedFilePinSuccess']);

            model = new ActionsModel(
                volumeManager, metadataModel, shortcutsModel, ui,
                [testDocument, testFile]);
            return model.initialize();
          })
          .then(() => {
            const actions = model.getActions();

            // 'Offline not Necessary' should be enabled.
            const offlineNotNecessaryAction =
                actions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY];
            assertTrue(!!offlineNotNecessaryAction);
            assertTrue(offlineNotNecessaryAction.canExecute());

            // 'Save for Offline' should not be enabled.
            assertFalse(actions.hasOwnProperty(
                ActionsModel.CommonActionId.SAVE_FOR_OFFLINE));

            // For pinning, invalidating is done asynchronously, so we need to
            // wait for it with a promise.
            return new Promise((fulfill, reject) => {
              model.addEventListener('invalidated', () => {
                fulfill();
              });
              offlineNotNecessaryAction.execute();
            });
          })
          .then(() => {
            assertFalse(!!metadataModel.getCache([testDocument])[0].pinned);
            assertFalse(!!metadataModel.getCache([testFile])[0].pinned);
          }),
      callback);
}

/**
 * Tests that the correct actions are available for a Drive file that cannot be
 * pinned.
 */
export function testUnpinnableDriveHostedFileEntry(callback) {
  const testDocument = MockFileEntry.create(driveFileSystem, '/test.gdoc');
  const testFile = MockFileEntry.create(driveFileSystem, '/test.txt');
  driveFileSystem.entries['/test.gdoc'] = testDocument;
  driveFileSystem.entries['/test.txt'] = testFile;

  const metadataModel = new MockMetadataModel({});
  metadataModel.set(testDocument, {hosted: true, pinned: false, canPin: false});
  metadataModel.set(testFile, {hosted: false, pinned: false, canPin: true});
  volumeManager.driveConnectionState = {
    type: chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
    hasCellularNetworkAccess: false,
    canPinHostedFiles: false,
  };
  chrome.fileManagerPrivate.pinDriveFile = (entry, pin, callback) => {
    const metadata = metadataModel.getCache([entry])[0];
    metadata.pinned = pin;
    metadataModel.set(entry, metadata);
    callback();
  };

  let model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, ui, [testDocument]);

  return reportPromise(
      model.initialize()
          .then(() => {
            const actions = model.getActions();

            // 'Save for Offline' should be enabled, but not executable.
            const saveForOfflineAction =
                actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
            assertTrue(!!saveForOfflineAction);
            assertFalse(saveForOfflineAction.canExecute());

            // Check the actions for multiple selection.
            model = new ActionsModel(
                volumeManager, metadataModel, shortcutsModel, ui,
                [testDocument, testFile]);
            return model.initialize();
          })
          .then(() => {
            const actions = model.getActions();

            // 'Offline not Necessary' should not be enabled.
            assertFalse(actions.hasOwnProperty(
                ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY));

            // 'Save for Offline' should be enabled even though we can't pin
            // hosted files as we've also selected a non-hosted file.
            const saveForOfflineAction =
                actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
            assertTrue(!!saveForOfflineAction);
            assertTrue(saveForOfflineAction.canExecute());

            // For pinning, invalidating is done asynchronously, so we need to
            // wait for it with a promise.
            return new Promise((fulfill, reject) => {
              model.addEventListener('invalidated', () => {
                fulfill();
              });
              saveForOfflineAction.execute();
            });
          })
          .then(() => {
            assertFalse(!!metadataModel.getCache([testDocument])[0].pinned);
            assertTrue(!!metadataModel.getCache([testFile])[0].pinned);

            assertEquals(1, metrics.calls['DrivePinSuccess']);
            assertEquals(0, metrics.calls['DriveHostedFilePinSuccess']);

            model = new ActionsModel(
                volumeManager, metadataModel, shortcutsModel, ui,
                [testDocument, testFile]);
            return model.initialize();
          })
          .then(() => {
            const actions = model.getActions();

            // 'Offline not Necessary' should be enabled.
            const offlineNotNecessaryAction =
                actions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY];
            assertTrue(!!offlineNotNecessaryAction);
            assertTrue(offlineNotNecessaryAction.canExecute());

            // 'Save for Offline' should be enabled, but not executable.
            const saveForOfflineAction =
                actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
            assertTrue(!!saveForOfflineAction);
            assertFalse(saveForOfflineAction.canExecute());

            // For pinning, invalidating is done asynchronously, so we need to
            // wait for it with a promise.
            return new Promise((fulfill, reject) => {
              model.addEventListener('invalidated', () => {
                fulfill();
              });
              offlineNotNecessaryAction.execute();
            });
          })
          .then(() => {
            assertFalse(!!metadataModel.getCache([testDocument])[0].pinned);
            assertFalse(!!metadataModel.getCache([testFile])[0].pinned);
          }),
      callback);
}

/**
 * Tests that a Team Drive Root entry has the correct actions available.
 */
export function testTeamDriveRootEntry(callback) {
  driveFileSystem.entries['/team_drives/ABC Team'] =
      MockDirectoryEntry.create(driveFileSystem, '/team_drives/ABC Team');

  const metadataModel = new MockMetadataModel({
    canShare: true,
  });

  const model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, ui,
      [driveFileSystem.entries['/team_drives/ABC Team']]);

  return reportPromise(
      model.initialize().then(() => {
        const actions = model.getActions();
        console.log(Object.keys(actions));
        assertEquals(4, Object.keys(actions).length);

        // "share" action is enabled for Team Drive Root entries.
        const shareAction = actions[ActionsModel.CommonActionId.SHARE];
        assertTrue(!!shareAction);
        assertTrue(shareAction.canExecute());

        // "manage in drive" action is disabled for Team Drive Root entries.
        const manageAction =
            actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
        assertTrue(!!manageAction);
        assertTrue(manageAction.canExecute());
      }),
      callback);
}

/**
 * Tests that a Team Drive directory entry has the correct actions available.
 */
export function testTeamDriveDirectoryEntry(callback) {
  driveFileSystem.entries['/team_drives/ABC Team/Folder 1'] =
      MockDirectoryEntry.create(
          driveFileSystem, '/team_drives/ABC Team/Folder 1');

  const metadataModel = new MockMetadataModel({
    canShare: true,
    canPin: true,
  });

  const model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, ui,
      [driveFileSystem.entries['/team_drives/ABC Team/Folder 1']]);

  return reportPromise(
      model.initialize().then(() => {
        const actions = model.getActions();
        assertEquals(5, Object.keys(actions).length);

        // "Share" is enabled for Team Drive directories.
        const shareAction = actions[ActionsModel.CommonActionId.SHARE];
        assertTrue(!!shareAction);
        assertTrue(shareAction.canExecute());

        // "Available Offline" toggle is enabled for Team Drive directories.
        const saveForOfflineAction =
            actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
        assertTrue(!!saveForOfflineAction);
        assertTrue(saveForOfflineAction.canExecute());

        // "Available Offline" toggle is enabled for Team Drive directories.
        const offlineNotNecessaryAction =
            actions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY];
        assertTrue(!!offlineNotNecessaryAction);
        assertTrue(offlineNotNecessaryAction.canExecute());

        // "Manage in drive" is enabled for Team Drive directories.
        const manageAction =
            actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
        assertTrue(!!manageAction);
        assertTrue(manageAction.canExecute());

        // 'Create shortcut' should be enabled.
        const createFolderShortcutAction =
            actions[ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT];
        assertTrue(!!createFolderShortcutAction);
        assertTrue(createFolderShortcutAction.canExecute());
      }),
      callback);
}

/**
 * Tests that a Team Drive file entry has the correct actions available.
 */
export function testTeamDriveFileEntry(callback) {
  driveFileSystem.entries['/team_drives/ABC Team/Folder 1/test.txt'] =
      MockFileEntry.create(
          driveFileSystem, '/team_drives/ABC Team/Folder 1/test.txt');

  const metadataModel = new MockMetadataModel({
    hosted: false,
    pinned: false,
    canPin: true,
  });

  const model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, ui,
      [driveFileSystem.entries['/team_drives/ABC Team/Folder 1/test.txt']]);

  return reportPromise(
      model.initialize().then(() => {
        const actions = model.getActions();
        assertEquals(3, Object.keys(actions).length);

        // "save for offline" action is enabled for Team Drive file entries.
        const saveForOfflineAction =
            actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
        assertTrue(!!saveForOfflineAction);
        assertTrue(saveForOfflineAction.canExecute());

        // "share" action is enabled for Team Drive file entries.
        const shareAction = actions[ActionsModel.CommonActionId.SHARE];
        assertTrue(!!shareAction);
        assertTrue(shareAction.canExecute());

        // "manage in drive" action is enabled for Team Drive file entries.
        const manageAction =
            actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
        assertTrue(!!manageAction);
        assertTrue(manageAction.canExecute());
      }),
      callback);
}

/**
 * Tests that if actions are provided with getCustomActions(), they appear
 * correctly for the file.
 */
export function testProvidedEntry(callback) {
  providedFileSystem.entries['/test'] =
      MockDirectoryEntry.create(providedFileSystem, '/test');

  chrome.fileManagerPrivate.getCustomActions = (entries, callback) => {
    assertEquals(1, entries.length);
    assertEquals(providedFileSystem.entries['/test'], entries[0]);
    callback([
      {
        id: ActionsModel.CommonActionId.SHARE,
        title: 'Share it!',
      },
      {
        id: 'some-custom-id',
        title: 'Turn into chocolate!',
      },
    ]);
  };

  const metadataModel = new MockMetadataModel(null);

  const model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, ui,
      [providedFileSystem.entries['/test']]);

  let invalidated = 0;
  model.addEventListener('invalidated', () => {
    invalidated++;
  });

  return reportPromise(
      model.initialize().then(() => {
        const actions = model.getActions();
        assertEquals(2, Object.keys(actions).length);

        const shareAction = actions[ActionsModel.CommonActionId.SHARE];
        assertTrue(!!shareAction);
        // Sharing on FSP is possible even if Drive is offline. Custom actions
        // are always executable, as we don't know the actions implementation.
        volumeManager.driveConnectionState = {
          type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
          hasCellularNetworkAccess: false,
          canPinHostedFiles: false,
        };
        assertTrue(shareAction.canExecute());
        assertEquals('Share it!', shareAction.getTitle());

        chrome.fileManagerPrivate.executeCustomAction =
            (entries, actionId, callback) => {
              assertEquals(1, entries.length);
              assertEquals(providedFileSystem.entries['/test'], entries[0]);
              assertEquals(ActionsModel.CommonActionId.SHARE, actionId);
              callback();
            };
        shareAction.execute();
        assertEquals(1, invalidated);

        assertTrue(!!actions['some-custom-id']);
        assertTrue(actions['some-custom-id'].canExecute());
        assertEquals(
            'Turn into chocolate!', actions['some-custom-id'].getTitle());

        chrome.fileManagerPrivate.executeCustomAction =
            (entries, actionId, callback) => {
              assertEquals(1, entries.length);
              assertEquals(providedFileSystem.entries['/test'], entries[0]);
              assertEquals('some-custom-id', actionId);
              callback();
            };

        actions['some-custom-id'].execute();
        assertEquals(2, invalidated);
      }),
      callback);
}

/**
 * Tests that no actions are available when getCustomActions() throws an error.
 */
export function testProvidedEntryWithError(callback) {
  providedFileSystem.entries['/test'] =
      MockDirectoryEntry.create(providedFileSystem, '/test');

  chrome.fileManagerPrivate.getCustomActions = (entries, callback) => {
    chrome.runtime.lastError = {
      message: 'Failed to fetch custom actions.',
    };
    callback(['error']);
  };

  const metadataModel = new MockMetadataModel(null);

  const model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, ui,
      [providedFileSystem.entries['/test']]);

  return reportPromise(
      model.initialize().then(() => {
        const actions = model.getActions();
        assertEquals(0, Object.keys(actions).length);
      }),
      callback);
}
