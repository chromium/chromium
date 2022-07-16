// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertArrayEquals, assertEquals, assertFalse, assertLT, assertNotReached, assertTrue} from 'chrome://test/chai_assert.js';

import {FileOperationError, FileOperationProgressEvent} from '../../common/js/file_operation_common.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {joinPath, MockDirectoryEntry, MockEntry, MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise, waitUntil} from '../../common/js/test_error_reporting.js';
import {util} from '../../common/js/util.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {EntryLocation} from '../../externs/entry_location.js';

import {FileOperationManagerImpl} from './file_operation_manager.js';
import {fileOperationUtil} from './file_operation_util.js';
import {volumeManagerFactory} from './volume_manager_factory.js';

/**
 * Mock chrome APIs.
 * @type {Object}
 */
const mockChrome = {};

mockChrome.runtime = {
  lastError: null
};

mockChrome.power = {
  requestKeepAwake: function() {
    mockChrome.power.keepAwakeRequested = true;
  },
  releaseKeepAwake: function() {
    mockChrome.power.keepAwakeRequested = false;
  },
  keepAwakeRequested: false
};

mockChrome.fileManagerPrivate = {
  onCopyProgress: {
    addListener: function(callback) {
      mockChrome.fileManagerPrivate.onCopyProgress.listener_ = callback;
    },
    removeListener: function() {
      mockChrome.fileManagerPrivate.onCopyProgress.listener_ = null;
    },
    listener_: null
  },

};

/**
 * Logs copy-progress events from a file operation manager.
 */
class EventLogger {
  /**
   * @param {!FileOperationManager} fileOperationManager The target file
   *     operation manager.
   */
  constructor(fileOperationManager) {
    this.events = [];
    this.numberOfBeginEvents = 0;
    this.numberOfErrorEvents = 0;
    this.numberOfSuccessEvents = 0;
    fileOperationManager.addEventListener(
        'copy-progress', this.onCopyProgress_.bind(this));
  }

  /**
   * Log file operation manager copy-progress event details.
   * @param {Event} event An event.
   * @private
   */
  onCopyProgress_(event) {
    event = /** @type {FileOperationProgressEvent} */ (event);
    if (event.reason === 'BEGIN') {
      this.events.push(event);
      this.numberOfBeginEvents++;
    }
    if (event.reason === 'ERROR') {
      this.events.push(event);
      this.numberOfErrorEvents++;
    }
    if (event.reason === 'SUCCESS') {
      this.events.push(event);
      this.numberOfSuccessEvents++;
    }
  }
}

/**
 * Provides fake implementation of chrome.fileManagerPrivate.startCopy.
 */
class BlockableFakeStartCopy {
  /**
   * @param {string} blockedDestination Destination url of an entry whose
   *     request should be blocked.
   * @param {!Entry} sourceEntry Source entry. Single source entry is supported.
   * @param {!Array<!MockFileSystem>} fileSystems File systems array.
   */
  constructor(blockedDestination, sourceEntry, fileSystems) {
    this.resolveBlockedOperationCallback = null;
    this.blockedDestination_ = blockedDestination;
    this.sourceEntry_ = sourceEntry;
    this.fileSystems_ = fileSystems;
    this.startCopyId_ = 0;
  }

  /**
   * Fake implementation of startCopy function.
   * @param {!Entry} source
   * @param {!Entry} destination
   * @param {string} newName
   * @param {function(number)} callback
   */
  startCopyFunc(source, destination, newName, callback) {
    const makeStatus = type => {
      return {
        type: type,
        sourceUrl: source.toURL(),
        destinationUrl: destination.toURL()
      };
    };

    const completeCopyOperation = copyId => {
      const newPath = joinPath('/', newName);
      const fileSystem =
          getFileSystemForURL(this.fileSystems_, destination.toURL());
      const mockEntry = /** @type {!MockEntry} */ (this.sourceEntry_);
      fileSystem.entries[newPath] =
          /** @type {!MockEntry} */ (mockEntry.clone(newPath));
      listener(copyId, makeStatus('end_copy'));
      listener(copyId, makeStatus('success'));
    };

    this.startCopyId_++;

    callback(this.startCopyId_);
    const listener = mockChrome.fileManagerPrivate.onCopyProgress.listener_;
    listener(this.startCopyId_, makeStatus('begin'));
    listener(this.startCopyId_, makeStatus('progress'));

    if (destination.toURL() === this.blockedDestination_) {
      this.resolveBlockedOperationCallback =
          completeCopyOperation.bind(this, this.startCopyId_);
    } else {
      completeCopyOperation(this.startCopyId_);
    }
  }
}

/**
 * Fake volume manager.
 */
class FakeVolumeManager {
  /**
   * Returns fake volume info.
   * @param {!Entry} entry
   * @return {!Object}
   */
  getVolumeInfo(entry) {
    return {volumeId: entry.filesystem.name};
  }
  /**
   * Return fake location info.
   * @param {!Entry} entry
   * @return {?EntryLocation}
   */
  getLocationInfo(entry) {
    return /** @type {!EntryLocation} */ ({
      rootType: 'downloads',
      volumeInfo:
          {volumeType: 'downloads', label: 'Downloads', remoteMountPath: ''}
    });
  }
}

/**
 * Returns file system of the url.
 * @param {!Array<!MockFileSystem>} fileSystems
 * @param {string} url
 * @return {!MockFileSystem}
 */
function getFileSystemForURL(fileSystems, url) {
  for (let i = 0; i < fileSystems.length; i++) {
    if (new RegExp('^filesystem:' + fileSystems[i].name + '/').test(url)) {
      return fileSystems[i];
    }
  }

  throw new Error('Unexpected url: ' + url);
}

/**
 * Size of directory.
 * @type {number}
 * @const
 */
const DIRECTORY_SIZE = -1;

/**
 * Creates test file system.
 * @param {string} id File system Id.
 * @param {Object<number>} entries Map of entry paths and their size.
 *     If the entry size is DIRECTORY_SIZE, the entry is a directory.
 * @return {!MockFileSystem}
 */
function createTestFileSystem(id, entries) {
  const fileSystem = new MockFileSystem(id, 'filesystem:' + id);
  for (const path in entries) {
    if (entries[path] === DIRECTORY_SIZE) {
      fileSystem.entries[path] = MockDirectoryEntry.create(fileSystem, path);
    } else {
      const metadata = /** @type {!Metadata} */ ({size: entries[path]});
      fileSystem.entries[path] =
          MockFileEntry.create(fileSystem, path, metadata);
    }
  }
  return fileSystem;
}

/**
 * Resolves URL on the file system.
 * @param {!MockFileSystem} fileSystem File system.
 * @param {string} url URL.
 * @param {function(!Entry)} success Success callback.
 * @param {function(!FileError)=} opt_failure Failure callback.
 */
function resolveTestFileSystemURL(fileSystem, url, success, opt_failure) {
  for (const name in fileSystem.entries) {
    const entry = fileSystem.entries[name];
    if (entry.toURL() == url) {
      success(entry);
      return;
    }
  }

  if (opt_failure) {
    opt_failure(new FileError());
  }
}

/**
 * Waits for events until 'success' or 'error'.
 * @param {FileOperationManager} fileOperationManager File operation manager.
 * @return {Promise} Promise to be fulfilled with an event list.
 */
function waitForEvents(fileOperationManager) {
  return new Promise(fulfill => {
    const events = [];
    fileOperationManager.addEventListener('copy-progress', event => {
      event = /** @type {FileOperationProgressEvent} */ (event);
      events.push(event);
      if (event.reason === 'SUCCESS' || event.reason === 'ERROR') {
        fulfill(events);
      }
    });
    fileOperationManager.addEventListener('entries-changed', event => {
      event = /** @type {FileOperationProgressEvent} */ (event);
      events.push(event);
    });
    fileOperationManager.addEventListener('delete', event => {
      event = /** @type {FileOperationProgressEvent} */ (event);
      events.push(event);
      if (event.reason === 'SUCCESS' || event.reason === 'ERROR') {
        fulfill(events);
      }
    });
  });
}

/**
 * Placeholder for mocked volume manager.
 * @type {(FakeVolumeManager|{getVolumeInfo: function()}?)}
 */
let volumeManager;

/**
 * Provide VolumeManager.getInstance() for FileOperationManager using mocked
 * volume manager instance.
 * @return {Promise}
 */
volumeManagerFactory.getInstance = () => {
  return Promise.resolve(volumeManager);
};

/**
 * Test target.
 * @type {FileOperationManagerImpl}
 */
let fileOperationManager;

/**
 * Initializes the test environment.
 */
export function setUp() {
  // Mock LoadTimeData strings.
  loadTimeData.resetForTesting({
    'FILES_TRASH_ENABLED': true,
  });
  loadTimeData.getBoolean = function(key) {
    return loadTimeData.data_[key];
  };
  loadTimeData.getString = id => id;

  // Install mock chrome APIs.
  installMockChrome(mockChrome);
}

/**
 * Tests the fileOperationUtil.resolvePath function.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testResolvePath(callback) {
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file': 10,
    '/directory': DIRECTORY_SIZE,
  });
  const root = fileSystem.root;
  const rootPromise = fileOperationUtil.resolvePath(root, '/');
  const filePromise = fileOperationUtil.resolvePath(root, '/file');
  const directoryPromise = fileOperationUtil.resolvePath(root, '/directory');
  const errorPromise =
      fileOperationUtil.resolvePath(root, '/not_found')
          .then(
              () => {
                assertTrue(false, 'The NOT_FOUND error is not reported.');
              },
              error => {
                return error.name;
              });
  reportPromise(
      Promise
          .all([
            rootPromise,
            filePromise,
            directoryPromise,
            errorPromise,
          ])
          .then(results => {
            assertArrayEquals(
                [
                  fileSystem.entries['/'],
                  fileSystem.entries['/file'],
                  fileSystem.entries['/directory'],
                  'NotFoundError',
                ],
                results);
          }),
      callback);
}

/**
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testFindEntriesRecursively(callback) {
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file.txt': 10,
    '/file (1).txt': 10,
    '/file (2).txt': 10,
    '/file (3).txt': 10,
    '/file (4).txt': 10,
    '/file (5).txt': 10,
    '/DCIM/': DIRECTORY_SIZE,
    '/DCIM/IMG_1232.txt': 10,
    '/DCIM/IMG_1233 (7).txt': 10,
    '/DCIM/IMG_1234 (8).txt': 10,
    '/DCIM/IMG_1235 (9).txt': 10,
  });

  const foundFiles = [];
  fileOperationUtil
      .findEntriesRecursively(
          fileSystem.root,
          fileEntry => {
            foundFiles.push(fileEntry);
          })
      .then(() => {
        assertEquals(12, foundFiles.length);
        callback(false);
      })
      .catch(() => {
        const error = true;
        callback(error);
      });
}

/**
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testFindFilesRecursively(callback) {
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file.txt': 10,
    '/file (1).txt': 10,
    '/file (2).txt': 10,
    '/file (3).txt': 10,
    '/file (4).txt': 10,
    '/file (5).txt': 10,
    '/DCIM/': DIRECTORY_SIZE,
    '/DCIM/IMG_1232.txt': 10,
    '/DCIM/IMG_1233 (7).txt': 10,
    '/DCIM/IMG_1234 (8).txt': 10,
    '/DCIM/IMG_1235 (9).txt': 10,
  });

  const foundFiles = [];
  fileOperationUtil
      .findFilesRecursively(
          fileSystem.root,
          fileEntry => {
            foundFiles.push(fileEntry);
          })
      .then(() => {
        assertEquals(10, foundFiles.length);
        foundFiles.forEach(entry => {
          assertTrue(entry.isFile);
        });
        callback(false);
      })
      .catch(() => {
        const error = true;
        callback(error);
      });
}

/**
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testGatherEntriesRecursively(callback) {
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/file.txt': 10,
    '/file (1).txt': 10,
    '/file (2).txt': 10,
    '/file (3).txt': 10,
    '/file (4).txt': 10,
    '/file (5).txt': 10,
    '/DCIM/': DIRECTORY_SIZE,
    '/DCIM/IMG_1232.txt': 10,
    '/DCIM/IMG_1233 (7).txt': 10,
    '/DCIM/IMG_1234 (8).txt': 10,
    '/DCIM/IMG_1235 (9).txt': 10,
  });

  fileOperationUtil.gatherEntriesRecursively(fileSystem.root)
      .then(gatheredFiles => {
        assertEquals(12, gatheredFiles.length);
        callback(false);
      })
      .catch(() => {
        const error = true;
        callback(error);
      });
}

/**
 * Tests the fileOperationUtil.deduplicatePath
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testDeduplicatePath(callback) {
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

  const nonExistingPromise =
      fileOperationUtil.deduplicatePath(fileSystem1.root, 'file.txt')
          .then(path => {
            assertEquals('file.txt', path);
          });
  const existingPathPromise =
      fileOperationUtil.deduplicatePath(fileSystem2.root, 'file.txt')
          .then(path => {
            assertEquals('file (1).txt', path);
          });
  const moreExistingPathPromise =
      fileOperationUtil.deduplicatePath(fileSystem3.root, 'file.txt')
          .then(path => {
            assertEquals('file (10).txt', path);
          });

  const testPromise = Promise.all([
    nonExistingPromise,
    existingPathPromise,
    moreExistingPathPromise,
  ]);
  reportPromise(testPromise, callback);
}

/**
 * Tests fileOperationManager copy.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testCopy(callback) {
  // Prepare entries and their resolver.
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = (url, success, failure) => {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  mockChrome.fileManagerPrivate.startCopy =
      (source, destination, newName, callback) => {
        const makeStatus = type => {
          return {
            type: type,
            sourceUrl: source.toURL(),
            destinationUrl: destination.toURL()
          };
        };
        callback(1);
        const listener = mockChrome.fileManagerPrivate.onCopyProgress.listener_;
        listener(1, makeStatus('begin'));
        listener(1, makeStatus('progress'));
        const newPath = joinPath('/', newName);
        const entry = /** @type {!MockEntry} */
            (fileSystem.entries['/test.txt']);
        fileSystem.entries[newPath] =
            /** @type {!MockEntry} */ (entry.clone(newPath));
        listener(1, makeStatus('end_copy'));
        listener(1, makeStatus('success'));
      };

  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();

  // Observe the file operation manager's events.
  const eventsPromise = waitForEvents(fileOperationManager);

  // Verify the events.
  reportPromise(
      eventsPromise.then(events => {
        const firstEvent = events[0];
        assertEquals('BEGIN', firstEvent.reason);
        assertEquals(1, firstEvent.status.numRemainingItems);
        assertEquals(0, firstEvent.status.processedBytes);
        assertEquals(1, firstEvent.status.totalBytes);

        const lastEvent = events[events.length - 1];
        assertEquals('SUCCESS', lastEvent.reason);
        assertEquals(0, lastEvent.status.numRemainingItems);
        assertEquals(10, lastEvent.status.processedBytes);
        assertEquals(10, lastEvent.status.totalBytes);

        assertTrue(events.some(event => {
          return event.type === 'entries-changed' &&
              event.kind === util.EntryChangedKind.CREATED &&
              event.entries[0].fullPath === '/test (1).txt';
        }));

        assertFalse(events.some(event => {
          return event.type === 'delete';
        }));
      }),
      callback);

  fileOperationManager.paste(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/']), false);
}

/**
 * Tests copying files when the destination volumes are same: the copy
 * operations should be run sequentially.
 */
export function testCopyInSequential(callback) {
  // Prepare entries and their resolver.
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/dest': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = (url, success, failure) => {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  const blockableFakeStartCopy = new BlockableFakeStartCopy(
      'filesystem:testVolume/dest', fileSystem.entries['/test.txt'],
      [fileSystem]);
  mockChrome.fileManagerPrivate.startCopy =
      blockableFakeStartCopy.startCopyFunc.bind(blockableFakeStartCopy);

  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();

  // Observe the file operation manager's events.
  const eventLogger = new EventLogger(fileOperationManager);

  // Copy test.txt to /dest. This operation should be blocked.
  fileOperationManager.paste(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/dest']), false);

  let firstOperationTaskId;
  reportPromise(
      waitUntil(() => {
        // Wait until the first operation is blocked.
        return blockableFakeStartCopy.resolveBlockedOperationCallback !== null;
      })
          .then(() => {
            assertEquals(1, eventLogger.events.length);
            assertEquals('BEGIN', eventLogger.events[0].reason);
            firstOperationTaskId = eventLogger.events[0].taskId;

            // Copy test.txt to /. This operation should be blocked.
            fileOperationManager.paste(
                [fileSystem.entries['/test.txt']],
                /** @type {!DirectoryEntry} */ (fileSystem.entries['/']),
                false);

            return waitUntil(() => {
              return fileOperationManager.getPendingCopyTasksForTesting()
                         .length === 1;
            });
          })
          .then(() => {
            // Asserts that the second operation is added to pending copy tasks.
            // Current implementation run tasks synchronusly after adding it to
            // pending tasks.
            // TODO(yawano) This check deeply depends on the implementation.
            // Find a
            //     better way to test this.
            const pendingTask =
                fileOperationManager.getPendingCopyTasksForTesting()[0];
            assertEquals(fileSystem.entries['/'], pendingTask.targetDirEntry);

            blockableFakeStartCopy.resolveBlockedOperationCallback();

            return waitUntil(() => {
              return eventLogger.numberOfSuccessEvents === 2;
            });
          })
          .then(() => {
            // Events should be the following.
            // BEGIN: first operation
            // BEGIN: second operation
            // SUCCESS: first operation
            // SUCCESS: second operation
            const events = eventLogger.events;
            assertEquals(4, events.length);
            assertEquals('BEGIN', events[0].reason);
            assertEquals(firstOperationTaskId, events[0].taskId);
            assertEquals('BEGIN', events[1].reason);
            assertTrue(events[1].taskId !== firstOperationTaskId);
            assertEquals('SUCCESS', events[2].reason);
            assertEquals(firstOperationTaskId, events[2].taskId);
            assertEquals('SUCCESS', events[3].reason);
            assertEquals(events[1].taskId, events[3].taskId);
          }),
      callback);
}

/**
 * Tests copying files when the destination volumes are different: the copy
 * operations should be run in parallel.
 */
export function testCopyInParallel(callback) {
  // Prepare entries and their resolver.
  const fileSystemA = createTestFileSystem('volumeA', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  const fileSystemB = createTestFileSystem('volumeB', {
    '/': DIRECTORY_SIZE,
  });
  const fileSystems = [fileSystemA, fileSystemB];

  window.webkitResolveLocalFileSystemURL = (url, success, failure) => {
    const system = getFileSystemForURL(fileSystems, url);
    resolveTestFileSystemURL(system, url, success, failure);
  };

  const blockableFakeStartCopy = new BlockableFakeStartCopy(
      'filesystem:volumeB/', fileSystemA.entries['/test.txt'], fileSystems);
  mockChrome.fileManagerPrivate.startCopy =
      blockableFakeStartCopy.startCopyFunc.bind(blockableFakeStartCopy);

  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();

  // Observe the file operation manager's events.
  const eventLogger = new EventLogger(fileOperationManager);

  // Copy test.txt from volume A to volume B.
  fileOperationManager.paste(
      [fileSystemA.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystemB.entries['/']), false);

  let firstOperationTaskId;
  reportPromise(
      waitUntil(() => {
        return blockableFakeStartCopy.resolveBlockedOperationCallback !== null;
      })
          .then(() => {
            assertEquals(1, eventLogger.events.length);
            assertEquals('BEGIN', eventLogger.events[0].reason);
            firstOperationTaskId = eventLogger.events[0].taskId;

            // Copy test.txt from volume A to volume A. This should not be
            // blocked by the previous operation.
            fileOperationManager.paste(
                [fileSystemA.entries['/test.txt']],
                /** @type {!DirectoryEntry} */ (fileSystemA.entries['/']),
                false);

            // Wait until the second operation is completed.
            return waitUntil(() => {
              return eventLogger.numberOfSuccessEvents === 1;
            });
          })
          .then(() => {
            // Resolve the blocked operation.
            blockableFakeStartCopy.resolveBlockedOperationCallback();

            // Wait until the blocked operation is completed.
            return waitUntil(() => {
              return eventLogger.numberOfSuccessEvents === 2;
            });
          })
          .then(() => {
            // Events should be following.
            // BEGIN: first operation
            // BEGIN: second operation
            // SUCCESS: second operation
            // SUCCESS: first operation
            const events = eventLogger.events;
            assertEquals(4, events.length);
            assertEquals('BEGIN', events[0].reason);
            assertEquals(firstOperationTaskId, events[0].taskId);
            assertEquals('BEGIN', events[1].reason);
            assertTrue(firstOperationTaskId !== events[1].taskId);
            assertEquals('SUCCESS', events[2].reason);
            assertEquals(events[1].taskId, events[2].taskId);
            assertEquals('SUCCESS', events[3].reason);
            assertEquals(firstOperationTaskId, events[3].taskId);
          }),
      callback);
}

/**
 * Tests that copy operations fail when the destination volume is not
 * available.
 */
export function testCopyFails(callback) {
  // Prepare entries.
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });

  volumeManager = {
    /* Mocking volume manager. */
    getVolumeInfo: function() {
      // Returns null to indicate that the volume is not available.
      return null;
    },
    getLocationInfo: function() {
      return null;
    }
  };
  fileOperationManager = new FileOperationManagerImpl();

  // Observe the file operation manager's events.
  const eventLogger = new EventLogger(fileOperationManager);

  // Copy test.txt to /, which should fail.
  fileOperationManager.paste(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/']), false);

  reportPromise(
      waitUntil(() => {
        return eventLogger.numberOfErrorEvents === 1;
      }).then(() => {
        // Since the task fails with an error, pending copy tasks should be
        // empty.
        assertEquals(
            0, fileOperationManager.getPendingCopyTasksForTesting().length);

        // Check events.
        const events = eventLogger.events;
        assertEquals(2, events.length);
        assertEquals('BEGIN', events[0].reason);
        assertEquals('ERROR', events[1].reason);
        assertEquals(events[0].taskId, events[1].taskId);
      }),
      callback);
}

/**
 * Tests the fileOperationUtil.paste for move.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testMove(callback) {
  // Prepare entries and their resolver.
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/directory': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = (url, success, failure) => {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();

  // Observe the file operation manager's events.
  const eventsPromise = waitForEvents(fileOperationManager);

  // Verify the events.
  reportPromise(
      eventsPromise.then(events => {
        const firstEvent = events[0];
        assertEquals('BEGIN', firstEvent.reason);
        assertEquals(1, firstEvent.status.numRemainingItems);
        assertEquals(0, firstEvent.status.processedBytes);
        assertEquals(1, firstEvent.status.totalBytes);

        const lastEvent = events[events.length - 1];
        assertEquals('SUCCESS', lastEvent.reason);
        assertEquals(0, lastEvent.status.numRemainingItems);
        assertEquals(1, lastEvent.status.processedBytes);
        assertEquals(1, lastEvent.status.totalBytes);

        assertTrue(events.some(event => {
          return event.type === 'entries-changed' &&
              event.kind === util.EntryChangedKind.DELETED &&
              event.entries[0].fullPath === '/test.txt';
        }));

        assertTrue(events.some(event => {
          return event.type === 'entries-changed' &&
              event.kind === util.EntryChangedKind.CREATED &&
              event.entries[0].fullPath === '/directory/test.txt';
        }));

        assertFalse(events.some(event => {
          return event.type === 'delete';
        }));
      }),
      callback);

  fileOperationManager.paste(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/directory']), true);
}

/**
 * Tests moving files within My files (Downloads excluded) or within Downloads.
 * When the source and the destination volumes are the same, the local
 * implementation of move should be used instead of copy + delete.
 */
export async function testMoveSameVolume(done) {
  // Prepare entries.
  const fileSystem = createTestFileSystem('downloads', {
    '/dest': DIRECTORY_SIZE,
    '/test.txt': 10,
    '/Downloads': DIRECTORY_SIZE,
    '/Downloads2': DIRECTORY_SIZE,
    '/Downloads/dest': DIRECTORY_SIZE,
    '/Downloads/test.txt': 10,
    '/dummyEntry.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = (url, success, failure) => {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  /**
   * Fake implementation of Entry.moveTo.
   * @param {!DirectoryEntry} destination
   * @param {string=} destinationName
   * @param {function(!Entry)=} success_callback
   * @param {function(!FileError)=} error_callback
   */
  const moveToFunc =
      (destination, destinationName, success_callback, error_callback) => {
        moveToCount++;
        // The callback argument is meant to be the entry created by
        // Entry.moveTo, it has no importance here.
        success_callback(fileSystem.entries['/dummyEntry.txt']);
      };

  // Override the moveTo method of the entries to move, as well as
  // fileManagerPrivate.startCopy. If the same-volume implementation is
  // used, moveTo is called but not startCopy.
  let moveToCount = 0;
  fileSystem.entries['/test.txt'].moveTo = moveToFunc;
  fileSystem.entries['/Downloads/test.txt'].moveTo = moveToFunc;

  let startCopyCount = 0;
  mockChrome.fileManagerPrivate.startCopy = () => {
    startCopyCount++;
  };

  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();

  const isMove = true;

  // Move /test.txt to /dest.
  fileOperationManager.paste(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/dest']), isMove);

  // Wait until fileSystem.entries['/test.txt'].moveTo is called.
  await waitUntil(() => moveToCount == 1 && !startCopyCount);

  // Move /test.txt to /Downloads2, Downloads2 is not a Downloads location, so a
  // local move can also be executed.
  fileOperationManager.paste(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/Downloads2']),
      isMove);

  // Wait until fileSystem.entries['/test.txt'].moveTo is called.
  await waitUntil(() => moveToCount == 2 && !startCopyCount);

  // Move /Downloads/test.txt to /Downloads/dest.
  fileOperationManager.paste(
      [fileSystem.entries['/Downloads/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/Downloads/dest']),
      isMove);

  // Wait until fileSystem.entries['/Downloads/test.txt'].moveTo is called.
  await waitUntil(() => moveToCount == 3 && !startCopyCount);

  done();
}

/**
 * Tests moving files between My files and Downloads: Downloads being a bind
 * mount into My files, the copy + delete implementation of move should be used.
 */
export async function testMoveBetweenMyFilesDownloads(done) {
  // Prepare entries and their resolver.
  const fileSystem = createTestFileSystem('downloads', {
    '/Downloads': DIRECTORY_SIZE,
    '/Downloads/test.txt': 10,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = (url, success, failure) => {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  /**
   * Fake implementation of fileManagerPrivate.startCopy.
   * @param {!Entry} source
   * @param {!Entry} destination
   * @param {string} newName
   * @param {function(number)} callback
   */
  const startCopyFunc = (source, destination, newName, callback) => {
    const makeStatus = type => {
      return {
        type: type,
        sourceUrl: source.toURL(),
        destinationUrl: destination.toURL()
      };
    };

    startCopyCount++;

    // To signal to the background that the copy is finished, we need to
    // provide its copyId first.
    callback(copyId);

    const listener = mockChrome.fileManagerPrivate.onCopyProgress.listener_;
    // The success status is dispatched to the background to indicate that
    // the copy associated with copyId has terminated.
    listener(copyId, makeStatus('success'));

    copyId++;
  };

  // Override the moveTo method of the entries to move, as well as
  // fileManagerPrivate.startCopy. If the cross-filesystem implementation is
  // used, startCopy is called (followed by the removal of the source entry) but
  // not moveTo.
  let moveToCount = 0;
  fileSystem.entries['/test.txt'].moveTo = () => {
    moveToCount++;
  };
  fileSystem.entries['/Downloads/test.txt'].moveTo = () => {
    moveToCount++;
  };

  let startCopyCount = 0;
  let copyId = 0;
  mockChrome.fileManagerPrivate.startCopy = startCopyFunc;

  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();

  const isMove = true;

  // Move /test.txt to /Downloads/.
  fileOperationManager.paste(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/Downloads']),
      isMove);

  // Wait until fileManagerPrivate.startCopy is called
  await waitUntil(() => startCopyCount == 1 && !moveToCount);

  // Move /Downloads/test.txt to /.
  fileOperationManager.paste(
      [fileSystem.entries['/Downloads/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/']), isMove);

  // Wait until fileManagerPrivate.startCopy is called a second time.
  await waitUntil(() => startCopyCount == 2 && !moveToCount);

  done();
}

/**
 * Tests fileOperationManager.deleteEntries.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testDelete(callback) {
  // Prepare entries and their resolver.
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = (url, success, failure) => {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  // Observing manager's events.
  reportPromise(
      waitForEvents(fileOperationManager).then(events => {
        assertEquals('delete', events[0].type);
        assertEquals('DELETE', events[0].status.operationType);
        assertEquals('BEGIN', events[0].reason);
        assertEquals(10, events[0].status.totalBytes);
        assertEquals(0, events[0].status.processedBytes);

        const lastEvent = events[events.length - 1];
        assertEquals('delete', lastEvent.type);
        assertEquals('DELETE', lastEvent.status.operationType);
        assertEquals('SUCCESS', lastEvent.reason);
        assertEquals(10, lastEvent.status.totalBytes);
        assertEquals(10, lastEvent.status.processedBytes);

        assertFalse(events.some(e => e.type === 'copy-progress'));
      }),
      callback);

  fileOperationManager.deleteEntries([fileSystem.entries['/test.txt']]);
}

/**
 * Tests fileOperationManager.restore.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testRestore(callback) {
  // Prepare entries and their resolver.
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = (url, success, failure) => {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  const onDeleted = (e) => {
    if (e.status.operationType !== 'DELETE' || e.reason !== 'SUCCESS') {
      return;
    }
    fileOperationManager.removeEventListener('delete', onDeleted);

    // Step 2. Once we receive 'DELETE' 'COMPLETED', observe 'RESTORE' events.
    reportPromise(
        waitForEvents(fileOperationManager).then(events => {
          // Step 4. Validate restore events.
          assertEquals('delete', events[0].type);
          assertEquals('RESTORE', events[0].status.operationType);
          assertEquals('BEGIN', events[0].reason);
          assertEquals(10, events[0].status.totalBytes);
          assertEquals(0, events[0].status.processedBytes);

          const lastEvent = events[events.length - 1];
          assertEquals('delete', lastEvent.type);
          assertEquals('RESTORE', lastEvent.status.operationType);
          assertEquals('SUCCESS', lastEvent.reason);
          assertEquals(10, lastEvent.status.totalBytes);
          assertEquals(10, lastEvent.status.processedBytes);

          assertFalse(events.some(e => e.type === 'copy-progress'));
        }),
        callback);

    // Step 3. Restore files.
    fileOperationManager.restoreDeleted(e.trashedEntries);
  };


  // Step 1. Delete files.
  fileOperationManager.addEventListener('delete', onDeleted);
  fileOperationManager.deleteEntries([fileSystem.entries['/test.txt']]);
}

/**
 * Tests fileOperationManager.zipSelection() under normal conditions.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testZipProgress(callback) {
  // Prepare entries and their resolver.
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = function(url, success, failure) {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  const destSize = 9876;
  const wantZipId = 42;
  const maxSteps = 5;
  let step = 0;

  mockChrome.fileManagerPrivate.zipSelection =
      (sources, parent, newName, callback) => {
        setTimeout(callback, 100, wantZipId, destSize);
      };

  mockChrome.fileManagerPrivate.getZipProgress = (zipId, callback) => {
    assertEquals(wantZipId, zipId);
    ++step;
    const result = step < maxSteps ? -1 : 0;
    const bytes = Math.round(destSize * step / maxSteps);
    setTimeout(callback, 100, result, bytes);
  };

  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();

  // Observing manager's events.
  reportPromise(
      waitForEvents(fileOperationManager).then(events => {
        assertEquals('copy-progress', events[0].type);
        assertEquals('BEGIN', events[0].reason);
        assertEquals(1, events[0].status.totalBytes);
        assertEquals(0, events[0].status.processedBytes);

        const lastEvent = events[events.length - 1];
        assertEquals('copy-progress', lastEvent.type);
        assertEquals('SUCCESS', lastEvent.reason);
        assertEquals(destSize, lastEvent.status.totalBytes);
        assertEquals(destSize, lastEvent.status.processedBytes);

        assertFalse(events.some(event => {
          return event.type === 'delete';
        }));
      }),
      callback);

  fileOperationManager.zipSelection(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/']));
}

/**
 * Tests fileOperationManager.zipSelection() in case of error.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export function testZipError(callback) {
  // Prepare entries and their resolver.
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = function(url, success, failure) {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  const destSize = 9876;
  const wantZipId = 42;

  mockChrome.fileManagerPrivate.zipSelection =
      (sources, parent, newName, callback) => {
        setTimeout(callback, 100, wantZipId, destSize);
      };

  mockChrome.fileManagerPrivate.getZipProgress = (zipId, callback) => {
    assertEquals(wantZipId, zipId);
    const result = 2;  // Positive error code.
    setTimeout(callback, 100, result, 3524);
  };

  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();

  // Observing manager's events.
  reportPromise(
      waitForEvents(fileOperationManager).then(events => {
        assertEquals('copy-progress', events[0].type);
        assertEquals('BEGIN', events[0].reason);
        assertEquals(1, events[0].status.totalBytes);
        assertEquals(0, events[0].status.processedBytes);

        const lastEvent = events[events.length - 1];
        assertEquals('copy-progress', lastEvent.type);
        assertEquals('ERROR', lastEvent.reason);
        assertEquals(destSize, lastEvent.status.totalBytes);
        assertEquals(0, lastEvent.status.processedBytes);

        assertFalse(events.some(event => {
          return event.type === 'delete';
        }));
      }),
      callback);

  fileOperationManager.zipSelection(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/']));
}

/**
 * Tests ZipTask initialization.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export async function testZipTaskInit(callback) {
  try {
    const taskId = 'file-operation-23460982';
    const fileSystem = createTestFileSystem('testVolume', {
      '/out': DIRECTORY_SIZE,
      '/in': DIRECTORY_SIZE,
      '/in/input 1.txt': 10,
      '/in/input 2.txt': 20,
    });

    const sourceEntries = [
      fileSystem.entries['/in/input 1.txt'],
      fileSystem.entries['/in/input 2.txt'],
    ];
    const baseEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/in']);
    const targetEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/out']);

    const task = new fileOperationUtil.ZipTask(
        taskId, sourceEntries, targetEntry, baseEntry);

    assertEquals(util.FileOperationType.ZIP, task.operationType);
    assertEquals(taskId, task.taskId);
    assertEquals(baseEntry, task.zipBaseDirEntry);
    assertEquals(targetEntry, task.targetDirEntry);
    assertArrayEquals(sourceEntries, task.sourceEntries);

    await new Promise(resolve => task.initialize(resolve));

    // The real number of bytes is computed in task.run().
    assertEquals(1, task.totalBytes);

    callback(false);
  } catch (error) {
    console.error(error);
    callback(true);
  }
}

/**
 * Tests ZipTask progress in normal circumstances.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export async function testZipTaskRun(callback) {
  try {
    const taskId = 'file-operation-23460982';
    const fileSystem = createTestFileSystem('testVolume', {
      '/out': DIRECTORY_SIZE,
      '/in': DIRECTORY_SIZE,
      '/in/input 1.txt': 10,
      '/in/input 2.txt': 20,
    });

    const sourceEntries = [
      fileSystem.entries['/in/input 1.txt'],
      fileSystem.entries['/in/input 2.txt'],
    ];
    const baseEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/in']);
    const targetEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/out']);

    const task = new fileOperationUtil.ZipTask(
        taskId, sourceEntries, targetEntry, baseEntry);

    await new Promise(resolve => task.initialize(resolve));

    const entryChangedCallback = () => assertNotReached();

    let progressCount = 0;
    const progressCallback = () => void ++progressCount;

    const destSize = 9876;
    const wantZipId = 42;
    const maxSteps = 5;
    let step = 0;

    let zipSelectionCount = 0;
    mockChrome.fileManagerPrivate.zipSelection =
        (sources, parent, newName, callback) => {
          assertEquals(0, zipSelectionCount++);
          assertEquals(0, step);
          assertArrayEquals(sourceEntries, sources);
          const newPath = joinPath('/out', newName);
          const newEntry = MockFileEntry.create(
              fileSystem, newPath, /** @type {!Metadata} */ ({size: destSize}));
          fileSystem.entries[newPath] = newEntry;
          setTimeout(callback, 100, wantZipId, destSize);
        };

    mockChrome.fileManagerPrivate.getZipProgress = (zipId, callback) => {
      assertEquals(wantZipId, zipId);
      // By now, task.totalBytes should be set to the expected value.
      assertEquals(destSize, task.totalBytes);
      assertLT(step, maxSteps);
      assertEquals(step, progressCount);
      ++step;
      const result = step < maxSteps ? -1 : 0;
      const bytes = Math.round(destSize * step / maxSteps);
      setTimeout(callback, 100, result, bytes);
    };

    mockChrome.fileManagerPrivate.cancelZip = (zipId) => assertNotReached();

    await new Promise(
        (resolve, reject) =>
            task.run(entryChangedCallback, progressCallback, resolve, reject));

    assertEquals(maxSteps, progressCount);
    assertEquals(maxSteps, step);

    callback(false);
  } catch (error) {
    console.error(error);
    callback(true);
  }
}

/**
 * Tests ZipTask cancellation before run() is called.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export async function testZipTaskCancellationBeforeRun(callback) {
  try {
    const taskId = 'file-operation-23460982';
    const fileSystem = createTestFileSystem('testVolume', {
      '/out': DIRECTORY_SIZE,
      '/in': DIRECTORY_SIZE,
      '/in/input 1.txt': 10,
      '/in/input 2.txt': 20,
    });

    const sourceEntries = [
      fileSystem.entries['/in/input 1.txt'],
      fileSystem.entries['/in/input 2.txt'],
    ];
    const baseEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/in']);
    const targetEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/out']);

    const task = new fileOperationUtil.ZipTask(
        taskId, sourceEntries, targetEntry, baseEntry);

    await new Promise(resolve => task.initialize(resolve));

    mockChrome.fileManagerPrivate.zipSelection = () => assertNotReached();
    mockChrome.fileManagerPrivate.getZipProgress = () => assertNotReached();
    mockChrome.fileManagerPrivate.cancelZip = () => assertNotReached();

    // Request cancellation before running the task.
    task.requestCancel();

    const entryChangedCallback = () => assertNotReached();
    const progressCallback = () => assertNotReached();
    const successCallback = () => assertNotReached();
    await new Promise(
        resolve => task.run(
            entryChangedCallback, progressCallback, successCallback, error => {
              assertTrue(error instanceof FileOperationError);
              assertEquals(
                  util.FileOperationErrorType.FILESYSTEM_ERROR, error.code);
              const domError = /** @type {!DOMError} */ (error.data);
              assertTrue(domError instanceof DOMError);
              assertEquals(util.FileError.ABORT_ERR, domError.name);
              resolve();
            }));

    callback(false);
  } catch (error) {
    console.error(error);
    callback(true);
  }
}

/**
 * Tests ZipTask cancellation when it is requested while
 * fileManagerPrivate.zipSelection() is running.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export async function testZipTaskCancellationDuringRunStart(callback) {
  try {
    const taskId = 'file-operation-23460982';
    const fileSystem = createTestFileSystem('testVolume', {
      '/out': DIRECTORY_SIZE,
      '/in': DIRECTORY_SIZE,
      '/in/input 1.txt': 10,
      '/in/input 2.txt': 20,
    });

    const sourceEntries = [
      fileSystem.entries['/in/input 1.txt'],
      fileSystem.entries['/in/input 2.txt'],
    ];
    const baseEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/in']);
    const targetEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/out']);

    const task = new fileOperationUtil.ZipTask(
        taskId, sourceEntries, targetEntry, baseEntry);

    await new Promise(resolve => task.initialize(resolve));

    const destSize = 9876;
    const wantZipId = 42;

    let zipSelectionCount = 0;
    let cancelZipCount = 0;
    let getZipProgressCount = 0;

    mockChrome.fileManagerPrivate.zipSelection =
        (sources, parent, newName, callback) => {
          assertEquals(0, zipSelectionCount++);
          assertEquals(0, cancelZipCount);
          // Request cancellation during fileManagerPrivate.zipSelection().
          setTimeout(() => task.requestCancel(), 50);
          setTimeout(callback, 100, wantZipId, destSize);
        };

    mockChrome.fileManagerPrivate.cancelZip = (zipId) => {
      assertEquals(0, cancelZipCount++);
      assertEquals(1, zipSelectionCount);
      assertEquals(wantZipId, zipId);
    };

    mockChrome.fileManagerPrivate.getZipProgress = (zipId, callback) => {
      assertEquals(0, getZipProgressCount++);
      assertEquals(1, zipSelectionCount);
      assertEquals(1, cancelZipCount);
      assertEquals(wantZipId, zipId);
      const result = +1;  // Cancelled
      const bytes = 0;
      setTimeout(callback, 100, result, bytes);
    };

    const entryChangedCallback = () => assertNotReached();
    const progressCallback = () => assertNotReached();
    const successCallback = () => assertNotReached();
    await new Promise(
        resolve => task.run(
            entryChangedCallback, progressCallback, successCallback, error => {
              assertTrue(error instanceof FileOperationError);
              assertEquals(
                  util.FileOperationErrorType.FILESYSTEM_ERROR, error.code);
              const domError = /** @type {!DOMError} */ (error.data);
              assertTrue(domError instanceof DOMError);
              assertEquals(util.FileError.ABORT_ERR, domError.name);
              resolve();
            }));

    assertEquals(1, getZipProgressCount);
    assertEquals(1, zipSelectionCount);
    assertEquals(1, cancelZipCount);

    callback(false);
  } catch (error) {
    console.error(error);
    callback(true);
  }
}

/**
 * Tests ZipTask cancellation when it is requested while
 * fileManagerPrivate.getZipProgress() is running.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export async function testZipTaskCancellationDuringProgress(callback) {
  try {
    const taskId = 'file-operation-23460982';
    const fileSystem = createTestFileSystem('testVolume', {
      '/out': DIRECTORY_SIZE,
      '/in': DIRECTORY_SIZE,
      '/in/input 1.txt': 10,
      '/in/input 2.txt': 20,
    });

    const sourceEntries = [
      fileSystem.entries['/in/input 1.txt'],
      fileSystem.entries['/in/input 2.txt'],
    ];
    const baseEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/in']);
    const targetEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/out']);

    const task = new fileOperationUtil.ZipTask(
        taskId, sourceEntries, targetEntry, baseEntry);

    await new Promise(resolve => task.initialize(resolve));

    const destSize = 9876;
    const wantZipId = 42;

    let zipSelectionCount = 0;
    let cancelZipCount = 0;
    let getZipProgressCount = 0;

    mockChrome.fileManagerPrivate.zipSelection =
        (sources, parent, newName, callback) => {
          assertEquals(0, zipSelectionCount++);
          assertEquals(0, cancelZipCount);
          setTimeout(callback, 100, wantZipId, destSize);
        };

    mockChrome.fileManagerPrivate.cancelZip = (zipId) => {
      assertEquals(0, cancelZipCount++);
      assertEquals(1, zipSelectionCount);
      assertEquals(1, getZipProgressCount);
      assertEquals(wantZipId, zipId);
    };

    mockChrome.fileManagerPrivate.getZipProgress = (zipId, callback) => {
      getZipProgressCount++;
      assertEquals(1, zipSelectionCount);
      assertEquals(wantZipId, zipId);
      const bytes = 0;
      if (getZipProgressCount == 1) {
        assertEquals(0, cancelZipCount);
        // Request cancellation during fileManagerPrivate.getZipProgress().
        setTimeout(() => task.requestCancel(), 50);
        const result = -1;  // In progress
        setTimeout(callback, 100, result, bytes);
      } else {
        assertEquals(2, getZipProgressCount);
        assertEquals(1, cancelZipCount);
        const result = +1;  // Cancelled
        setTimeout(callback, 100, result, bytes);
      }
    };

    let progressCallbackCount = 0;
    const progressCallback = () => void ++progressCallbackCount;
    const entryChangedCallback = () => assertNotReached();
    const successCallback = () => assertNotReached();
    await new Promise(
        resolve => task.run(
            entryChangedCallback, progressCallback, successCallback, error => {
              assertTrue(error instanceof FileOperationError);
              assertEquals(
                  util.FileOperationErrorType.FILESYSTEM_ERROR, error.code);
              const domError = /** @type {!DOMError} */ (error.data);
              assertTrue(domError instanceof DOMError);
              assertEquals(util.FileError.ABORT_ERR, domError.name);
              resolve();
            }));

    assertEquals(1, zipSelectionCount);
    assertEquals(2, getZipProgressCount);
    assertEquals(1, cancelZipCount);
    assertEquals(1, progressCallbackCount);

    callback(false);
  } catch (error) {
    console.error(error);
    callback(true);
  }
}

/**
 * Tests ZipTask error during fileManagerPrivate.getZipProgress().
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
export async function testZipTaskError(callback) {
  try {
    const taskId = 'file-operation-23460982';
    const fileSystem = createTestFileSystem('testVolume', {
      '/out': DIRECTORY_SIZE,
      '/in': DIRECTORY_SIZE,
      '/in/input 1.txt': 10,
      '/in/input 2.txt': 20,
    });

    const sourceEntries = [
      fileSystem.entries['/in/input 1.txt'],
      fileSystem.entries['/in/input 2.txt'],
    ];
    const baseEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/in']);
    const targetEntry =
        /** @type {!DirectoryEntry} */ (fileSystem.entries['/out']);

    const task = new fileOperationUtil.ZipTask(
        taskId, sourceEntries, targetEntry, baseEntry);

    await new Promise(resolve => task.initialize(resolve));

    const destSize = 9876;
    const wantZipId = 42;

    let zipSelectionCount = 0;
    let getZipProgressCount = 0;

    mockChrome.fileManagerPrivate.zipSelection =
        (sources, parent, newName, callback) => {
          assertEquals(0, zipSelectionCount++);
          setTimeout(callback, 100, wantZipId, destSize);
        };

    mockChrome.fileManagerPrivate.cancelZip = () => assertNotReached();

    mockChrome.fileManagerPrivate.getZipProgress = (zipId, callback) => {
      assertEquals(0, getZipProgressCount++);
      assertEquals(1, zipSelectionCount);
      assertEquals(wantZipId, zipId);
      const result = +2;  // Error
      const bytes = 0;
      setTimeout(callback, 100, result, bytes);
    };

    const progressCallback = () => assertNotReached();
    const entryChangedCallback = () => assertNotReached();
    const successCallback = () => assertNotReached();
    await new Promise(
        resolve => task.run(
            entryChangedCallback, progressCallback, successCallback, error => {
              assertTrue(error instanceof FileOperationError);
              assertEquals(
                  util.FileOperationErrorType.FILESYSTEM_ERROR, error.code);
              const domError = /** @type {!DOMError} */ (error.data);
              assertTrue(domError instanceof DOMError);
              assertEquals(
                  util.FileError.INVALID_MODIFICATION_ERR, domError.name);
              resolve();
            }));

    assertEquals(1, zipSelectionCount);
    assertEquals(1, getZipProgressCount);

    callback(false);
  } catch (error) {
    console.error(error);
    callback(true);
  }
}

/**
 * Test writeFile() with file dragged from browser.
 */
export async function testWriteFile(done) {
  const fileSystem = createTestFileSystem('testVolume', {
    '/testdir': DIRECTORY_SIZE,
  });
  volumeManager = new FakeVolumeManager();
  fileOperationManager = new FileOperationManagerImpl();
  const file = new File(['content'], 'browserfile', {type: 'text/plain'});
  await fileOperationManager.writeFile(
      file, /** @type {!DirectoryEntry} */ (fileSystem.entries['/testdir']));
  const writtenEntry = fileSystem.entries['/testdir/browserfile'];
  assertEquals('content', await writtenEntry.content.text());
  done();
}
