// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://test/chai_assert.js';
import {MetadataCacheSet, MetadataCacheSetStorageForObject} from './metadata_cache_set.js';

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

export function testMetadataCacheSetBasic() {
  const set = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
  const loadRequested = set.createRequests([entryA, entryB], ['property']);
  assertEquals(2, loadRequested.length);
  assertEquals(entryA, loadRequested[0].entry);
  assertEquals(1, loadRequested[0].names.length);
  assertEquals('property', loadRequested[0].names[0]);
  assertEquals(entryB, loadRequested[1].entry);
  assertEquals(1, loadRequested[1].names.length);
  assertEquals('property', loadRequested[1].names[0]);

  set.startRequests(1, loadRequested);
  assertTrue(set.storeProperties(
      1, [entryA, entryB], [{property: 'valueA'}, {property: 'valueB'}], []));

  const results = set.get([entryA, entryB], ['property']);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0].property);
  assertEquals('valueB', results[1].property);
}

export function testMetadataCacheSetStorePartial() {
  const set = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
  set.startRequests(1, set.createRequests([entryA, entryB], ['property']));

  assertTrue(set.storeProperties(1, [entryA], [{property: 'valueA'}], []));
  let results = set.get([entryA, entryB], ['property']);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0].property);
  assertEquals(null, results[1].property);

  assertTrue(set.storeProperties(1, [entryB], [{property: 'valueB'}], []));
  results = set.get([entryA, entryB], ['property']);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0].property);
  assertEquals('valueB', results[1].property);
}

export function testMetadataCacheSetCachePartial() {
  const set = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
  set.startRequests(1, set.createRequests([entryA], ['property']));
  set.storeProperties(1, [entryA], [{property: 'valueA'}], []);

  // entryA has already been cached.
  const loadRequested = set.createRequests([entryA, entryB], ['property']);
  assertEquals(1, loadRequested.length);
  assertEquals(entryB, loadRequested[0].entry);
  assertEquals(1, loadRequested[0].names.length);
  assertEquals('property', loadRequested[0].names[0]);
}

export function testMetadataCacheSetInvalidatePartial() {
  const set = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
  set.startRequests(1, set.createRequests([entryA, entryB], ['property']));
  set.invalidate(2, [entryA]);

  assertTrue(set.storeProperties(
      1, [entryA, entryB], [{property: 'valueA'}, {property: 'valueB'}], []));

  const results = set.get([entryA, entryB], ['property']);
  assertEquals(2, results.length);
  assertEquals(null, results[0].property);
  assertEquals('valueB', results[1].property);

  const loadRequested = set.createRequests([entryA, entryB], ['property']);
  assertEquals(1, loadRequested.length);
  assertEquals(entryA, loadRequested[0].entry);
  assertEquals(1, loadRequested[0].names.length);
  assertEquals('property', loadRequested[0].names[0]);
}

export function testMetadataCacheSetCreateSnapshot() {
  const setA = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
  setA.startRequests(1, setA.createRequests([entryA, entryB], ['property']));
  const setB = setA.createSnapshot([entryA]);
  setA.storeProperties(
      1, [entryA, entryB], [{property: 'valueA'}, {property: 'valueB'}], []);
  let results = setB.get([entryA, entryB], ['property']);
  assertEquals(2, results.length);
  assertEquals(null, results[0].property);
  assertEquals(undefined, results[1].property);

  setB.storeProperties(
      1, [entryA, entryB], [{property: 'valueA'}, {property: 'valueB'}], []);
  results = setB.get([entryA, entryB], ['property']);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0].property);
  assertEquals(undefined, results[1].property);

  setA.invalidate(2, [entryA, entryB]);
  results = setB.get([entryA, entryB], ['property']);
  assertEquals(2, results.length);
  assertEquals('valueA', results[0].property);
  assertEquals(undefined, results[1].property);
}

export function testMetadataCacheSetHasFreshCache() {
  const set = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
  assertFalse(set.hasFreshCache([entryA, entryB], ['property']));

  set.startRequests(1, set.createRequests([entryA, entryB], ['property']));
  set.storeProperties(
      1, [entryA, entryB], [{property: 'valueA'}, {property: 'valueB'}], []);
  assertTrue(set.hasFreshCache([entryA, entryB], ['property']));

  set.invalidate(2, [entryB]);
  assertFalse(set.hasFreshCache([entryA, entryB], ['property']));

  assertTrue(set.hasFreshCache([entryA], ['property']));
}

export function testMetadataCacheSetHasFreshCacheWithEmptyNames() {
  const set = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
  assertTrue(set.hasFreshCache([entryA, entryB], []));
}

export function testMetadataCacheSetClear() {
  const set = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
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
  const set = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
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
  const set = new MetadataCacheSet(new MetadataCacheSetStorageForObject({}));
  set.startRequests(1, set.createRequests([entryA, entryB], ['propertyA']));
  set.storeProperties(
      1, [entryA, entryB], [{propertyA: 'value'}, {propertyA: 'value'}], []);

  assertTrue(set.hasFreshCache([entryA, entryB], ['propertyA']));
  set.clearAll();
  assertFalse(set.hasFreshCache([entryA], ['propertyA']));
  assertFalse(set.hasFreshCache([entryB], ['propertyA']));
}
