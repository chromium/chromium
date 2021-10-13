/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.QuadTreeTest');
goog.setTestOnly();

const QuadTree = goog.require('goog.structs.QuadTree');
const structs = goog.require('goog.structs');
const testSuite = goog.require('goog.testing.testSuite');

function getTree() {
  const qt = new QuadTree(0, 0, 100, 100);
  qt.set(5, 20, 'Foo');
  qt.set(50, 32, 'Bar');
  qt.set(47, 96, 'Baz');
  qt.set(50, 50, 'Bing');
  qt.set(12, 0, 'Bong');
  return qt;
}

// Helper functions

function assertFails(context, fn, args) {
  assertThrows(
      'Exception expected from ' + fn.toString() + ' with arguments ' + args,
      () => {
        fn.apply(context, args);
      });
}

function assertTreesChildrenAreNull(qt) {
  const root = qt.getRootNode();
  assertNull('NE should be null', root.ne);
  assertNull('NW should be null', root.nw);
  assertNull('SE should be null', root.se);
  assertNull('SW should be null', root.sw);
}
testSuite({
  testGetCount() {
    const qt = getTree();
    assertEquals('Count should be 5', 5, qt.getCount());
    qt.remove(50, 32);
    assertEquals('Count should be 4', 4, qt.getCount());
  },

  testGetKeys() {
    const keys = getTree().getKeys();
    const keyString = keys.sort().join(' ');
    const expected = '(12, 0) (47, 96) (5, 20) (50, 32) (50, 50)';
    assertEquals(`Sorted keys should be ${expected}`, expected, keyString);
  },

  testGetValues() {
    const values = getTree().getValues();
    const valueString = values.sort().join(',');
    assertEquals(
        'Sorted values should be Bar,Baz,Bing,Bong,Foo',
        'Bar,Baz,Bing,Bong,Foo', valueString);
  },

  testContains() {
    const qt = getTree();
    assertTrue('Should contain (5, 20)', qt.contains(5, 20));
    assertFalse('Should not contain (13, 13)', qt.contains(13, 13));
  },

  testClear() {
    const qt = getTree();
    qt.clear();
    assertTrue('Tree should be empty', qt.isEmpty());
    assertFalse('Tree should not contain (5, 20)', qt.contains(5, 20));
  },

  testConstructor() {
    const qt = new QuadTree(-10, -5, 6, 12);
    const root = qt.getRootNode();
    assertEquals('X of root should be -10', -10, root.x);
    assertEquals('Y of root should be -5', -5, root.y);
    assertEquals('Width of root should be 16', 16, root.w);
    assertEquals('Height of root should be 17', 17, root.h);
    assertTrue('Tree should be empty', qt.isEmpty());
  },

  testClone() {
    const qt = getTree().clone();
    assertFalse('Clone should not be empty', qt.isEmpty());
    assertTrue('Should contain (47, 96)', qt.contains(47, 96));
  },

  testRemove() {
    const qt = getTree();
    assertEquals('(5, 20) should be removed', 'Foo', qt.remove(5, 20));
    assertEquals('(5, 20) should be removed', 'Bar', qt.remove(50, 32));
    assertEquals('(5, 20) should be removed', 'Baz', qt.remove(47, 96));
    assertEquals('(5, 20) should be removed', 'Bing', qt.remove(50, 50));
    assertEquals('(5, 20) should be removed', 'Bong', qt.remove(12, 0));
    assertNull('(6, 6) wasn\'t there to remove', qt.remove(6, 6));
    assertTrue('Tree should be empty', qt.isEmpty());
    assertTreesChildrenAreNull(qt);
  },

  testIsEmpty() {
    const qt = getTree();
    qt.clear();
    assertTrue(qt.isEmpty());
    assertEquals(
        'Root should  be empty node', QuadTree.NodeType.EMPTY,
        qt.getRootNode().nodeType);
    assertTreesChildrenAreNull(qt);
  },

  testForEach() {
    const qt = getTree();
    let s = '';
    structs.forEach(qt, (val, key, qt2) => {
      assertNotUndefined(key);
      assertEquals(qt, qt2);
      s += key.x + ',' + key.y + '=' + val + ' ';
    });
    assertEquals('50,32=Bar 50,50=Bing 47,96=Baz 5,20=Foo 12,0=Bong ', s);
  },

  testBalancing() {
    const qt = new QuadTree(0, 0, 100, 100);
    const root = qt.getRootNode();

    // Add a point to the NW quadrant.
    qt.set(25, 25, 'first');

    assertEquals(
        'Root should be a leaf node.', QuadTree.NodeType.LEAF, root.nodeType);
    assertTreesChildrenAreNull(qt);

    assertEquals('first', root.point.value);

    // Add another point in the NW quadrant
    qt.set(25, 30, 'second');

    assertEquals(
        'Root should now be a pointer.', QuadTree.NodeType.POINTER,
        root.nodeType);
    assertNotNull('NE should be not be null', root.ne);
    assertNotNull('NW should be not be null', root.nw);
    assertNotNull('SE should be not be null', root.se);
    assertNotNull('SW should be not be null', root.sw);
    assertNull(root.point);

    // Delete the second point.
    qt.remove(25, 30);

    assertEquals(
        'Root should have been rebalanced and be a leaf node.',
        QuadTree.NodeType.LEAF, root.nodeType);
    assertTreesChildrenAreNull(qt);
    assertEquals('first', root.point.value);
  },

  testTreeBounds() {
    const qt = getTree();
    assertFails(qt, qt.set, [-10, -10, 1]);
    assertFails(qt, qt.set, [-10, 10, 2]);
    assertFails(qt, qt.set, [10, -10, 3]);
    assertFails(qt, qt.set, [-10, 110, 4]);
    assertFails(qt, qt.set, [10, 130, 5]);
    assertFails(qt, qt.set, [110, -10, 6]);
    assertFails(qt, qt.set, [150, 14, 7]);
  },
});
