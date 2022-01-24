/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.async.FreeListTest');
goog.setTestOnly();

const FreeList = goog.require('goog.async.FreeList');
const testSuite = goog.require('goog.testing.testSuite');

let id = 0;
let list = null;

testSuite({
  setUp() {
    let id = 0;
    let data = 1;
    list = new FreeList(
        () => {
          data *= 2;
          return {id: id++, data: data, next: null};
        },
        (item) => {
          item.data = null;
        },
        2);  // max occupancy
  },

  tearDown() {
    list = null;
  },

  testItemsCreatedAsNeeded() {
    assertEquals(0, list.occupants());
    const item1 = list.get();
    assertNotNullNorUndefined(item1);
    const item2 = list.get();
    assertNotNullNorUndefined(item2);
    assertNotEquals(item1, item2);
    assertEquals(0, list.occupants());
  },

  testMaxOccupancy() {
    assertEquals(0, list.occupants());
    const item1 = list.get();
    const item2 = list.get();
    const item3 = list.get();

    list.put(item1);
    list.put(item2);
    list.put(item3);

    assertEquals(2, list.occupants());
  },

  testRecycling() {
    assertEquals(0, list.occupants());
    const item1 = list.get();
    assertNotNull(item1.data);

    list.put(item1);

    const item2 = list.get();

    // Item recycled
    assertEquals(item1, item2);
    // reset method called
    assertNull(item2.data);
  },
});
