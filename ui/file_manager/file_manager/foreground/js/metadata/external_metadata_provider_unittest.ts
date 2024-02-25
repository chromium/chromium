// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {installMockChrome} from '../../../common/js/mock_chrome.js';

import {ExternalMetadataProvider} from './external_metadata_provider.js';
import {MetadataRequest} from './metadata_request.js';

const entryA = {
  toURL: function() {
    return 'filesystem://A';
  },
  isFile: true,
} as Entry;

const entryB = {
  toURL: function() {
    return 'filesystem://B';
  },
  isFile: true,
} as Entry;

/**
 * Mock chrome APIs.
 */
let mockChrome;

export async function testExternalMetadataProviderBasic() {
  // Set up mock chrome APIs.
  mockChrome = {
    fileManagerPrivate: {
      getEntryProperties: function(
          entries: Entry[], names: string[],
          callback: (props: chrome.fileManagerPrivate.EntryProperties[]) =>
              void) {
        assertEquals(2, entries.length);
        assertEquals('filesystem://A', entries[0]?.toURL());
        assertEquals('filesystem://B', entries[1]?.toURL());
        assertEquals(2, names.length);
        assertEquals('modificationTime', names[0]);
        assertEquals('size', names[1]);
        callback([
          {
            modificationTime: new Date(2015, 0, 1).getTime(),
            size: 1024,
            isMachineRoot: true,
            isExternalMedia: true,
            isArbitrarySyncFolder: true,
          } as chrome.fileManagerPrivate.EntryProperties,
          {
            modificationTime: new Date(2015, 1, 2).getTime(),
            size: 2048,
            isMachineRoot: false,
            isExternalMedia: false,
            isArbitrarySyncFolder: false,
          } as chrome.fileManagerPrivate.EntryProperties,
        ]);
      },
    },
    runtime: {
      lastError: undefined,
    },
  };

  installMockChrome(mockChrome);

  const provider = new ExternalMetadataProvider();
  const results = await provider.get([
    new MetadataRequest(entryA, ['modificationTime', 'size']),
    new MetadataRequest(entryB, ['modificationTime', 'size']),
  ]);
  assertEquals(2, results.length);
  assertEquals(
      new Date(2015, 0, 1).toString(),
      results[0]!.modificationTime!.toString());
  assertEquals(1024, results[0]!.size);
  assertEquals(true, results[0]!.isMachineRoot);
  assertEquals(true, results[0]!.isExternalMedia);
  assertEquals(true, results[0]!.isArbitrarySyncFolder);
  assertEquals(
      new Date(2015, 1, 2).toString(),
      results[1]!.modificationTime!.toString());
  assertEquals(2048, results[1]!.size);
  assertEquals(false, results[1]!.isMachineRoot);
  assertEquals(false, results[1]!.isExternalMedia);
  assertEquals(false, results[1]!.isArbitrarySyncFolder);
}
