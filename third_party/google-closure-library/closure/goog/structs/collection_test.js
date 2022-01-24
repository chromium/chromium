/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.CollectionTest');
goog.setTestOnly();

const AvlTree = goog.require('goog.structs.AvlTree');
const StructsSet = goog.require('goog.structs.Set');
const testSuite = goog.require('goog.testing.testSuite');

// Simple exercise of a collection object.
function exerciseCollection(collection) {
  assertEquals(0, collection.getCount());

  for (let i = 1; i <= 10; i++) {
    assertFalse(collection.contains(i));
    collection.add(i);
    assertTrue(collection.contains(i));
    assertEquals(i, collection.getCount());
  }

  assertEquals(10, collection.getCount());

  for (let i = 10; i > 0; i--) {
    assertTrue(collection.contains(i));
    collection.remove(i);
    assertFalse(collection.contains(i));
    assertEquals(i - 1, collection.getCount());
  }

  assertEquals(0, collection.getCount());
}
testSuite({
  testSet() {
    const set = new StructsSet();
    exerciseCollection(set);
  },

  testAvlTree() {
    const tree = new AvlTree();
    exerciseCollection(tree);
  },
});
