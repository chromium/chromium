// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {isFakeEntry} from '../../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';
import {FakeEntryImpl} from '../../../common/js/files_app_entry_types.js';
import {installMockChrome} from '../../../common/js/mock_chrome.js';
import {MockFileEntry, MockFileSystem} from '../../../common/js/mock_entry.js';
import {RootType} from '../../../common/js/volume_manager_types.js';

import {DlpMetadataProvider} from './dlp_metadata_provider.js';
import {MetadataRequest} from './metadata_request.js';

let mockChrome: object;
let fakeEntry: FakeEntryImpl;
let realEntry: any;

export function setUp() {
  loadTimeData.overrideValues({
    DLP_ENABLED: true,
  });

  // Setup mock chrome APIs.
  mockChrome = {
    fileManagerPrivate: {
      getDlpMetadata: function(
          entries: Entry[],
          callback: (metadata: chrome.fileManagerPrivate.DlpMetadata[]) =>
              void) {
        assertEquals(1, entries.length);
        callback([
          {
            isDlpRestricted: true,
            sourceUrl: 'https://example.com',
            isRestrictedForDestination: false,
          },
        ]);
      },
    },
    runtime: {
      lastError: null,
    },
  };

  installMockChrome(mockChrome);

  fakeEntry = new FakeEntryImpl('fakeEntry', RootType.DRIVE_FAKE_ROOT);
  assertTrue(isFakeEntry(fakeEntry as unknown as FilesAppEntry));

  const mockFileSystem = new MockFileSystem('volumeId');
  realEntry = MockFileEntry.create(mockFileSystem, '/test.tiff');
  assertFalse(isFakeEntry(realEntry as unknown as FilesAppEntry));
}

/**
 * Tests that DlpMetadataProvider filters out fake entries before calling
 * `getDlpMetadata()` because the private API fails with entries that aren't
 * from FileSystem API. For fake entries the provider should return an empty
 * metadata object.
 */
export async function testDlpMetadataProviderIgnoresFakeEntries(
    done: () => void) {
  const provider = new DlpMetadataProvider();
  const results = await provider.get([
    new MetadataRequest(
        fakeEntry as unknown as FileSystemEntry,
        ['sourceUrl', 'isDlpRestricted', 'isRestrictedForDestination']),
    new MetadataRequest(
        realEntry as unknown as FileSystemEntry,
        ['sourceUrl', 'isDlpRestricted', 'isRestrictedForDestination']),
  ]);

  assertEquals(2, results.length);
  assertEquals(results[0]!.isDlpRestricted, undefined);
  assertEquals(results[0]!.sourceUrl, undefined);
  assertEquals(results[0]!.isRestrictedForDestination, undefined);
  assertEquals(results[1]!.isDlpRestricted, true);
  assertEquals(results[1]!.sourceUrl, 'https://example.com');
  assertEquals(results[1]!.isRestrictedForDestination, false);

  done();
}
