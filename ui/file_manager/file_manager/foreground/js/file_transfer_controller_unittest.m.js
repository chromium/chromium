// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {Command} from 'chrome://resources/js/cr/ui/command.m.js';
import {ListSelectionModel} from 'chrome://resources/js/cr/ui/list_selection_model.m.js';
import {queryRequiredElement} from 'chrome://resources/js/util.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://test/chai_assert.js';
import {installMockChrome} from '../../../base/js/mock_chrome.m.js';
import {VolumeManagerCommon} from '../../../base/js/volume_manager_types.m.js';
import {FileOperationManager} from '../../../externs/background/file_operation_manager.m.js';
import {importerHistoryInterfaces} from '../../../externs/background/import_history.m.js';
import {ProgressCenter} from '../../../externs/background/progress_center.m.js';
import {VolumeManager} from '../../../externs/volume_manager.m.js';
import {MockVolumeManager} from '../../background/js/mock_volume_manager.m.js';
import {MockDirectoryEntry, MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.m.js';
import {DialogType} from './dialog_type.m.js';
import {FakeFileSelectionHandler} from './fake_file_selection_handler.m.js';
import {FileListModel} from './file_list_model.m.js';
import {FileSelectionHandler} from './file_selection.m.js';
import {FileTransferController} from './file_transfer_controller.m.js';
import {MockMetadataModel} from './metadata/mock_metadata.m.js';
import {ThumbnailModel} from './metadata/thumbnail_model.m.js';
import {createFakeDirectoryModel} from './mock_directory_model.m.js';
import {A11yAnnounce} from './ui/a11y_announce.m.js';
import {DirectoryTree} from './ui/directory_tree.m.js';
import {FileGrid} from './ui/file_grid.m.js';
import {FileTable} from './ui/file_table.m.js';
import {ListContainer} from './ui/list_container.m.js';

/** @type {!ListContainer} */
let listContainer;

/** @type {!FileTransferController} */
let fileTransferController;

/** @type {!DirectoryTree} */
let directoryTree;

/** @type {!FileSelectionHandler} */
let selectionHandler;

/** @type {!VolumeManager} */
let volumeManager;
/**
 * Mock chrome APIs.
 * @type {!Object}
 */
let mockChrome;

export function setUp() {
  // Setup page DOM.
  document.body.innerHTML = [
    '<style>',
    '  .hide {',
    '    display: none;',
    '  }',
    '</style>',
    '<command id="cut">',
    '<command id="copy">',
    '<div class="dialog-container">',
    '  <div tabindex="0" id="directory-tree">',
    '  </div>',
    '  <div id="list-container">',
    '    <files-spinner class="loading-indicator" hidden></files-spinner>',
    '    <div id="detail-table">',
    '      <list id="file-list" contextmenu="#file-context-menu" tabindex="0">',
    '      </list>',
    '    </div>',
    '    <grid id="file-grid" contextmenu="#file-context-menu" ',
    '          tabindex="0" hidden>',
    '    </grid>',
    '    <paper-progress class="loading-indicator" hidden></paper-progress>',
    '  </div>',
    '  <div id="dialog">',
    '  </div>',
    '  <div id="test-elements">',
    '    <input type="text" id="free-text">',
    '    <cr-input id="test-input" tabindex="0"></cr-input>',
    '    <input type="button" id="button">',
    '</div>',
  ].join('');

  // Mock LoadTimeData strings.
  window.loadTimeData.getString = id => id;
  window.loadTimeData.getBoolean = id => false;
  window.loadTimeData.data = {};

  // Mock chome APIs.
  mockChrome = {
    fileManagerPrivate: {
      enableExternalFileScheme: () => {},
      getProfiles: (callback) => {
        setTimeout(callback, 0, [], '', '');
      },
      grantAccess: (entryURLs, callback) => {
        setTimeout(callback, 0);
      },
    },
  };
  installMockChrome(mockChrome);

  // Initialize Command with the <command>s.
  decorate('command', Command);

  // Fake confirmation callback.
  const confirmationDialog = (isMove, messages) => Promise.resolve(true);

  // Fake ProgressCenter;
  const progressCenter = /** @type {!ProgressCenter} */ ({});

  // Fake FileOperationManager.
  const fileOperationManager = /** @type {!FileOperationManager} */ ({});

  // Fake MetadataModel.
  const metadataModel = new MockMetadataModel({});

  // Fake ThumbnailModel.
  const thumbnailModel = /** @type {!ThumbnailModel} */ ({});

  // Fake DirectoryModel.
  const directoryModel = createFakeDirectoryModel();

  // Create fake VolumeManager and install webkitResolveLocalFileSystemURL.
  volumeManager = new MockVolumeManager();
  window.webkitResolveLocalFileSystemURL =
      MockVolumeManager.resolveLocalFileSystemURL.bind(null, volumeManager);


  // Fake FileSelectionHandler.
  selectionHandler = new FakeFileSelectionHandler();

  // Fake HistoryLoader.
  const historyLoader =
      /** @type {!importerHistoryInterfaces.HistoryLoader} */ ({
        getHistory: () => {
          return Promise.resolve();
        },
      });

  // Fake A11yAnnounce.
  const a11Messages = [];
  const a11y = /** @type {!A11yAnnounce} */ ({
    speakA11yMessage: (text) => {
      a11Messages.push(text);
    },
  });

  // Setup FileTable.
  const table =
      /** @type {!FileTable} */ (queryRequiredElement('#detail-table'));
  FileTable.decorate(
      table, metadataModel, volumeManager, historyLoader, a11y,
      true /* fullPage */);
  const dataModel = new FileListModel(metadataModel);
  table.list.dataModel = dataModel;

  // Setup FileGrid.
  const grid = /** @type {!FileGrid} */ (queryRequiredElement('#file-grid'));
  FileGrid.decorate(grid, metadataModel, volumeManager, historyLoader, a11y);

  // Setup the ListContainer and its dependencies
  listContainer = new ListContainer(
      queryRequiredElement('#list-container'), table, grid,
      DialogType.FULL_PAGE);
  listContainer.dataModel = dataModel;
  listContainer.selectionModel = new ListSelectionModel();
  listContainer.setCurrentListType(ListContainer.ListType.DETAIL);

  // Setup DirectoryTree elements.
  directoryTree =
      /** @type {!DirectoryTree} */ (queryRequiredElement('#directory-tree'));

  // Initialize FileTransferController.
  fileTransferController = new FileTransferController(
      document,
      listContainer,
      directoryTree,
      confirmationDialog,
      progressCenter,
      fileOperationManager,
      metadataModel,
      thumbnailModel,
      directoryModel,
      volumeManager,
      selectionHandler,
  );
}

/**
 * Tests isDocumentWideEvent_.
 *
 * @suppress {accessControls} To be able to access private method
 * isDocumentWideEvent_
 */
export function testIsDocumentWideEvent() {
  const input = document.querySelector('#free-text');
  const crInput = document.querySelector('#test-input');
  const button = document.querySelector('#button');

  // Should return true when body is focused.
  document.body.focus();
  assertEquals(document.body, document.activeElement);
  assertTrue(fileTransferController.isDocumentWideEvent_());

  // Should return true when button is focused.
  button.focus();
  assertEquals(button, document.activeElement);
  assertTrue(fileTransferController.isDocumentWideEvent_());

  // Should return true when tree is focused.
  directoryTree.focus();
  assertEquals(directoryTree, document.activeElement);
  assertTrue(fileTransferController.isDocumentWideEvent_());

  // Should return true when FileList is focused.
  listContainer.focus();
  assertEquals(listContainer.table.list, document.activeElement);
  assertTrue(fileTransferController.isDocumentWideEvent_());

  // Should return true when document is focused.
  input.focus();
  assertEquals(input, document.activeElement);
  assertFalse(fileTransferController.isDocumentWideEvent_());

  // Should return true when document is focused.
  crInput.focus();
  assertEquals(crInput, document.activeElement);
  assertFalse(fileTransferController.isDocumentWideEvent_());
}

/**
 * Tests canCutOrDrag() respects non-modifiable entries like Downloads.
 */
export function testCanMoveDownloads() {
  // Item 1 of the volume info list should be Downloads volume type.
  assertEquals(
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      volumeManager.volumeInfoList.item(1).volumeType);

  // Create a downloads folder inside the item.
  const myFilesVolume = volumeManager.volumeInfoList.item(1);
  const myFilesMockFs =
      /** @type {!MockFileSystem} */ (myFilesVolume.fileSystem);

  myFilesMockFs.populate([
    '/Downloads/',
    '/otherFolder/',
  ]);
  const downloadsEntry = myFilesMockFs.entries['/Downloads'];
  const otherFolderEntry = myFilesMockFs.entries['/otherFolder'];

  assertTrue(!!downloadsEntry);
  assertTrue(!!otherFolderEntry);

  selectionHandler =
      /** @type {!FakeFileSelectionHandler} */ (selectionHandler);

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
export async function testPreparePaste(done) {
  const myFilesVolume = volumeManager.volumeInfoList.item(1);
  const myFilesMockFs =
      /** @type {!MockFileSystem} */ (myFilesVolume.fileSystem);
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
  const otherDataTransfer = /** @type {!DataTransfer} */ ({
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
  });
  const otherPastePlan =
      fileTransferController.preparePaste(otherDataTransfer, testDir);
  assertEquals(otherPastePlan.sourceURLs.length, 0);
  assertEquals(otherPastePlan.sourceEntries.length, 1);
  assertEquals(otherPastePlan.sourceEntries[0], otherFile);
  await otherPastePlan.resolveEntries();
  assertEquals(otherPastePlan.sourceURLs.length, 0);
  assertEquals(otherPastePlan.sourceEntries.length, 1);
  assertEquals(otherPastePlan.sourceEntries[0], otherFile);

  done();
}
