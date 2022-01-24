// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertArrayEquals, assertEquals, assertNotEquals, assertTrue} from 'chrome://test/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockDirectoryEntry, MockEntry} from '../../common/js/mock_entry.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

import {CommandHandler} from './file_manager_commands.js';

/**
 * Checks that the `toggle-holding-space` command is appropriately enabled/
 * disabled given the current selection state and executes as expected.
 */
export function testToggleHoldingSpaceCommand() {
  // Verify `toggle-holding-space` command exists.
  const command = CommandHandler.getCommand('toggle-holding-space');
  assertNotEquals(command, undefined);

  // Enable the holding space feature and provide strings.
  loadTimeData.resetForTesting({
    HOLDING_SPACE_ENABLED: true,
    HOLDING_SPACE_PIN_TO_SHELF_COMMAND_LABEL: 'Pin to shelf',
    HOLDING_SPACE_UNPIN_TO_SHELF_COMMAND_LABEL: 'Unpin to shelf',
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
      },
    },
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
    {
      description: 'Tests selection from supported volume in `Recent Audio`',
      currentRootType: VolumeManagerCommon.RootType.RECENT_AUDIO,
      currentVolumeInfo: null,
      selection: [audioFileEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [audioFileEntry],
      },
    },
    {
      description: 'Tests selection from supported volume in `Recent Images`',
      currentRootType: VolumeManagerCommon.RootType.RECENT_AUDIO,
      currentVolumeInfo: null,
      selection: [imageFileEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [imageFileEntry],
      },
    },
    {
      description: 'Tests selection from supported volume in `Recent Videos`',
      currentRootType: VolumeManagerCommon.RootType.RECENT_AUDIO,
      currentVolumeInfo: null,
      selection: [videoFileEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [videoFileEntry],
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
    command.canExecute(event, fileManager);
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
}
