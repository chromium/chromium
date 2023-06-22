// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MetadataCacheSet} from './metadata_cache_set.js';

/** @const {!Entry} */
const entryA = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://A';
  },
});

/** @const {!Entry} */
const entryB = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://B';
  },
});

/** @const {!Array<string>} */
const propertyNames = ['thumbnailUrl'];

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
  assertEquals('valueA', results[0].thumbnailUrl);
  assertEquals('valueB', results[1].thumbnailUrl);
}

export function testMetadataCacheSetStorePartial() {
  const set = new MetadataCacheSet();
  set.startRequests(1, set.createRequests([entryA, entryB], propertyNames));

  assertTrue(set.storeProperties(1, [entryA], [{thumbnailUrl: 'valueA'}], []));
  let results = set.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0].thumbnailUrl);
  assertEquals(null, results[1].thumbnailUrl);

  assertTrue(set.storeProperties(1, [entryB], [{thumbnailUrl: 'valueB'}], []));
  results = set.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0].thumbnailUrl);
  assertEquals('valueB', results[1].thumbnailUrl);
}

export function testMetadataCacheSetCachePartial() {
  const set = new MetadataCacheSet();
  set.startRequests(1, set.createRequests([entryA], propertyNames));
  set.storeProperties(1, [entryA], [{thumbnailUrl: 'valueA'}], []);

  // entryA has already been cached.
  const loadRequested = set.createRequests([entryA, entryB], propertyNames);
  assertEquals(1, loadRequested.length);
  assertEquals(entryB, loadRequested[0].entry);
  assertDeepEquals(propertyNames, loadRequested[0].names);
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
  assertEquals(null, results[0].thumbnailUrl);
  assertEquals('valueB', results[1].thumbnailUrl);

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
  assertEquals(null, results[0].thumbnailUrl);
  assertEquals(undefined, results[1].thumbnailUrl);

  setB.storeProperties(
      1, [entryA, entryB], [{thumbnailUrl: 'valueA'}, {thumbnailUrl: 'valueB'}],
      []);
  results = setB.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0].thumbnailUrl);
  assertEquals(undefined, results[1].thumbnailUrl);

  setA.invalidate(2, [entryA, entryB]);
  results = setB.get([entryA, entryB], propertyNames);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0].thumbnailUrl);
  assertEquals(undefined, results[1].thumbnailUrl);
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
  set.startRequests(1, set.createRequests([entryA], ['propertyA']));
  set.storeProperties(1, [entryA], [{propertyA: 'value'}], []);
  assertTrue(set.hasFreshCache([entryA], ['propertyA']));

  set.startRequests(1, set.createRequests([entryA], ['propertyB']));
  set.clear([entryA.toURL()]);
  // PropertyB should not be stored because it is requsted before clear.
  set.storeProperties(1, [entryA], [{propertyB: 'value'}], []);

  assertFalse(set.hasFreshCache([entryA], ['propertyA']));
  assertFalse(set.hasFreshCache([entryA], ['propertyB']));
}

export function testMetadataCacheSetUpdateEvent() {
  const set = new MetadataCacheSet();
  let event = null;
  set.addEventListener('update', inEvent => {
    event = inEvent;
  });
  set.startRequests(1, set.createRequests([entryA], ['propertyA']));
  set.storeProperties(1, [entryA], [{propertyA: 'value'}], []);
  assertEquals(1, event.entries.length);
  assertEquals(entryA, event.entries[0]);
  assertTrue(event.entriesMap.has(entryA.toURL()));
  assertFalse(event.entriesMap.has(entryB.toURL()));
  assertFalse(event.names.has('propertyA'));
}

export function testMetadataCacheSetClearAll() {
  const set = new MetadataCacheSet();
  set.startRequests(1, set.createRequests([entryA, entryB], ['propertyA']));
  set.storeProperties(
      1, [entryA, entryB], [{propertyA: 'value'}, {propertyA: 'value'}], []);

  assertTrue(set.hasFreshCache([entryA, entryB], ['propertyA']));
  set.clearAll();
  assertFalse(set.hasFreshCache([entryA], ['propertyA']));
  assertFalse(set.hasFreshCache([entryB], ['propertyA']));
}
