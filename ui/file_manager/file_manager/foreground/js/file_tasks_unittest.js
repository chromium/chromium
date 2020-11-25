// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Utility function that appends value under a given name in the store.
 * @param {!Map<string, !Array<string|number>>} store
 * @param {string} name
 * @param {string|number} value
 */
function record(store, name, value) {
  let recorded = store.get(name);
  if (!recorded) {
    recorded = [];
    store.set(name, recorded);
  }
  recorded.push(value);
}

/**
 * A map from histogram name to all enums recorded for it.
 */
const enumMap = new Map();

/**
 * A map from histogram name to all counts recorded for it.
 */
const countMap = new Map();

/**
 * Mock metrics
 * @type {!Object}
 */
window.metrics = {
  recordEnum: (name, value, valid) => {
    assertTrue(valid.includes(value));
    record(enumMap, name, value);
  },
  recordSmallCount: (name, value) => {
    record(countMap, name, value);
  },
};

/**
 * Mock chrome APIs.
 * @type {!Object}
 */
let mockChrome;

/**
 * Mock task history.
 * @type {!TaskHistory}
 */
const mockTaskHistory = /** @type {!TaskHistory} */ ({
  getLastExecutedTime: function(id) {
    return 0;
  },
  recordTaskExecuted: function(id) {},
});

/**
 * Mock file transfer controller.
 * @type {!FileTransferController}
 */
const mockFileTransferController = /** @type {!FileTransferController} */ ({});

/**
 * Mock directory change tracker.
 * @type {!Object}
 */
const fakeTracker = {
  hasChange: false,
};

/**
 * Fake url for mounted ZIP file.
 * @type {string}
 */
const fakeMountedZipUrl =
    'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
    'external/fakePath/test.zip';

/**
 * Panel IDs for ZIP mount operations.
 * @type {string}
 */
const zipMountPanelId = 'Mounting: ' + fakeMountedZipUrl;

/**
 * Panel IDs for ZIP mount errors.
 * @type {string}
 */
const errorZipMountPanelId = 'Cannot mount: ' + fakeMountedZipUrl;

// Set up test components.
function setUp() {
  // Mock LoadTimeData strings.
  window.loadTimeData.getString = id => id;
  window.loadTimeData.getBoolean = key => false;

  const mockTask = /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
    taskId: 'handler-extension-id|app|any',
    isDefault: false,
    isGenericFileHandler: true,
  });

  // Mock chome APIs.
  mockChrome = {
    fileManagerPrivate: {
      DriveConnectionStateType: {
        ONLINE: 'ONLINE',
        OFFLINE: 'OFFLINE',
        METERED: 'METERED',
      },
      DriveOfflineReason: {
        NOT_READY: 'NOT_READY',
        NO_NETWORK: 'NO_NETWORK',
        NO_SERVICE: 'NO_SERVICE',
      },
      Verb: {
        SHARE_WITH: 'share_with',
      },
      TaskResult: {
        MESSAGE_SENT: 'test_ms_task',
        FAILED_PLUGIN_VM_DIRECTORY_NOT_SHARED: 'test_fpvdns_task',
      },
      getFileTasks: function(entries, callback) {
        setTimeout(callback.bind(null, [mockTask]), 0);
      },
      executeTask: function(taskId, entries, onViewFiles) {
        onViewFiles('failed');
      },
      sharePathsWithCrostini: function(vmName, entries, persist, callback) {
        callback();
      },
    },
    runtime: {
      id: 'test-extension-id',
    },
  };

  installMockChrome(mockChrome);
  enumMap.clear();
  countMap.clear();
}

/**
 * Fail with an error message.
 * @param {string} message The error message.
 * @param {string=} opt_details Optional details.
 */
function failWithMessage(message, opt_details) {
  if (opt_details) {
    message += ': '.concat(opt_details);
  }
  throw new Error(message);
}

/**
 * Returns mocked file manager components.
 * @return {!Object}
 */
function getMockFileManager() {
  const crostini = createCrostiniForTest();

  const fileManager = {
    volumeManager: /** @type {!VolumeManager} */ ({
      getLocationInfo: function(entry) {
        return {
          rootType: VolumeManagerCommon.RootType.DRIVE,
        };
      },
      getDriveConnectionState: function() {
        return chrome.fileManagerPrivate.DriveConnectionStateType;
      },
      getVolumeInfo: function(entry) {
        return {
          volumeType: VolumeManagerCommon.VolumeType.DRIVE,
        };
      },
    }),
    ui: /** @type {!FileManagerUI} */ ({
      alertDialog: {
        showHtml: function(title, text, onOk, onCancel, onShow) {},
      },
      passwordDialog: /** @type {!FilesPasswordDialog} */ ({}),
      speakA11yMessage: (text) => {},
    }),
    metadataModel: /** @type {!MetadataModel} */ ({}),
    namingController: /** @type {!NamingController} */ ({}),
    directoryModel: /** @type {!DirectoryModel} */ ({
      getCurrentRootType: function() {
        return null;
      },
      changeDirectoryEntry: function(displayRoot) {}
    }),
    crostini: crostini,
    progressCenter: /** @type {!ProgressCenter} */ (new MockProgressCenter()),
  };

  fileManager.crostini.initVolumeManager(fileManager.volumeManager);
  return fileManager;
}

/**
 * Returns a promise that resolves when the showHtml method of alert dialog is
 * called with the expected title and text.
 *
 * @param {!Array<!Entry>} entries Entries.
 * @param {string} expectedTitle The expected title.
 * @param {string} expectedText The expected text.
 * @return {!Promise}
 */
function showHtmlOfAlertDialogIsCalled(entries, expectedTitle, expectedText) {
  return new Promise((resolve, reject) => {
    const fileManager = getMockFileManager();
    fileManager.ui.alertDialog.showHtml =
        (title, text, onOk, onCancel, onShow) => {
          assertEquals(expectedTitle, title);
          assertEquals(expectedText, text);
          resolve();
        };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, entries, [null], mockTaskHistory,
            fileManager.namingController, fileManager.crostini,
            fileManager.progressCenter)
        .then(tasks => {
          tasks.executeDefault();
        });
  });
}

/**
 * Returns a promise that resolves when openSuggestAppsDialog is called.
 *
 * @param {!Array<!Entry>} entries Entries.
 * @param {!Array<?string>} mimeTypes Mime types.
 * @return {!Promise}
 */
function openSuggestAppsDialogIsCalled(entries, mimeTypes) {
  return new Promise((resolve, reject) => {
    const fileManager = getMockFileManager();
    fileManager.ui.suggestAppsDialog = {
      showByExtensionAndMime: function(extension, mimeType, onDialogClosed) {
        resolve();
      },
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, entries, mimeTypes, mockTaskHistory,
            fileManager.namingController, fileManager.crostini,
            fileManager.progressCenter)
        .then(tasks => {
          tasks.executeDefault();
        });
  });
}

/**
 * Returns a promise that resolves when the task picker is called.
 *
 * @param {!Array<!Entry>} entries Entries.
 * @param {!Array<?string>} mimeTypes Mime types.
 * @return {!Promise}
 */
function showDefaultTaskDialogCalled(entries, mimeTypes) {
  return new Promise((resolve, reject) => {
    const fileManager = getMockFileManager();
    fileManager.ui.defaultTaskPicker = {
      showDefaultTaskDialog: function(
          title, message, items, defaultIdx, onSuccess) {
        resolve();
      },
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, entries, mimeTypes, mockTaskHistory,
            fileManager.namingController, fileManager.crostini,
            fileManager.progressCenter)
        .then(tasks => {
          tasks.executeDefault();
        });
  });
}

/**
 * Returns a promise that resolves when showImportCrostiniImageDialog is called.
 *
 * @param {!Array<!Entry>} entries Entries.
 * @return {!Promise}
 */
function showImportCrostiniImageDialogIsCalled(entries) {
  return new Promise((resolve, reject) => {
    const fileManager = getMockFileManager();
    fileManager.ui.importCrostiniImageDialog = {
      showImportCrostiniImageDialog: (entry) => {
        resolve();
      },
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, entries, [null], mockTaskHistory,
            fileManager.namingController, fileManager.crostini,
            fileManager.progressCenter)
        .then(tasks => {
          tasks.executeDefault();
        });
  });
}

/**
 * Tests opening a .exe file.
 */
function testToOpenExeFile(callback) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.exe');

  reportPromise(
      showHtmlOfAlertDialogIsCalled(
          [mockEntry], 'test.exe', 'NO_TASK_FOR_EXECUTABLE'),
      callback);
}

/**
 * Tests opening a .dmg file.
 */
function testToOpenDmgFile(callback) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.dmg');

  reportPromise(
      showHtmlOfAlertDialogIsCalled([mockEntry], 'test.dmg', 'NO_TASK_FOR_DMG'),
      callback);
}

/**
 * Tests opening a .crx file.
 */
function testToOpenCrxFile(callback) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.crx');

  reportPromise(
      showHtmlOfAlertDialogIsCalled(
          [mockEntry], 'NO_TASK_FOR_CRX_TITLE', 'NO_TASK_FOR_CRX'),
      callback);
}

/**
 * Tests opening a .rtf file.
 */
function testToOpenRtfFile(callback) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.rtf');

  reportPromise(
      openSuggestAppsDialogIsCalled([mockEntry], ['application/rtf']),
      callback);
}

/**
 * Tests opening an entry that has external metadata type.
 */
function testOpenSuggestAppsDialogWithMetadata(callback) {
  const showByExtensionAndMimeIsCalled = new Promise((resolve, reject) => {
    const mockFileSystem = new MockFileSystem('volumeId');
    const mockEntry = MockFileEntry.create(mockFileSystem, '/test.rtf');
    const fileManager = getMockFileManager();

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, /** @type {!FileManagerUI} */ ({
              taskMenuButton: document.createElement('button'),
              fileContextMenu: {
                defaultActionMenuItem: document.createElement('div'),
              },
              suggestAppsDialog: {
                showByExtensionAndMime: function(
                    extension, mimeType, onDialogClosed) {
                  assertEquals('.rtf', extension);
                  assertEquals('application/rtf', mimeType);
                  resolve();
                },
              },
            }),
            mockFileTransferController, [mockEntry], ['application/rtf'],
            mockTaskHistory, fileManager.namingController, fileManager.crostini,
            fileManager.progressCenter)
        .then(tasks => {
          tasks.openSuggestAppsDialog(() => {}, () => {}, () => {});
        });
  });

  reportPromise(showByExtensionAndMimeIsCalled, callback);
}

/**
 * Tests opening an entry that has no extension. Since the entry extension and
 * entry MIME type are required, the onFalure method should be called.
 */
function testOpenSuggestAppsDialogFailure(callback) {
  const onFailureIsCalled = new Promise((resolve, reject) => {
    const mockFileSystem = new MockFileSystem('volumeId');
    const mockEntry = MockFileEntry.create(mockFileSystem, '/test');
    const fileManager = getMockFileManager();

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, [mockEntry], [null], mockTaskHistory,
            fileManager.namingController, fileManager.crostini,
            fileManager.progressCenter)
        .then(tasks => {
          tasks.openSuggestAppsDialog(() => {}, () => {}, resolve);
        });
  });

  reportPromise(onFailureIsCalled, callback);
}

/**
 * Tests opening the task picker with an entry that does not have a default app
 * but there are multiple apps that could open it.
 */
function testOpenTaskPicker(callback) {
  window.chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback.bind(
            null,
            [
              {
                taskId: 'handler-extension-id1|app|any',
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 1',
              },
              {
                taskId: 'handler-extension-id2|app|any',
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 2',
              },
            ]),
        0);
  };

  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.tiff');

  reportPromise(
      showDefaultTaskDialogCalled([mockEntry], ['image/tiff']), callback);
}

/**
 * Tests opening the task picker with an entry that does not have a default app
 * but there are multiple apps that could open it. The app with the most recent
 * task execution order should execute.
 */
function testOpenWithMostRecentlyExecuted(callback) {
  const latestTaskId = 'handler-extension-most-recently-executed|app|any';
  const oldTaskId = 'handler-extension-executed-before|app|any';

  window.chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback.bind(
            null,
            // File tasks is sorted by last executed time, latest first.
            [
              {
                taskId: latestTaskId,
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 1',
              },
              {
                taskId: oldTaskId,
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 2',
              },
              {
                taskId: 'handler-extension-never-executed|app|any',
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 3',
              },
            ]),
        0);
  };

  const taskHistory = /** @type {!TaskHistory} */ ({
    getLastExecutedTime: function(id) {
      if (id == oldTaskId) {
        return 10000;
      }
      if (id == latestTaskId) {
        return 20000;
      }
      return 0;
    },
    recordTaskExecuted: function(id) {},
  });

  let executedTask = null;
  window.chrome.fileManagerPrivate.executeTask =
      (taskId, entries, onViewFiles) => {
        executedTask = taskId;
        onViewFiles('success');
      };

  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.tiff');

  const promise = new Promise((resolve, reject) => {
    const fileManager = getMockFileManager();
    fileManager.ui.defaultTaskPicker = {
      showDefaultTaskDialog: function(
          title, message, items, defaultIdx, onSuccess) {
        failWithMessage('should not show task picker');
      },
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, [mockEntry], [null], taskHistory,
            fileManager.namingController, fileManager.crostini,
            fileManager.progressCenter)
        .then(tasks => {
          tasks.executeDefault();
          assertEquals(latestTaskId, executedTask);
          resolve();
        });
  });

  reportPromise(promise, callback);
}

/**
 * Tests opening a .zip file.
 */
function testOpenZipWithZipArchiver(callback) {
  const zipArchiverTaskId = 'dmboannefpncccogfdikhmhpmdnddgoe|app|open';

  window.chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback.bind(
            null,
            [
              {
                taskId: zipArchiverTaskId,
                isDefault: false,
                isGenericFileHandler: false,
                title: 'Zip Archiver',
              },
            ]),
        0);
  };

  // None of the tasks has ever been executed.
  const taskHistory = /** @type {!TaskHistory} */ ({
    getLastExecutedTime: function(id) {
      return 0;
    },
    recordTaskExecuted: function(id) {},
  });

  let executedTask = null;
  window.chrome.fileManagerPrivate.executeTask =
      (taskId, entries, onViewFiles) => {
        executedTask = taskId;
        onViewFiles('success');
      };

  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.zip');

  const promise = new Promise((resolve, reject) => {
    const fileManager = getMockFileManager();
    fileManager.ui.defaultTaskPicker = {
      showDefaultTaskDialog: function(
          title, message, items, defaultIdx, onSuccess) {
        failWithMessage('run zip archiver', 'default task picker was shown');
      },
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, [mockEntry], [null], taskHistory,
            fileManager.namingController, fileManager.crostini,
            fileManager.progressCenter)
        .then(tasks => {
          tasks.executeDefault();
          assertEquals(zipArchiverTaskId, executedTask);
          resolve();
        });
  });

  reportPromise(promise, callback);
}

function setUpInstallLinuxPackage() {
  const fileManager = getMockFileManager();
  fileManager.volumeManager.getLocationInfo = entry => {
    return /** @type {!EntryLocation} */ ({
      rootType: VolumeManagerCommon.RootType.CROSTINI,
    });
  };
  const fileTask = {
    taskId: 'test-extension-id|app|install-linux-package',
    isDefault: false,
    isGenericFileHandler: false,
    title: '__MSG_INSTALL_LINUX_PACKAGE__',
  };
  window.chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(callback.bind(null, [fileTask]), 0);
  };
  return fileManager;
}

/**
 * Tests opening a .deb file. The crostini linux package install dialog should
 * be called.
 */
function testOpenInstallLinuxPackageDialog(callback) {
  const fileManager = setUpInstallLinuxPackage();
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.deb');

  const promise = new Promise((resolve, reject) => {
    fileManager.ui.installLinuxPackageDialog = {
      showInstallLinuxPackageDialog: function(entry) {
        resolve();
      },
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, [mockEntry], [null], mockTaskHistory,
            fileManager.namingController, fileManager.crostini,
            fileManager.progressCenter)
        .then(tasks => {
          tasks.executeDefault();
        });
  });

  reportPromise(promise, callback);
}

/**
 * Tests opening a .tini file. The import crostini image dialog should be
 * called.
 */
function testToOpenTiniFileOpensImportCrostiniImageDialog(callback) {
  window.chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback.bind(
            null,
            [
              {
                taskId: 'test-extension-id|app|import-crostini-image',
                isDefault: false,
                isGenericFileHandler: false,
              },
            ]),
        0);
  };

  const mockEntry =
      MockFileEntry.create(new MockFileSystem('testfilesystem'), '/test.tini');

  reportPromise(showImportCrostiniImageDialogIsCalled([mockEntry]), callback);
}

/**
 * Checks that the function that returns a file type for file entry handles
 * correctly identifies files with known and unknown extensions.
 */
function testGetViewFileType() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const testData = [
    {extension: 'log', expected: '.log'},
    {extension: '__unknown_extension__', expected: 'other'},
  ];
  for (const data of testData) {
    const mockEntry =
        MockFileEntry.create(mockFileSystem, `/report.${data.extension}`);
    const type = FileTasks.getViewFileType(mockEntry);
    assertEquals(data.expected, type);
  }
}

/**
 * Checks that we are correctly recording UMA about Share action.
 */
function testRecordSharingAction() {
  // Setup: create a fake metrics object that can be examined for content.
  const mockFileSystem = new MockFileSystem('volumeId');

  // Actual tests.
  FileTasks.recordSharingActionUMA_(
      FileTasks.SharingActionSourceForUMA.CONTEXT_MENU, [
        MockFileEntry.create(mockFileSystem, '/test.log'),
        MockFileEntry.create(mockFileSystem, '/test.doc'),
        MockFileEntry.create(mockFileSystem, '/test.__no_such_extension__'),
      ]);
  assertArrayEquals(
      enumMap.get('Share.ActionSource'),
      [FileTasks.SharingActionSourceForUMA.CONTEXT_MENU]);
  assertArrayEquals(countMap.get('Share.FileCount'), [3]);
  assertArrayEquals(enumMap.get('Share.FileType'), ['.log', '.doc', 'other']);

  FileTasks.recordSharingActionUMA_(
      FileTasks.SharingActionSourceForUMA.SHARE_BUTTON, [
        MockFileEntry.create(mockFileSystem, '/test.log'),
      ]);
  assertArrayEquals(enumMap.get('Share.ActionSource'), [
    FileTasks.SharingActionSourceForUMA.CONTEXT_MENU,
    FileTasks.SharingActionSourceForUMA.SHARE_BUTTON,
  ]);
  assertArrayEquals(countMap.get('Share.FileCount'), [3, 1]);
  assertArrayEquals(
      enumMap.get('Share.FileType'), ['.log', '.doc', 'other', '.log']);
}

/**
 * Checks that file task is correctly recognized as a file sharing task.
 */
function testIsSharingTask() {
  const mockShareTask = /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
    verb: chrome.fileManagerPrivate.Verb.SHARE_WITH,
  });
  assertTrue(FileTasks.isShareTask(mockShareTask));
  const mockPackTask = /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
    verb: '__no_such_verb__',
  });
  assertFalse(FileTasks.isShareTask(mockPackTask));
}

/**
 * Checks that a task sharing files with external apps correctly records
 * UMA statistics.
 */
async function testShareWith(done) {
  const fileManager = getMockFileManager();
  const mockFileSystem = new MockFileSystem('volumeId');
  const entries = [
    MockFileEntry.create(mockFileSystem, '/image1.jpg'),
    MockFileEntry.create(mockFileSystem, '/image2.jpg'),
  ];

  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      entries, ['application/jpg'], mockTaskHistory,
      fileManager.namingController, fileManager.crostini,
      fileManager.progressCenter);

  const mockTask = /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
    taskId: 'com.acme/com.acme.android.PhotosApp|arc|send_multiple',
    isDefault: false,
    verb: chrome.fileManagerPrivate.Verb.SHARE_WITH,
    isGenericFileHandler: true,
  });
  tasks.execute(mockTask);
  assertArrayEquals(['.jpg', '.jpg'], enumMap.get('Share.FileType'));
  assertArrayEquals([2], countMap.get('Share.FileCount'));

  done();
}

/**
 * Checks that the progress center is properly updated when mounting archives
 * successfully.
 * @suppress {visibility}
 */
async function testMountArchiveAndChangeDirectoryNotificationSuccess(done) {
  const fileManager = getMockFileManager();

  // Define FileTasks instance.
  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      [], [null], mockTaskHistory, fileManager.namingController,
      fileManager.crostini, fileManager.progressCenter);

  fileManager.volumeManager.mountArchive = function(url, password) {
    // Check: progressing state.
    assertEquals(
        ProgressItemState.PROGRESSING,
        fileManager.progressCenter.getItemById(zipMountPanelId).state);

    const volumeInfo = {resolveDisplayRoot: () => null};
    return volumeInfo;
  };

  // Mount archive.
  await tasks.mountArchiveAndChangeDirectory_(fakeTracker, fakeMountedZipUrl);

  // Check: mount completed, no error.
  assertEquals(
      ProgressItemState.COMPLETED,
      fileManager.progressCenter.getItemById(zipMountPanelId).state);
  assertEquals(
      undefined, fileManager.progressCenter.getItemById(errorZipMountPanelId));

  done();
}

/**
 * Checks that the progress center is properly updated when mounting an archive
 * resolves with an error.
 * @suppress {visibility}
 */
async function testMountArchiveAndChangeDirectoryNotificationInvalidArchive(
    done) {
  const fileManager = getMockFileManager();

  // Define FileTasks instance.
  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      [], [null], mockTaskHistory, fileManager.namingController,
      fileManager.crostini, fileManager.progressCenter);

  fileManager.volumeManager.mountArchive = function(url, password) {
    return Promise.reject(VolumeManagerCommon.VolumeError.INTERNAL);
  };

  // Mount archive.
  await tasks.mountArchiveAndChangeDirectory_(fakeTracker, fakeMountedZipUrl);

  // Check: mount is completed with an error.
  assertEquals(
      ProgressItemState.COMPLETED,
      fileManager.progressCenter.getItemById(zipMountPanelId).state);
  assertEquals(
      ProgressItemState.ERROR,
      fileManager.progressCenter.getItemById(errorZipMountPanelId).state);

  done();
}

/**
 * Checks that the progress center is properly updated when the password prompt
 * for an encrypted archive is canceled.
 * @suppress {visibility}
 */
async function testMountArchiveAndChangeDirectoryNotificationCancelPassword(
    done) {
  const fileManager = getMockFileManager();

  // Define FileTasks instance.
  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      [], [null], mockTaskHistory, fileManager.namingController,
      fileManager.crostini, fileManager.progressCenter);

  fileManager.volumeManager.mountArchive = function(url, password) {
    return Promise.reject(VolumeManagerCommon.VolumeError.NEED_PASSWORD);
  };

  fileManager.ui.passwordDialog.askForPassword =
      async function(filename, password = null) {
    return Promise.reject(FilesPasswordDialog.USER_CANCELLED);
  };

  // Mount archive.
  await tasks.mountArchiveAndChangeDirectory_(fakeTracker, fakeMountedZipUrl);

  // Check: mount is completed, no error since the user canceled the password
  // prompt.
  assertEquals(
      ProgressItemState.COMPLETED,
      fileManager.progressCenter.getItemById(zipMountPanelId).state);
  assertEquals(
      undefined, fileManager.progressCenter.getItemById(errorZipMountPanelId));

  done();
}

/**
 * Checks that the progress center is properly updated when mounting an
 * encrypted archive.
 * @suppress {visibility}
 */
async function testMountArchiveAndChangeDirectoryNotificationEncryptedArchive(
    done) {
  const fileManager = getMockFileManager();

  // Define FileTasks instance.
  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      [], [null], mockTaskHistory, fileManager.namingController,
      fileManager.crostini, fileManager.progressCenter);

  fileManager.volumeManager.mountArchive = function(url, password) {
    return new Promise((resolve, reject) => {
      if (password) {
        assertEquals('testpassword', password);
        const volumeInfo = {resolveDisplayRoot: () => null};
        resolve(volumeInfo);
      } else {
        reject(VolumeManagerCommon.VolumeError.NEED_PASSWORD);
      }
    });
  };

  fileManager.ui.passwordDialog.askForPassword =
      async function(filename, password = null) {
    return Promise.resolve('testpassword');
  };

  // Mount archive.
  await tasks.mountArchiveAndChangeDirectory_(fakeTracker, fakeMountedZipUrl);

  // Check: mount is completed, no error since the user entered a valid
  // password.
  assertEquals(
      ProgressItemState.COMPLETED,
      fileManager.progressCenter.getItemById(zipMountPanelId).state);
  assertEquals(
      undefined, fileManager.progressCenter.getItemById(errorZipMountPanelId));

  done();
}
