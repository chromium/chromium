// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {RootType} from '../../common/js/volume_manager_types.js';

import {FileFilter, RecentContentScanner} from './directory_contents.js';

/**
 * Mock chrome APIs.
 */
const mockChrome = {
  fileManagerPrivate: {
    getRecentFiles:
        (_1: chrome.fileManagerPrivate.SourceRestriction, query: string,
         _2: number, _3: chrome.fileManagerPrivate.FileCategory, _4: boolean,
         callback: (entries: FileEntry[]) => void) => {
          const entries: FileEntry[] = [
            {name: '1.txt'} as FileEntry,
            {name: '2.txt'} as FileEntry,
            {name: '3.png'} as FileEntry,
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
  let volumeManagerRootType = RootType.DOWNLOADS;
  // Create a fake volume manager that provides entry location info.
  const volumeManager = {
    getLocationInfo: (_: FileEntry) => {
      return {
        rootType: volumeManagerRootType,
      };
    },
  } as VolumeManager;

  const entry = (fullPath: string): FileEntry => {
    return {
      name: fullPath.split('/').pop()!,
      fullPath,
      filesystem: {name: 'test'} as FileSystem,
    } as FileEntry;
  };

  // Create a set of entries in root dir, and in /PvmDefault dir.
  const entries: FileEntry[] = [];
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
  assertEquals('/.test', hidden[0]!.fullPath);
  assertEquals('/PvmDefault/.test', hidden[1]!.fullPath);
  assertEquals('/PvmDefault/$RECYCLE.BIN', hidden[2]!.fullPath);

  // No files hidden when we show hidden files.
  filter.setHiddenFilesVisible(true);
  hidden = entries.filter(entry => !filter.filter(entry));
  assertEquals(0, hidden.length);

  // $RECYCLE.BIN is not hidden in other volumes.
  volumeManagerRootType = 'testroot' as RootType;
  filter.setHiddenFilesVisible(false);
  hidden = entries.filter(entry => !filter.filter(entry));
  assertEquals(2, hidden.length);
  assertEquals('/.test', hidden[0]!.fullPath);
  assertEquals('/PvmDefault/.test', hidden[1]!.fullPath);

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
  const volumeManager = {
    getVolumeInfo: (entry) => {
      if (entry.name === '1.txt') {
        return null;
      }
      return {
        volumeId: 'fakeId',
      } as VolumeInfo;
    },
  } as VolumeManager;
  const scanner = new RecentContentScanner('txt', 30, volumeManager);
  let entriesCallbackResult: Array<Entry|FilesAppEntry> = [];
  function entriesCallback(entries: Array<Entry|FilesAppEntry>) {
    entriesCallbackResult = entries;
  }
  function otherCallback() {}
  await scanner.scan(entriesCallback, otherCallback, otherCallback);
  // 1.txt: volume is not allowed; 3.png: query is not matched.
  assertEquals(1, entriesCallbackResult.length);
  assertEquals('2.txt', entriesCallbackResult[0]!.name);
}
