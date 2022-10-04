// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockDirectoryEntry, MockEntry} from '../../common/js/mock_entry.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

import {CommandHandler} from './file_manager_commands.js';

/**
 * Checks that the `toggle-holding-space` command is appropriately enabled/
 * disabled given the current selection state and executes as expected.
 */
export async function testToggleHoldingSpaceCommand(done) {
  // Verify `toggle-holding-space` command exists.
  const command = CommandHandler.getCommand('toggle-holding-space');
  assertNotEquals(command, undefined);

  let getHoldingSpaceStateCalled = false;

  // Enable the holding space feature and provide strings.
  loadTimeData.resetForTesting({
    HOLDING_SPACE_PIN_COMMAND_LABEL: 'Pin to shelf',
    HOLDING_SPACE_UNPIN_COMMAND_LABEL: 'Unpin to shelf',
  });
  loadTimeData.getString = id => {
    return loadTimeData.data_[id] || id;
  };

  /**
   * Mock chrome APIs.
   * @type {Object}
   */
  const mockChrome = {
    fileManagerPrivate: {
      getHoldingSpaceState: (callback) => {
        callback({itemUrls: []});
        getHoldingSpaceStateCalled = true;
      },
    },

    runtime: {},
  };
  installMockChrome(mockChrome);

  // Mock volume manager.
  const volumeManager = new MockVolumeManager();

  // Create `DOWNLOADS` volume.
  const downloadsVolumeInfo = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS, 'downloadsVolumeId',
      'Downloads volume');
  const downloadsFileSystem = downloadsVolumeInfo.fileSystem;

  // Create `REMOVABLE` volume.
  const removableVolumeInfo = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removableVolumeId',
      'Removable volume');
  const removableFileSystem = removableVolumeInfo.fileSystem;

  // Mock file/folder entries.
  const audioFileEntry = new MockEntry(downloadsFileSystem, '/audio.mp3');
  const downloadFileEntry = new MockEntry(downloadsFileSystem, '/download.txt');
  const folderEntry = MockDirectoryEntry.create(downloadsFileSystem, '/folder');
  const imageFileEntry = new MockEntry(downloadsFileSystem, '/image.png');
  const removableFileEntry =
      new MockEntry(removableFileSystem, '/removable.txt');
  const videoFileEntry = new MockEntry(downloadsFileSystem, 'video.mp4');

  // Define test cases.
  const testCases = [
    {
      description: 'Tests empty selection in `Downloads`',
      currentRootType: VolumeManagerCommon.RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
      },
      selection: [],
      expect: {
        canExecute: false,
        hidden: true,
      },
    },
    {
      description: 'Tests selection from supported volume in `Downloads`',
      currentRootType: VolumeManagerCommon.RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
      },
      selection: [downloadFileEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [downloadFileEntry],
      },
    },
    {
      description:
          'Tests folder selection from supported volume in `Downloads`',
      currentRootType: VolumeManagerCommon.RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
      },
      selection: [folderEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [folderEntry],
      },
    },
    {
      description: 'Tests selection from supported volume in `Recent`',
      currentRootType: VolumeManagerCommon.RootType.RECENT,
      currentVolumeInfo: null,
      selection: [downloadFileEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [downloadFileEntry],
      },
    },
    {
      description: 'Test selection from unsupported volume in `Recent`',
      currentRootType: VolumeManagerCommon.RootType.RECENT,
      currentVolumeInfo: null,
      selection: [removableFileEntry],
      expect: {
        canExecute: false,
        hidden: true,
      },
    },
    {
      description: 'Test selection from mix of volumes in `Recent`',
      currentRootType: VolumeManagerCommon.RootType.RECENT,
      currentVolumeInfo: null,
      selection: [audioFileEntry, removableFileEntry, downloadFileEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [audioFileEntry, downloadFileEntry],
      },
    },
  ];

  // Run test cases.
  for (const testCase of testCases) {
    console.log('Starting test case... ' + testCase.description);

    // Mock `Event`.
    const event = {
      canExecute: true,
      command: {
        hidden: false,
        setHidden: (hidden) => {
          event.command.hidden = hidden;
        },
      },
    };

    // Mock `FileManager`.
    const fileManager = {
      directoryModel: {
        getCurrentRootType: () => testCase.currentRootType,
        getCurrentVolumeInfo: () => testCase.currentVolumeInfo,
      },
      selectionHandler: {
        selection: {entries: testCase.selection},
      },
      volumeManager: volumeManager,
    };

    // Verify `command.canExecute()` results in expected `event` state.
    getHoldingSpaceStateCalled = false;
    command.canExecute(event, fileManager);
    if (testCase.expect.canExecute) {
      await waitUntil(() => getHoldingSpaceStateCalled);
      // Wait for the command.checkHoldingSpaceState() promise to finish.
      await new Promise(resolve => setTimeout(resolve));
    }

    assertEquals(event.canExecute, testCase.expect.canExecute);
    assertEquals(event.command.hidden, testCase.expect.hidden);

    if (!event.canExecute || event.command.hidden) {
      continue;
    }

    // Mock private API.
    let didInteractWithMockPrivateApi = false;
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace = (entries, isAdd) => {
      didInteractWithMockPrivateApi = true;
      assertArrayEquals(entries, testCase.expect.entries);
      assertTrue(isAdd);
    };

    // Verify `command.execute()` results in expected mock API interactions.
    command.execute(event, fileManager);
    assertTrue(didInteractWithMockPrivateApi);
  }

  done();
}

/**
 * Checks that the 'extract-all' command is enabled or disabled
 * dependent on the current selection.
 */
export async function testExtractAllCommand(done) {
  // Check: `extract-all` command exists.
  const command = CommandHandler.getCommand('extract-all');
  assertNotEquals(command, undefined);

  // Enable the extract all feature and provide strings.
  loadTimeData.resetForTesting({
    EXTRACT_ARCHIVE: true,
    EXTRACT_ALL_BUTTON_LABEL: 'Extract all',
  });
  loadTimeData.getString = id => {
    return loadTimeData.data_[id] || id;
  };

  let startIOTaskCalled = false;

  /**
   * Mock chrome startIOTask API.
   * @type {Object}
   */
  const mockChrome = {
    fileManagerPrivate: {
      startIOTask: () => {
        startIOTaskCalled = true;
      },
    },

    runtime: {},
  };
  installMockChrome(mockChrome);

  // Mock volume manager.
  const volumeManager = new MockVolumeManager();

  // Create `DOWNLOADS` volume.
  const downloadsVolumeInfo = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS, 'downloadsVolumeId',
      'Downloads volume');
  const downloadsFileSystem = downloadsVolumeInfo.fileSystem;

  // Mock file entries.
  const folderEntry = MockDirectoryEntry.create(downloadsFileSystem, '/folder');
  const textFileEntry = new MockEntry(downloadsFileSystem, '/file.txt');
  const zipFileEntry = new MockEntry(downloadsFileSystem, '/archive.zip');
  const imageFileEntry = new MockEntry(downloadsFileSystem, '/image.jpg');

  // Mock `Event`.
  const event = {
    canExecute: true,
    command: {
      hidden: false,
      setHidden: (hidden) => {
        event.command.hidden = hidden;
      },
    },
  };

  // The current selection for testing.
  const currentSelection = {
    entries: [],
    iconType: 'none',
    totalCount: 0,
  };

  // Mock `FileManager`.
  const fileManager = {
    directoryModel: {
      isOnNative: () => true,
      isReadOnly: () => false,
    },
    getCurrentDirectoryEntry: () => folderEntry,
    getSelection: () => currentSelection,
    volumeManager: volumeManager,
  };

  // Check: canExecute is false and command is hidden with no selection.
  command.canExecute(event, fileManager);
  assertFalse(event.canExecute);
  assertTrue(event.command.hidden);

  // Check: canExecute is true and command is visible with a single ZIP file.
  currentSelection.entries = [zipFileEntry];
  currentSelection.iconType = 'archive';
  currentSelection.totalCount = 1;
  command.canExecute(event, fileManager);
  assertTrue(event.canExecute);
  assertFalse(event.command.hidden);

  // Check: `zip-selection` command exists.
  const zipCommand = CommandHandler.getCommand('zip-selection');
  assertNotEquals(command, undefined);

  // Check: ZIP canExecute is false and command hidden with a single ZIP file.
  zipCommand.canExecute(event, fileManager);
  assertFalse(event.canExecute);
  assertTrue(event.command.hidden);

  // Check: canExecute is false and command hidden for no ZIP multi-selection.
  currentSelection.entries = [imageFileEntry, textFileEntry];
  currentSelection.totalCount = 2;
  command.canExecute(event, fileManager);
  assertFalse(event.canExecute);
  assertTrue(event.command.hidden);

  // Check: canExecute is true and command visible for ZIP multiple selection.
  currentSelection.entries = [zipFileEntry, textFileEntry];
  currentSelection.totalCount = 2;
  command.canExecute(event, fileManager);
  assertTrue(event.canExecute);
  assertFalse(event.command.hidden);

  // Check: ZIP canExecute is true and command visible for multiple selection.
  zipCommand.canExecute(event, fileManager);
  assertTrue(event.canExecute);
  assertFalse(event.command.hidden);

  done();
}

/**
 * Tests that rename command should be disabled for Recent entry.
 */
export async function testRenameCommand(done) {
  loadTimeData.resetForTesting({});

  // Check: `rename` command exists.
  const command = CommandHandler.getCommand('rename');
  assertNotEquals(command, undefined);

  // Mock volume manager.
  const volumeManager = new MockVolumeManager();

  // Create `documents_root` volume.
  const documentsRootVolumeInfo = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.MEDIA_VIEW,
      'com.android.providers.media.documents:documents_root', 'Documents');

  // Mock file entries.
  const recentEntry =
      new FakeEntryImpl('Recent', VolumeManagerCommon.RootType.RECENT);
  const pdfEntry = MockDirectoryEntry.create(
      documentsRootVolumeInfo.fileSystem, 'Documents/abc.pdf');

  // Mock `Event`.
  const event = {
    canExecute: true,
    target: {
      entry: pdfEntry,
    },
    command: {
      hidden: false,
      setHidden: (hidden) => {
        event.command.hidden = hidden;
      },
    },
  };

  // The current selection for testing.
  const currentSelection = {
    entries: [pdfEntry],
    iconType: 'none',
    totalCount: 1,
  };

  // Mock `FileManager`.
  const fileManager = {
    directoryModel: {
      isOnNative: () => true,
      isReadOnly: () => false,
      getCurrentRootType: () => null,
    },
    getCurrentDirectoryEntry: () => recentEntry,
    getSelection: () => currentSelection,
    volumeManager: volumeManager,
  };

  // Check: canExecute is false and command is disabled.
  command.canExecute(event, fileManager);
  assertFalse(event.canExecute);
  assertFalse(event.command.hidden);

  done();
}
