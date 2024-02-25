// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {PermutationEvent, SpliceEvent} from '../../common/js/array_data_model.js';
import {str} from '../../common/js/translations.js';

import {FileListModel, GROUP_BY_FIELD_DIRECTORY, GROUP_BY_FIELD_MODIFICATION_TIME, type GroupHeader} from './file_list_model.js';
import type {MetadataItem} from './metadata/metadata_item.js';
import type {MetadataModel} from './metadata/metadata_model.js';


const TEST_METADATA: Record<string, MetadataItem> = {
  'a.txt': {
    contentMimeType: 'text/plain',
    size: 1023,
    modificationTime: new Date(2016, 1, 1, 0, 0, 2),
  },
  'b.html': {
    contentMimeType: 'text/html',
    size: 206,
    modificationTime: new Date(2016, 1, 1, 0, 0, 1),
  },
  'c.jpg': {
    contentMimeType: 'image/jpeg',
    size: 342134,
    modificationTime: new Date(2016, 1, 1, 0, 0, 0),
  },
};

let originalNow: () => number;

export function setUp() {
  // Mock Date.now() to: Jun 8 2022, 12:00:00 local time.
  originalNow = window.Date.now;
  window.Date.now = () => new Date(2022, 5, 8, 12, 0, 0).getTime();
}

export function tearDown() {
  // Restore Date.now().
  window.Date.now = originalNow;
}

function assertFileListModelElementNames(
    fileListModel: FileListModel, names: string[]) {
  assertEquals(fileListModel.length, names.length);
  for (let i = 0; i < fileListModel.length; i++) {
    assertEquals(fileListModel.item(i)!.name, names[i]);
  }
}

function assertEntryArrayEquals(entryArray: Entry[], names: string[]) {
  assertEquals(entryArray.length, names.length);
  assertArrayEquals(entryArray.map((e) => e.name), names);
}

function makeSimpleFileListModel(names: string[]) {
  const fileListModel = new FileListModel(createFakeMetadataModel({}));
  for (let i = 0; i < names.length; i++) {
    fileListModel.push(({name: names[i]!, isDirectory: false}) as Entry);
  }
  return fileListModel;
}

/**
 * Returns a fake MetadataModel, used to provide metadata from the given |data|
 * object (usually TEST_METADATA) to the FileListModel.
 */
function createFakeMetadataModel(data: Record<string, MetadataItem>):
    MetadataModel {
  return {
    getCache: (entries, names) => {
      const result = [];
      for (const entry of entries) {
        const metadata: MetadataItem = {};
        if (!entry.isDirectory && data[entry.name]) {
          for (const metadataField of names) {
            const value = data[entry.name]![metadataField];
            // `undefined` is the intersection of all possible properties of
            // MetadataItem.
            metadata[metadataField] = value as undefined;
          }
        }
        result.push(metadata);
      }
      return result;
    },
  } as MetadataModel;
}

export function testSortWithFolders() {
  const fileListModel =
      new FileListModel(createFakeMetadataModel(TEST_METADATA));
  fileListModel.push({name: 'dirA', isDirectory: true} as Entry);
  fileListModel.push({name: 'dirB', isDirectory: true} as Entry);
  fileListModel.push({name: 'a.txt', isDirectory: false} as Entry);
  fileListModel.push({name: 'b.html', isDirectory: false} as Entry);
  fileListModel.push({name: 'c.jpg', isDirectory: false} as Entry);

  // In following sort tests, note that folders should always be prior to files.
  fileListModel.sort('name', 'asc');
  assertFileListModelElementNames(
      fileListModel, ['dirA', 'dirB', 'a.txt', 'b.html', 'c.jpg']);
  fileListModel.sort('name', 'desc');
  assertFileListModelElementNames(
      fileListModel, ['dirB', 'dirA', 'c.jpg', 'b.html', 'a.txt']);
  // Sort files by size. Folders should be sorted by their names.
  fileListModel.sort('size', 'asc');
  assertFileListModelElementNames(
      fileListModel, ['dirA', 'dirB', 'b.html', 'a.txt', 'c.jpg']);
  fileListModel.sort('size', 'desc');
  assertFileListModelElementNames(
      fileListModel, ['dirB', 'dirA', 'c.jpg', 'a.txt', 'b.html']);
  // Sort files by modification. Folders should be sorted by their names.
  fileListModel.sort('modificationTime', 'asc');
  assertFileListModelElementNames(
      fileListModel, ['dirA', 'dirB', 'c.jpg', 'b.html', 'a.txt']);
  fileListModel.sort('modificationTime', 'desc');
  assertFileListModelElementNames(
      fileListModel, ['dirB', 'dirA', 'a.txt', 'b.html', 'c.jpg']);
}

export function testSplice() {
  const fileListModel = makeSimpleFileListModel(['d', 'a', 'x', 'n']);
  fileListModel.sort('name', 'asc');

  fileListModel.addEventListener('splice', (event: SpliceEvent) => {
    const spliceEventDetail = event.detail;
    assertEntryArrayEquals(spliceEventDetail.added, ['p', 'b']);
    assertEntryArrayEquals(spliceEventDetail.removed, ['n']);
    // The first inserted item, 'p', should be at index:3 after splice.
    assertEquals(spliceEventDetail.index, 3);
  });

  fileListModel.addEventListener('permuted', (event: PermutationEvent) => {
    const permutedEventDetail = event.detail;
    assertArrayEquals(permutedEventDetail.permutation, [0, 2, -1, 4]);
    assertEquals(permutedEventDetail.newLength, 5);
  });

  fileListModel.splice(
      2, 1, {name: 'p', isDirectory: false} as Entry,
      {name: 'b', isDirectory: false} as Entry);
  assertFileListModelElementNames(fileListModel, ['a', 'b', 'd', 'p', 'x']);
}

export function testSpliceWithoutSortStatus() {
  const fileListModel = makeSimpleFileListModel(['d', 'a', 'x', 'n']);

  fileListModel.addEventListener('splice', (event: SpliceEvent) => {
    const spliceEventDetail = event.detail;
    assertEntryArrayEquals(spliceEventDetail.added, ['p', 'b']);
    assertEntryArrayEquals(spliceEventDetail.removed, ['x']);
    // The first inserted item, 'p', should be at index:2 after splice.
    assertEquals(spliceEventDetail.index, 2);
  });

  fileListModel.addEventListener('permuted', (event: PermutationEvent) => {
    const permutedEventDetail = event.detail;
    assertArrayEquals(permutedEventDetail.permutation, [0, 1, -1, 4]);
    assertEquals(permutedEventDetail.newLength, 5);
  });

  fileListModel.splice(
      2, 1, {name: 'p', isDirectory: false} as Entry,
      {name: 'b', isDirectory: false} as Entry);
  // If the sort status is not specified, the original order should be kept.
  // i.e. the 2nd element in the original array, 'x', should be removed, and
  // 'p' and 'b' should be inserted at the position without changing the order.
  assertFileListModelElementNames(fileListModel, ['d', 'a', 'p', 'b', 'n']);
}

export function testSpliceWithoutAddingNewItems() {
  const fileListModel = makeSimpleFileListModel(['d', 'a', 'x', 'n']);
  fileListModel.sort('name', 'asc');

  fileListModel.addEventListener('splice', (event: SpliceEvent) => {
    const spliceEventDetail = event.detail;
    assertEntryArrayEquals(spliceEventDetail.added, []);
    assertEntryArrayEquals(spliceEventDetail.removed, ['n']);
    // The first item after insertion/deletion point is 'x', which should be at
    // 2nd position after the sort.
    assertEquals(spliceEventDetail.index, 2);
  });

  fileListModel.addEventListener('permuted', (event: PermutationEvent) => {
    const permutedEventDetail = event.detail;
    assertArrayEquals(permutedEventDetail.permutation, [0, 1, -1, 2]);
    assertEquals(permutedEventDetail.newLength, 3);
  });

  fileListModel.splice(2, 1);
  assertFileListModelElementNames(fileListModel, ['a', 'd', 'x']);
}

export function testSpliceWithoutDeletingItems() {
  const fileListModel = makeSimpleFileListModel(['d', 'a', 'x', 'n']);
  fileListModel.sort('name', 'asc');

  fileListModel.addEventListener('splice', (event: SpliceEvent) => {
    const spliceEventDetail = event.detail;
    assertEntryArrayEquals(spliceEventDetail.added, ['p', 'b']);
    assertEntryArrayEquals(spliceEventDetail.removed, []);
    assertEquals(spliceEventDetail.index, 4);
  });

  fileListModel.addEventListener('permuted', (event: PermutationEvent) => {
    const permutedEventDetail = event.detail;
    assertArrayEquals(permutedEventDetail.permutation, [0, 2, 3, 5]);
    assertEquals(permutedEventDetail.newLength, 6);
  });

  fileListModel.splice(
      2, 0, {name: 'p', isDirectory: false} as Entry,
      {name: 'b', isDirectory: false} as Entry);
  assertFileListModelElementNames(
      fileListModel, ['a', 'b', 'd', 'n', 'p', 'x']);
}

export function testShouldShowGroupHeading() {
  const fileListModel = makeSimpleFileListModel([]);
  assertFalse(fileListModel.shouldShowGroupHeading());
  fileListModel.groupByField = GROUP_BY_FIELD_MODIFICATION_TIME;
  assertFalse(fileListModel.shouldShowGroupHeading());
  fileListModel.sort(GROUP_BY_FIELD_MODIFICATION_TIME, 'asc');
  assertTrue(fileListModel.shouldShowGroupHeading());
  fileListModel.groupByField = GROUP_BY_FIELD_DIRECTORY;
  assertTrue(fileListModel.shouldShowGroupHeading());
}

export function testGroupByModificationTime() {
  const RecentDateBucket = chrome.fileManagerPrivate.RecentDateBucket;

  const testData: Array<{
    metadataMap: Record<string, MetadataItem>,
    expectedGroups: GroupHeader[],
    expectedReversedGroups: GroupHeader[],
  }> =
      [
        // Empty list.
        {
          metadataMap: {},
          expectedGroups: [],
          expectedReversedGroups: [],
        },
        // Only one item.
        {
          metadataMap: {
            'a.txt': {
              // Today.
              modificationTime: new Date(2022, 5, 8, 8, 0, 2),
            },
          },
          expectedGroups: [{
            startIndex: 0,
            endIndex: 0,
            label: str('RECENT_TIME_HEADING_TODAY'),
            group: RecentDateBucket.TODAY,
          }],
          expectedReversedGroups: [{
            startIndex: 0,
            endIndex: 0,
            label: str('RECENT_TIME_HEADING_TODAY'),
            group: RecentDateBucket.TODAY,
          }],
        },
        // All items are in the same group.
        {
          metadataMap: {
            'a.txt': {
              // Today.
              modificationTime: new Date(2022, 5, 8, 10, 0, 2),
            },
            'b.txt': {
              // Today.
              modificationTime: new Date(2022, 5, 8, 8, 0, 2),
            },
            'c.txt': {
              // Today.
              modificationTime: new Date(2022, 5, 8, 6, 0, 2),
            },
          },
          expectedGroups: [{
            startIndex: 0,
            endIndex: 2,
            label: str('RECENT_TIME_HEADING_TODAY'),
            group: RecentDateBucket.TODAY,
          }],
          expectedReversedGroups: [{
            startIndex: 0,
            endIndex: 2,
            label: str('RECENT_TIME_HEADING_TODAY'),
            group: RecentDateBucket.TODAY,
          }],
        },
        // Items belong to different groups.
        {
          metadataMap: {
            'a.txt': {
              // Today.
              modificationTime: new Date(2022, 5, 8, 8, 0, 2),
            },
            'b.txt': {
              // Today.
              modificationTime: new Date(2022, 5, 8, 6, 0, 2),
            },
            'c.txt': {
              // Yesterday.
              modificationTime: new Date(2022, 5, 7, 10, 0, 2),
            },
            'd.txt': {
              // This week.
              modificationTime: new Date(2022, 5, 6, 10, 0, 2),
            },
            'e.txt': {
              // This week.
              modificationTime: new Date(2022, 5, 5, 10, 0, 2),
            },
            'f.txt': {
              // This month.
              modificationTime: new Date(2022, 5, 1, 10, 0, 2),
            },
            'g.txt': {
              // This year.
              modificationTime: new Date(2022, 4, 5, 10, 0, 2),
            },
          },
          expectedGroups: [
            {
              startIndex: 0,
              endIndex: 1,
              label: str('RECENT_TIME_HEADING_TODAY'),
              group: RecentDateBucket.TODAY,
            },
            {
              startIndex: 2,
              endIndex: 2,
              label: str('RECENT_TIME_HEADING_YESTERDAY'),
              group: RecentDateBucket.YESTERDAY,
            },
            {
              startIndex: 3,
              endIndex: 4,
              label: str('RECENT_TIME_HEADING_THIS_WEEK'),
              group: RecentDateBucket.EARLIER_THIS_WEEK,
            },
            {
              startIndex: 5,
              endIndex: 5,
              label: str('RECENT_TIME_HEADING_THIS_MONTH'),
              group: RecentDateBucket.EARLIER_THIS_MONTH,
            },
            {
              startIndex: 6,
              endIndex: 6,
              label: str('RECENT_TIME_HEADING_THIS_YEAR'),
              group: RecentDateBucket.EARLIER_THIS_YEAR,
            },
          ],
          expectedReversedGroups: [
            {
              startIndex: 0,
              endIndex: 0,
              label: str('RECENT_TIME_HEADING_THIS_YEAR'),
              group: RecentDateBucket.EARLIER_THIS_YEAR,
            },
            {
              startIndex: 1,
              endIndex: 1,
              label: str('RECENT_TIME_HEADING_THIS_MONTH'),
              group: RecentDateBucket.EARLIER_THIS_MONTH,
            },
            {
              startIndex: 2,
              endIndex: 3,
              label: str('RECENT_TIME_HEADING_THIS_WEEK'),
              group: RecentDateBucket.EARLIER_THIS_WEEK,
            },
            {
              startIndex: 4,
              endIndex: 4,
              label: str('RECENT_TIME_HEADING_YESTERDAY'),
              group: RecentDateBucket.YESTERDAY,
            },
            {
              startIndex: 5,
              endIndex: 6,
              label: str('RECENT_TIME_HEADING_TODAY'),
              group: RecentDateBucket.TODAY,
            },
          ],
        },
      ];

  for (const test of testData) {
    const fileListModel =
        new FileListModel(createFakeMetadataModel(test.metadataMap));
    fileListModel.groupByField = GROUP_BY_FIELD_MODIFICATION_TIME;
    fileListModel.sort(GROUP_BY_FIELD_MODIFICATION_TIME, 'desc');
    const files = Object.keys(test.metadataMap).map(fileName => {
      return {name: fileName, isDirectory: false} as Entry;
    });
    fileListModel.push(...files);
    const snapshot = fileListModel.getGroupBySnapshot();
    assertArrayEquals(snapshot, test.expectedGroups);
    // Reverse order.
    fileListModel.sort(GROUP_BY_FIELD_MODIFICATION_TIME, 'asc');
    const snapshotReverse = fileListModel.getGroupBySnapshot();
    assertArrayEquals(snapshotReverse, test.expectedReversedGroups);
  }
}

export function testGroupByDirectory() {
  const testData: Array<{
    metadataMap: Record<string, {isDirectory: boolean}>,
    expectedGroups: GroupHeader[],
    expectedFileList: string[],
    expectedReversedFileList: string[],
  }> =
      [
        // Empty list.
        {
          metadataMap: {},
          expectedGroups: [],
          expectedFileList: [],
          expectedReversedFileList: [],
        },
        // Only one item.
        {
          metadataMap: {
            'a.txt': {isDirectory: false},
          },
          expectedGroups: [{
            startIndex: 0,
            endIndex: 0,
            label: str('GRID_VIEW_FILES_TITLE'),
            group: false,
          }],
          expectedFileList: ['a.txt'],
          expectedReversedFileList: ['a.txt'],
        },
        // All items are in the same group.
        {
          metadataMap: {
            'a': {isDirectory: true},
            'b': {isDirectory: true},
            'c': {isDirectory: true},
          },
          expectedGroups: [{
            startIndex: 0,
            endIndex: 2,
            label: str('GRID_VIEW_FOLDERS_TITLE'),
            group: true,
          }],
          expectedFileList: ['a', 'b', 'c'],
          expectedReversedFileList: ['c', 'b', 'a'],
        },
        // Items belong to different groups.
        {
          metadataMap: {
            'a': {isDirectory: true},
            'c': {isDirectory: true},
            'f': {isDirectory: true},
            'b.txt': {isDirectory: false},
            'd.txt': {isDirectory: false},
            'e.txt': {isDirectory: false},
          },
          expectedGroups: [
            {
              startIndex: 0,
              endIndex: 2,
              label: str('GRID_VIEW_FOLDERS_TITLE'),
              group: true,
            },
            {
              startIndex: 3,
              endIndex: 5,
              label: str('GRID_VIEW_FILES_TITLE'),
              group: false,
            },
          ],
          expectedFileList: ['a', 'c', 'f', 'b.txt', 'd.txt', 'e.txt'],
          expectedReversedFileList: ['f', 'c', 'a', 'e.txt', 'd.txt', 'b.txt'],
        },
      ];

  for (const test of testData) {
    const fileListModel = new FileListModel(createFakeMetadataModel({}));
    fileListModel.groupByField = GROUP_BY_FIELD_DIRECTORY;
    fileListModel.sort('name', 'asc');
    const files = Object.keys(test.metadataMap).map(fileName => {
      return {
        name: fileName,
        isDirectory: test.metadataMap[fileName]!.isDirectory,
      } as Entry;
    });
    fileListModel.push(...files);
    const snapshot = fileListModel.getGroupBySnapshot();
    assertArrayEquals(snapshot, test.expectedGroups);
    for (let i = 0; i < fileListModel.length; i++) {
      const item = fileListModel.item(i);
      assertEquals(item?.name, test.expectedFileList[i]);
    }
    // Reverse order won't change the group snapshot, e.g Folders are always
    // at the beginning.
    fileListModel.sort('name', 'desc');
    const snapshotReverse = fileListModel.getGroupBySnapshot();
    assertArrayEquals(snapshotReverse, test.expectedGroups);
    for (let i = 0; i < fileListModel.length; i++) {
      const item = fileListModel.item(i);
      assertEquals(item?.name, test.expectedReversedFileList[i]);
    }
  }
}
