// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';

import {installMockChrome} from '../../common/js/mock_chrome.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {FileFilter, RecentContentScanner} from './directory_contents.js';

/**
 * Mock chrome APIs.
 * @type {!Object}
 */
const mockChrome = {
  fileManagerPrivate: {
    getRecentFiles:
        // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an
        // 'any' type.
        (sourceRestriction, query, fileType, invalidateCache, callback) => {
          /** @type {!Array<!FileEntry>} */
          const entries = [
            /** @type {!FileEntry} */ ({name: '1.txt'}),
            /** @type {!FileEntry} */ ({name: '2.txt'}),
            /** @type {!FileEntry} */ ({name: '3.png'}),
          ];
          callback(entries.filter(e => e.name.indexOf(query) !== -1));
        },
  },
};

/**
 * Initializes the test environment.
 */
export function setUp() {
  // Install mock chrome APIs.
  installMockChrome(mockChrome);
}

/**
 * Check that files are shown or hidden correctly.
 */
export function testHiddenFiles() {
  let volumeManagerRootType = VolumeManagerCommon.RootType.DOWNLOADS;
  // Create a fake volume manager that provides entry location info.
  const volumeManager = /** @type {!VolumeManager} */ ({
    // @ts-ignore: error TS6133: 'entry' is declared but its value is never
    // read.
    getLocationInfo: (entry) => {
      return /** @type {!EntryLocation} */ ({
        rootType: volumeManagerRootType,
      });
    },
  });

  // @ts-ignore: error TS7006: Parameter 'fullPath' implicitly has an 'any'
  // type.
  const entry = (fullPath) =>
      // @ts-ignore: error TS2352: Conversion of type '{ name: any; fullPath:
      // any; filesystem: string; }' to type 'FileSystemEntry' may be a mistake
      // because neither type sufficiently overlaps with the other. If this was
      // intentional, convert the expression to 'unknown' first.
      /** @type {!Entry} */ (
          {name: fullPath.split('/').pop(), fullPath, filesystem: 'test'});

  // Create a set of entries in root dir, and in /PvmDefault dir.
  const entries = [];
  for (const e of ['.test', 'test', '$RECYCLE.BIN', '$okForNow', '~okForNow']) {
    entries.push(entry('/' + e));
    entries.push(entry('/PvmDefault/' + e));
  }

  const filter = new FileFilter(volumeManager);

  // .test should be hidden in any dir.
  // $RECYCLE.BIN is hidden in downloads:/PvmDefault only.
  assertFalse(filter.isHiddenFilesVisible());
  let hidden = entries.filter(entry => !filter.filter(entry));
  assertEquals(3, hidden.length);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('/.test', hidden[0].fullPath);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('/PvmDefault/.test', hidden[1].fullPath);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('/PvmDefault/$RECYCLE.BIN', hidden[2].fullPath);

  // No files hidden when we show hidden files.
  filter.setHiddenFilesVisible(true);
  hidden = entries.filter(entry => !filter.filter(entry));
  assertEquals(0, hidden.length);

  // $RECYCLE.BIN is not hidden in other volumes.
  volumeManagerRootType = 'testroot';
  filter.setHiddenFilesVisible(false);
  hidden = entries.filter(entry => !filter.filter(entry));
  assertEquals(2, hidden.length);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('/.test', hidden[0].fullPath);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('/PvmDefault/.test', hidden[1].fullPath);

  // Still no files hidden when we show hidden files.
  filter.setHiddenFilesVisible(true);
  hidden = entries.filter(entry => !filter.filter(entry));
  assertEquals(0, hidden.length);
}

/**
 * Check the recent entries which doesn't match the query or whose volume is
 * disallowed will be filtered out.
 */
export async function testRecentScannerFilter() {
  // Mock volume manager
  const volumeManager = /** @type {!VolumeManager} */ ({
    getVolumeInfo: (entry) => {
      if (entry.name === '1.txt') {
        return null;
      }
      return /** @type {!import("../../externs/volume_info.js").VolumeInfo} */ (
          {
            volumeId: 'fakeId',
          });
    },
  });
  const scanner = new RecentContentScanner('txt', volumeManager);
  /** @type {!Array<!FileEntry>} */
  let entriesCallbackResult = [];
  // @ts-ignore: error TS7006: Parameter 'entries' implicitly has an 'any' type.
  function entriesCallback(entries) {
    entriesCallbackResult = entries;
  }
  function otherCallback() {}
  await scanner.scan(entriesCallback, otherCallback, otherCallback);
  // 1.txt: volume is not allowed; 3.png: query is not matched.
  assertEquals(1, entriesCallbackResult.length);
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  assertEquals('2.txt', entriesCallbackResult[0].name);
}
