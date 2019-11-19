// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

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
  }
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
      listener(copyId, makeStatus('end_copy_entry'));
      listener(copyId, makeStatus('success'));
    };

    this.startCopyId_++;

    callback(this.startCopyId_);
    var listener = mockChrome.fileManagerPrivate.onCopyProgress.listener_;
    listener(this.startCopyId_, makeStatus('begin_copy_entry'));
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
 * Waits for events until 'success'.
 * @param {FileOperationManager} fileOperationManager File operation manager.
 * @return {Promise} Promise to be fulfilled with an event list.
 */
function waitForEvents(fileOperationManager) {
  return new Promise(fulfill => {
    const events = [];
    fileOperationManager.addEventListener('copy-progress', event => {
      event = /** @type {FileOperationProgressEvent} */ (event);
      events.push(event);
      if (event.reason === 'SUCCESS') {
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
      if (event.reason === 'SUCCESS') {
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

var volumeManagerFactory = volumeManagerFactory || {};

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
function setUp() {
  // Install mock chrome APIs.
  installMockChrome(mockChrome);
}

/**
 * Tests the fileOperationUtil.resolvePath function.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
function testResolvePath(callback) {
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
function testFindEntriesRecursively(callback) {
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
function testFindFilesRecursively(callback) {
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
function testGatherEntriesRecursively(callback) {
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
function testDeduplicatePath(callback) {
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
function testCopy(callback) {
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
        listener(1, makeStatus('begin_copy_entry'));
        listener(1, makeStatus('progress'));
        const newPath = joinPath('/', newName);
        const entry = /** @type {!MockEntry} */
            (fileSystem.entries['/test.txt']);
        fileSystem.entries[newPath] =
            /** @type {!MockEntry} */ (entry.clone(newPath));
        listener(1, makeStatus('end_copy_entry'));
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
function testCopyInSequential(callback) {
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
function testCopyInParallel(callback) {
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
function testCopyFails(callback) {
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
function testMove(callback) {
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
 * Tests fileOperationManager.deleteEntries.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
function testDelete(callback) {
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
        assertEquals('BEGIN', events[0].reason);
        assertEquals(10, events[0].totalBytes);
        assertEquals(0, events[0].processedBytes);

        const lastEvent = events[events.length - 1];
        assertEquals('delete', lastEvent.type);
        assertEquals('SUCCESS', lastEvent.reason);
        assertEquals(10, lastEvent.totalBytes);
        assertEquals(10, lastEvent.processedBytes);

        assertFalse(events.some(event => {
          return event.type === 'copy-progress';
        }));
      }),
      callback);

  fileOperationManager.deleteEntries([fileSystem.entries['/test.txt']]);
}

/**
 * Tests fileOperationManager.zipSelection.
 * @param {function(boolean)} callback Callback to be passed true on error.
 */
function testZip(callback) {
  // Prepare entries and their resolver.
  const fileSystem = createTestFileSystem('testVolume', {
    '/': DIRECTORY_SIZE,
    '/test.txt': 10,
  });
  window.webkitResolveLocalFileSystemURL = function(url, success, failure) {
    resolveTestFileSystemURL(fileSystem, url, success, failure);
  };

  mockChrome.fileManagerPrivate.zipSelection = function(
      sources, parent, newName, success, error) {
    const newPath = joinPath('/', newName);
    const newEntry = MockFileEntry.create(
        fileSystem, newPath, /** @type {!Metadata} */ ({size: 10}));
    fileSystem.entries[newPath] = newEntry;
    success(newEntry);
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
        assertEquals(10, lastEvent.status.totalBytes);
        assertEquals(10, lastEvent.status.processedBytes);

        assertFalse(events.some(event => {
          return event.type === 'delete';
        }));

        assertTrue(events.some(event => {
          return event.type === 'entries-changed' &&
              event.entries[0].fullPath === '/test.zip';
        }));
      }),
      callback);

  fileOperationManager.zipSelection(
      [fileSystem.entries['/test.txt']],
      /** @type {!DirectoryEntry} */ (fileSystem.entries['/']));
}
