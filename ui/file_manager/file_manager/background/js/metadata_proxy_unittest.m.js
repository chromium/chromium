// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assertEquals} from 'chrome://test/chai_assert.js';
import {reportPromise} from '../../../base/js/test_error_reporting.m.js';
import {MockFileSystem} from '../../common/js/mock_entry.m.js';
import {metadataProxy} from './metadata_proxy.m.js';
// clang-format on

export function testMetadataCaching(doneCallback) {
  reportPromise(
      (async function() {
        const mockFileSystem = new MockFileSystem('volumeId');
        mockFileSystem.populate(['/testMetadataCaching']);
        const entry = /** @type {FileEntry} */ (
            mockFileSystem.entries['/testMetadataCaching']);

        // Make sure getMetadata is called only once.
        let called = 0;
        entry.getMetadata =
            /**
             * @this {Entry}
             * @param {function(!Metadata)} onSuccess
             * @param {function(!FileError)=} onError
             */
            function(onSuccess, onError) {
              called++;
              onSuccess(/** @type {!Metadata} */ ({}));
            };

        await metadataProxy.getEntryMetadata(entry);
        await metadataProxy.getEntryMetadata(entry);
        assertEquals(1, called);
      })(),
      doneCallback);
}

export function testMetadataCacheExpiry(doneCallback) {
  reportPromise(
      (async function() {
        const mockFileSystem = new MockFileSystem('volumeId');
        mockFileSystem.populate(['/testMetadataCacheExpiry']);
        const entry = /** @type {FileEntry} */ (
            mockFileSystem.entries['/testMetadataCacheExpiry']);

        // Make sure getMetadata is called twice.
        let called = 0;
        entry.getMetadata =
            /**
             * @this {Entry}
             * @param {function(!Metadata)} onSuccess
             * @param {function(!FileError)=} onError
             */
            function(onSuccess, onError) {
              called++;
              onSuccess(/** @type {!Metadata} */ ({}));
            };

        metadataProxy.overrideCacheTtlForTesting(1);

        await metadataProxy.getEntryMetadata(entry);
        await new Promise((resolve) => setTimeout(resolve, 1200));
        await metadataProxy.getEntryMetadata(entry);

        metadataProxy.overrideCacheTtlForTesting();

        assertEquals(2, called);
      })(),
      doneCallback);
}
