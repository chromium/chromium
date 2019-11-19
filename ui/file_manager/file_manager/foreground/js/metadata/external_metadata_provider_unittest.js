// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {!Entry} */
const entryA = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://A';
  },
  isFile: true,
});

/** @const {!Entry} */
const entryB = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://B';
  },
  isFile: true,
});

/**
 * Mock chrome APIs.
 * @type {!Object}
 */
let mockChrome;

function testExternalMetadataProviderBasic(callback) {
  // Setup mock chrome APIs.
  mockChrome = {
    fileManagerPrivate: {
      getEntryProperties: function(entries, names, callback) {
        assertEquals(2, entries.length);
        assertEquals('filesystem://A', entries[0].toURL());
        assertEquals('filesystem://B', entries[1].toURL());
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
          },
          {
            modificationTime: new Date(2015, 1, 2).getTime(),
            size: 2048,
            isMachineRoot: false,
            isExternalMedia: false,
            isArbitrarySyncFolder: false,
          }
        ]);
      }
    },
    runtime: {
      lastError: null,
    },
  };

  installMockChrome(mockChrome);

  const provider = new ExternalMetadataProvider();
  reportPromise(
      provider
          .get([
            new MetadataRequest(entryA, ['modificationTime', 'size']),
            new MetadataRequest(entryB, ['modificationTime', 'size']),
          ])
          .then(results => {
            assertEquals(2, results.length);
            assertEquals(
                new Date(2015, 0, 1).toString(),
                results[0].modificationTime.toString());
            assertEquals(1024, results[0].size);
            assertEquals(true, results[0].isMachineRoot);
            assertEquals(true, results[0].isExternalMedia);
            assertEquals(true, results[0].isArbitrarySyncFolder);
            assertEquals(
                new Date(2015, 1, 2).toString(),
                results[1].modificationTime.toString());
            assertEquals(2048, results[1].size);
            assertEquals(false, results[1].isMachineRoot);
            assertEquals(false, results[1].isExternalMedia);
            assertEquals(false, results[1].isArbitrarySyncFolder);
          }),
      callback);
}
