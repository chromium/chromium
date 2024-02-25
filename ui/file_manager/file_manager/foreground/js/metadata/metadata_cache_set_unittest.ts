// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {MetadataSetEvent} from './metadata_cache_set.js';
import {MetadataCacheSet} from './metadata_cache_set.js';
import type {MetadataKey} from './metadata_item.js';

const entryA = {
  toURL: function() {
    return 'filesystem://A';
  },
} as Entry;

const entryB = {
  toURL: function() {
    return 'filesystem://B';
  },
} as Entry;

const propertyNames: MetadataKey[] = ['thumbnailUrl'];

export function testMetadataCacheSetBasic() {
  const set = new MetadataCacheSet();
  const loadRequested = set.createRequests([entryA, entryB], propertyNames);
  assertDeepEquals(
      [
        {entry: entryA, names: propertyNames},
        {entry: entryB, names: propertyNames},
      ],
      loadRequested);

  set.startRequests(1, loadRequested);
  assertTrue(set.storeProperties(
      1, [entryA, entryB], [{thumbnailUrl: 'valueA'}, {thumbnailUrl: 'valueB'}],
      []));

  const results = set.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0]!.thumbnailUrl);
  assertEquals('valueB', results[1]!.thumbnailUrl);
}

export function testMetadataCacheSetStorePartial() {
  const set = new MetadataCacheSet();
  set.startRequests(1, set.createRequests([entryA, entryB], propertyNames));

  assertTrue(set.storeProperties(1, [entryA], [{thumbnailUrl: 'valueA'}], []));
  let results = set.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0]!.thumbnailUrl);
  assertEquals(null, results[1]!.thumbnailUrl);

  assertTrue(set.storeProperties(1, [entryB], [{thumbnailUrl: 'valueB'}], []));
  results = set.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0]!.thumbnailUrl);
  assertEquals('valueB', results[1]!.thumbnailUrl);
}

export function testMetadataCacheSetCachePartial() {
  const set = new MetadataCacheSet();
  set.startRequests(1, set.createRequests([entryA], propertyNames));
  set.storeProperties(1, [entryA], [{thumbnailUrl: 'valueA'}], []);

  // entryA has already been cached.
  const loadRequested = set.createRequests([entryA, entryB], propertyNames);
  assertEquals(1, loadRequested.length);
  assertEquals(entryB, loadRequested[0]!.entry);
  assertDeepEquals(propertyNames, loadRequested[0]!.names);
}

export function testMetadataCacheSetInvalidatePartial() {
  const set = new MetadataCacheSet();
  set.startRequests(1, set.createRequests([entryA, entryB], propertyNames));
  set.invalidate(2, [entryA]);

  assertTrue(set.storeProperties(
      1, [entryA, entryB], [{thumbnailUrl: 'valueA'}, {thumbnailUrl: 'valueB'}],
      []));

  const results = set.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals(null, results[0]!.thumbnailUrl);
  assertEquals('valueB', results[1]!.thumbnailUrl);

  const loadRequested = set.createRequests([entryA, entryB], propertyNames);
  assertDeepEquals([{entry: entryA, names: propertyNames}], loadRequested);
}

export function testMetadataCacheSetCreateSnapshot() {
  const setA = new MetadataCacheSet();
  setA.startRequests(1, setA.createRequests([entryA, entryB], propertyNames));
  const setB = setA.createSnapshot([entryA]);
  setA.storeProperties(
      1, [entryA, entryB], [{thumbnailUrl: 'valueA'}, {thumbnailUrl: 'valueB'}],
      []);
  let results = setB.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals(null, results[0]!.thumbnailUrl);
  assertEquals(undefined, results[1]!.thumbnailUrl);

  setB.storeProperties(
      1, [entryA, entryB], [{thumbnailUrl: 'valueA'}, {thumbnailUrl: 'valueB'}],
      []);
  results = setB.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0]!.thumbnailUrl);
  assertEquals(undefined, results[1]!.thumbnailUrl);

  setA.invalidate(2, [entryA, entryB]);
  results = setB.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0]!.thumbnailUrl);
  assertEquals(undefined, results[1]!.thumbnailUrl);
}

export function testMetadataCacheSetHasFreshCache() {
  const set = new MetadataCacheSet();
  assertFalse(set.hasFreshCache([entryA, entryB], propertyNames));

  set.startRequests(1, set.createRequests([entryA, entryB], propertyNames));
  set.storeProperties(
      1, [entryA, entryB], [{thumbnailUrl: 'valueA'}, {thumbnailUrl: 'valueB'}],
      []);
  assertTrue(set.hasFreshCache([entryA, entryB], propertyNames));

  set.invalidate(2, [entryB]);
  assertFalse(set.hasFreshCache([entryA, entryB], propertyNames));

  assertTrue(set.hasFreshCache([entryA], propertyNames));
}

export function testMetadataCacheSetHasFreshCacheWithEmptyNames() {
  const set = new MetadataCacheSet();
  assertTrue(set.hasFreshCache([entryA, entryB], []));
}

export function testMetadataCacheSetClear() {
  const set = new MetadataCacheSet();
  set.startRequests(1, set.createRequests([entryA], ['contentThumbnailUrl']));
  set.storeProperties(1, [entryA], [{contentThumbnailUrl: 'value'}], []);
  assertTrue(set.hasFreshCache([entryA], ['contentThumbnailUrl']));

  set.startRequests(1, set.createRequests([entryA], ['contentMimeType']));
  set.clear([entryA.toURL()]);
  // contentMimeType should not be stored because it is requested before clear.
  set.storeProperties(1, [entryA], [{contentMimeType: 'value'}], []);

  assertFalse(set.hasFreshCache([entryA], ['contentThumbnailUrl']));
  assertFalse(set.hasFreshCache([entryA], ['contentMimeType']));
}

export function testMetadataCacheSetUpdateEvent() {
  const set = new MetadataCacheSet();
  let event: MetadataSetEvent|null = null;
  set.addEventListener('update', inEvent => {
    event = inEvent as MetadataSetEvent;
  });
  set.startRequests(1, set.createRequests([entryA], ['contentThumbnailUrl']));
  set.storeProperties(1, [entryA], [{contentThumbnailUrl: 'value'}], []);
  assertEquals(1, event!.entries.length);
  assertEquals(entryA, event!.entries[0]);
  assertTrue(event!.entriesMap.has(entryA.toURL()));
  assertFalse(event!.entriesMap.has(entryB.toURL()));
  assertFalse(event!.names.has('contentThumbnailUrl'));
}

export function testMetadataCacheSetClearAll() {
  const set = new MetadataCacheSet();
  set.startRequests(
      1, set.createRequests([entryA, entryB], ['contentThumbnailUrl']));
  set.storeProperties(
      1, [entryA, entryB],
      [{contentThumbnailUrl: 'value'}, {contentThumbnailUrl: 'value'}], []);

  assertTrue(set.hasFreshCache([entryA, entryB], ['contentThumbnailUrl']));
  set.clearAll();
  assertFalse(set.hasFreshCache([entryA], ['contentThumbnailUrl']));
  assertFalse(set.hasFreshCache([entryB], ['contentThumbnailUrl']));
}
