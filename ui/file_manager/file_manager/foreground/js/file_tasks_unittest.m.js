// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://test/chai_assert.js';

import {createCrostiniForTest} from '../../background/js/mock_crostini.js';
import {MockProgressCenter} from '../../background/js/mock_progress_center.js';
import {metrics} from '../../common/js/metrics.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {ProgressItemState} from '../../common/js/progress_center_common.js';
import {reportPromise} from '../../common/js/test_error_reporting.js';
import {LEGACY_FILES_EXTENSION_ID} from '../../common/js/url_constants.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {FilesPasswordDialog} from '../elements/files_password_dialog.js';

import {DirectoryModel} from './directory_model.js';
import {FileTasks} from './file_tasks.js';
import {FileTransferController} from './file_transfer_controller.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {NamingController} from './naming_controller.js';
import {TaskHistory} from './task_history.js';
import {FileManagerUI} from './ui/file_manager_ui.js';

/**
 * Utility function that appends value under a given name in the store.
 * @param {!Map<string, !Array<string|number>>} store
 * @param {string} name
 * @param {!*} value
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
 * A map from histogram name to all times recorded for it.
 */
const timeMap = new Map();

/**
 * Mock metrics.recordEnum.
 * @param {string} name
 * @param {*} value
 * @param {Array<*>|number=} valid
 */
metrics.recordEnum = function(name, value, valid) {
  assertTrue(valid.includes(value));
  record(enumMap, name, value);
};

/**
 * Mock metrics.recordSmallCount.
 * @param {string} name Short metric name.
 * @param {number} value Value to be recorded.
 */
metrics.recordSmallCount = function(name, value) {
  record(countMap, name, value);
};

/**
 * Mock metrics.recordTime.
 * @param {string} name Short metric name.
 * @param {number} time Time to be recorded in milliseconds.
 */
metrics.recordTime = function(name, time) {
  record(timeMap, name, time);
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
  getLastExecutedTime: function(descriptor) {
    return 0;
  },
  recordTaskExecuted: function(descriptor) {},
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
export function setUp() {
  // Mock LoadTimeData strings.
  window.loadTimeData.getString = id => id;
  window.loadTimeData.getBoolean = key => false;

  const mockTask = /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
    descriptor: {
      appId: 'handler-extension-id',
      taskType: 'app',
      actionId: 'any',
    },
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
      executeTask: function(descriptor, entries, onViewFiles) {
        onViewFiles('failed');
      },
      sharePathsWithCrostini: function(vmName, entries, persist, callback) {
        callback();
      },
    },
    runtime: {},
  };

  installMockChrome(mockChrome);
  enumMap.clear();
  countMap.clear();
  timeMap.clear();
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
export function testToOpenExeFile(callback) {
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
export function testToOpenDmgFile(callback) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.dmg');

  reportPromise(
      showHtmlOfAlertDialogIsCalled([mockEntry], 'test.dmg', 'NO_TASK_FOR_DMG'),
      callback);
}

/**
 * Tests opening a .crx file.
 */
export function testToOpenCrxFile(callback) {
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
export function testToOpenRtfFile(callback) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.rtf');

  reportPromise(
      showHtmlOfAlertDialogIsCalled(
          [mockEntry], 'test.rtf', 'NO_TASK_FOR_FILE'),
      callback);
}

/**
 * Tests opening the task picker with an entry that does not have a default app
 * but there are multiple apps that could open it.
 */
export function testOpenTaskPicker(callback) {
  window.chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback.bind(
            null,
            [
              {
                descriptor: {
                  appId: 'handler-extension-id1',
                  taskType: 'app',
                  actionId: 'any',
                },
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 1',
              },
              {
                descriptor: {
                  appId: 'handler-extension-id2',
                  taskType: 'app',
                  actionId: 'any',
                },
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
export function testOpenWithMostRecentlyExecuted(callback) {
  const latestTaskDescriptor = {
    appId: 'handler-extension-most-recently-executed',
    taskType: 'app',
    actionId: 'any',
  };
  const oldTaskDescriptor = {
    appId: 'handler-extension-executed-before',
    taskType: 'app',
    actionId: 'any',
  };

  window.chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback.bind(
            null,
            // File tasks is sorted by last executed time, latest first.
            [
              {
                descriptor: latestTaskDescriptor,
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 1',
              },
              {
                descriptor: oldTaskDescriptor,
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 2',
              },
              {
                descriptor: {
                  appId: 'handler-extension-never-executed',
                  taskType: 'app',
                  actionId: 'any',
                },
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 3',
              },
            ]),
        0);
  };

  const taskHistory = /** @type {!TaskHistory} */ ({
    getLastExecutedTime: function(descriptor) {
      if (util.descriptorEqual(descriptor, oldTaskDescriptor)) {
        return 10000;
      }
      if (util.descriptorEqual(descriptor, latestTaskDescriptor)) {
        return 20000;
      }
      return 0;
    },
    recordTaskExecuted: function(descriptor) {},
  });

  let executedTask = null;
  window.chrome.fileManagerPrivate.executeTask =
      (descriptor, entries, onViewFiles) => {
        executedTask = descriptor;
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
          assertTrue(util.descriptorEqual(latestTaskDescriptor, executedTask));
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
    descriptor: {
      appId: LEGACY_FILES_EXTENSION_ID,
      taskType: 'app',
      actionId: 'install-linux-package'
    },
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
export function testOpenInstallLinuxPackageDialog(callback) {
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
export function testToOpenTiniFileOpensImportCrostiniImageDialog(callback) {
  window.chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback.bind(
            null,
            [
              {
                descriptor: {
                  appId: LEGACY_FILES_EXTENSION_ID,
                  taskType: 'app',
                  actionId: 'import-crostini-image'
                },
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
export function testGetViewFileType() {
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
 * Checks that the progress center is properly updated when mounting archives
 * successfully.
 * @suppress {visibility}
 */
export async function testMountArchiveAndChangeDirectoryNotificationSuccess(
    done) {
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

  // Check: a zip mount time UMA has been recorded.
  assertTrue(timeMap.has('ZipMountTime.Other'));

  done();
}

/**
 * Checks that the progress center is properly updated when mounting an archive
 * resolves with an error.
 * @suppress {visibility}
 */
export async function
testMountArchiveAndChangeDirectoryNotificationInvalidArchive(done) {
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

  // Check: no zip mount time UMA has been recorded since mounting the archive
  // failed.
  assertFalse(timeMap.has('ZipMountTime.Other'));

  done();
}

/**
 * Checks that the progress center is properly updated when the password prompt
 * for an encrypted archive is canceled.
 * @suppress {visibility}
 */
export async function
testMountArchiveAndChangeDirectoryNotificationCancelPassword(done) {
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

  // Check: no zip mount time UMA has been recorded since the mount has been
  // cancelled.
  assertFalse(timeMap.has('ZipMountTime.Other'));

  done();
}

/**
 * Checks that the progress center is properly updated when mounting an
 * encrypted archive.
 * @suppress {visibility}
 */
export async function
testMountArchiveAndChangeDirectoryNotificationEncryptedArchive(done) {
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
