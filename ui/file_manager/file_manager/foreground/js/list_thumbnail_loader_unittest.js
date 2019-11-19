// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {string} */
let currentVolumeType;

/** @type {!ListThumbnailLoader} */
let listThumbnailLoader;

/** @type {!Object} */
let getCallbacks;

/** @type {!Array<Event>} */
let thumbnailLoadedEvents;

/** @type {!ThumbnailModel} */
let thumbnailModel;

/** @type {!MetadataModel} */
let metadataModel;

/** @type {!FileListModel} */
let fileListModel;

/** @type {!DirectoryModel} */
let directoryModel;

/** @type {boolean} */
let isScanningForTest;

/** @type {!MockFileSystem} */
const fileSystem = new MockFileSystem('volume-id');

/** @type {!DirectoryEntry} */
const directory1 = MockDirectoryEntry.create(fileSystem, '/TestDirectory');

/** @type {!MockEntry} */
const entry1 = new MockEntry(fileSystem, '/Test1.jpg');
/** @type {!MockEntry} */
const entry2 = new MockEntry(fileSystem, '/Test2.jpg');
/** @type {!MockEntry} */
const entry3 = new MockEntry(fileSystem, '/Test3.jpg');
/** @type {!MockEntry} */
const entry4 = new MockEntry(fileSystem, '/Test4.jpg');
/** @type {!MockEntry} */
const entry5 = new MockEntry(fileSystem, '/Test5.jpg');
/** @type {!MockEntry} */
const entry6 = new MockEntry(fileSystem, '/Test6.jpg');

function setUp() {
  currentVolumeType = ListThumbnailLoader.TEST_VOLUME_TYPE;
  /** @suppress {const} */
  ListThumbnailLoader.CACHE_SIZE = 5;
  /** @suppress {const} */
  ListThumbnailLoader.numOfMaxActiveTasksForTest = 2;

  /** @suppress {const} */
  MockThumbnailLoader.errorUrls = [];
  /** @suppress {const} */
  MockThumbnailLoader.testImageWidth = 160;
  /** @suppress {const} */
  MockThumbnailLoader.testImageHeight = 160;

  // Create an image dataURL for testing.
  const canvas = document.createElement('canvas');
  canvas.width = MockThumbnailLoader.testImageWidth;
  canvas.height = MockThumbnailLoader.testImageHeight;
  const context = canvas.getContext('2d');
  context.fillStyle = 'black';
  context.fillRect(0, 0, 80, 80);
  context.fillRect(80, 80, 80, 80);
  /** @const {string} */
  const testImageDataUrl = canvas.toDataURL('image/jpeg', 0.5);

  /** @suppress {const} */
  MockThumbnailLoader.testImageDataUrl = testImageDataUrl;

  getCallbacks = {};

  thumbnailModel = /** @type {!ThumbnailModel} */ ({
    get: function(entries) {
      return new Promise(fulfill => {
        getCallbacks[getKeyOfGetCallback_(entries)] = fulfill;
      });
    },
  });

  metadataModel = /** @type {!MetadataModel} */ ({
    get: function() {},
    getCache: function(entries, names) {
      return [{}];
    },
  });

  fileListModel = new FileListModel(metadataModel);

  isScanningForTest = false;

  class TestDirectoryModel extends cr.EventTarget {
    getFileList() {
      return fileListModel;
    }
    isScanning() {
      return isScanningForTest;
    }
  }

  /** @suppress {checkTypes} */
  directoryModel = /** @type {!DirectoryModel} */ (new TestDirectoryModel());

  const fakeVolumeManager = /** @type {!VolumeManager} */ ({
    getVolumeInfo: function(entry) {
      return {
        volumeType: currentVolumeType,
      };
    },
  });

  listThumbnailLoader = new ListThumbnailLoader(
      directoryModel, thumbnailModel, fakeVolumeManager, MockThumbnailLoader);

  thumbnailLoadedEvents = [];
  listThumbnailLoader.addEventListener('thumbnailLoaded', event => {
    thumbnailLoadedEvents.push(event);
  });
}

function getKeyOfGetCallback_(entries) {
  return entries.reduce((previous, current) => {
    return previous + '|' + current.toURL();
  }, '');
}

function resolveGetLatestCallback(entries) {
  const key = getKeyOfGetCallback_(entries);
  assert(getCallbacks[key]);
  getCallbacks[key](entries.map(() => {
    return {thumbnail: {}};
  }));
  delete getCallbacks[key];
}

function hasPendingGetLatestCallback(entries) {
  return !!getCallbacks[getKeyOfGetCallback_(entries)];
}

function areEntriesInCache(entries) {
  for (let i = 0; i < entries.length; i++) {
    if (null === listThumbnailLoader.getThumbnailFromCache(entries[i])) {
      return false;
    }
  }
  return true;
}

/**
 * Story test for list thumbnail loader.
 */
function testStory(callback) {
  fileListModel.push(directory1, entry1, entry2, entry3, entry4, entry5);

  // Set high priority range to 0 - 2.
  listThumbnailLoader.setHighPriorityRange(0, 2);

  // Assert that 2 fetch tasks are running.
  assertTrue(hasPendingGetLatestCallback([entry1]));
  assertTrue(hasPendingGetLatestCallback([entry2]));
  assertEquals(2, Object.keys(getCallbacks).length);

  // Fails to get thumbnail from cache for Test2.jpg.
  assertEquals(null, listThumbnailLoader.getThumbnailFromCache(entry2));

  // Set high priority range to 4 - 6.
  listThumbnailLoader.setHighPriorityRange(4, 6);

  // Assert that no new tasks are enqueued.
  assertTrue(hasPendingGetLatestCallback([entry1]));
  assertTrue(hasPendingGetLatestCallback([entry2]));
  assertEquals(2, Object.keys(getCallbacks).length);

  resolveGetLatestCallback([entry2]);

  reportPromise(
      waitUntil(() => {
        // Assert that thumbnailLoaded event is fired for Test2.jpg.
        return thumbnailLoadedEvents.length === 1;
      })
          .then(() => {
            const event = thumbnailLoadedEvents.shift();
            assertEquals('filesystem:volume-id/Test2.jpg', event.fileUrl);
            assertTrue(event.dataUrl.length > 0);
            assertEquals(160, event.width);
            assertEquals(160, event.height);

            // Since thumbnail of Test2.jpg is loaded into the cache,
            // getThumbnailFromCache returns thumbnail for the image.
            const thumbnail = listThumbnailLoader.getThumbnailFromCache(entry2);
            assertEquals('filesystem:volume-id/Test2.jpg', thumbnail.fileUrl);
            assertTrue(thumbnail.dataUrl.length > 0);
            assertEquals(160, thumbnail.width);
            assertEquals(160, thumbnail.height);

            // Assert that new task is enqueued.
            return waitUntil(() => {
              return hasPendingGetLatestCallback([entry1]) &&
                  hasPendingGetLatestCallback([entry4]) &&
                  Object.keys(getCallbacks).length === 2;
            });
          })
          .then(() => {
            // Set high priority range to 2 - 4.
            listThumbnailLoader.setHighPriorityRange(2, 4);

            resolveGetLatestCallback([entry1]);

            // Assert that task for (Test3.jpg) is enqueued.
            return waitUntil(() => {
              return hasPendingGetLatestCallback([entry3]) &&
                  hasPendingGetLatestCallback([entry4]) &&
                  Object.keys(getCallbacks).length === 2;
            });
          }),
      callback);
}

/**
 * Do not enqueue prefetch task when high priority range is at the end of list.
 */
function testRangeIsAtTheEndOfList() {
  // Set high priority range to 5 - 6.
  listThumbnailLoader.setHighPriorityRange(5, 6);

  fileListModel.push(directory1, entry1, entry2, entry3, entry4, entry5);

  // Assert that a task is enqueued for entry5.
  assertTrue(hasPendingGetLatestCallback([entry5]));
  assertEquals(1, Object.keys(getCallbacks).length);
}

function testCache(callback) {
  ListThumbnailLoader.numOfMaxActiveTasksForTest = 5;

  // Set high priority range to 0 - 2.
  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(entry1, entry2, entry3, entry4, entry5, entry6);

  resolveGetLatestCallback([entry1]);
  // In this test case, entry 3 is resolved earlier than entry 2.
  resolveGetLatestCallback([entry3]);
  resolveGetLatestCallback([entry2]);
  assertEquals(0, Object.keys(getCallbacks).length);

  reportPromise(
      waitUntil(() => {
        return areEntriesInCache([entry3, entry2, entry1]);
      })
          .then(() => {
            // Move high priority range to 1 - 3.
            listThumbnailLoader.setHighPriorityRange(1, 3);
            resolveGetLatestCallback([entry4]);
            assertEquals(0, Object.keys(getCallbacks).length);

            return waitUntil(() => {
              return areEntriesInCache([entry4, entry3, entry2, entry1]);
            });
          })
          .then(() => {
            // Move high priority range to 4 - 6.
            listThumbnailLoader.setHighPriorityRange(4, 6);
            resolveGetLatestCallback([entry5]);
            resolveGetLatestCallback([entry6]);
            assertEquals(0, Object.keys(getCallbacks).length);

            return waitUntil(() => {
              return areEntriesInCache(
                  [entry6, entry5, entry4, entry3, entry2]);
            });
          })
          .then(() => {
            // Move high priority range to 3 - 5.
            listThumbnailLoader.setHighPriorityRange(3, 5);
            assertEquals(0, Object.keys(getCallbacks).length);
            assertTrue(
                areEntriesInCache([entry6, entry5, entry4, entry3, entry2]));

            // Move high priority range to 0 - 2.
            listThumbnailLoader.setHighPriorityRange(0, 2);
            resolveGetLatestCallback([entry1]);
            assertEquals(0, Object.keys(getCallbacks).length);

            return waitUntil(() => {
              return areEntriesInCache(
                  [entry3, entry2, entry1, entry6, entry5]);
            });
          }),
      callback);
}

/**
 * Test case for thumbnail fetch error. In this test case, thumbnail fetch for
 * entry 2 is failed.
 */
function testErrorHandling(callback) {
  MockThumbnailLoader.errorUrls = [entry2.toURL()];

  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(entry1, entry2, entry3, entry4);

  resolveGetLatestCallback([entry2]);

  // Assert that new task is enqueued for entry3.
  reportPromise(
      waitUntil(() => {
        return hasPendingGetLatestCallback([entry3]);
      }),
      callback);
}

/**
 * Test case for handling sorted event in data model.
 */
function testSortedEvent(callback) {
  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(directory1, entry1, entry2, entry3, entry4, entry5);

  resolveGetLatestCallback([entry1]);
  resolveGetLatestCallback([entry2]);
  assertEquals(0, Object.keys(getCallbacks).length);

  // In order to assert that following task enqueues are fired by sorted event,
  // wait until all thumbnail loads are completed.
  reportPromise(
      waitUntil(() => {
        return thumbnailLoadedEvents.length === 2;
      }).then(() => {
        // After the sort, list should be
        // directory1, entry5, entry4, entry3, entry2, entry1.
        fileListModel.sort('name', 'desc');

        return waitUntil(() => {
          return hasPendingGetLatestCallback([entry5]) &&
              hasPendingGetLatestCallback([entry4]);
        });
      }),
      callback);
}

/**
 * Test case for handling change event in data model.
 */
function testChangeEvent(callback) {
  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(directory1, entry1, entry2, entry3);

  resolveGetLatestCallback([entry1]);
  resolveGetLatestCallback([entry2]);
  assertEquals(0, Object.keys(getCallbacks).length);

  reportPromise(
      waitUntil(() => {
        return thumbnailLoadedEvents.length === 2;
      }).then(() => {
        // entry1 is changed.
        const changeEvent = new Event('change');
        changeEvent.index = 1;
        fileListModel.dispatchEvent(changeEvent);

        // cache of entry1 should become invalid.
        const thumbnail = listThumbnailLoader.getThumbnailFromCache(entry1);
        assertTrue(thumbnail.outdated);

        resolveGetLatestCallback([entry1]);

        // Wait until thumbnailLoaded event is fired again for the change.
        return waitUntil(() => {
          return thumbnailLoadedEvents.length === 3;
        });
      }),
      callback);
}

/**
 * Test case for MTP volume.
 */
function testMTPVolume() {
  currentVolumeType = VolumeManagerCommon.VolumeType.MTP;

  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(directory1, entry1, entry2, entry3);

  // Only one request should be enqueued on MTP volume.
  assertEquals(1, Object.keys(getCallbacks).length);
}

/**
 * Test case that directory scan is running.
 */
function testDirectoryScanIsRunning() {
  // Items are added during directory scan.
  isScanningForTest = true;

  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(directory1, entry1, entry2);
  assertEquals(0, Object.keys(getCallbacks).length);

  // Scan completed after adding the last item.
  fileListModel.push(entry3);
  isScanningForTest = false;
  directoryModel.dispatchEvent(new Event('scan-completed'));

  assertEquals(2, Object.keys(getCallbacks).length);
}

/**
 * Test case for EXIF IO error and retrying logic.
 */
function testExifIOError(callback) {
  const task = new ListThumbnailLoader.Task(
      entry1,
      /** @type {!VolumeManager} */ ({
        getVolumeInfo: function(entry) {
          return {
            volumeType: currentVolumeType,
          };
        },
      }),
      /** @type {!ThumbnailModel} */ ({
        get: function(entries) {
          return Promise.resolve([{
            thumbnail: {
              urlError: {
                errorDescription: 'Error: Unexpected EOF @0',
              },
            },
          }]);
        },
      }),
      () => {
        // Thumbnails should be fetched only from EXIF on IO error.
        assertTrue(false);
      });

  return reportPromise(
      task.fetch().then(thumbnailData => {
        assertEquals(null, thumbnailData.dataUrl);
        assertFalse(thumbnailData.outdated);
        return waitUntil(() => {
          return thumbnailData.outdated;
        });
      }),
      callback);
}
