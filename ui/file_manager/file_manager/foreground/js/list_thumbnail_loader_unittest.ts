// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {VolumeManager} from '../../background/js/volume_manager.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {MockDirectoryEntry, MockEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';

import type {DirectoryModel} from './directory_model.js';
import {FileListModel} from './file_list_model.js';
import {ListThumbnailLoader, ListThumbnailLoaderTask, TEST_VOLUME_TYPE, type ThumbnailLoadedEvent} from './list_thumbnail_loader.js';
import type {MetadataKey} from './metadata/metadata_item.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import type {ThumbnailModel} from './metadata/thumbnail_model.js';
import {MockThumbnailLoader} from './mock_thumbnail_loader.js';
import type {ThumbnailLoader} from './thumbnail_loader.js';

let currentVolumeType: string;
let listThumbnailLoader: ListThumbnailLoader;
let getCallbacks: Record<string, Function>;
let thumbnailLoadedEvents: ThumbnailLoadedEvent[];
let thumbnailModel: ThumbnailModel;
let metadataModel: MetadataModel;
let fileListModel: FileListModel;
let directoryModel: DirectoryModel;
let isScanningForTest: boolean;

const fileSystem = new MockFileSystem('volume-id');
const directory1 =
    MockDirectoryEntry.create(fileSystem, '/TestDirectory') as DirectoryEntry;

const entry1 = new MockEntry(fileSystem, '/Test1.jpg');
const entry2 = new MockEntry(fileSystem, '/Test2.jpg');
const entry3 = new MockEntry(fileSystem, '/Test3.jpg');
const entry4 = new MockEntry(fileSystem, '/Test4.jpg');
const entry5 = new MockEntry(fileSystem, '/Test5.jpg');
const entry6 = new MockEntry(fileSystem, '/Test6.jpg');

export function setUp() {
  currentVolumeType = TEST_VOLUME_TYPE;

  MockThumbnailLoader.errorUrls = [];
  MockThumbnailLoader.testImageWidth = 160;
  MockThumbnailLoader.testImageHeight = 160;

  // Create an image dataURL for testing.
  const canvas = document.createElement('canvas');
  canvas.width = MockThumbnailLoader.testImageWidth;
  canvas.height = MockThumbnailLoader.testImageHeight;
  const context = canvas.getContext('2d');
  if (!context) {
    throw new Error('Failed to get context from canvas');
  }
  context.fillStyle = 'black';
  context.fillRect(0, 0, 80, 80);
  context.fillRect(80, 80, 80, 80);
  const testImageDataUrl = canvas.toDataURL('image/jpeg', 0.5);

  MockThumbnailLoader.testImageDataUrl = testImageDataUrl;

  getCallbacks = {};

  thumbnailModel = {
    get: function(entries) {
      return new Promise(fulfill => {
        getCallbacks[getKeyOfGetCallback(entries)] = fulfill;
      });
    },
  } as ThumbnailModel;

  metadataModel = {
    get: (_entries: Array<Entry|FilesAppEntry>, _names: MetadataKey[]) => {},
    getCache: (_entries: Array<Entry|FilesAppEntry>, _names: MetadataKey[]) => {
      return [{}];
    },
  } as MetadataModel;

  fileListModel = new FileListModel(metadataModel);

  isScanningForTest = false;

  class TestDirectoryModel extends EventTarget {
    getFileList() {
      return fileListModel;
    }
    isScanning() {
      return isScanningForTest;
    }
  }

  directoryModel = new TestDirectoryModel() as DirectoryModel;

  const fakeVolumeManager = {
    getVolumeInfo: (_entry: Entry|FilesAppEntry) => {
      return {
        volumeType: currentVolumeType,
      };
    },
  } as VolumeManager;

  listThumbnailLoader = new ListThumbnailLoader(
      directoryModel, thumbnailModel, fakeVolumeManager,
      MockThumbnailLoader as unknown as typeof ThumbnailLoader, 5);
  listThumbnailLoader.numOfMaxActiveTasksForTest = 2;

  thumbnailLoadedEvents = [];
  listThumbnailLoader.addEventListener(
      'thumbnailLoaded', (event: ThumbnailLoadedEvent) => {
        thumbnailLoadedEvents.push(event);
      });
}

function getKeyOfGetCallback(entries: Array<Entry|FilesAppEntry>): string {
  return entries.reduce((previous, current) => {
    return previous + '|' + current.toURL();
  }, '');
}

function resolveGetLatestCallback(entries: Entry[]) {
  const key = getKeyOfGetCallback(entries);
  assert(getCallbacks[key]);
  getCallbacks[key]?.(entries.map(() => {
    return {thumbnail: {}};
  }));
  delete getCallbacks[key];
}

function hasPendingGetLatestCallback(entries: Entry[]) {
  return !!getCallbacks[getKeyOfGetCallback(entries)];
}

function areEntriesInCache(entries: Entry[]) {
  for (const entry of entries) {
    if (null === listThumbnailLoader.getThumbnailFromCache(entry)) {
      return false;
    }
  }
  return true;
}

/**
 * Story test for list thumbnail loader.
 */
export async function testStory() {
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

  // Assert that thumbnailLoaded event is fired for Test2.jpg.
  await waitUntil(() => thumbnailLoadedEvents.length === 1);

  const event = thumbnailLoadedEvents.shift()!;
  assertEquals('filesystem:volume-id/Test2.jpg', event.detail.fileUrl);
  assertTrue((event.detail.dataUrl?.length ?? -1) > 0);
  assertEquals(160, event.detail.width);
  assertEquals(160, event.detail.height);

  // Since thumbnail of Test2.jpg is loaded into the cache,
  // getThumbnailFromCache returns thumbnail for the image.
  const thumbnail = listThumbnailLoader.getThumbnailFromCache(entry2)!;
  assertEquals('filesystem:volume-id/Test2.jpg', thumbnail.fileUrl);
  assertTrue((thumbnail.dataUrl?.length ?? -1) > 0);
  assertEquals(160, thumbnail.width);
  assertEquals(160, thumbnail.height);

  // Assert that new task is enqueued.
  await waitUntil(() => {
    return hasPendingGetLatestCallback([entry1]) &&
        hasPendingGetLatestCallback([entry4]) &&
        Object.keys(getCallbacks).length === 2;
  });

  // Set high priority range to 2 - 4.
  listThumbnailLoader.setHighPriorityRange(2, 4);

  resolveGetLatestCallback([entry1]);

  // Assert that task for (Test3.jpg) is enqueued.
  await waitUntil(() => {
    return hasPendingGetLatestCallback([entry3]) &&
        hasPendingGetLatestCallback([entry4]) &&
        Object.keys(getCallbacks).length === 2;
  });
}

/**
 * Do not enqueue prefetch task when high priority range is at the end of list.
 */
export function testRangeIsAtTheEndOfList() {
  // Set high priority range to 5 - 6.
  listThumbnailLoader.setHighPriorityRange(5, 6);

  fileListModel.push(directory1, entry1, entry2, entry3, entry4, entry5);

  // Assert that a task is enqueued for entry5.
  assertTrue(hasPendingGetLatestCallback([entry5]));
  assertEquals(1, Object.keys(getCallbacks).length);
}

export async function testCache() {
  listThumbnailLoader.numOfMaxActiveTasksForTest = 5;

  // Set high priority range to 0 - 2.
  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(entry1, entry2, entry3, entry4, entry5, entry6);

  resolveGetLatestCallback([entry1]);
  // In this test case, entry 3 is resolved earlier than entry 2.
  resolveGetLatestCallback([entry3]);
  resolveGetLatestCallback([entry2]);
  assertEquals(0, Object.keys(getCallbacks).length);

  await waitUntil(() => {
    return areEntriesInCache([entry3, entry2, entry1]);
  });

  // Move high priority range to 1 - 3.
  listThumbnailLoader.setHighPriorityRange(1, 3);
  resolveGetLatestCallback([entry4]);
  assertEquals(0, Object.keys(getCallbacks).length);

  await waitUntil(() => {
    return areEntriesInCache([entry4, entry3, entry2, entry1]);
  });

  // Move high priority range to 4 - 6.
  listThumbnailLoader.setHighPriorityRange(4, 6);
  resolveGetLatestCallback([entry5]);
  resolveGetLatestCallback([entry6]);
  assertEquals(0, Object.keys(getCallbacks).length);

  await waitUntil(() => {
    return areEntriesInCache([entry6, entry5, entry4, entry3, entry2]);
  });

  // Move high priority range to 3 - 5.
  listThumbnailLoader.setHighPriorityRange(3, 5);
  assertEquals(0, Object.keys(getCallbacks).length);
  assertTrue(areEntriesInCache([entry6, entry5, entry4, entry3, entry2]));

  // Move high priority range to 0 - 2.
  listThumbnailLoader.setHighPriorityRange(0, 2);
  resolveGetLatestCallback([entry1]);
  assertEquals(0, Object.keys(getCallbacks).length);

  await waitUntil(() => {
    return areEntriesInCache([entry3, entry2, entry1, entry6, entry5]);
  });
}

/**
 * Test case for thumbnail fetch error. In this test case, thumbnail fetch for
 * entry 2 is failed.
 */
export async function testErrorHandling() {
  MockThumbnailLoader.errorUrls = [entry2.toURL()];

  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(entry1, entry2, entry3, entry4);

  resolveGetLatestCallback([entry2]);

  // Assert that new task is enqueued for entry3.
  await waitUntil(() => {
    return hasPendingGetLatestCallback([entry3]);
  });
}

/**
 * Test case for handling sorted event in data model.
 */
export async function testSortedEvent() {
  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(directory1, entry1, entry2, entry3, entry4, entry5);

  resolveGetLatestCallback([entry1]);
  resolveGetLatestCallback([entry2]);
  assertEquals(0, Object.keys(getCallbacks).length);

  // In order to assert that following task enqueues are fired by sorted event,
  // wait until all thumbnail loads are completed.
  await waitUntil(() => {
    return thumbnailLoadedEvents.length === 2;
  });

  // After the sort, list should be
  // directory1, entry5, entry4, entry3, entry2, entry1.
  fileListModel.sort('name', 'desc');

  await waitUntil(() => {
    return hasPendingGetLatestCallback([entry5]) &&
        hasPendingGetLatestCallback([entry4]);
  });
}

/**
 * Test case for handling change event in data model.
 */
export async function testChangeEvent() {
  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(directory1, entry1, entry2, entry3);

  resolveGetLatestCallback([entry1]);
  resolveGetLatestCallback([entry2]);
  assertEquals(0, Object.keys(getCallbacks).length);

  await waitUntil(() => {
    return thumbnailLoadedEvents.length === 2;
  });

  // entry1 is changed.
  const changeEvent = new CustomEvent('change', {detail: {index: 1}});
  fileListModel.dispatchEvent(changeEvent);

  // cache of entry1 should become invalid.
  const thumbnail = listThumbnailLoader.getThumbnailFromCache(entry1)!;
  assertTrue(thumbnail.outdated);

  resolveGetLatestCallback([entry1]);

  // Wait until thumbnailLoaded event is fired again for the change.
  await waitUntil(() => thumbnailLoadedEvents.length === 3);
}

/**
 * Test case for MTP volume.
 */
export function testMTPVolume() {
  currentVolumeType = VolumeType.MTP;

  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(directory1, entry1, entry2, entry3);

  // Only one request should be enqueued on MTP volume.
  assertEquals(1, Object.keys(getCallbacks).length);
}

/**
 * Test case that directory scan is running.
 */
export function testDirectoryScanIsRunning() {
  // Items are added during directory scan.
  isScanningForTest = true;

  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(directory1, entry1, entry2);
  assertEquals(0, Object.keys(getCallbacks).length);

  // Scan completed after adding the last item.
  fileListModel.push(entry3);
  isScanningForTest = false;
  directoryModel.dispatchEvent(new CustomEvent('cur-dir-scan-completed'));

  assertEquals(2, Object.keys(getCallbacks).length);
}

/**
 * Test case for EXIF IO error and retrying logic.
 */
export async function testExifIOError() {
  const volumeManager = {
    getVolumeInfo: (_entry: Entry|FilesAppEntry) => {
      return {
        volumeType: currentVolumeType,
      };
    },
  } as VolumeManager;

  const thumbnailModel = {
    get: function(_entries: Array<Entry|FilesAppEntry>) {
      return Promise.resolve([{
        thumbnail: {
          urlError: {
            errorDescription: 'Error: Unexpected EOF @0',
          },
        },
      }]);
    },
  } as ThumbnailModel;

  const thumbnailLoaderConstructor = (() => {
                                       // Thumbnails should be fetched only from
                                       // EXIF on IO error.
                                       assertTrue(false);
                                     }) as unknown as typeof ThumbnailLoader;

  const task = new ListThumbnailLoaderTask(
      entry1, volumeManager, thumbnailModel, thumbnailLoaderConstructor);


  const thumbnailData = await task.fetch();
  assertEquals(null, thumbnailData.dataUrl);
  assertFalse(thumbnailData.outdated);
  await waitUntil(() => {
    return thumbnailData.outdated;
  });
}
