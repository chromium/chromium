// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertThrows} from 'chrome://webui-test/chromeos/chai_assert.js';

import {reportPromise} from '../../../common/js/test_error_reporting.js';
import {FilesAppEntry} from '../../../externs/files_app_entry_interfaces.js';

import {MetadataItem, MetadataKey} from './metadata_item.js';
import {MetadataModel} from './metadata_model.js';
import {MetadataProvider} from './metadata_provider.js';
import {MetadataRequest} from './metadata_request.js';

class TestMetadataProvider extends MetadataProvider {
  requestCount = 0;

  constructor() {
    super(['thumbnailUrl', 'mediaAlbum', 'mediaArtist']);
  }

  override get(requests: MetadataRequest[]) {
    this.requestCount++;
    return Promise.resolve(requests.map(request => {
      const entry = request.entry;
      const result: MetadataItem = {};
      for (const name of request.names) {
        // Only string metadata properties are valid in this metadata provider.
        (result[name] as string) = entry.toURL() + ':' + name;
      }
      return result;
    }));
  }
}

class TestEmptyMetadataProvider extends MetadataProvider {
  constructor() {
    super(['thumbnailUrl']);
  }

  override get(requests: MetadataRequest[]) {
    return Promise.resolve(requests.map(() => {
      return {};
    }));
  }
}

class ManualTestMetadataProvider extends MetadataProvider {
  callback: Array<(value: MetadataItem[]) => void> = [];

  constructor() {
    super(['mediaAlbum', 'mediaArtist', 'alternateUrl']);
  }

  override get(_requests: MetadataRequest[]) {
    return new Promise<MetadataItem[]>(fulfill => {
      this.callback.push(fulfill);
    });
  }
}

class TestFilesAppEntry extends FilesAppEntry {
  constructor(private url_: string) {
    super();
  }

  // This function implements an existing function from FileSystemEntry, so we
  // can't change the name.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override toURL() {
    return this.url_;
  }
}

const entryA = new TestFilesAppEntry('filesystem://A');
const entryB = new TestFilesAppEntry('filesystem://B');

/**
 * Returns a property of a Metadata result object.
 */
function getProperty<K extends MetadataKey>(
    result: MetadataItem|undefined, property: K): MetadataItem[K] {
  if (!result) {
    throw new Error('Fail: Metadata result is undefined');
  }
  return result[property];
}

export function testMetadataModelBasic(callback: () => void) {
  const provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['thumbnailUrl']).then(results => {
        assertEquals(1, provider.requestCount);
        assertEquals(
            'filesystem://A:thumbnailUrl',
            getProperty(results[0], 'thumbnailUrl'));
        assertEquals(
            'filesystem://B:thumbnailUrl',
            getProperty(results[1], 'thumbnailUrl'));
      }),
      callback);
}

export function testMetadataModelRequestForCachedProperty(
    callback: () => void) {
  const provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['thumbnailUrl'])
          .then(() => {
            // All the results should be cached here.
            return model.get([entryA, entryB], ['thumbnailUrl']);
          })
          .then(results => {
            assertEquals(1, provider.requestCount);
            assertEquals(
                'filesystem://A:thumbnailUrl',
                getProperty(results[0], 'thumbnailUrl'));
            assertEquals(
                'filesystem://B:thumbnailUrl',
                getProperty(results[1], 'thumbnailUrl'));
          }),
      callback);
}

export function testMetadataModelRequestForCachedAndNonCachedProperty(
    callback: () => void) {
  const provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['mediaAlbum'])
          .then(() => {
            assertEquals(1, provider.requestCount);
            // mediaArtist has not been cached here.
            return model.get([entryA, entryB], ['mediaAlbum', 'mediaArtist']);
          })
          .then(results => {
            assertEquals(2, provider.requestCount);
            assertEquals(
                'filesystem://A:mediaAlbum',
                getProperty(results[0], 'mediaAlbum'));
            assertEquals(
                'filesystem://A:mediaArtist',
                getProperty(results[0], 'mediaArtist'));
            assertEquals(
                'filesystem://B:mediaAlbum',
                getProperty(results[1], 'mediaAlbum'));
            assertEquals(
                'filesystem://B:mediaArtist',
                getProperty(results[1], 'mediaArtist'));
          }),
      callback);
}

export function testMetadataModelRequestForCachedAndNonCachedEntry(
    callback: () => void) {
  const provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA], ['thumbnailUrl'])
          .then(() => {
            assertEquals(1, provider.requestCount);
            // entryB has not been cached here.
            return model.get([entryA, entryB], ['thumbnailUrl']);
          })
          .then(results => {
            assertEquals(2, provider.requestCount);
            assertEquals(
                'filesystem://A:thumbnailUrl',
                getProperty(results[0], 'thumbnailUrl'));
            assertEquals(
                'filesystem://B:thumbnailUrl',
                getProperty(results[1], 'thumbnailUrl'));
          }),
      callback);
}

export function testMetadataModelRequestBeforeCompletingPreviousRequest(
    callback: () => void) {
  const provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  model.get([entryA], ['thumbnailUrl']);
  assertEquals(1, provider.requestCount);

  // The result of first call has not been fetched yet.
  reportPromise(
      model.get([entryA], ['thumbnailUrl']).then(results => {
        assertEquals(1, provider.requestCount);
        assertEquals(
            'filesystem://A:thumbnailUrl',
            getProperty(results[0], 'thumbnailUrl'));
      }),
      callback);
}

export function testMetadataModelNotUpdateCachedResultAfterRequest(
    callback: () => void) {
  const provider = new ManualTestMetadataProvider();
  const model = new MetadataModel(provider);

  const promise = model.get([entryA], ['mediaAlbum']);
  provider.callback[0]!([{mediaAlbum: 'album1'}]);

  reportPromise(
      promise
          .then(() => {
            // 'mediaAlbum' is cached here.
            const promise1 = model.get([entryA], ['mediaAlbum', 'mediaArtist']);
            const promise2 = model.get([entryA], ['alternateUrl']);
            // Returns alternateUrl.
            provider.callback[2]!
                ([{mediaAlbum: 'album2', alternateUrl: 'urlC'}]);
            provider.callback[1]!([{mediaArtist: 'artistB'}]);
            return Promise.all([promise1, promise2]);
          })
          .then(results => {
            // The result should be cached value at the time when get was
            // called.
            assertEquals('album1', getProperty(results[0][0], 'mediaAlbum'));
            assertEquals('artistB', getProperty(results[0][0], 'mediaArtist'));
            assertEquals('urlC', getProperty(results[1][0], 'alternateUrl'));
          }),
      callback);
}

export function testMetadataModelGetCache(callback: () => void) {
  const provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  const promise = model.get([entryA], ['thumbnailUrl']);
  const cache = model.getCache([entryA], ['thumbnailUrl']);
  assertEquals(null, getProperty(cache[0], 'thumbnailUrl'));

  reportPromise(
      promise.then(() => {
        const cache = model.getCache([entryA], ['thumbnailUrl']);
        assertEquals(1, provider.requestCount);
        assertEquals(
            'filesystem://A:thumbnailUrl',
            getProperty(cache[0], 'thumbnailUrl'));
      }),
      callback);
}

export function testMetadataModelUnknownProperty() {
  const provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  assertThrows(() => {
    model.get([entryA], ['unknown']);
  });
}

export function testMetadataModelEmptyResult(callback: () => void) {
  const provider = new TestEmptyMetadataProvider();
  const model = new MetadataModel(provider);

  // getImpl returns empty result.
  reportPromise(
      model.get([entryA], ['thumbnailUrl']).then(results => {
        assertEquals(undefined, getProperty(results[0], 'thumbnailUrl'));
      }),
      callback);
}
