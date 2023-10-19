// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {reportPromise} from '../../../common/js/test_error_reporting.js';

import {FileSystemMetadataProvider} from './file_system_metadata_provider.js';
import {MetadataRequest} from './metadata_request.js';

/** @const @type {!Entry} */
const entryA = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://A';
  },
  getMetadata: function(fulfill, reject) {
    Promise.resolve({modificationTime: new Date(2015, 1, 1), size: 1024})
        .then(fulfill, reject);
  },
});

/** @const @type {!Entry} */
const entryB = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://B';
  },
  getMetadata: function(fulfill, reject) {
    Promise.resolve({modificationTime: new Date(2015, 2, 2), size: 2048})
        .then(fulfill, reject);
  },
});

/** @param {function(): void} callback */
export function testFileSystemMetadataProviderBasic(callback) {
  const provider = new FileSystemMetadataProvider();
  const names = [
    'modificationTime',
    'size',
    'contentMimeType',
    'present',
    'availableOffline',
  ];
  reportPromise(
      provider
          .get([
            new MetadataRequest(entryA, names),
            new MetadataRequest(entryB, names),
          ])
          .then(results => {
            assertEquals(2, results.length);
            assertEquals(
                new Date(2015, 1, 1).toString(),
                results[0].modificationTime.toString());
            assertEquals(1024, results[0].size);
            assertTrue(!!results[0].present);
            assertTrue(!!results[0].availableOffline);
            assertEquals(
                new Date(2015, 2, 2).toString(),
                results[1].modificationTime.toString());
            assertEquals(2048, results[1].size);
            assertTrue(!!results[1].present);
            assertTrue(!!results[1].availableOffline);
          }),
      callback);
}

/** @param {function(): void} callback */
export function testFileSystemMetadataProviderPartialRequest(callback) {
  const provider = new FileSystemMetadataProvider();
  reportPromise(
      provider.get([new MetadataRequest(entryA, ['modificationTime', 'size'])])
          .then(results => {
            assertEquals(1, results.length);
            assertEquals(
                new Date(2015, 1, 1).toString(),
                results[0].modificationTime.toString());
            assertEquals(1024, results[0].size);
          }),
      callback);
}
