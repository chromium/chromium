// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {importer} from '../../common/js/importer_common.js';
import {MockChromeStorageAPI} from '../../common/js/mock_chrome.js';
import {MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise} from '../../common/js/test_error_reporting.js';
import {importerTest} from '../../common/js/test_importer_common.js';
import {TestCallRecorder} from '../../common/js/unittest_util.js';
import {importerHistoryInterfaces} from '../../externs/background/import_history.js';

import {importerHistory} from './import_history.js';

/** @const {string} */
const FILE_LAST_MODIFIED = new Date('Dec 4 1968').toString();

/** @const {number} */
const FILE_SIZE = 1234;

/** @const {string} */
const FILE_PATH = 'test/data';

/** @const {number} */
const TEMPORARY = window.TEMPORARY || 0;

/** @const {!importer.Destination<string>} */
const GOOGLE_DRIVE = importer.Destination.GOOGLE_DRIVE;

/**
 * Space Cloud: Your source for interstellar cloud storage.
 * @const {!importer.Destination<string>}
 */
const SPACE_CAMP = /** @type !importer.Destination<string> */ ('Space Camp');

/** @type {!MockFileSystem} */
let testFileSystem;

/** @type {!FileEntry} */
let testFileEntry;

/** @type {!importerTest.TestLogger} */
let testLogger;

/** @type {!importerHistory.RecordStorage} */
let storage;

/** @type {!Promise<!importerHistoryInterfaces.ImportHistory>} */
let historyProvider;

/** @type {Promise} */
let testPromise;

// Set up the test components.
export function setUp() {
  setupChromeApis();
  installTestLogger();

  testFileSystem = new MockFileSystem('abc-123', 'filesystem:abc-123');

  testFileEntry = MockFileEntry.create(
      testFileSystem, FILE_PATH,
      /** @type Metadata */ ({
        size: FILE_SIZE,
        modificationTime: FILE_LAST_MODIFIED,
      }));

  testFileSystem.entries[FILE_PATH] = testFileEntry;

  storage = new TestRecordStorage();

  const history = new importerHistory.PersistentImportHistory(
      importerHistory.createMetadataHashcode, storage);

  historyProvider = history.whenReady();
}

function tearDown() {
  testLogger.errorRecorder.assertCallCount(0);
  testPromise = null;
}

export function testWasCopied_FalseForUnknownEntry(callback) {
  // TestRecordWriter is pre-configured with a Space Cloud entry
  // but not for this file.
  testPromise = historyProvider.then(history => {
    return history.wasCopied(testFileEntry, SPACE_CAMP).then(assertFalse);
  });

  reportPromise(testPromise, callback);
}

export function testWasCopied_TrueForKnownEntryLoadedFromStorage(callback) {
  // TestRecordWriter is pre-configured with this entry.
  testPromise = historyProvider.then(history => {
    return history.wasCopied(testFileEntry, GOOGLE_DRIVE).then(assertTrue);
  });

  reportPromise(testPromise, callback);
}


export function testMarkCopied_FiresChangedEvent(callback) {
  testPromise = historyProvider.then(history => {
    const recorder = new TestCallRecorder();
    history.addObserver(recorder.callback);
    return history.markCopied(testFileEntry, SPACE_CAMP, 'url1').then(() => {
      return Promise.resolve().then(() => {
        recorder.assertCallCount(1);
        assertEquals(
            importerHistory.ImportHistoryState.COPIED,
            recorder.getLastArguments()[0]['state']);
      });
    });
  });

  reportPromise(testPromise, callback);
}

export function testMarkImported_ByUrl(callback) {
  const destinationUrl =
      'filesystem:chrome-extension://abc/photos/splosion.jpg';
  testPromise = historyProvider.then(history => {
    return history.markCopied(testFileEntry, SPACE_CAMP, destinationUrl)
        .then(() => {
          return history.markImportedByUrl(destinationUrl).then(() => {
            return history.wasImported(testFileEntry, SPACE_CAMP)
                .then(assertTrue);
          });
        });
  });

  reportPromise(testPromise, callback);
}

export function testWasImported_FalseForUnknownEntry(callback) {
  // TestRecordWriter is pre-configured with a Space Cloud entry
  // but not for this file.
  testPromise = historyProvider.then(history => {
    return history.wasImported(testFileEntry, SPACE_CAMP).then(assertFalse);
  });

  reportPromise(testPromise, callback);
}

export function testWasImported_TrueForKnownEntryLoadedFromStorage(callback) {
  // TestRecordWriter is pre-configured with this entry.
  testPromise = historyProvider.then(history => {
    return history.wasImported(testFileEntry, GOOGLE_DRIVE).then(assertTrue);
  });

  reportPromise(testPromise, callback);
}

export function testWasImported_TrueForKnownEntrySetAtRuntime(callback) {
  testPromise = historyProvider.then(history => {
    return history.markImported(testFileEntry, SPACE_CAMP).then(() => {
      return history.wasImported(testFileEntry, SPACE_CAMP).then(assertTrue);
    });
  });

  reportPromise(testPromise, callback);
}

export function testMarkImport_FiresChangedEvent(callback) {
  testPromise = historyProvider.then(history => {
    const recorder = new TestCallRecorder();
    history.addObserver(recorder.callback);
    return history.markImported(testFileEntry, SPACE_CAMP).then(() => {
      return Promise.resolve().then(() => {
        recorder.assertCallCount(1);
        assertEquals(
            importerHistory.ImportHistoryState.IMPORTED,
            recorder.getLastArguments()[0]['state']);
      });
    });
  });

  reportPromise(testPromise, callback);
}

export function testHistoryObserver_Unsubscribe(callback) {
  testPromise = historyProvider.then(history => {
    const recorder = new TestCallRecorder();
    history.addObserver(recorder.callback);
    history.removeObserver(recorder.callback);

    const promises = [];
    promises.push(history.markCopied(testFileEntry, SPACE_CAMP, 'url2'));
    promises.push(history.markImported(testFileEntry, SPACE_CAMP));
    return Promise.all(promises).then(() => {
      return Promise.resolve().then(() => {
        recorder.assertCallCount(0);
      });
    });
  });

  reportPromise(testPromise, callback);
}

export function testRecordStorage_RemembersPreviouslyWrittenRecords(callback) {
  const recorder = new TestCallRecorder();
  testPromise = createRealStorage(['recordStorageTest.data']).then(storage => {
    return storage.write(['abc', '123']).then(() => {
      return storage.readAll(recorder.callback).then(() => {
        recorder.assertCallCount(1);
      });
    });
  });

  reportPromise(testPromise, callback);
}

export function testRecordStorage_LoadsRecordsFromMultipleHistoryFiles(
    callback) {
  const recorder = new TestCallRecorder();

  const remoteData =
      createRealStorage(['multiStorage-1.data']).then(storage => {
        return storage.write(['remote-data', '98765432']);
      });
  const moreRemoteData =
      createRealStorage(['multiStorage-2.data']).then(storage => {
        return storage.write(['antarctica-data', '777777777777']);
      });

  testPromise = Promise.all([remoteData, moreRemoteData]).then(() => {
    return createRealStorage([
             'multiStorage-0.data',
             'multiStorage-1.data',
             'multiStorage-2.data',
           ])
        .then(storage => {
          const writePromises = [storage.write(['local-data', '111'])];
          return Promise.all(writePromises).then(() => {
            return storage.readAll(recorder.callback).then(() => {
              recorder.assertCallCount(3);
              assertEquals('local-data', recorder.getArguments(0)[0][0]);
              assertEquals('remote-data', recorder.getArguments(1)[0][0]);
              assertEquals('antarctica-data', recorder.getArguments(2)[0][0]);
            });
          });
        });
  });

  reportPromise(testPromise, callback);
}

export function testRecordStorage_SerializingOperations(callback) {
  const recorder = new TestCallRecorder();
  testPromise = createRealStorage([
                  'recordStorageTestForSerializing.data',
                ]).then(storage => {
    const writePromises = [];
    const WRITES_COUNT = 20;
    for (let i = 0; i < WRITES_COUNT; i++) {
      writePromises.push(storage.write(['abc', '123']));
    }
    const readAllPromise = storage.readAll(recorder.callback).then(() => {
      recorder.assertCallCount(WRITES_COUNT);
    });
    // Write an extra record, which must be executed afte reading is
    // completed.
    writePromises.push(storage.write(['abc', '123']));
    return Promise.all(writePromises.concat([readAllPromise]));
  });

  reportPromise(testPromise, callback);
}

export function testCreateMetadataHashcode(callback) {
  const promise =
      importerHistory.createMetadataHashcode(testFileEntry).then(hashcode => {
        // Note that the expression matches at least 4 numbers
        // in the last segment, since we hard code the byte
        // size in our test file to a four digit size.
        // In reality it will vary.
        assertEquals(
            0, hashcode.search(/[\-0-9]{9,}_[0-9]{4,}/),
            'Hashcode (' + hashcode + ') does not match next pattern.');
      });

  reportPromise(promise, callback);
}

/**
 * Installs stub chrome APIs.
 */
function setupChromeApis() {
  new MockChromeStorageAPI();
}

/**
 * Installs importer test logger.
 */
function installTestLogger() {
  testLogger = new importerTest.TestLogger();
  importerTest.getLogger = () => {
    return testLogger;
  };
}

/**
 * @param {!Array<string>} fileNames
 * @return {!Promise<!importerHistory.RecordStorage>}
 */
function createRealStorage(fileNames) {
  const filePromises = fileNames.map(createFileEntry);
  return Promise.all(filePromises).then(fileEntries => {
    return new importerHistory.FileBasedRecordStorage(fileEntries);
  });
}

/**
 * Creates a *real* FileEntry in the DOM filesystem.
 *
 * @param {string} fileName
 * @return {!Promise<!FileEntry>}
 */
function createFileEntry(fileName) {
  return new Promise((resolve, reject) => {
    const onFileSystemReady = fileSystem => {
      fileSystem.root.getFile(
          fileName, {create: true, exclusive: false}, resolve, reject);
    };

    window.webkitRequestFileSystem(
        TEMPORARY, 1024 * 1024, onFileSystemReady, reject);
  });
}

/**
 * In-memory test implementation of {@code RecordStorage}.
 *
 * @implements {importerHistory.RecordStorage}
 */
class TestRecordStorage {
  constructor() {
    const timeStamp = importer.toSecondsFromEpoch(FILE_LAST_MODIFIED);

    // Pre-populate the store with some "previously written" data <wink>.
    /** @private {!Array<!Array<string>>} */
    this.records_ = [
      [1, timeStamp + '_' + FILE_SIZE, GOOGLE_DRIVE],
      [
        0,
        timeStamp + '_' + FILE_SIZE,
        'google-drive',
        '$/some/url/snazzy.pants',
        '$/someother/url/snazzy.pants',
      ],
      [1, '99999_99999', SPACE_CAMP],
    ];
  }

  /** @override */
  readAll(recordCallback) {
    this.records_.forEach(recordCallback);
    return Promise.resolve(this.records_);
  }

  /** @override */
  write(record) {
    this.records_.push(record);
    return Promise.resolve();
  }
}
