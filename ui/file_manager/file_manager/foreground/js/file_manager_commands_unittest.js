// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {entriesToURLs} from '../../common/js/entry_utils.js';
import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {installMockChrome, MockMetrics} from '../../common/js/mock_chrome.js';
import {MockDirectoryEntry, MockEntry} from '../../common/js/mock_entry.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {addVolume, convertVolumeInfoAndMetadataToVolume, updateIsInteractiveVolume} from '../../state/ducks/volumes.js';
import {createMyFilesDataWithVolumeEntry} from '../../state/ducks/volumes_unittest.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore, waitDeepEquals} from '../../state/for_tests.js';

import {CommandHandler} from './file_manager_commands.js';

/** @type {!MockMetrics} */
let mockMetrics;

/**
 * @param {number} metricIndex
 * @returns {string|undefined}
 */
function getMetricName(metricIndex) {
  return CommandHandler.ValidMenuCommandsForUMA[metricIndex];
}

/**
 * Checks that the `toggle-holding-space` command is appropriately enabled/
 * disabled given the current selection state and executes as expected.
 * @param {()=>void} done
 */
export async function testToggleHoldingSpaceCommand(done) {
  // Verify `toggle-holding-space` command exists.
  const command = CommandHandler.getCommand('toggle-holding-space');
  assertNotEquals(command, undefined);

  let getHoldingSpaceStateCalled = false;

  /**
   * Mock chrome APIs.
   * @type {!Object}
   */
  let itemUrls = [];
  mockMetrics = new MockMetrics();
  const mockChrome = {
    metricsPrivate: mockMetrics,
    fileManagerPrivate: {
      // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
      // type.
      getHoldingSpaceState: (callback) => {
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
  new MockEntry(downloadsFileSystem, '/image.png');
  const removableFileEntry =
      new MockEntry(removableFileSystem, '/removable.txt');
  new MockEntry(downloadsFileSystem, 'video.mp4');

  // Define test cases.
  const testCases = [
    {
      description: 'Tests empty selection in `Downloads`',
      currentRootType: VolumeManagerCommon.RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
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
      currentRootType: VolumeManagerCommon.RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
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
      currentRootType: VolumeManagerCommon.RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
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
      currentRootType: VolumeManagerCommon.RootType.DOWNLOADS,
      currentVolumeInfo: {
        volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
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
      currentRootType: VolumeManagerCommon.RootType.RECENT,
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
      currentRootType: VolumeManagerCommon.RootType.RECENT,
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
      currentRootType: VolumeManagerCommon.RootType.RECENT,
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
    console.log('Starting test case... ' + testCase.description);

    // Mock `Event`.
    const event = {
      canExecute: true,
      command: {
        hidden: false,
        // @ts-ignore: error TS7006: Parameter 'hidden' implicitly has an 'any'
        // type.
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

    // Mock `chrome.fileManagerPrivate.getHoldingSpaceState()` response.
    itemUrls = testCase.itemUrls;

    // Verify `command.canExecute()` results in expected `event` state.
    getHoldingSpaceStateCalled = false;
    // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
    // command: { hidden: boolean; setHidden: (hidden: any) => void; }; }' is
    // not assignable to parameter of type 'Event'.
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
      // @ts-ignore: error TS2345: Argument of type 'MockEntry[] |
      // FileSystemDirectoryEntry[] | undefined' is not assignable to parameter
      // of type 'any[]'.
      assertArrayEquals(entries, testCase.expect.entries);
      assertEquals(isAdd, testCase.expect.isAdd);
    };

    // Reset cache of metrics recorded.
    mockMetrics.metricCalls['FileBrowser.MenuItemSelected'] = [];

    // Verify `command.execute()` results in expected mock API interactions.
    // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
    // command: { hidden: boolean; setHidden: (hidden: any) => void; }; }' is
    // not assignable to parameter of type 'Event'.
    command.execute(event, fileManager);
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

  done();
}

/**
 * Checks that the 'extract-all' command is enabled or disabled
 * dependent on the current selection.
 * @param {()=>void} done
 */
export async function testExtractAllCommand(done) {
  // Check: `extract-all` command exists.
  const command = CommandHandler.getCommand('extract-all');
  assertNotEquals(command, undefined);

  // @ts-ignore: error TS6133: 'startIOTaskCalled' is declared but its value is
  // never read.
  let startIOTaskCalled = false;

  /**
   * Mock chrome startIOTask API.
   * @type {!Object}
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
      // @ts-ignore: error TS7006: Parameter 'hidden' implicitly has an 'any'
      // type.
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
      getCurrentRootType: () => VolumeManagerCommon.RootType.DOWNLOADS,
    },
    metadataModel: {
      getCache: () => [],
    },
    getCurrentDirectoryEntry: () => folderEntry,
    getSelection: () => currentSelection,
    volumeManager: volumeManager,
  };

  // Check: canExecute is false and command is hidden with no selection.
  // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
  // command: { hidden: boolean; setHidden: (hidden: any) => void; }; }' is not
  // assignable to parameter of type 'Event'.
  command.canExecute(event, fileManager);
  assertFalse(event.canExecute);
  assertTrue(event.command.hidden);

  // Check: canExecute is true and command is visible with a single ZIP file.
  // @ts-ignore: error TS2322: Type 'MockEntry' is not assignable to type
  // 'never'.
  currentSelection.entries = [zipFileEntry];
  currentSelection.iconType = 'archive';
  currentSelection.totalCount = 1;
  // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
  // command: { hidden: boolean; setHidden: (hidden: any) => void; }; }' is not
  // assignable to parameter of type 'Event'.
  command.canExecute(event, fileManager);
  assertTrue(event.canExecute);
  assertFalse(event.command.hidden);

  // Check: `zip-selection` command exists.
  const zipCommand = CommandHandler.getCommand('zip-selection');
  assertNotEquals(command, undefined);

  // Check: ZIP canExecute is false and command hidden with a single ZIP file.
  // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
  // command: { hidden: boolean; setHidden: (hidden: any) => void; }; }' is not
  // assignable to parameter of type 'Event'.
  zipCommand.canExecute(event, fileManager);
  assertFalse(event.canExecute);
  assertTrue(event.command.hidden);

  // Check: canExecute is false and command hidden for no ZIP multi-selection.
  // @ts-ignore: error TS2322: Type 'MockEntry' is not assignable to type
  // 'never'.
  currentSelection.entries = [imageFileEntry, textFileEntry];
  currentSelection.totalCount = 2;
  // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
  // command: { hidden: boolean; setHidden: (hidden: any) => void; }; }' is not
  // assignable to parameter of type 'Event'.
  command.canExecute(event, fileManager);
  assertFalse(event.canExecute);
  assertTrue(event.command.hidden);

  // Check: canExecute is true and command visible for ZIP multiple selection.
  // @ts-ignore: error TS2322: Type 'MockEntry' is not assignable to type
  // 'never'.
  currentSelection.entries = [zipFileEntry, textFileEntry];
  currentSelection.totalCount = 2;
  // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
  // command: { hidden: boolean; setHidden: (hidden: any) => void; }; }' is not
  // assignable to parameter of type 'Event'.
  command.canExecute(event, fileManager);
  assertTrue(event.canExecute);
  assertFalse(event.command.hidden);

  // Check: ZIP canExecute is true and command visible for multiple selection.
  // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
  // command: { hidden: boolean; setHidden: (hidden: any) => void; }; }' is not
  // assignable to parameter of type 'Event'.
  zipCommand.canExecute(event, fileManager);
  assertTrue(event.canExecute);
  assertFalse(event.command.hidden);

  done();
}

/**
 * Tests that rename command should be disabled for Recent entry.
 * @param {()=>void} done
 */
export async function testRenameCommand(done) {
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
      // @ts-ignore: error TS7006: Parameter 'hidden' implicitly has an 'any'
      // type.
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
  // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
  // target: { entry: FileSystemDirectoryEntry; }; command: { hidden: boolean;
  // setHidden: (hidden: any) => void; }; }' is not assignable to parameter of
  // type 'Event'.
  command.canExecute(event, fileManager);
  assertFalse(event.canExecute);
  assertFalse(event.command.hidden);

  done();
}

/**
 * Create and add a Downloads volume to the store. Update the volume as
 * non-interactive.
 * @return {!Promise<import("../../externs/volume_info.js").VolumeInfo>}
 */
async function createAndAddNonInteractiveDownloadsVolume() {
  setUpFileManagerOnWindow();
  // Dispatch an action to add MyFiles volume.
  const store = setupStore();
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesVolumeEntry = fileData.entry;
  const volumeMetadata = createFakeVolumeMetadata(volumeInfo);
  const volume =
      convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata);
  store.dispatch(addVolume({
    volumeInfo,
    volumeMetadata,
  }));

  // Expect the newly added volume is in the store.
  const wantNewVol = {
    allEntries: {
      [myFilesVolumeEntry.toURL()]: fileData,
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
 * @param {()=>void} done
 */
export async function testCommandsForNonInteractiveVolumeAndNoEntries(done) {
  const nonInteractiveVolumeInfo =
      await createAndAddNonInteractiveDownloadsVolume();

  const currentSelection = {
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
      getCurrentRootType: () => VolumeManagerCommon.RootType.DOWNLOADS,
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
  };

  // Check each command is disabled and hidden.
  const commandNames = [
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
    const event = {
      canExecute: true,
      target: {
        parentElement: {
          contextElement: null,
        },
      },
      command: {
        hidden: false,
        // @ts-ignore: error TS7006: Parameter 'hidden' implicitly has an 'any'
        // type.
        setHidden: (hidden) => {
          event.command.hidden = hidden;
        },
        id: commandName,
      },
    };

    // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
    // target: { parentElement: { contextElement: null; }; }; command: { hidden:
    // boolean; setHidden: (hidden: any) => void; id: string; }; }' is not
    // assignable to parameter of type 'Event'.
    command.canExecute(event, fileManager);
    assertFalse(event.canExecute);
    assertTrue(event.command.hidden);
  }

  done();
}

/**
 * Tests that the paste, cut, copy, new-folder, delete, move-to-trash,
 * paste-into-folder, rename, extract-all and zip-selection commands should be
 * disabled and hidden for an entry on a non-interactive volume.
 * @param {()=>void} done
 */
export async function testCommandsForEntriesOnNonInteractiveVolume(done) {
  // Create non-interactive volume.
  const nonInteractiveVolumeInfo =
      await createAndAddNonInteractiveDownloadsVolume();

  // Mock volume manager.
  const volumeManager = new MockVolumeManager();

  // Create file entry on non-interactive volume.
  const nonInteractiveVolumeEntry =
      MockDirectoryEntry.create(nonInteractiveVolumeInfo.fileSystem, 'abc.pdf');
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
    },
    volumeManager: volumeManager,
  };

  // Check each command is disabled and hidden.
  const commandNames = [
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
    const event = {
      canExecute: true,
      target: {
        entry: nonInteractiveVolumeEntry,
      },
      command: {
        hidden: false,
        // @ts-ignore: error TS7006: Parameter 'hidden' implicitly has an 'any'
        // type.
        setHidden: (hidden) => {
          event.command.hidden = hidden;
        },
        id: commandName,
      },
    };

    // @ts-ignore: error TS2345: Argument of type '{ canExecute: boolean;
    // target: { entry: FileSystemDirectoryEntry; }; command: { hidden: boolean;
    // setHidden: (hidden: any) => void; id: string; }; }' is not assignable to
    // parameter of type 'Event'.
    command.canExecute(event, fileManager);
    assertFalse(event.canExecute);
    assertTrue(event.command.hidden);
  }

  done();
}
