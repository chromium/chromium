// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {FileSystemMetadataProvider} from './file_system_metadata_provider.js';
import type {MetadataKey} from './metadata_item.js';
import {MetadataRequest} from './metadata_request.js';

const entryA: Entry = {
  toURL: function() {
    return 'filesystem://A';
  },
  getMetadata: function(fulfill, reject) {
    Promise.resolve({modificationTime: new Date(2015, 1, 1), size: 1024})
        .then(fulfill, reject);
  },
} as Entry;

const entryB: Entry = {
  toURL: function() {
    return 'filesystem://B';
  },
  getMetadata: function(fulfill, reject) {
    Promise.resolve({modificationTime: new Date(2015, 2, 2), size: 2048})
        .then(fulfill, reject);
  },
} as Entry;

export async function testFileSystemMetadataProviderBasic() {
  const provider = new FileSystemMetadataProvider();
  const names: MetadataKey[] = [
    'modificationTime',
    'size',
    'contentMimeType',
    'present',
    'availableOffline',
  ];
  const results = await provider.get([
    new MetadataRequest(entryA, names),
    new MetadataRequest(entryB, names),
  ]);
  assertEquals(2, results.length);
  assertEquals(
      new Date(2015, 1, 1).toString(),
      results[0]!.modificationTime!.toString());
  assertEquals(1024, results[0]!.size);
  assertTrue(!!results[0]!.present);
  assertTrue(!!results[0]!.availableOffline);
  assertEquals(
      new Date(2015, 2, 2).toString(),
      results[1]!.modificationTime!.toString());
  assertEquals(2048, results[1]!.size);
  assertTrue(!!results[1]!.present);
  assertTrue(!!results[1]!.availableOffline);
}

export async function testFileSystemMetadataProviderPartialRequest() {
  const provider = new FileSystemMetadataProvider();
  const results = await provider.get(
      [new MetadataRequest(entryA, ['modificationTime', 'size'])]);

  assertEquals(1, results.length);
  assertEquals(
      new Date(2015, 1, 1).toString(),
      results[0]!.modificationTime!.toString());
  assertEquals(1024, results[0]!.size);
}
