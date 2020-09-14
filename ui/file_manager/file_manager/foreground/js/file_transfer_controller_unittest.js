// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

function setUp() {
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
    '    <cr-input id="test-input" tabindex="0">',
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
      }
    },
  };
  installMockChrome(mockChrome);

  // Initialize cr.ui.Command with the <command>s.
  cr.ui.decorate('command', cr.ui.Command);

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

  // Fake VolumeManager.
  volumeManager = new MockVolumeManager();

  // Fake FileSelectionHandler.
  selectionHandler = new FakeFileSelectionHandler();

  // Fake HistoryLoader.
  const historyLoader = /** @type {!importer.HistoryLoader} */ ({
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
  table.list = document.querySelector('#file-list');
  const dataModel = new FileListModel(metadataModel);
  table.list.dataModel = dataModel;

  // Setup FileGrid.
  const grid = /** @type {!FileGrid} */ (queryRequiredElement('#file-grid'));
  FileGrid.decorate(grid, metadataModel, volumeManager, historyLoader, a11y);

  // Setup the ListContainer and its dependencies
  listContainer =
      new ListContainer(queryRequiredElement('#list-container'), table, grid);
  listContainer.dataModel = dataModel;
  listContainer.selectionModel = new cr.ui.ListSelectionModel();
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
function testIsDocumentWideEvent() {
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
function testCanMoveDownloads() {
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
