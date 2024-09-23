// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {installMockChrome, MockMetrics} from '../../common/js/mock_chrome.js';
import type {MockFileSystem} from '../../common/js/mock_entry.js';
import {MockDirectoryEntry, MockFileEntry} from '../../common/js/mock_entry.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';

import {ActionsModel, CommonActionId, InternalActionId} from './actions_model.js';
import {FSP_ACTION_HIDDEN_ONEDRIVE_ACCOUNT_STATE, FSP_ACTION_HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED, FSP_ACTION_HIDDEN_ONEDRIVE_URL, FSP_ACTION_HIDDEN_ONEDRIVE_USER_EMAIL} from './constants.js';
import type {FolderShortcutsDataModel} from './folder_shortcuts_data_model.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';
import type {ActionModelUi} from './ui/action_model_ui.js';
import type {FilesAlertDialog} from './ui/files_alert_dialog.js';
import type {ListContainer} from './ui/list_container.js';

type GetCustomActionsCallback =
    (actions: chrome.fileManagerPrivate.FileSystemProviderAction[]) => void;

let mockVolumeManager: MockVolumeManager;
let driveFileSystem: MockFileSystem;
let providedFileSystem: MockFileSystem;
let mockMetrics: MockMetrics;
let shortcutsModel: FolderShortcutsDataModel;

/**
 * @return total number of calls for that metric.
 */
function countMetricCalls(metricName: string): number {
  const calls = mockMetrics.metricCalls;
  return (calls[metricName] || []).length;
}

function createFakeFolderShortcutsDataModel(): FolderShortcutsDataModel {
  class FakeFolderShortcutsModel extends EventTarget {
    private has_ = false;

    constructor() {
      super();
    }

    exists() {
      return this.has_;
    }

    add() {
      this.has_ = true;
      return 0;
    }

    remove() {
      this.has_ = false;
      return 0;
    }
  }

  const model = new FakeFolderShortcutsModel();
  return model as unknown as FolderShortcutsDataModel;
}

class MockUi implements ActionModelUi {
  listContainer: ListContainer;
  alertDialog: FilesAlertDialog;

  constructor() {
    this.listContainer = {
      currentView: {
        updateListItemsMetadata: function() {},
      },
    } as unknown as ListContainer;

    this.alertDialog = {
      showHtml: () => {},
    } as unknown as FilesAlertDialog;
  }
}

let ui: MockUi;

function getVolumeManager(): VolumeManager {
  return mockVolumeManager as unknown as VolumeManager;
}

function asMetadataModel(metadataModel: MockMetadataModel): MetadataModel {
  return metadataModel as unknown as MetadataModel;
}

export function setUp() {
  mockMetrics = new MockMetrics();
  // Mock Chrome APIs.
  const mockChrome = {
    metricsPrivate: mockMetrics,
    runtime: {
      lastError: undefined,
    },
  };
  installMockChrome(mockChrome);

  // Setup Drive file system.
  mockVolumeManager = new MockVolumeManager();
  let type = VolumeType.DRIVE;
  assert(mockVolumeManager.getCurrentProfileVolumeInfo(type)!.fileSystem);
  driveFileSystem = mockVolumeManager.getCurrentProfileVolumeInfo(
                                         type)!.fileSystem as MockFileSystem;

  // Setup Provided file system.
  type = VolumeType.PROVIDED;
  mockVolumeManager.createVolumeInfo(type, 'provided', 'Provided');
  assert(mockVolumeManager.getCurrentProfileVolumeInfo(type)!.fileSystem);
  providedFileSystem = mockVolumeManager.getCurrentProfileVolumeInfo(
                                            type)!.fileSystem as MockFileSystem;

  // Create mock action model components.
  shortcutsModel = createFakeFolderShortcutsDataModel();
  ui = new MockUi();
}

/**
 * Tests that the correct actions are available for a Google Drive directory.
 */
export async function testDriveDirectoryEntry() {
  driveFileSystem.entries!['/test'] =
      MockDirectoryEntry.create(driveFileSystem, '/test');

  const mockMetadataModel = new MockMetadataModel({
    canShare: true,
  });

  let model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [driveFileSystem.entries!['/test']]);

  let invalidated = 0;
  model.addEventListener('invalidated', () => {
    invalidated++;
  });

  await model.initialize();
  let actions = model.getActions();
  assertEquals(4, Object.keys(actions).length);

  // 'Manage in Drive' should be disabled in offline mode.
  const manageInDriveAction = actions[InternalActionId.MANAGE_IN_DRIVE]!;
  assertTrue(!!manageInDriveAction);
  mockVolumeManager.driveConnectionState = {
    type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
    reason: undefined,
  };
  assertFalse(manageInDriveAction.canExecute());

  // 'Create Shortcut' should be enabled, until it's executed, then
  // disabled.
  let createFolderShortcutAction =
      actions[InternalActionId.CREATE_FOLDER_SHORTCUT]!;
  assertTrue(!!createFolderShortcutAction);
  assertTrue(createFolderShortcutAction.canExecute());
  createFolderShortcutAction.execute();
  assertFalse(createFolderShortcutAction.canExecute());
  assertEquals(1, invalidated);

  // The model is invalidated, as list of actions have changed.
  // Recreated the model and check that the actions are updated.
  model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [driveFileSystem.entries!['/test']]);
  model.addEventListener('invalidated', () => {
    invalidated++;
  });
  await model.initialize();
  actions = model.getActions();
  assertEquals(5, Object.keys(actions).length);
  assertTrue(!!actions[InternalActionId.MANAGE_IN_DRIVE]);
  assertTrue(!!actions[InternalActionId.REMOVE_FOLDER_SHORTCUT]);

  // 'Create shortcut' should be disabled.
  createFolderShortcutAction =
      actions[InternalActionId.CREATE_FOLDER_SHORTCUT]!;
  assertTrue(!!createFolderShortcutAction);
  assertFalse(createFolderShortcutAction.canExecute());
  assertEquals(1, invalidated);
}

/**
 * Tests that the correct actions are available for a Google Drive file.
 */
export async function testDriveFileEntry() {
  driveFileSystem.entries!['/test.txt'] =
      MockFileEntry.create(driveFileSystem, '/test.txt');

  const mockMetadataModel = new MockMetadataModel({
    hosted: false,
    pinned: false,
    canPin: true,
  });

  let model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [driveFileSystem.entries!['/test.txt']]);
  let invalidated = 0;

  await model.initialize();
  let actions = model.getActions();
  assertEquals(2, Object.keys(actions).length);

  // 'Save for Offline' should be enabled.
  const saveForOfflineAction = actions[CommonActionId.SAVE_FOR_OFFLINE]!;
  assertTrue(!!saveForOfflineAction);
  assertTrue(saveForOfflineAction.canExecute());

  // 'Manage in Drive' should be enabled.
  let manageInDriveAction = actions[InternalActionId.MANAGE_IN_DRIVE]!;
  assertTrue(!!manageInDriveAction);
  assertTrue(manageInDriveAction.canExecute());

  chrome.fileManagerPrivate.pinDriveFile =
      (entry: Entry, pin: boolean, callback: VoidCallback) => {
        mockMetadataModel.properties['pinned'] = true;
        assertEquals(driveFileSystem.entries!['/test.txt'], entry);
        assertTrue(pin);
        callback();
      };

  // For pinning, invalidating is done asynchronously, so we need to
  // wait for it with a promise.
  await new Promise<void>(resolve => {
    model.addEventListener('invalidated', () => {
      invalidated++;
      resolve();
    });
    saveForOfflineAction.execute();
  });
  assertTrue(mockMetadataModel.properties['pinned']!);
  assertEquals(1, invalidated);

  assertEquals(1, countMetricCalls('FileBrowser.DrivePinSuccess'));
  assertEquals(0, countMetricCalls('FileBrowser.DriveHostedFilePinSuccess'));

  // The model is invalidated, as list of actions have changed.
  // Recreated the model and check that the actions are updated.
  model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [driveFileSystem.entries!['/test.txt']]);
  await model.initialize();
  actions = model.getActions();
  assertEquals(2, Object.keys(actions).length);

  // 'Offline not Necessary' should be enabled.
  const offlineNotNecessaryAction =
      actions[CommonActionId.OFFLINE_NOT_NECESSARY]!;
  assertTrue(!!offlineNotNecessaryAction);
  assertTrue(offlineNotNecessaryAction.canExecute());

  // 'Manage in Drive' should be enabled.
  manageInDriveAction = actions[InternalActionId.MANAGE_IN_DRIVE]!;
  assertTrue(!!manageInDriveAction);
  assertTrue(manageInDriveAction.canExecute());

  chrome.fileManagerPrivate.pinDriveFile =
      (entry: Entry, pin: boolean, callback: VoidCallback) => {
        mockMetadataModel.properties['pinned'] = false;
        assertEquals(driveFileSystem.entries!['/test.txt'], entry);
        assertFalse(pin);
        callback();
      };

  await new Promise<void>(resolve => {
    model.addEventListener('invalidated', () => {
      invalidated++;
      resolve();
    });
    offlineNotNecessaryAction.execute();
  });
  assertFalse(mockMetadataModel.properties['pinned']!);
  assertEquals(2, invalidated);
}

/**
 * Tests that the correct actions are available for a Google Drive hosted file.
 */
export async function testDriveHostedFileEntry() {
  const testDocument = MockFileEntry.create(driveFileSystem, '/test.gdoc');
  const testFile = MockFileEntry.create(driveFileSystem, '/test.txt');
  driveFileSystem.entries!['/test.gdoc'] = testDocument;
  driveFileSystem.entries!['/test.txt'] = testFile;

  const mockMetadataModel = new MockMetadataModel({});
  mockMetadataModel.set(
      testDocument, {hosted: true, pinned: false, canPin: true});
  mockMetadataModel.set(testFile, {hosted: false, pinned: false, canPin: true});
  mockVolumeManager.driveConnectionState = {
    type: chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
    reason: undefined,
  };
  chrome.fileManagerPrivate.pinDriveFile =
      (entry: Entry, pin: boolean, callback: VoidCallback) => {
        const metadata = mockMetadataModel.getCache([entry])[0]!;
        metadata['pinned'] = pin;
        mockMetadataModel.set(entry, metadata);
        callback();
      };

  let model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [testDocument]);

  await model.initialize();
  let actions = model.getActions();

  // 'Save for Offline' should be enabled.
  let saveForOfflineAction = actions[CommonActionId.SAVE_FOR_OFFLINE]!;
  assertTrue(!!saveForOfflineAction);
  assertTrue(saveForOfflineAction.canExecute());

  // Check the actions for multiple selection.
  model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [testDocument, testFile]);
  await model.initialize();
  actions = model.getActions();

  // 'Offline not Necessary' should not be enabled.
  assertFalse(actions.hasOwnProperty(CommonActionId.OFFLINE_NOT_NECESSARY));

  // 'Save for Offline' should be enabled.
  saveForOfflineAction = actions[CommonActionId.SAVE_FOR_OFFLINE]!;
  assertTrue(!!saveForOfflineAction);
  assertTrue(saveForOfflineAction.canExecute());

  // For pinning, invalidating is done asynchronously, so we need to
  // wait for it with a promise.
  await new Promise<void>(resolve => {
    model.addEventListener('invalidated', () => {
      resolve();
    });
    saveForOfflineAction.execute();
  });

  assertTrue(!!mockMetadataModel.getCache([testDocument])[0]!['pinned']);
  assertTrue(!!mockMetadataModel.getCache([testFile])[0]!['pinned']);

  assertEquals(2, countMetricCalls('FileBrowser.DrivePinSuccess'));
  assertEquals(1, countMetricCalls('FileBrowser.DriveHostedFilePinSuccess'));

  model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [testDocument, testFile]);
  await model.initialize();

  actions = model.getActions();

  // 'Offline not Necessary' should be enabled.
  const offlineNotNecessaryAction =
      actions[CommonActionId.OFFLINE_NOT_NECESSARY]!;
  assertTrue(!!offlineNotNecessaryAction);
  assertTrue(offlineNotNecessaryAction.canExecute());

  // 'Save for Offline' should not be enabled.
  assertFalse(actions.hasOwnProperty(CommonActionId.SAVE_FOR_OFFLINE));

  // For pinning, invalidating is done asynchronously, so we need to
  // wait for it with a promise.
  await new Promise<void>(resolve => {
    model.addEventListener('invalidated', () => {
      resolve();
    });
    offlineNotNecessaryAction.execute();
  });

  assertFalse(!!mockMetadataModel.getCache([testDocument])[0]!['pinned']);
  assertFalse(!!mockMetadataModel.getCache([testFile])[0]!['pinned']);
}

/**
 * Tests that the correct actions are available for a Drive file that cannot be
 * pinned.
 */
export async function testUnpinnableDriveHostedFileEntry() {
  const testDocument = MockFileEntry.create(driveFileSystem, '/test.gdoc');
  const testFile = MockFileEntry.create(driveFileSystem, '/test.txt');
  driveFileSystem.entries!['/test.gdoc'] = testDocument;
  driveFileSystem.entries!['/test.txt'] = testFile;

  const mockMetadataModel = new MockMetadataModel({});
  mockMetadataModel.set(
      testDocument, {hosted: true, pinned: false, canPin: false});
  mockMetadataModel.set(testFile, {hosted: false, pinned: false, canPin: true});
  mockVolumeManager.driveConnectionState = {
    type: chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
    reason: undefined,
  };
  chrome.fileManagerPrivate.pinDriveFile =
      (entry: Entry, pin: boolean, callback: VoidCallback) => {
        const metadata = mockMetadataModel.getCache([entry])[0]!;
        metadata['pinned'] = pin;
        mockMetadataModel.set(entry, metadata);
        callback();
      };

  let model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [testDocument]);

  await model.initialize();
  let actions = model.getActions();

  // 'Save for Offline' should be enabled, but not executable.
  let saveForOfflineAction = actions[CommonActionId.SAVE_FOR_OFFLINE]!;
  assertTrue(!!saveForOfflineAction);
  assertFalse(saveForOfflineAction.canExecute());

  // Check the actions for multiple selection.
  model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [testDocument, testFile]);
  await model.initialize();

  actions = model.getActions();

  // 'Offline not Necessary' should not be enabled.
  assertFalse(actions.hasOwnProperty(CommonActionId.OFFLINE_NOT_NECESSARY));

  // 'Save for Offline' should be enabled even though we can't pin
  // hosted files as we've also selected a non-hosted file.
  saveForOfflineAction = actions[CommonActionId.SAVE_FOR_OFFLINE]!;
  assertTrue(!!saveForOfflineAction);
  assertTrue(saveForOfflineAction.canExecute());

  // For pinning, invalidating is done asynchronously, so we need to
  // wait for it with a promise.
  await new Promise<void>(resolve => {
    model.addEventListener('invalidated', () => {
      resolve();
    });
    saveForOfflineAction.execute();
  });

  assertFalse(!!mockMetadataModel.getCache([testDocument])[0]!['pinned']);
  assertTrue(!!mockMetadataModel.getCache([testFile])[0]!['pinned']);

  assertEquals(1, countMetricCalls('FileBrowser.DrivePinSuccess'));
  assertEquals(0, countMetricCalls('FileBrowser.DriveHostedFilePinSuccess'));

  model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [testDocument, testFile]);
  await model.initialize();

  actions = model.getActions();

  // 'Offline not Necessary' should be enabled.
  const offlineNotNecessaryAction =
      actions[CommonActionId.OFFLINE_NOT_NECESSARY]!;
  assertTrue(!!offlineNotNecessaryAction);
  assertTrue(offlineNotNecessaryAction.canExecute());

  // 'Save for Offline' should be enabled, but not executable.
  saveForOfflineAction = actions[CommonActionId.SAVE_FOR_OFFLINE]!;
  assertTrue(!!saveForOfflineAction);
  assertFalse(saveForOfflineAction.canExecute());

  // For pinning, invalidating is done asynchronously, so we need to
  // wait for it with a promise.
  await new Promise<void>(resolve => {
    model.addEventListener('invalidated', () => {
      resolve();
    });
    offlineNotNecessaryAction.execute();
  });

  assertFalse(!!mockMetadataModel.getCache([testDocument])[0]!['pinned']);
  assertFalse(!!mockMetadataModel.getCache([testFile])[0]!['pinned']);
}

/**
 * Tests that a Team Drive Root entry has the correct actions available.
 */
export async function testTeamDriveRootEntry() {
  driveFileSystem.entries!['/team_drives/ABC Team'] =
      MockDirectoryEntry.create(driveFileSystem, '/team_drives/ABC Team');

  const mockMetadataModel = new MockMetadataModel({
    canShare: true,
  });

  const model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [driveFileSystem.entries!['/team_drives/ABC Team']]);

  await model.initialize();
  const actions = model.getActions();
  assertEquals(3, Object.keys(actions).length);

  // "manage in drive" action is disabled for Team Drive Root entries.
  const manageAction = actions[InternalActionId.MANAGE_IN_DRIVE]!;
  assertTrue(!!manageAction);
  assertTrue(manageAction.canExecute());
}

/**
 * Tests that a Team Drive directory entry has the correct actions available.
 */
export async function testTeamDriveDirectoryEntry() {
  driveFileSystem.entries!['/team_drives/ABC Team/Folder 1'] =
      MockDirectoryEntry.create(
          driveFileSystem, '/team_drives/ABC Team/Folder 1');

  const mockMetadataModel = new MockMetadataModel({
    canShare: true,
    canPin: true,
  });

  const model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [driveFileSystem.entries!['/team_drives/ABC Team/Folder 1']]);

  await model.initialize();
  const actions = model.getActions();
  assertEquals(4, Object.keys(actions).length);

  // "Available Offline" toggle is enabled for Team Drive directories.
  const saveForOfflineAction = actions[CommonActionId.SAVE_FOR_OFFLINE]!;
  assertTrue(!!saveForOfflineAction);
  assertTrue(saveForOfflineAction.canExecute());

  // "Available Offline" toggle is enabled for Team Drive directories.
  const offlineNotNecessaryAction =
      actions[CommonActionId.OFFLINE_NOT_NECESSARY]!;
  assertTrue(!!offlineNotNecessaryAction);
  assertTrue(offlineNotNecessaryAction.canExecute());

  // "Manage in drive" is enabled for Team Drive directories.
  const manageAction = actions[InternalActionId.MANAGE_IN_DRIVE]!;
  assertTrue(!!manageAction);
  assertTrue(manageAction.canExecute());

  // 'Create shortcut' should be enabled.
  const createFolderShortcutAction =
      actions[InternalActionId.CREATE_FOLDER_SHORTCUT]!;
  assertTrue(!!createFolderShortcutAction);
  assertTrue(createFolderShortcutAction.canExecute());
}

/**
 * Tests that a Team Drive file entry has the correct actions available.
 */
export async function testTeamDriveFileEntry() {
  driveFileSystem.entries!['/team_drives/ABC Team/Folder 1/test.txt'] =
      MockFileEntry.create(
          driveFileSystem, '/team_drives/ABC Team/Folder 1/test.txt');

  const mockMetadataModel = new MockMetadataModel({
    hosted: false,
    pinned: false,
    canPin: true,
  });

  const model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui,
      [driveFileSystem.entries!['/team_drives/ABC Team/Folder 1/test.txt']]);

  await model.initialize();
  const actions = model.getActions();
  assertEquals(2, Object.keys(actions).length);

  // "save for offline" action is enabled for Team Drive file entries.
  const saveForOfflineAction = actions[CommonActionId.SAVE_FOR_OFFLINE]!;
  assertTrue(!!saveForOfflineAction);
  assertTrue(saveForOfflineAction.canExecute());

  // "manage in drive" action is enabled for Team Drive file entries.
  const manageAction = actions[InternalActionId.MANAGE_IN_DRIVE]!;
  assertTrue(!!manageAction);
  assertTrue(manageAction.canExecute());
}

/**
 * Tests that if actions are provided with getCustomActions(), they appear
 * correctly for the file.
 */
export async function testProvidedEntry() {
  providedFileSystem.entries!['/test'] =
      MockDirectoryEntry.create(providedFileSystem, '/test');

  chrome.fileManagerPrivate.getCustomActions =
      (entries: Entry[], callback: GetCustomActionsCallback) => {
        assertEquals(1, entries.length);
        assertEquals(providedFileSystem.entries!['/test'], entries[0]);
        callback([
          {
            id: CommonActionId.SHARE,
            title: 'Share it!',
          },
          {
            id: 'some-custom-id',
            title: 'Turn into chocolate!',
          },
          {
            id: FSP_ACTION_HIDDEN_ONEDRIVE_URL,
            title: 'url',
          },
          {
            id: FSP_ACTION_HIDDEN_ONEDRIVE_USER_EMAIL,
            title: 'email',
          },
          {
            id: FSP_ACTION_HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED,
            title: 'false',
          },
          {
            id: FSP_ACTION_HIDDEN_ONEDRIVE_ACCOUNT_STATE,
            title: 'account state',
          },
        ]);
      };

  const mockMetadataModel = new MockMetadataModel({});

  const model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [providedFileSystem.entries!['/test']]);

  let invalidated = 0;
  model.addEventListener('invalidated', () => {
    invalidated++;
  });

  await model.initialize();
  const actions = model.getActions();
  // The fake actions are hidden.
  assertEquals(2, Object.keys(actions).length);

  const shareAction = actions[CommonActionId.SHARE]!;
  assertTrue(!!shareAction);
  // Sharing on FSP is possible even if Drive is offline. Custom actions
  // are always executable, as we don't know the actions implementation.
  mockVolumeManager.driveConnectionState = {
    type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
  };
  assertTrue(shareAction.canExecute());
  assertEquals('Share it!', shareAction.getTitle());

  chrome.fileManagerPrivate.executeCustomAction =
      (entries: Entry[], actionId: string, callback: VoidCallback) => {
        assertEquals(1, entries.length);
        assertEquals(providedFileSystem.entries!['/test'], entries[0]);
        assertEquals(CommonActionId.SHARE, actionId);
        callback();
      };
  shareAction.execute();
  assertEquals(1, invalidated);

  assertTrue(!!actions['some-custom-id']);
  assertTrue(actions['some-custom-id']!.canExecute());
  assertEquals('Turn into chocolate!', actions['some-custom-id']!.getTitle());

  chrome.fileManagerPrivate.executeCustomAction =
      (entries: Entry[], actionId: string, callback: VoidCallback) => {
        assertEquals(1, entries.length);
        assertEquals(providedFileSystem.entries!['/test'], entries[0]);
        assertEquals('some-custom-id', actionId);
        callback();
      };

  actions['some-custom-id']!.execute();
  assertEquals(2, invalidated);
}

/**
 * Tests that no actions are available when getCustomActions() throws an error.
 */
export async function testProvidedEntryWithError() {
  providedFileSystem.entries!['/test'] =
      MockDirectoryEntry.create(providedFileSystem, '/test');

  chrome.fileManagerPrivate.getCustomActions =
      (_entries: Entry[], callback: GetCustomActionsCallback) => {
        chrome.runtime.lastError = {
          message: 'Failed to fetch custom actions.',
        };
        callback([]);
      };

  const mockMetadataModel = new MockMetadataModel({});

  const model = new ActionsModel(
      getVolumeManager(), asMetadataModel(mockMetadataModel), shortcutsModel,
      ui, [providedFileSystem.entries!['/test']]);

  await model.initialize();
  const actions = model.getActions();
  assertEquals(0, Object.keys(actions).length);
}
