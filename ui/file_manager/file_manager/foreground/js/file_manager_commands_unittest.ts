// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import type {VolumeInfo} from '../../background/js/volume_info.js';
import {entriesToURLs} from '../../common/js/entry_utils.js';
import {FakeEntryImpl, type FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {installMockChrome, MockMetrics} from '../../common/js/mock_chrome.js';
import type {MockFileSystem} from '../../common/js/mock_entry.js';
import {MockDirectoryEntry, MockEntry} from '../../common/js/mock_entry.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {addVolume, convertVolumeInfoAndMetadataToVolume, updateIsInteractiveVolume} from '../../state/ducks/volumes.js';
import {createMyFilesDataWithVolumeEntry} from '../../state/ducks/volumes_unittest.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore, waitDeepEquals} from '../../state/for_tests.js';

import type {CommandHandlerDeps, FilesCommandId} from './command_handler.js';
import {CommandHandler, ValidMenuCommandsForUma} from './command_handler.js';
import type {Command} from './ui/command.js';
import {CanExecuteEvent} from './ui/command.js';

let mockMetrics: MockMetrics;

function getMetricName(metricIndex: number): string|undefined {
  return ValidMenuCommandsForUma[metricIndex];
}

interface ExtraCanExecuteCommandProperties {
  target: {
    selectedItems?: Array<Entry|FilesAppEntry>,
                 classList: {contains: () => boolean},
    parentElement?: {contextElement: null},
    dataset?: Record<string, any>,
  };
}

interface CurrentSelection {
  entries: MockEntry[];
  iconType: string;
  totalCount: number;
}

function createMockEvent(
    commandId?: string,
    properties?: ExtraCanExecuteCommandProperties): CanExecuteEvent {
  const command = {
    hidden: false,
    setHidden: (hidden: boolean) => {
      event.command.hidden = hidden;
    },
  } as unknown as Command;
  if (commandId) {
    command.id = commandId;
  }
  let event = new CanExecuteEvent(command);
  event.canExecute = true;
  if (properties) {
    event = Object.assign(properties, event);
  }
  return event;
}

/**
 * Checks that the `toggle-holding-space` command is appropriately enabled/
 * disabled given the current selection state and executes as expected.
 */
export async function testToggleHoldingSpaceCommand() {
  // Verify `toggle-holding-space` command exists.
  const command = CommandHandler.getCommand('toggle-holding-space');
  assertNotEquals(command, undefined);

  let getHoldingSpaceStateCalled = false;

  /**
   * Mock chrome APIs.
   */
  let itemUrls: string[] = [];
  mockMetrics = new MockMetrics();
  const mockChrome = {
    metricsPrivate: mockMetrics,
    fileManagerPrivate: {
      getHoldingSpaceState:
          (callback: (state: chrome.fileManagerPrivate.HoldingSpaceState) =>
               void) => {
            callback({itemUrls});
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
      VolumeType.DOWNLOADS, 'downloadsVolumeId', 'Downloads volume');
  const downloadsFileSystem = downloadsVolumeInfo.fileSystem as MockFileSystem;

  // Create `REMOVABLE` volume.
  const removableVolumeInfo = volumeManager.createVolumeInfo(
      VolumeType.REMOVABLE, 'removableVolumeId', 'Removable volume');
  const removableFileSystem = removableVolumeInfo.fileSystem as MockFileSystem;

  // Mock file/folder entries.
  const audioFileEntry = new MockEntry(downloadsFileSystem, '/audio.mp3');
  const downloadFileEntry = new MockEntry(downloadsFileSystem, '/download.txt');
  const folderEntry = MockDirectoryEntry.create(downloadsFileSystem, '/folder');
  new MockEntry(downloadsFileSystem, '/image.png');
  const removableFileEntry =
      new MockEntry(removableFileSystem, '/removable.txt');
  new MockEntry(downloadsFileSystem, 'video.mp4');

  // Define test cases.
  const testCases = [
    {
      description: 'Tests empty selection in `Downloads`',
      currentRootType: RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeType.DOWNLOADS,
      },
      itemUrls: [],
      selection: [],
      expect: {
        canExecute: false,
        hidden: true,
        isAdd: false,
      },
    },
    {
      description: 'Tests selection from supported volume in `Downloads`',
      currentRootType: RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeType.DOWNLOADS,
      },
      itemUrls: [],
      selection: [downloadFileEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [downloadFileEntry],
        isAdd: true,
      },
    },
    {
      description:
          'Tests folder selection from supported volume in `Downloads`',
      currentRootType: RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeType.DOWNLOADS,
      },
      itemUrls: [],
      selection: [folderEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [folderEntry],
        isAdd: true,
      },
    },
    {
      description:
          'Tests pinned selection from supported volume in `Downloads`',
      currentRootType: RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeType.DOWNLOADS,
      },
      itemUrls: entriesToURLs([downloadFileEntry]),
      selection: [downloadFileEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [downloadFileEntry],
        isAdd: false,
      },
    },
    {
      description: 'Tests selection from supported volume in `Recent`',
      currentRootType: RootType.RECENT,
      currentVolumeInfo: null,
      selection: [downloadFileEntry],
      itemUrls: [],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [downloadFileEntry],
        isAdd: true,
      },
    },
    {
      description: 'Test selection from unsupported volume in `Recent`',
      currentRootType: RootType.RECENT,
      currentVolumeInfo: null,
      itemUrls: [],
      selection: [removableFileEntry],
      expect: {
        canExecute: false,
        hidden: true,
        isAdd: false,
      },
    },
    {
      description: 'Test selection from mix of volumes in `Recent`',
      currentRootType: RootType.RECENT,
      currentVolumeInfo: null,
      itemUrls: [],
      selection: [audioFileEntry, removableFileEntry, downloadFileEntry],
      expect: {
        canExecute: true,
        hidden: false,
        entries: [audioFileEntry, downloadFileEntry],
        isAdd: true,
      },
    },
  ];

  // Run test cases.
  for (const testCase of testCases) {
    console.info('Starting test case... ' + testCase.description);

    // Mock `Event`.
    const event = createMockEvent();

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
    } as unknown as CommandHandlerDeps;

    // Mock `chrome.fileManagerPrivate.getHoldingSpaceState()` response.
    itemUrls = testCase.itemUrls;

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
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace =
        (entries: Entry[], isAdd: boolean) => {
          didInteractWithMockPrivateApi = true;
          assertArrayEquals(entries, testCase.expect.entries as Entry[]);
          assertEquals(isAdd, testCase.expect.isAdd);
        };

    // Reset cache of metrics recorded.
    mockMetrics.metricCalls['FileBrowser.MenuItemSelected'] = [];

    const commandEvent = new CustomEvent('command', {
      detail: {
        command: {
          hidden: false,
          setHidden: (hidden: boolean) => {
            event.command.hidden = hidden;
          },
        } as unknown as Command,
      },
    });

    // Verify `command.execute()` results in expected mock API interactions.
    command.execute(commandEvent, fileManager);
    assertTrue(didInteractWithMockPrivateApi);

    // Verify metrics recorded.
    const calls = mockMetrics.metricCalls['FileBrowser.MenuItemSelected'] || [];
    assertTrue(calls.length > 0);
    // The index is 2nd position argument, we're only checking the first call.
    const metricIndex = calls[0][1];
    assertEquals(
        getMetricName(metricIndex),
        testCase.expect.isAdd ? 'pin-to-holding-space' :
                                'unpin-from-holding-space');
  }
}

/**
 * Checks that the 'extract-all' command is enabled or disabled
 * dependent on the current selection.
 */
export async function testExtractAllCommand() {
  // Check: `extract-all` command exists.
  const command = CommandHandler.getCommand('extract-all');
  assertNotEquals(command, undefined);

  /**
   * Mock chrome startIOTask API.
   */
  const mockChrome = {
    fileManagerPrivate: {
      startIOTask: () => {},
    },
    runtime: {},
  };
  installMockChrome(mockChrome);

  // Mock volume manager.
  const volumeManager = new MockVolumeManager();

  // Create `DOWNLOADS` volume.
  const downloadsVolumeInfo = volumeManager.createVolumeInfo(
      VolumeType.DOWNLOADS, 'downloadsVolumeId', 'Downloads volume');
  const downloadsFileSystem = downloadsVolumeInfo.fileSystem as MockFileSystem;

  // Mock file entries.
  const folderEntry = MockDirectoryEntry.create(downloadsFileSystem, '/folder');
  const textFileEntry = new MockEntry(downloadsFileSystem, '/file.txt');
  const zipFileEntry = new MockEntry(downloadsFileSystem, '/archive.zip');
  const imageFileEntry = new MockEntry(downloadsFileSystem, '/image.jpg');

  // Mock `Event`.
  const event = createMockEvent();

  // The current selection for testing.
  const currentSelection: CurrentSelection = {
    entries: [],
    iconType: 'none',
    totalCount: 0,
  };

  // Mock `FileManager`.
  const fileManager = {
    directoryModel: {
      isOnNative: () => true,
      isReadOnly: () => false,
      getCurrentRootType: () => RootType.DOWNLOADS,
    },
    metadataModel: {
      getCache: () => [],
    },
    getCurrentDirectoryEntry: () => folderEntry,
    getSelection: () => currentSelection,
    volumeManager: volumeManager,
  } as unknown as CommandHandlerDeps;

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
}

/**
 * Tests that rename command should be disabled for Recent entry.
 */
export async function testRenameCommand() {
  // Check: `rename` command exists.
  const command = CommandHandler.getCommand('rename');
  assertNotEquals(command, undefined);

  // Mock volume manager.
  const volumeManager = new MockVolumeManager();

  // Create `documents_root` volume.
  const documentsRootVolumeInfo = volumeManager.createVolumeInfo(
      VolumeType.MEDIA_VIEW,
      'com.android.providers.media.documents:documents_root', 'Documents');

  // Mock file entries.
  const recentEntry = new FakeEntryImpl('Recent', RootType.RECENT);
  const pdfEntry = MockDirectoryEntry.create(
      documentsRootVolumeInfo.fileSystem as MockFileSystem,
      'Documents/abc.pdf');


  // Mock `Event`.
  const event = createMockEvent(undefined, {
    target: {
      classList: {contains: () => false},
      selectedItems: [pdfEntry],
    },
  });

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
    ui: {
      actionbar: {
        contains: () => false,
      },
    },
  } as unknown as CommandHandlerDeps;

  // Check: canExecute is false and command is disabled.
  command.canExecute(event, fileManager);
  assertFalse(event.canExecute);
  assertFalse(event.command.hidden);
}

/**
 * Create and add a Downloads volume to the store. Update the volume as
 * non-interactive.
 */
async function createAndAddNonInteractiveDownloadsVolume():
    Promise<VolumeInfo> {
  setUpFileManagerOnWindow();
  // Dispatch an action to add MyFiles volume.
  const store = setupStore();
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const volumeMetadata = createFakeVolumeMetadata(volumeInfo);
  const volume =
      convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata);
  store.dispatch(addVolume(volumeInfo, volumeMetadata));

  // Expect the newly added volume is in the store.
  const wantNewVol = {
    allEntries: {
      [fileData.key]: fileData,
    },
    volumes: {
      [volume.volumeId]: volume,
    },
  };
  await waitDeepEquals(store, wantNewVol, (state) => ({
                                            allEntries: state.allEntries,
                                            volumes: state.volumes,
                                          }));

  // Dispatch an action to set |isInteractive| for the volume to false.
  store.dispatch(updateIsInteractiveVolume({
    volumeId: volumeInfo.volumeId,
    isInteractive: false,
  }));
  // Expect the volume is set to non-interactive.
  const wantUpdatedVol = {
    volumes: {
      [volumeInfo.volumeId]: {
        ...volume,
        isInteractive: false,
      },
    },
  };
  await waitDeepEquals(store, wantUpdatedVol, (state) => ({
                                                volumes: state.volumes,
                                              }));
  return volumeInfo;
}

/**
 * Tests that the paste, cut, copy and new-folder commands should be
 * disabled and hidden when there are no selected entries but the current
 * directory is on a non-interactive volume (e.g. when the blank space in a
 * non-interactive directory is right clicked).
 */
export async function testCommandsForNonInteractiveVolumeAndNoEntries() {
  const nonInteractiveVolumeInfo =
      await createAndAddNonInteractiveDownloadsVolume();

  const currentSelection: CurrentSelection = {
    entries: [],
    iconType: 'none',
    totalCount: 0,
  };

  // Mock `FileManager`.
  const fileManager = {
    getCurrentDirectoryEntry: () => null,
    // Selection includes entry on non-interactive volume.
    getSelection: () => currentSelection,
    directoryModel: {
      getCurrentDirEntry: () => null,
      // Navigate to the non-interactive volume.
      getCurrentRootType: () => RootType.DOWNLOADS,
      getCurrentVolumeInfo: () => nonInteractiveVolumeInfo,
    },
    document: {
      getElementsByClassName: () => [],
    },
    // Allow paste command.
    fileTransferController: {
      queryPasteCommandEnabled: () => true,
    },
    ui: {
      actionbar: {
        contains: () => false,
      },
      directoryTree: {
        contains: () => false,
      },
    },
  } as unknown as CommandHandlerDeps;

  // Check each command is disabled and hidden.
  const commandNames: FilesCommandId[] = [
    'paste',
    'cut',
    'copy',
    'new-folder',
  ];
  for (const commandName of commandNames) {
    // Check: command exists.
    const command = CommandHandler.getCommand(commandName);
    assertNotEquals(command, undefined);

    // Mock `Event`.
    const event = createMockEvent(commandName, {
      target: {
        classList: {contains: () => false},
        parentElement: {
          contextElement: null,
        },
        dataset: {},
      },
    });
    command.canExecute(event, fileManager);
    assertFalse(event.canExecute);
    assertTrue(event.command.hidden);
  }
}

/**
 * Tests that the paste, cut, copy, new-folder, delete, move-to-trash,
 * paste-into-folder, rename, extract-all and zip-selection commands should be
 * disabled and hidden for an entry on a non-interactive volume.
 */
export async function testCommandsForEntriesOnNonInteractiveVolume() {
  // Create non-interactive volume.
  const nonInteractiveVolumeInfo =
      await createAndAddNonInteractiveDownloadsVolume();

  // Mock volume manager.
  const volumeManager = new MockVolumeManager();

  // Create file entry on non-interactive volume.
  const nonInteractiveVolumeEntry = MockDirectoryEntry.create(
      nonInteractiveVolumeInfo.fileSystem as MockFileSystem, 'abc.pdf');
  const currentSelection = {
    entries: [nonInteractiveVolumeEntry],
    iconType: 'none',
    totalCount: 1,
  };

  // Mock `FileManager`.
  const fileManager = {
    getCurrentDirectoryEntry: () => null,
    // Selection includes entry on non-interactive volume.
    getSelection: () => currentSelection,
    directoryModel: {
      getCurrentDirEntry: () => null,
      getCurrentRootType: () => null,
      getCurrentVolumeInfo: () => null,
    },
    document: {
      getElementsByClassName: () => [],
    },
    // Allow copy, cut and paste command.
    fileTransferController: {
      canCopyOrDrag: () => true,
      canCutOrDrag: () => true,
      queryPasteCommandEnabled: () => true,
    },
    ui: {
      directoryTree: {
        contains: () => false,
      },
      actionbar: {
        contains: () => false,
      },
    },
    volumeManager: volumeManager,
  } as unknown as CommandHandlerDeps;

  // Check each command is disabled and hidden.
  const commandNames: FilesCommandId[] = [
    'paste',
    'cut',
    'copy',
    'new-folder',
    'delete',
    'move-to-trash',
    'paste-into-folder',
    'rename',
    'extract-all',
    'zip-selection',
  ];
  for (const commandName of commandNames) {
    // Check: command exists.
    const command = CommandHandler.getCommand(commandName);
    assertNotEquals(command, undefined);

    // Mock `Event`.
    const event = createMockEvent(commandName, {
      target: {
        selectedItems: currentSelection.entries,
        classList: {contains: () => false},
      },
    });

    command.canExecute(event, fileManager);
    assertFalse(event.canExecute);
    assertTrue(event.command.hidden);
  }
}
