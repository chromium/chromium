// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import type {ProgressCenter} from '../../background/js/progress_center.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {crInjectTypeAndInit} from '../../common/js/cr_ui.js';
import {entriesToURLs} from '../../common/js/entry_utils.js';
import {MockDirectoryEntry, MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';
import {cacheEntries} from '../../state/ducks/all_entries.js';
import {setUpFileManagerOnWindow, setupStore} from '../../state/for_tests.js';
import {DialogType} from '../../state/state.js';
import {getFileData} from '../../state/store.js';
import type {XfTree} from '../../widgets/xf_tree.js';
import type {FilesToast} from '../elements/files_toast.js';

import {FakeFileSelectionHandler} from './fake_file_selection_handler.js';
import {FileListModel} from './file_list_model.js';
import type {FileSelectionHandler} from './file_selection.js';
import {deduplicatePath, DRAG_AND_DROP_GLOBAL_DATA, ENCRYPTED, FileTransferController, MISSING_FILE_CONTENTS, resolvePath, SOURCE_ROOT_URL, SOURCE_URLS, writeFile} from './file_transfer_controller.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';
import {createFakeDirectoryModel} from './mock_directory_model.js';
import type {A11yAnnounce} from './ui/a11y_announce.js';
import {Command} from './ui/command.js';
import {FileGrid} from './ui/file_grid.js';
import {FileListSelectionModel} from './ui/file_list_selection_model.js';
import {FileTable} from './ui/file_table.js';
import {ListContainer, ListType} from './ui/list_container.js';

class TestFileTransferController extends FileTransferController {
  isDocumentWideEvent() {
    return super.isDocumentWideEvent_();
  }

  isDropTargetAllowed(destinationEntryURL: string) {
    return super.isDropTargetAllowed_(destinationEntryURL);
  }
}

let listContainer: ListContainer;
let fileTransferController: TestFileTransferController;
let directoryTree: XfTree;
let selectionHandler: FakeFileSelectionHandler;
let volumeManager: VolumeManager;

export function setUp() {
  setUpFileManagerOnWindow();

  // Setup page DOM.
  document.body.innerHTML = getTrustedHTML`
    <style>
    .hide {
      display: none;
    }
    </style>
    <command id="cut">
    <command id="copy">
    <div class="dialog-container">
      <div tabindex="0" id="directory-tree">
      </div>
      <div id="list-container">
        <files-spinner class="loading-indicator" hidden></files-spinner>
        <div id="detail-table">
          <list id="file-list" contextmenu="#file-context-menu" tabindex="0">
          </list>
        </div>
        <grid id="file-grid" contextmenu="#file-context-menu"
          tabindex="0" hidden>
        </grid>
        <paper-progress class="loading-indicator" hidden></paper-progress>
      </div>
      <div id="dialog">
      </div>
      <div id="test-elements">
        <input type="text" id="free-text">
        <cr-input id="test-input"></cr-input>
        <input type="button" id="button">
    </div>
  `;

  // Initialize Command with the <command>s.
  for (const command of document.querySelectorAll<Command>('command')) {
    crInjectTypeAndInit(command, Command);
  }
  // Fake confirmation callback.
  const confirmationDialog = () => Promise.resolve(true);

  // Fake ProgressCenter;
  const progressCenter = {} as unknown as ProgressCenter;

  // Fake MetadataModel.
  const metadataModel = new MockMetadataModel({}) as unknown as MetadataModel;

  // Fake DirectoryModel.
  const directoryModel = createFakeDirectoryModel();

  // Create fake VolumeManager and install webkitResolveLocalFileSystemURL.
  volumeManager = window.fileManager.volumeManager;
  window.webkitResolveLocalFileSystemURL =
      MockVolumeManager.resolveLocalFileSystemUrl.bind(null, volumeManager);


  // Fake FileSelectionHandler.
  selectionHandler = new FakeFileSelectionHandler();

  // Fake A11yAnnounce.
  const a11Messages = [];
  const a11y = {
    speakA11yMessage: (text: string) => {
      a11Messages.push(text);
    },
  } as A11yAnnounce;

  // Setup FileTable.
  const table = document.querySelector<FileTable>('#detail-table')!;
  FileTable.decorate(
      table as unknown as HTMLElement, metadataModel, volumeManager, a11y,
      true /* fullPage */);
  const dataModel = new FileListModel(metadataModel);
  table.list.dataModel = dataModel;

  // Setup FileGrid.
  const grid = document.querySelector<FileGrid>('#file-grid')!;
  FileGrid.decorate(grid, metadataModel, volumeManager, a11y);

  // Setup the ListContainer and its dependencies
  listContainer = new ListContainer(
      document.querySelector<HTMLElement>('#list-container')!, table, grid,
      DialogType.FULL_PAGE);
  listContainer.dataModel = dataModel;
  listContainer.selectionModel = new FileListSelectionModel();
  listContainer.setCurrentListType(ListType.DETAIL);

  // Setup DirectoryTree elements.
  directoryTree = document.querySelector<XfTree>('#directory-tree')!;

  const filesToast = document.querySelector<FilesToast>('files-toast')!;

  // Initialize FileTransferController.
  fileTransferController = new TestFileTransferController(
      document,
      listContainer,
      directoryTree,
      confirmationDialog,
      progressCenter,
      metadataModel,
      directoryModel,
      volumeManager,
      selectionHandler as unknown as FileSelectionHandler,
      filesToast,
  );
}

/**
 * Tests isDocumentWideEvent_.
 */
export function testIsDocumentWideEvent() {
  const input = document.querySelector<HTMLInputElement>('#free-text')!;
  const crInput = document.querySelector<CrInputElement>('#test-input')!;
  const button = document.querySelector<HTMLInputElement>('#button')!;

  // Should return true when body is focused.
  document.body.focus();
  assertEquals(document.body, document.activeElement);
  assertTrue(fileTransferController.isDocumentWideEvent());

  // Should return true when button is focused.
  button.focus();
  assertEquals(button, document.activeElement);
  assertTrue(fileTransferController.isDocumentWideEvent());

  // Should return true when tree is focused.
  directoryTree.focus();
  assertEquals(directoryTree, document.activeElement);
  assertTrue(fileTransferController.isDocumentWideEvent());

  // Should return true when FileList is focused.
  listContainer.focus();
  assertEquals(listContainer.table.list, document.activeElement);
  assertTrue(fileTransferController.isDocumentWideEvent());

  // Should return true when document is focused.
  input.focus();
  assertEquals(input, document.activeElement);
  assertFalse(fileTransferController.isDocumentWideEvent());

  // Should return true when document is focused.
  crInput.focus();
  assertEquals(crInput, document.activeElement);
  assertEquals(crInput.inputElement, crInput.shadowRoot!.activeElement);
  assertFalse(fileTransferController.isDocumentWideEvent());
}

/**
 * Tests canCutOrDrag() respects non-modifiable entries like Downloads.
 */
export function testCanMoveDownloads() {
  // Item 1 of the volume info list should be Downloads volume type.
  assertEquals(
      VolumeType.DOWNLOADS, volumeManager.volumeInfoList.item(1).volumeType);

  // Create a downloads folder inside the item.
  const myFilesVolume = volumeManager.volumeInfoList.item(1);
  const myFilesMockFs = myFilesVolume.fileSystem as MockFileSystem;

  myFilesMockFs.populate([
    '/Downloads/',
    '/otherFolder/',
  ]);
  const downloadsEntry = myFilesMockFs.entries['/Downloads']!;
  const otherFolderEntry = myFilesMockFs.entries['/otherFolder']!;

  assertTrue(!!downloadsEntry);
  assertTrue(!!otherFolderEntry);

  // Downloads can't be cut.
  selectionHandler.updateSelection([downloadsEntry], []);
  assertFalse(fileTransferController.canCutOrDrag());

  // otherFolder can be cut.
  selectionHandler.updateSelection([otherFolderEntry], []);
  assertTrue(fileTransferController.canCutOrDrag());
}

/**
 * Tests preparePaste() with FilesApp fs/sources and standard DataTransfer.
 */
export async function testPreparePaste(done: () => void) {
  const myFilesVolume = volumeManager.volumeInfoList.item(1);
  const myFilesMockFs = myFilesVolume.fileSystem as MockFileSystem;
  myFilesMockFs.populate(['/testfile.txt', '/testdir/']);
  const testFile = MockFileEntry.create(myFilesMockFs, '/testfile.txt');
  const testDir = MockDirectoryEntry.create(myFilesMockFs, '/testdir');

  // FilesApp internal drag and drop should populate sourceURLs at first, and
  // only populate sourceEntries after calling resolveEntries().
  const filesAppDataTransfer = new DataTransfer();
  filesAppDataTransfer.setData('fs/sources', testFile.toURL());
  const filesAppPastePlan =
      fileTransferController.preparePaste(filesAppDataTransfer, testDir);
  assertEquals(filesAppPastePlan.sourceURLs.length, 1);
  assertEquals(filesAppPastePlan.sourceEntries.length, 0);
  await filesAppPastePlan.resolveEntries();
  assertEquals(filesAppPastePlan.sourceEntries.length, 1);
  assertEquals(filesAppPastePlan.sourceEntries[0], testFile);

  // Drag and drop from other apps will use DataTransfer.item with
  // item.kind === 'file', and use webkitGetAsEntry() to populate sourceEntries.
  const otherMockFs = new MockFileSystem('not-filesapp');
  const otherFile = MockFileEntry.create(otherMockFs, '/otherfile.txt');
  const otherDataTransfer = {
    effectAllowed: 'copy',
    getData: () => {
      return '';
    },
    items: [{
      kind: 'file',
      webkitGetAsEntry: () => {
        return otherFile;
      },
    }],
  } as unknown as DataTransfer;
  const otherPastePlan =
      fileTransferController.preparePaste(otherDataTransfer, testDir);
  assertEquals(otherPastePlan.sourceURLs.length, 0);
  assertEquals(otherPastePlan.sourceEntries.length, 1);
  assertEquals(otherPastePlan.sourceEntries[0], otherFile);
  await otherPastePlan.resolveEntries();
  assertEquals(otherPastePlan.sourceURLs.length, 0);
  assertEquals(otherPastePlan.sourceEntries.length, 1);
  assertEquals(otherPastePlan.sourceEntries[0], otherFile);

  // Drag and drop browser file will use DataTransfer.item with
  // item.kind === 'file', but webkitGetAsEntry() will not resolve the file.
  const fakeWriteFile: typeof writeFile = async (file, dir) => {
    return MockFileEntry.create(
        myFilesMockFs, `${dir.fullPath}/${file.name}`, undefined, file);
  };

  const browserFileDataTransfer = new DataTransfer();
  browserFileDataTransfer.items.add(
      new File(['content'], 'browserfile', {type: 'text/plain'}));
  const browserFilePastePlan = fileTransferController.preparePaste(
      browserFileDataTransfer, testDir, undefined, fakeWriteFile);
  // sourceURLs and sourceEntries should not be populated from File instances.
  assertEquals(browserFilePastePlan.sourceURLs.length, 0);
  assertEquals(browserFilePastePlan.sourceEntries.length, 0);

  // File instances should still be copied to target folder.
  const writtenEntry =
      myFilesMockFs.entries['/testdir/browserfile']! as MockFileEntry;
  assertEquals('content', await writtenEntry.content.text());

  done();
}

/**
 * Tests the drag-and-drop's `isDropTargetAllowed_` utility function, which
 * relies on the local storage that mirrors the data stored in the clipboard,
 * and on the "disabled" state of the target entry in the store.
 *
 * Note: Setting the drop target used to infer the dragged entries from the
 * selection handler, which is not valid when entries are dragged from one Files
 * window to another.
 */
export async function testDropTargetAllowed(done: () => void) {
  // Item 1 of the volume info list should be Downloads volume type.
  assertEquals(
      VolumeType.DOWNLOADS, volumeManager.volumeInfoList.item(1).volumeType);
  const myFilesVolume = volumeManager.volumeInfoList.item(1);
  const myFilesMockFs = myFilesVolume.fileSystem as MockFileSystem;

  // Create entries under myFiles and cache them in the store.
  myFilesMockFs.populate(
      ['/file.txt', '/dir/', '/dir/file2.txt', '/dir/dir2/']);
  const file = myFilesMockFs.entries['/file.txt'] as FileEntry;
  const dir = myFilesMockFs.entries['/dir'] as DirectoryEntry;
  const file2 = myFilesMockFs.entries['/dir/file2.txt'] as FileEntry;
  const dir2 = myFilesMockFs.entries['/dir/dir2'] as FileEntry;
  const store = setupStore();
  cacheEntries(store.getState(), [file, dir, file2]);

  // Update the local storage to simulate dragging the "/dir" entry.
  const storage = window.localStorage;
  storage.setItem(
      `${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_URLS}`,
      entriesToURLs([dir]).join('\n'));
  storage.setItem(
      `${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_ROOT_URL}`, myFilesMockFs.rootURL);
  storage.setItem(
      `${DRAG_AND_DROP_GLOBAL_DATA}.${MISSING_FILE_CONTENTS}`, 'false');
  storage.setItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${ENCRYPTED}`, 'false');

  // "/dir" is not a valid target for itself.
  assertFalse(fileTransferController.isDropTargetAllowed(dir.toURL()));

  // Check that "/dir" is a valid target for "/file.txt".
  storage.setItem(
      `${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_URLS}`,
      entriesToURLs([file]).join('\n'));
  assertTrue(fileTransferController.isDropTargetAllowed(dir.toURL()));

  // Check that "/dir" is a valid target for "/dir/file2.txt".
  storage.setItem(
      `${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_URLS}`,
      entriesToURLs([file2]).join('\n'));
  assertTrue(fileTransferController.isDropTargetAllowed(dir.toURL()));

  // Check that "/dir/dir2" is not a valid target for "/dir".
  storage.setItem(
      `${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_URLS}`,
      entriesToURLs([dir]).join('\n'));
  assertFalse(fileTransferController.isDropTargetAllowed(dir2.toURL()));

  // Disable the directory "/dir" in the store and check that it can't be used
  // as a drop target anymore for "/file.txt".
  const dirFileData = getFileData(store.getState(), dir.toURL())!;
  dirFileData.disabled = true;
  storage.setItem(
      `${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_URLS}`,
      entriesToURLs([file]).join('\n'));
  assertFalse(fileTransferController.isDropTargetAllowed(dir.toURL()));

  done();
}

/**
 * Size of directory.
 */
const DIRECTORY_SIZE = -1;

/**
 * Creates test file system.
 * @param id File system Id.
 * @param entries Map of entry paths and their size.
 *     If the entry size is DIRECTORY_SIZE, the entry is a directory.
 */
function createTestFileSystem(
    id: string, entries: Record<string, number>): MockFileSystem {
  const fileSystem = new MockFileSystem(id, 'filesystem:' + id);
  for (const path in entries) {
    if (entries[path] === DIRECTORY_SIZE) {
      fileSystem.entries[path] = MockDirectoryEntry.create(fileSystem, path);
    } else {
      const metadata:
          Metadata = {size: entries[path]!, modificationTime: new Date()};
      fileSystem.entries[path] =
          MockFileEntry.create(fileSystem, path, metadata);
    }
  }
  return fileSystem;
}

/**
 * Tests the resolvePath() function.
 */
export async function testResolvePath(done: VoidCallback) {
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file': 10,
    '/directory': DIRECTORY_SIZE,
  });
  const root = fileSystem.root;
  const resolvedRoot = await resolvePath(root, '/');
  assertEquals(fileSystem.entries['/'], resolvedRoot);

  const resolvedFile = await resolvePath(root, '/file');
  assertEquals(fileSystem.entries['/file'], resolvedFile);

  const resolvedDirectory = await resolvePath(root, '/directory');
  assertEquals(fileSystem.entries['/directory'], resolvedDirectory);

  try {
    await resolvePath(root, '/not_found');
    assertNotReached('The NOT_FOUND error is not reported.');
  } catch (error: unknown) {
    assertEquals('NotFoundError', (error as any).name);
  }

  done();
}

/**
 * Tests the deduplicatePath() function.
 */
export async function testDeduplicatePath(done: VoidCallback) {
  const fileSystem1 = createTestFileSystem('testVolume', {'/': DIRECTORY_SIZE});
  const fileSystem2 = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file.txt': 10,
  });
  const fileSystem3 = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file.txt': 10,
    '/file (1).txt': 10,
    '/file (2).txt': 10,
    '/file (3).txt': 10,
    '/file (4).txt': 10,
    '/file (5).txt': 10,
    '/file (6).txt': 10,
    '/file (7).txt': 10,
    '/file (8).txt': 10,
    '/file (9).txt': 10,
  });

  const nonExistingPath = await deduplicatePath(fileSystem1.root, 'file.txt');
  assertEquals('file.txt', nonExistingPath);

  const existingPath = await deduplicatePath(fileSystem2.root, 'file.txt');
  assertEquals('file (1).txt', existingPath);

  const moreExistingPath = await deduplicatePath(fileSystem3.root, 'file.txt');
  assertEquals('file (10).txt', moreExistingPath);

  done();
}

/**
 * Test writeFile() with file dragged from browser.
 */
export async function testWriteFile(done: VoidCallback) {
  const fileSystem = createTestFileSystem('testVolume', {
    '/testdir': DIRECTORY_SIZE,
  });

  const file = new File(['content'], 'browserfile', {type: 'text/plain'});
  await writeFile(file, fileSystem.entries['/testdir'] as DirectoryEntry);
  const writtenEntry =
      fileSystem.entries['/testdir/browserfile'] as MockFileEntry;
  assertEquals('content', await writtenEntry.content.text());
  done();
}
