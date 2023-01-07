/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.AvlTreeTest');
goog.setTestOnly();

const AvlTree = goog.require('goog.structs.AvlTree');
const googArray = goog.require('goog.array');
const testSuite = goog.require('goog.testing.testSuite');

// See https://github.com/google/closure-library/issues/896

/**
 * Asserts expected properties of an AVL tree.
 * @param {function(?, ?): number} comparator
 * @param {?} node
 * @suppress {checkTypes} suppression added to enable type checking
 */
function assertAvlTree(comparator, node) {
  if (node) {
    assertTrue(node.height < 1.4405 * Math.log2(node.count + 2) - 0.3277);
    assertTrue(node.height >= Math.log2(node.count + 1));
    let expectedCount = 1;
    let balanceFactor = 0;
    if (node.left) {
      balanceFactor -= node.left.height;
      expectedCount += node.left.count;
      assertTrue(comparator(node.value, node.left.value) > 0);
      assertAvlTree(node.left);
    }
    if (node.right) {
      balanceFactor += node.right.height;
      expectedCount += node.right.count;
      assertTrue(comparator(node.value, node.right.value) < 0);
      assertAvlTree(node.right);
    }
    assertTrue(Math.abs(balanceFactor) < 2);
    assertEquals(expectedCount, node.count);
  }
}

testSuite({
  /**
   * This test verifies that we can insert strings into the AvlTree and have
   * them be stored and sorted correctly by the default comparator.
   */
  testInsertsWithDefaultComparator() {
    const tree = new AvlTree();
    const values = ['bill', 'blake', 'elliot', 'jacob', 'john', 'myles', 'ted'];

    // Insert strings into tree out of order
    tree.add(values[4]);
    tree.add(values[3]);
    tree.add(values[0]);
    tree.add(values[6]);
    tree.add(values[5]);
    tree.add(values[1]);
    tree.add(values[2]);

    // Verify strings are stored in sorted order
    let i = 0;
    tree.inOrderTraverse((value) => {
      assertEquals(values[i], value);
      i += 1;
    });
    assertEquals(i, values.length);

    // Verify that no nodes are visited if the start value is larger than all
    // values
    tree.inOrderTraverse(/**
                            @suppress {checkTypes} suppression added to enable
                            type checking
                          */
                         (value) => {
                           fail();
                         },
                         'zed');

    // Verify strings are stored in sorted order
    i = values.length;
    tree.reverseOrderTraverse((value) => {
      i--;
      assertEquals(values[i], value);
    });
    assertEquals(i, 0);

    // Verify that no nodes are visited if the start value is smaller than all
    // values
    tree.reverseOrderTraverse(/**
                                 @suppress {checkTypes} suppression added to
                                 enable type checking
                               */
                              (value) => {
                                fail();
                              },
                              'aardvark');
  },

  /**
   * This test verifies that we can insert strings into and remove strings from
   * the AvlTree and have the only the non-removed values be stored and sorted
   * correctly by the default comparator.
   */
  testRemovesWithDefaultComparator() {
    const tree = new AvlTree();
    const values = ['bill', 'blake', 'elliot', 'jacob', 'john', 'myles', 'ted'];

    // Insert strings into tree out of order
    tree.add('frodo');
    tree.add(values[4]);
    tree.add(values[3]);
    tree.add(values[0]);
    tree.add(values[6]);
    tree.add('samwise');
    tree.add(values[5]);
    tree.add(values[1]);
    tree.add(values[2]);
    tree.add('pippin');

    // Remove strings from tree
    assertEquals(tree.remove('samwise'), 'samwise');
    assertEquals(tree.remove('pippin'), 'pippin');
    assertEquals(tree.remove('frodo'), 'frodo');
    assertEquals(tree.remove('merry'), null);

    // Verify strings are stored in sorted order
    let i = 0;
    tree.inOrderTraverse((value) => {
      assertEquals(values[i], value);
      i += 1;
    });
    assertEquals(i, values.length);
  },

  /**
   * This test verifies that we can insert values into and remove values from
   * the AvlTree and have them be stored and sorted correctly by a custom
   * comparator.
   */
  testInsertsAndRemovesWithCustomComparator() {
    const tree = new AvlTree((a, b) => a - b);

    const NUM_TO_INSERT = 37;
    const valuesToRemove = [1, 0, 6, 7, 36];

    // Insert ints into tree out of order
    const values = [];
    for (let i = 0; i < NUM_TO_INSERT; i += 1) {
      tree.add(i);
      values.push(i);
    }

    for (let i = 0; i < valuesToRemove.length; i += 1) {
      assertEquals(tree.remove(valuesToRemove[i]), valuesToRemove[i]);
      googArray.remove(values, valuesToRemove[i]);
    }
    assertEquals(tree.remove(-1), null);
    assertEquals(tree.remove(37), null);

    // Verify strings are stored in sorted order
    let i = 0;
    tree.inOrderTraverse((value) => {
      assertEquals(values[i], value);
      i += 1;
    });
    assertEquals(i, values.length);
  },

  /**
   * This test verifies that we can insert values into and remove values from
   * the AvlTree and have it maintain the AVL-Tree upperbound on its height.
   */
  testAvlTreeHeight() {
    const tree = new AvlTree((a, b) => a - b);

    const NUM_TO_INSERT = 2000;
    const NUM_TO_REMOVE = 500;

    // Insert ints into tree out of order
    for (let i = 0; i < NUM_TO_INSERT; i += 1) {
      tree.add(i);
    }

    // Remove valuse
    for (let i = 0; i < NUM_TO_REMOVE; i += 1) {
      tree.remove(i);
    }

    assertTrue(
        tree.getHeight() <=
        1.4405 * (Math.log(NUM_TO_INSERT - NUM_TO_REMOVE + 2) / Math.log(2)) -
            1.3277);
  },

  /**
   * This test verifies that we can insert values into and remove values from
   * the AvlTree and have its contains method correctly determine the values it
   * is contains.
   */
  testAvlTreeContains() {
    const tree = new AvlTree();
    const values = ['bill', 'blake', 'elliot', 'jacob', 'john', 'myles', 'ted'];

    // Insert strings into tree out of order
    tree.add('frodo');
    tree.add(values[4]);
    tree.add(values[3]);
    tree.add(values[0]);
    tree.add(values[6]);
    tree.add('samwise');
    tree.add(values[5]);
    tree.add(values[1]);
    tree.add(values[2]);
    tree.add('pippin');

    // Remove strings from tree
    assertEquals(tree.remove('samwise'), 'samwise');
    assertEquals(tree.remove('pippin'), 'pippin');
    assertEquals(tree.remove('frodo'), 'frodo');

    for (let i = 0; i < values.length; i += 1) {
      assertTrue(tree.contains(values[i]));
    }
    assertFalse(tree.contains('samwise'));
    assertFalse(tree.contains('pippin'));
    assertFalse(tree.contains('frodo'));
  },

  /**
   * This test verifies that we can insert values into and remove values from
   * the AvlTree and have its indexOf method correctly determine the in-order
   * index of the values it contains.
   */
  testAvlTreeIndexOf() {
    const tree = new AvlTree();
    const values = ['bill', 'blake', 'elliot', 'jacob', 'john', 'myles', 'ted'];

    // Insert strings into tree out of order
    tree.add('frodo');
    tree.add(values[4]);
    tree.add(values[3]);
    tree.add(values[0]);
    tree.add(values[6]);
    tree.add('samwise');
    tree.add(values[5]);
    tree.add(values[1]);
    tree.add(values[2]);
    tree.add('pippin');

    // Remove strings from tree
    assertEquals('samwise', tree.remove('samwise'));
    assertEquals('pippin', tree.remove('pippin'));
    assertEquals('frodo', tree.remove('frodo'));

    for (let i = 0; i < values.length; i += 1) {
      assertEquals(i, tree.indexOf(values[i]));
    }
    assertEquals(-1, tree.indexOf('samwise'));
    assertEquals(-1, tree.indexOf('pippin'));
    assertEquals(-1, tree.indexOf('frodo'));
  },

  /**
   * This test verifies that we can insert values into and remove values from
   * the AvlTree and have its minValue and maxValue routines return the correct
   * min and max values contained by the tree.
   */
  testMinAndMaxValues() {
    const tree = new AvlTree((a, b) => a - b);

    const NUM_TO_INSERT = 2000;
    const NUM_TO_REMOVE = 500;

    // Insert ints into tree out of order
    for (let i = 0; i < NUM_TO_INSERT; i += 1) {
      tree.add(i);
    }

    // Remove valuse
    for (let i = 0; i < NUM_TO_REMOVE; i += 1) {
      tree.remove(i);
    }

    assertEquals(tree.getMinimum(), NUM_TO_REMOVE);
    assertEquals(tree.getMaximum(), NUM_TO_INSERT - 1);
  },

  /**
   * This test verifies that we can insert values into and remove values from
   * the AvlTree and traverse the tree in reverse order using the
   * reverseOrderTraverse routine.
   */
  testReverseOrderTraverse() {
    const tree = new AvlTree((a, b) => a - b);

    const NUM_TO_INSERT = 2000;
    const NUM_TO_REMOVE = 500;

    // Insert ints into tree out of order
    for (let i = 0; i < NUM_TO_INSERT; i += 1) {
      tree.add(i);
    }

    // Remove valuse
    for (let i = 0; i < NUM_TO_REMOVE; i += 1) {
      tree.remove(i);
    }

    let i = NUM_TO_INSERT - 1;
    tree.reverseOrderTraverse((value) => {
      assertEquals(value, i);
      i -= 1;
    });
    assertEquals(i, NUM_TO_REMOVE - 1);
  },

  /**
     Verifies correct behavior of getCount(). See http://b/4347755
     @suppress {visibility} suppression added to enable type checking
   */
  testGetCountBehavior() {
    const tree = new AvlTree();
    tree.add(1);
    tree.remove(1);
    assertEquals(0, tree.getCount());

    const values = ['bill', 'blake', 'elliot', 'jacob', 'john', 'myles', 'ted'];

    // Insert strings into tree out of order
    tree.add('frodo');
    tree.add(values[4]);
    tree.add(values[3]);
    tree.add(values[0]);
    tree.add(values[6]);
    tree.add('samwise');
    tree.add(values[5]);
    tree.add(values[1]);
    tree.add(values[2]);
    tree.add('pippin');
    assertEquals(10, tree.getCount());
    assertEquals(
        tree.root_.left.count + tree.root_.right.count + 1, tree.getCount());

    // Remove strings from tree
    assertEquals('samwise', tree.remove('samwise'));
    assertEquals('pippin', tree.remove('pippin'));
    assertEquals('frodo', tree.remove('frodo'));
    assertEquals(null, tree.remove('merry'));
    assertEquals(7, tree.getCount());

    assertEquals(
        tree.root_.left.count + tree.root_.right.count + 1, tree.getCount());
  },

  /** This test verifies that getKthOrder gets the correct value. */
  testGetKthOrder() {
    const tree = new AvlTree((a, b) => a - b);

    const NUM_TO_INSERT = 2000;
    const NUM_TO_REMOVE = 500;

    // Insert ints into tree out of order
    for (let i = 0; i < NUM_TO_INSERT; i += 1) {
      tree.add(i);
    }

    // Remove values.
    for (let i = 0; i < NUM_TO_REMOVE; i += 1) {
      tree.remove(i);
    }
    for (let k = 0; k < tree.getCount(); ++k) {
      assertEquals(NUM_TO_REMOVE + k, tree.getKthValue(k));
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testTreeHeightAfterRightRotate() {
    const tree = new AvlTree();
    tree.add(0);
    tree.add(8);
    tree.add(5);
    assertEquals(2, tree.getHeight());
    assertEquals(5, tree.root_.value);
    assertEquals(0, tree.root_.left.value);
    assertEquals(8, tree.root_.right.value);

    assertEquals(2, tree.root_.height);
    assertEquals(1, tree.root_.left.height);
    assertEquals(1, tree.root_.right.height);

    assertEquals(0, tree.getMinimum());
    assertEquals(8, tree.getMaximum());
    assertEquals(3, tree.getCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddLeftLeftCase() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(100);
    tree.add(150);
    tree.add(50);
    tree.add(25);
    tree.add(75);
    tree.add(0);

    //        100                             50
    //       /   \                           /  \
    //      50   150   Rotate Right (100)   25   100
    //     /  \        ----------------->  /     /  \
    //    25   75                         0     75  150
    //   /
    //  0
    assertEquals(3, tree.getHeight());
    assertEquals(50, tree.root_.value);
    assertEquals(25, tree.root_.left.value);
    assertEquals(0, tree.root_.left.left.value);
    assertEquals(100, tree.root_.right.value);
    assertEquals(75, tree.root_.right.left.value);
    assertEquals(150, tree.root_.right.right.value);

    assertEquals(0, tree.getMinimum());
    assertEquals(150, tree.getMaximum());
    assertEquals(6, tree.getCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveLeftLeftCase() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(100);
    tree.add(150);
    tree.add(50);
    tree.add(25);
    tree.add(75);
    tree.add(200);
    tree.add(0);

    tree.remove(200);

    //        100                             50
    //       /   \                           /  \
    //      50   150   Rotate Right (100)   25   100
    //     /  \        ----------------->  /     /  \
    //    25   75                         0     75  150
    //   /
    //  0
    assertEquals(3, tree.getHeight());
    assertEquals(50, tree.root_.value);
    assertEquals(25, tree.root_.left.value);
    assertEquals(0, tree.root_.left.left.value);
    assertEquals(100, tree.root_.right.value);
    assertEquals(75, tree.root_.right.left.value);
    assertEquals(150, tree.root_.right.right.value);

    assertEquals(0, tree.getMinimum());
    assertEquals(150, tree.getMaximum());
    assertEquals(6, tree.getCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddLeftRightCase() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(100);
    tree.add(50);
    tree.add(150);
    tree.add(25);
    tree.add(75);
    tree.add(60);

    //     100                             100                           75
    //     / \                            /   \                         /  \
    //    50   150  Left Rotate (50)     75    150  Right Rotate(100)  50  100
    //   / \        - - - - - - - ->    /           - - - - - - - ->  / \     \
    //  25  75                         50                            25 60 150
    //     /                          / \
    //   60                          25  60

    assertEquals(3, tree.getHeight());
    assertEquals(75, tree.root_.value);
    assertEquals(50, tree.root_.left.value);
    assertEquals(25, tree.root_.left.left.value);
    assertEquals(60, tree.root_.left.right.value);
    assertEquals(100, tree.root_.right.value);
    assertEquals(150, tree.root_.right.right.value);

    assertEquals(25, tree.getMinimum());
    assertEquals(150, tree.getMaximum());
    assertEquals(6, tree.getCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveLeftRightCase() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(100);
    tree.add(50);
    tree.add(150);
    tree.add(25);
    tree.add(75);
    tree.add(200);
    tree.add(60);

    tree.remove(200);

    //     100                             100                           75
    //     / \                            /   \                         /  \
    //    50   150  Left Rotate (50)     75    150  Right Rotate(100)  50  100
    //   / \        - - - - - - - ->    /           - - - - - - - ->  / \     \
    //  25  75                         50                            25 60 150
    //     /                          / \
    //   60                          25  60

    assertEquals(3, tree.getHeight());
    assertEquals(75, tree.root_.value);
    assertEquals(50, tree.root_.left.value);
    assertEquals(25, tree.root_.left.left.value);
    assertEquals(60, tree.root_.left.right.value);
    assertEquals(100, tree.root_.right.value);
    assertEquals(150, tree.root_.right.right.value);

    assertEquals(25, tree.getMinimum());
    assertEquals(150, tree.getMaximum());
    assertEquals(6, tree.getCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddRightRightCase() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(100);
    tree.add(50);
    tree.add(150);
    tree.add(125);
    tree.add(200);
    tree.add(250);

    //   100                             150
    //  /  \                            /   \
    // 50   150     Left Rotate(100)   100   200
    //      / \   - - - - - - - ->    /  \      \
    //     125 200                   50  125    250
    //          \
    //          250

    assertEquals(3, tree.getHeight());
    assertEquals(150, tree.root_.value);
    assertEquals(100, tree.root_.left.value);
    assertEquals(50, tree.root_.left.left.value);
    assertEquals(125, tree.root_.left.right.value);
    assertEquals(200, tree.root_.right.value);
    assertEquals(250, tree.root_.right.right.value);

    assertEquals(50, tree.getMinimum());
    assertEquals(250, tree.getMaximum());
    assertEquals(6, tree.getCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveRightRightCase() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(100);
    tree.add(50);
    tree.add(150);
    tree.add(0);
    tree.add(125);
    tree.add(200);
    tree.add(250);

    tree.remove(0);

    //   100                             150
    //  /  \                            /   \
    // 50   150     Left Rotate(100)   100   200
    //      / \   - - - - - - - ->    /  \      \
    //     125 200                   50  125    250
    //          \
    //          250

    assertEquals(3, tree.getHeight());
    assertEquals(150, tree.root_.value);
    assertEquals(100, tree.root_.left.value);
    assertEquals(50, tree.root_.left.left.value);
    assertEquals(125, tree.root_.left.right.value);
    assertEquals(200, tree.root_.right.value);
    assertEquals(250, tree.root_.right.right.value);

    assertEquals(50, tree.getMinimum());
    assertEquals(250, tree.getMaximum());
    assertEquals(6, tree.getCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddRightLeftCase() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(100);
    tree.add(150);
    tree.add(50);
    tree.add(112);
    tree.add(200);
    tree.add(125);

    //   100                            100                             112
    //   / \                            / \                            /  \
    // 50   150   Right Rotate (150)   50  112      Left Rotate(100)  100  150
    //     /   \   - - - - - - - ->           \     - - - - - - - ->  /   /   \
    //    112  200                            150                    50   125
    //    200
    //     \                                  /  \
    //     125                               125 200

    assertEquals(3, tree.getHeight());
    assertEquals(112, tree.root_.value);
    assertEquals(100, tree.root_.left.value);
    assertEquals(50, tree.root_.left.left.value);
    assertEquals(150, tree.root_.right.value);
    assertEquals(125, tree.root_.right.left.value);
    assertEquals(200, tree.root_.right.right.value);

    assertEquals(50, tree.getMinimum());
    assertEquals(200, tree.getMaximum());
    assertEquals(6, tree.getCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveRightLeftCase() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(100);
    tree.add(150);
    tree.add(50);
    tree.add(0);
    tree.add(112);
    tree.add(200);
    tree.add(125);

    tree.remove(0);

    //   100                            100                             112
    //   / \                            / \                            /  \
    // 50   150   Right Rotate (150)   50  112      Left Rotate(100)  100  150
    //     /   \   - - - - - - - ->           \     - - - - - - - ->  /   /   \
    //    112  200                            150                    50   125
    //    200
    //     \                                  /  \
    //     125                               125 200

    assertEquals(3, tree.getHeight());
    assertEquals(112, tree.root_.value);
    assertEquals(100, tree.root_.left.value);
    assertEquals(50, tree.root_.left.left.value);
    assertEquals(150, tree.root_.right.value);
    assertEquals(125, tree.root_.right.left.value);
    assertEquals(200, tree.root_.right.right.value);

    assertEquals(50, tree.getMinimum());
    assertEquals(200, tree.getMaximum());
    assertEquals(6, tree.getCount());
  },

  testCopyEmptyTree() {
    const tree = new AvlTree().copy();
    assertEquals(0, tree.getCount());
    assertEquals(0, tree.getHeight());
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testCopy() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(100);
    tree.add(150);
    tree.add(50);
    tree.add(0);
    tree.add(112);
    tree.add(200);
    tree.add(125);

    const copy = tree.copy();

    assertEquals(4, copy.getHeight());
    assertEquals(100, copy.root_.value);
    assertEquals(50, copy.root_.left.value);
    assertEquals(0, copy.root_.left.left.value);
    assertEquals(150, copy.root_.right.value);
    assertEquals(112, copy.root_.right.left.value);
    assertEquals(125, copy.root_.right.left.right.value);
    assertEquals(200, copy.root_.right.right.value);

    assertEquals(0, tree.getMinimum());
    assertEquals(200, tree.getMaximum());
    assertEquals(7, tree.getCount());

    assertAvlTree(tree);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testCopyCustomCopyFunction() {
    const tree = new AvlTree((a, b) => a.i - b.i);
    tree.add({i: 100});
    tree.add({i: 150});
    tree.add({i: 50});
    tree.add({i: 0});
    tree.add({i: 112});
    tree.add({i: 200});
    tree.add({i: 125});

    const copy = tree.copy(v => ({i: v.i}));

    assertEquals(4, copy.getHeight());
    assertEquals(100, copy.root_.value.i);
    assertEquals(50, copy.root_.left.value.i);
    assertEquals(0, copy.root_.left.left.value.i);
    assertEquals(150, copy.root_.right.value.i);
    assertEquals(112, copy.root_.right.left.value.i);
    assertEquals(125, copy.root_.right.left.right.value.i);
    assertEquals(200, copy.root_.right.right.value.i);

    assertEquals(0, tree.getMinimum().i);
    assertEquals(200, tree.getMaximum().i);
    assertEquals(7, tree.getCount());

    assertAvlTree(tree);
  },

  testBadCopyFunction() {
    const tree = new AvlTree((a, b) => a - b);
    tree.add(1);
    assertThrows(() => tree.copy(n => n * -1));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testLargeDatasetIsAvlTree() {
    let arr = [];
    for (let i = 0; i < 1000; i++) {
      arr.push(i);
    }
    const comparator = (a, b) => a - b;
    const tree = new AvlTree(comparator);

    while (arr.length) {
      tree.add(arr.splice(Math.floor(Math.random() * arr.length), 1)[0]);
      assertAvlTree(comparator, tree.root_);
    }

    arr = tree.getValues();
    for (let i = 0; i < 1000; i++) {
      assertEquals(i, arr[i]);
      assertEquals(i, tree.indexOf(i));
      assertEquals(i, tree.getKthValue(i));
    }

    while (arr.length) {
      tree.remove(arr.splice(Math.floor(Math.random() * arr.length), 1)[0]);
      assertAvlTree(comparator, tree.root_);
    }
  },
});
