// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {!MockProgressCenter} */
let progressCenter;

/** @type {!importer.MediaImportHandler} */
let mediaImporter;

/** @type {!importer.TestImportHistory} */
let importHistory;

/** @type {!importer.DispositionChecker.CheckerFunction} */
let dispositionChecker;

/** @type {!MockCopyTo} */
let mockCopier;

/** @type {!MockFileSystem} */
let destinationFileSystem;

/** @type {!Promise<!DirectoryEntry>} */
let destinationFactory;

/** @type {!MockDriveSyncHandler} */
let driveSyncHandler;

/**
 * Mock metrics.
 * @type {!Object}
 */
window.metrics = {
  recordSmallCount: function() {},
  recordUserAction: function() {},
  recordMediumCount: function() {},
  recordBoolean: function() {},
};

/**
 * Mock Chrome APIs
 * @type {!Object}
 */
let mockChrome;

// Set up the test components.
function setUp() {
  // Mock loadTimeData strings.
  window.loadTimeData.getString = id => id;

  // Setup mock chrome APIs.
  mockChrome = {
    power: {
      requestKeepAwakeWasCalled: false,
      requestKeepAwakeStatus: false,
      requestKeepAwake: function() {
        mockChrome.power.requestKeepAwakeWasCalled = true;
        mockChrome.power.requestKeepAwakeStatus = true;
      },
      releaseKeepAwake: function() {
        mockChrome.power.requestKeepAwakeStatus = false;
      }
    },
  };

  installMockChrome(mockChrome);
  new MockCommandLinePrivate();

  // Replace fileOperationUtil.copyTo with mock test function.
  mockCopier = new MockCopyTo();

  // Create and install MockVolumeManager.
  const volumeManager = new MockVolumeManager();
  MockVolumeManager.installMockSingleton(volumeManager);

  // Add fake parented and non-parented roots: /root/ and /other/.
  const driveVolumeType = VolumeManagerCommon.VolumeType.DRIVE;
  const driveVolumeInfo = /** @type {!VolumeInfo} */
      (volumeManager.getCurrentProfileVolumeInfo(driveVolumeType));
  const driveFileSystem =
      /** @type {!MockFileSystem} */ (driveVolumeInfo.fileSystem);
  driveFileSystem.populate(['/root/', '/other/']);

  // Setup a default disposition checker. Tests can replace it at runtime
  // if they need specialized disposition check behavior.
  dispositionChecker = () => {
    return Promise.resolve(importer.Disposition.ORIGINAL);
  };

  // Setup MediaImporter.
  progressCenter = new MockProgressCenter();
  importHistory = new importer.TestImportHistory();
  driveSyncHandler = new MockDriveSyncHandler();
  importer.setupTestLogger();
  mediaImporter = new importer.MediaImportHandlerImpl(
      progressCenter, importHistory, dispositionChecker, driveSyncHandler);

  // Setup the copy destination.
  destinationFileSystem = new MockFileSystem('googleDriveFilesystem');
  destinationFactory = Promise.resolve(destinationFileSystem.root);
}

/**
 * Tests media imports.
 */
function testImportMedia(callback) {
  const media = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00004.jpg',
    '/DCIM/photos1/IMG00005.jpg',
    '/DCIM/photos1/IMG00006.jpg',
  ]);

  const scanResult = new TestScanResult(media);
  const importTask = mediaImporter.importFromScanResult(
      scanResult, importer.Destination.GOOGLE_DRIVE, destinationFactory);

  const whenImportDone = new Promise((resolve, reject) => {
    importTask.addObserver(
        /**
         * @param {!importer.TaskQueue.UpdateType} updateType
         * @param {Object=} opt_task
         */
        (updateType, opt_task) => {
          switch (updateType) {
            case importer.TaskQueue.UpdateType.COMPLETE:
              resolve();
              break;
            case importer.TaskQueue.UpdateType.ERROR:
              reject(new Error(importer.TaskQueue.UpdateType.ERROR));
              break;
          }
        });
  });

  reportPromise(
      whenImportDone.then(() => {
        const mockDirectoryEntry =
            /** @type {!MockDirectoryEntry} */ (destinationFileSystem.root);
        const copiedEntries = mockDirectoryEntry.getAllChildren();
        assertEquals(media.length, copiedEntries.length);
      }),
      callback);

  scanResult.finalize();
}

/**
 * Tests media import duplicate detection.
 */
function testImportMedia_skipAndMarkDuplicatedFiles(callback) {
  const DUPLICATED_FILE_PATH_1 = '/DCIM/photos0/duplicated_1.jpg';
  const DUPLICATED_FILE_PATH_2 = '/DCIM/photos0/duplicated_2.jpg';
  const ORIGINAL_FILE_NAME = 'new_image.jpg';
  const ORIGINAL_FILE_SRC_PATH = '/DCIM/photos0/' + ORIGINAL_FILE_NAME;
  const ORIGINAL_FILE_DEST_PATH = '/' + ORIGINAL_FILE_NAME;
  const media = setupFileSystem([
    DUPLICATED_FILE_PATH_1,
    ORIGINAL_FILE_NAME,
    DUPLICATED_FILE_PATH_2,
  ]);

  dispositionChecker = (entry, destination) => {
    if (entry.fullPath == DUPLICATED_FILE_PATH_1) {
      return Promise.resolve(importer.Disposition.HISTORY_DUPLICATE);
    }
    if (entry.fullPath == DUPLICATED_FILE_PATH_2) {
      return Promise.resolve(importer.Disposition.CONTENT_DUPLICATE);
    }
    return Promise.resolve(importer.Disposition.ORIGINAL);
  };
  mediaImporter = new importer.MediaImportHandlerImpl(
      progressCenter, importHistory, dispositionChecker, driveSyncHandler);
  const scanResult = new TestScanResult(media);
  const importTask = mediaImporter.importFromScanResult(
      scanResult, importer.Destination.GOOGLE_DRIVE, destinationFactory);

  const whenImportDone = new Promise((resolve, reject) => {
    importTask.addObserver(
        /**
         * @param {!importer.TaskQueue.UpdateType} updateType
         * @param {Object=} opt_task
         */
        (updateType, opt_task) => {
          switch (updateType) {
            case importer.TaskQueue.UpdateType.COMPLETE:
              resolve();
              break;
            case importer.TaskQueue.UpdateType.ERROR:
              reject(new Error(importer.TaskQueue.UpdateType.ERROR));
              break;
          }
        });
  });

  reportPromise(
      whenImportDone.then(() => {
        // Only the new file should be copied.
        const mockDirectoryEntry =
            /** @type {!MockDirectoryEntry} */ (destinationFileSystem.root);
        const copiedEntries = mockDirectoryEntry.getAllChildren();
        assertEquals(1, copiedEntries.length);
        assertEquals(ORIGINAL_FILE_DEST_PATH, copiedEntries[0].fullPath);
        const mockFileEntry = /** @type {!FileEntry} */ (media[1]);
        importHistory.assertCopied(
            mockFileEntry, importer.Destination.GOOGLE_DRIVE);
        // The 2 duplicated files should be marked as imported.
        [media[0], media[2]].forEach(entry => {
          entry = /** @type {!FileEntry} */ (entry);
          importHistory.assertImported(
              entry, importer.Destination.GOOGLE_DRIVE);
        });
      }),
      callback);

  scanResult.finalize();
}

/**
 * Tests media import uses encoded URLs.
 */
function testImportMedia_EmploysEncodedUrls(callback) {
  const media = setupFileSystem([
    '/DCIM/photos0/Mom and Dad.jpg',
  ]);

  const scanResult = new TestScanResult(media);
  const importTask = mediaImporter.importFromScanResult(
      scanResult, importer.Destination.GOOGLE_DRIVE, destinationFactory);

  const promise =
      new Promise((resolve, reject) => {
        importTask.addObserver(
            /**
             * @param {!importer.TaskQueue.UpdateType} updateType
             * @param {Object=} opt_task
             */
            (updateType, opt_task) => {
              switch (updateType) {
                case importer.TaskQueue.UpdateType.COMPLETE:
                  resolve(/** @type {!MockDirectoryEntry} */
                          (destinationFileSystem.root).getAllChildren());
                  break;
                case importer.TaskQueue.UpdateType.ERROR:
                  reject('Task failed :(');
                  break;
              }
            });
      }).then(copiedEntries => {
        const expected = 'Mom%20and%20Dad.jpg';
        const url = copiedEntries[0].toURL();
        assertTrue(url.length > expected.length);
        const actual = url.substring(url.length - expected.length);
        assertEquals(expected, actual);
      });

  reportPromise(promise, callback);

  scanResult.finalize();
}

/**
 * Tests that when files with duplicate names are imported, that they don't
 * overwrite one another.
 */
function testImportMediaWithDuplicateFilenames(callback) {
  const media = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00001.jpg',
    '/DCIM/photos1/IMG00002.jpg',
    '/DCIM/photos1/IMG00003.jpg',
  ]);

  const scanResult = new TestScanResult(media);
  const importTask = mediaImporter.importFromScanResult(
      scanResult, importer.Destination.GOOGLE_DRIVE, destinationFactory);

  const whenImportDone = new Promise((resolve, reject) => {
    importTask.addObserver(
        /**
         * @param {!importer.TaskQueue.UpdateType} updateType
         * @param {Object=} opt_task
         */
        (updateType, opt_task) => {
          switch (updateType) {
            case importer.TaskQueue.UpdateType.COMPLETE:
              resolve();
              break;
            case importer.TaskQueue.UpdateType.ERROR:
              reject(new Error(importer.TaskQueue.UpdateType.ERROR));
              break;
          }
        });
  });

  // Verify that we end up with 6, and not 3, destination entries.
  reportPromise(
      whenImportDone.then(() => {
        const mockDirectoryEntry =
            /** @type {!MockDirectoryEntry} */ (destinationFileSystem.root);
        const copiedEntries = mockDirectoryEntry.getAllChildren();
        assertEquals(media.length, copiedEntries.length);
      }),
      callback);

  scanResult.finalize();
}

/**
 * Tests that active media imports keep chrome.power awake.
 */
function testKeepAwakeDuringImport(callback) {
  const media = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00004.jpg',
    '/DCIM/photos1/IMG00005.jpg',
    '/DCIM/photos1/IMG00006.jpg',
  ]);

  const scanResult = new TestScanResult(media);
  const importTask = mediaImporter.importFromScanResult(
      scanResult, importer.Destination.GOOGLE_DRIVE, destinationFactory);

  const whenImportDone = new Promise((resolve, reject) => {
    importTask.addObserver(
        /**
         * @param {!importer.TaskQueue.UpdateType} updateType
         * @param {Object=} opt_task
         */
        (updateType, opt_task) => {
          // Assert that keepAwake is set while the task is active.
          assertTrue(mockChrome.power.requestKeepAwakeStatus);
          switch (updateType) {
            case importer.TaskQueue.UpdateType.COMPLETE:
              resolve();
              break;
            case importer.TaskQueue.UpdateType.ERROR:
              reject(new Error(importer.TaskQueue.UpdateType.ERROR));
              break;
          }
        });
  });

  reportPromise(
      whenImportDone.then(() => {
        assertTrue(mockChrome.power.requestKeepAwakeWasCalled);
        assertFalse(mockChrome.power.requestKeepAwakeStatus);
        const mockDirectoryEntry =
            /** @type {!MockDirectoryEntry} */ (destinationFileSystem.root);
        const copiedEntries = mockDirectoryEntry.getAllChildren();
        assertEquals(media.length, copiedEntries.length);
      }),
      callback);

  scanResult.finalize();
}

/**
 * Tests that media imports update import history.
 */
function testUpdatesHistoryAfterImport(callback) {
  const entries = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos1/IMG00003.jpg',
    '/DCIM/photos0/DRIVEDUPE00001.jpg',
    '/DCIM/photos1/DRIVEDUPE99999.jpg',
  ]);

  const newFiles = entries.slice(0, 2);
  const dupeFiles = entries.slice(2);

  const scanResult = new TestScanResult(entries.slice(0, 2));
  scanResult.duplicateFileEntries = dupeFiles;
  const importTask = mediaImporter.importFromScanResult(
      scanResult, importer.Destination.GOOGLE_DRIVE, destinationFactory);

  const whenImportDone = new Promise((resolve, reject) => {
    importTask.addObserver(
        /**
         * @param {!importer.TaskQueue.UpdateType} updateType
         * @param {Object=} opt_task
         */
        (updateType, opt_task) => {
          switch (updateType) {
            case importer.TaskQueue.UpdateType.COMPLETE:
              resolve();
              break;
            case importer.TaskQueue.UpdateType.ERROR:
              reject(new Error(importer.TaskQueue.UpdateType.ERROR));
              break;
          }
        });
  });

  const promise = whenImportDone.then(() => {
    mockCopier.copiedFiles.forEach(
        /** @param {!MockCopyTo.CopyInfo} copy */
        copy => {
          const mockFileEntry = /** @type {!FileEntry} */ (copy.source);
          importHistory.assertCopied(
              mockFileEntry, importer.Destination.GOOGLE_DRIVE);
        });
    dupeFiles.forEach(entry => {
      const mockFileEntry = /** @type {!FileEntry} */ (entry);
      importHistory.assertImported(
          mockFileEntry, importer.Destination.GOOGLE_DRIVE);
    });
  });

  scanResult.finalize();
  reportPromise(promise, callback);
}

/**
 * Tests cancelling a media import.
 */
function testImportCancellation(callback) {
  const media = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00004.jpg',
    '/DCIM/photos1/IMG00005.jpg',
    '/DCIM/photos1/IMG00006.jpg',
  ]);

  /** @const {number} */
  const EXPECTED_COPY_COUNT = 3;

  const scanResult = new TestScanResult(media);
  const importTask = mediaImporter.importFromScanResult(
      scanResult, importer.Destination.GOOGLE_DRIVE, destinationFactory);

  const whenImportCancelled = new Promise((resolve, reject) => {
    importTask.addObserver(
        /**
         * @param {!importer.TaskQueue.UpdateType} updateType
         * @param {Object=} opt_task
         */
        (updateType, opt_task) => {
          if (updateType === importer.TaskQueue.UpdateType.CANCELED) {
            resolve();
          }
        });
  });

  // Simulate cancellation after the expected number of copies is done.
  let copyCount = 0;
  importTask.addObserver(updateType => {
    if (updateType ===
        importer.MediaImportHandler.ImportTask.UpdateType.ENTRY_CHANGED) {
      copyCount++;
      if (copyCount === EXPECTED_COPY_COUNT) {
        importTask.requestCancel();
      }
    }
  });

  reportPromise(
      whenImportCancelled.then(() => {
        const mockDirectoryEntry =
            /** @type {!MockDirectoryEntry} */ (destinationFileSystem.root);
        const copiedEntries = mockDirectoryEntry.getAllChildren();
        assertEquals(EXPECTED_COPY_COUNT, copiedEntries.length);
      }),
      callback);

  scanResult.finalize();
}

/**
 * Tests media imports with errors.
 */
function testImportWithErrors(callback) {
  // Quieten the logger just in this test, since we expect errors.
  // Elsewhere, it's better for errors to be seen by test authors.
  importer.setupTestLogger().quiet();

  const media = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00004.jpg',
    '/DCIM/photos1/IMG00005.jpg',
    '/DCIM/photos1/IMG00006.jpg',
  ]);

  /** @const {number} */
  const EXPECTED_COPY_COUNT = 5;

  const scanResult = new TestScanResult(media);
  const importTask = mediaImporter.importFromScanResult(
      scanResult, importer.Destination.GOOGLE_DRIVE, destinationFactory);

  const whenImportDone = new Promise((resolve, reject) => {
    importTask.addObserver(
        /**
         * @param {!importer.TaskQueue.UpdateType} updateType
         * @param {Object=} opt_task
         */
        (updateType, opt_task) => {
          if (updateType === importer.TaskQueue.UpdateType.COMPLETE) {
            resolve();
          }
        });
  });

  // Simulate an error after 3 imports.
  let copyCount = 0;
  importTask.addObserver(updateType => {
    if (updateType ===
        importer.MediaImportHandler.ImportTask.UpdateType.ENTRY_CHANGED) {
      copyCount++;
      if (copyCount === 3) {
        mockCopier.simulateOneError();
      }
    }
  });

  // Verify that the error didn't result in some files not being copied.
  reportPromise(
      whenImportDone.then(() => {
        const mockDirectoryEntry =
            /** @type {!MockDirectoryEntry} */ (destinationFileSystem.root);
        const copiedEntries = mockDirectoryEntry.getAllChildren();
        assertEquals(EXPECTED_COPY_COUNT, copiedEntries.length);
      }),
      callback);

  scanResult.finalize();
}

/**
 * Setup a file system containing the given |fileNames| and return the file
 * system's entries in an array.
 *
 * @param {!Array<string>} fileNames
 * @return {!Array<!Entry>}
 */
function setupFileSystem(fileNames) {
  let fileSystem = new MockFileSystem('fake-media-volume');
  fileSystem.populate(fileNames);
  return fileNames.map((name) => fileSystem.entries[name]);
}

/**
 * Replaces fileOperationUtil.copyTo with a mock for testing.
 */
class MockCopyTo {
  constructor() {
    /** @type {!Array<!MockCopyTo.CopyInfo>} */
    this.copiedFiles = [];

    // Replace fileOperationUtil.copyTo with our mock test function.
    fileOperationUtil.copyTo =
        /** @type {function(*)} */ (this.copyTo_.bind(this));

    /** @private {boolean} */
    this.simulateError_ = false;

    this.entryChangedCallback_ = null;
    this.progressCallback_ = null;
    this.successCallback_ = null;
    this.errorCallback_ = null;
  }

  /**
   * Makes the mock copier simulate an error the next time copyTo_ is called.
   */
  simulateOneError() {
    this.simulateError_ = true;
  }

  /**
   * A mock to replace fileOperationUtil.copyTo.  See the original for details.
   * @param {!Entry} source
   * @param {!DirectoryEntry} parent
   * @param {string} newName
   * @param {function(string, Entry)} entryChangedCallback
   * @param {function(string, number)} progressCallback
   * @param {function(Entry)} successCallback
   * @param {function(Error)} errorCallback
   */
  copyTo_(
      source, parent, newName, entryChangedCallback, progressCallback,
      successCallback, errorCallback) {
    this.entryChangedCallback_ = entryChangedCallback;
    this.progressCallback_ = progressCallback;
    this.successCallback_ = successCallback;
    this.errorCallback_ = errorCallback;

    if (this.simulateError_) {
      this.simulateError_ = false;
      const error = new Error('test error');
      this.errorCallback_(error);
      return;
    }

    // Log the copy details.
    this.copiedFiles.push(/** @type {!MockCopyTo.CopyInfo} */ ({
      source: source,
      destination: parent,
      newName: newName,
    }));

    // Copy the file.
    const copyErrorCallback = /** @type {!function(FileError):*} */
        (this.errorCallback_.bind(this));
    source.copyTo(parent, newName, newEntry => {
      this.entryChangedCallback_(source.toURL(), parent);
      this.successCallback_(newEntry);
    }, copyErrorCallback);
  }
}

/**
 * @typedef {{
 *   source: !Entry,
 *   destination: !DirectoryEntry,
 *   newName: string,
 * }}
 */
MockCopyTo.CopyInfo;
