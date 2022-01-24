/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.RangeSetTest');
goog.setTestOnly();

const Range = goog.require('goog.math.Range');
const RangeSet = goog.require('goog.math.RangeSet');
const iter = goog.require('goog.iter');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Produce legible assertion results for comparing ranges. The expected range
 * may be defined as a Range or as a two-element array of numbers. If
 * two ranges are not equal, the error message will be in the format:
 * "Expected <[1, 2]> (Object) but was <[3, 4]> (Object)"
 * @param {!Range|!Array<number>|string} a A descriptive string or the expected
 *     range.
 * @param {!Range|!Array<number>} b The expected range when a descriptive string
 *     is present, or the range to compare.
 * @param {Range=} c The range to compare when a descriptive string is present.
 */
function assertRangesEqual(a, b, c = undefined) {
  const message = c ? a : '';
  let expected = c ? b : a;
  const actual = c ? c : b;

  if (Array.isArray(expected)) {
    assertEquals(
        `${message}
` +
            'Expected ranges must be specified as goog.math.Range ' +
            'objects or as 2-element number arrays. Found [' +
            expected.join(', ') + ']',
        2, expected.length);
    expected = new Range(expected[0], expected[1]);
  }

  if (!Range.equals(
          /** @type {!Range} */ (expected),
          /** @type {!Range} */ (actual))) {
    if (message) {
      assertEquals(message, expected, actual);
    } else {
      assertEquals(expected, actual);
    }
  }
}

/**
 * Produce legible assertion results for comparing two lists of ranges. Expected
 * lists may be specified as a list of goog.math.Ranges, or as a list of
 * two-element arrays of numbers.
 * @param {Array<Range|Array<number>>|string} a A help string or the list of
 *     expected ranges.
 * @param {Array<Range|Array<number>>} b The list of expected ranges when a
 *     descriptive string is present, or the list of ranges to compare.
 * @param {Array<Range>=} c The list of ranges to compare when a descriptive
 *     string is present.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function assertRangeListsEqual(a, b, c = undefined) {
  const message = c ? `${a}
` :
                      '';
  const expected = c ? b : a;
  const actual = c ? c : b;

  assertEquals(
      `${message}Array lengths unequal.`, expected.length, actual.length);

  for (let i = 0; i < expected.length; i++) {
    assertRangesEqual(`${message}Range ${i} mismatch.`, expected[i], actual[i]);
  }
}

testSuite({
  /** @suppress {visibility} suppression added to enable type checking */
  testClone() {
    const r = new RangeSet();

    /** @suppress {checkTypes} suppression added to enable type checking */
    let test = new RangeSet(r);
    assertRangeListsEqual([], test.ranges_);

    r.add(new Range(-10, -2));
    r.add(new Range(2.72, 3.14));
    r.add(new Range(8, 11));

    test = r.clone();
    assertRangeListsEqual([[-10, -2], [2.72, 3.14], [8, 11]], test.ranges_);

    const test2 = r.clone();
    assertRangeListsEqual(test.ranges_, test2.ranges_);

    assertNotEquals(
        'The clones should not share the same list reference.', test.ranges_,
        test2.ranges_);

    for (let i = 0; i < test.ranges_.length; i++) {
      assertNotEquals(
          'The clones should not share references to ranges.', test.ranges_[i],
          test2.ranges_[i]);
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddNoCorruption() {
    const r = new RangeSet();

    const range = new Range(1, 2);
    r.add(range);

    assertNotEquals(
        'Only a copy of the input range should be stored.', range,
        r.ranges_[0]);

    range.end = 5;
    assertRangeListsEqual(
        'Modifying an input range after use should not ' +
            'affect the set.',
        [[1, 2]], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAdd() {
    const r = new RangeSet();

    r.add(new Range(7, 12));
    assertRangeListsEqual([[7, 12]], r.ranges_);

    r.add(new Range(1, 3));
    assertRangeListsEqual([[1, 3], [7, 12]], r.ranges_);

    r.add(new Range(13, 18));
    assertRangeListsEqual([[1, 3], [7, 12], [13, 18]], r.ranges_);

    r.add(new Range(5, 5));
    assertRangeListsEqual(
        'Zero length ranges should be ignored.', [[1, 3], [7, 12], [13, 18]],
        r.ranges_);

    const badRange = new Range(5, 5);
    badRange.end = 4;
    r.add(badRange);
    assertRangeListsEqual(
        'Negative length ranges should be ignored.',
        [[1, 3], [7, 12], [13, 18]], r.ranges_);

    r.add(new Range(-22, -15));
    assertRangeListsEqual(
        'Negative ranges should work fine.',
        [[-22, -15], [1, 3], [7, 12], [13, 18]], r.ranges_);

    r.add(new Range(3.1, 6.9));
    assertRangeListsEqual(
        'Non-integer ranges should work fine.',
        [[-22, -15], [1, 3], [3.1, 6.9], [7, 12], [13, 18]], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddWithOverlaps() {
    const r = new RangeSet();

    r.add(new Range(7, 12));
    r.add(new Range(5, 8));
    assertRangeListsEqual([[5, 12]], r.ranges_);

    r.add(new Range(15, 20));
    r.add(new Range(18, 25));
    assertRangeListsEqual([[5, 12], [15, 25]], r.ranges_);

    r.add(new Range(10, 17));
    assertRangeListsEqual([[5, 25]], r.ranges_);

    r.add(new Range(-4, 4.5));
    assertRangeListsEqual([[-4, 4.5], [5, 25]], r.ranges_);

    r.add(new Range(4.2, 5.3));
    assertRangeListsEqual([[-4, 25]], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddWithAdjacentSpans() {
    const r = new RangeSet();

    r.add(new Range(7, 12));
    r.add(new Range(13, 19));
    assertRangeListsEqual([[7, 12], [13, 19]], r.ranges_);

    r.add(new Range(4, 6));
    assertRangeListsEqual([[4, 6], [7, 12], [13, 19]], r.ranges_);

    r.add(new Range(6, 7));
    assertRangeListsEqual([[4, 12], [13, 19]], r.ranges_);

    r.add(new Range(12, 13));
    assertRangeListsEqual([[4, 19]], r.ranges_);

    r.add(new Range(19.1, 22));
    assertRangeListsEqual([[4, 19], [19.1, 22]], r.ranges_);

    r.add(new Range(19, 19.1));
    assertRangeListsEqual([[4, 22]], r.ranges_);

    r.add(new Range(-3, -2));
    assertRangeListsEqual([[-3, -2], [4, 22]], r.ranges_);

    r.add(new Range(-2, 4));
    assertRangeListsEqual([[-3, 22]], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddWithSubsets() {
    const r = new RangeSet();

    r.add(new Range(7, 12));
    assertRangeListsEqual([[7, 12]], r.ranges_);

    r.add(new Range(7, 12));
    assertRangeListsEqual([[7, 12]], r.ranges_);

    r.add(new Range(8, 11));
    assertRangeListsEqual([[7, 12]], r.ranges_);

    for (let i = 20; i < 30; i += 2) {
      r.add(new Range(i, i + 1));
    }
    assertRangeListsEqual(
        [[7, 12], [20, 21], [22, 23], [24, 25], [26, 27], [28, 29]], r.ranges_);

    r.add(new Range(1, 30));
    assertRangeListsEqual([[1, 30]], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemove() {
    const r = new RangeSet();

    r.add(new Range(1, 3));
    r.add(new Range(7, 8));
    r.add(new Range(10, 20));

    r.remove(new Range(3, 6));
    assertRangeListsEqual([[1, 3], [7, 8], [10, 20]], r.ranges_);

    r.remove(new Range(7, 8));
    assertRangeListsEqual([[1, 3], [10, 20]], r.ranges_);

    r.remove(new Range(1, 3));
    assertRangeListsEqual([[10, 20]], r.ranges_);

    r.remove(new Range(8, 11));
    assertRangeListsEqual([[11, 20]], r.ranges_);

    r.remove(new Range(18, 25));
    assertRangeListsEqual([[11, 18]], r.ranges_);

    r.remove(new Range(15, 16));
    assertRangeListsEqual([[11, 15], [16, 18]], r.ranges_);

    r.remove(new Range(11, 15));
    assertRangeListsEqual([[16, 18]], r.ranges_);

    r.remove(new Range(16, 16));
    assertRangeListsEqual(
        'Empty ranges should be ignored.', [[16, 18]], r.ranges_);

    r.remove(new Range(16, 17));
    assertRangeListsEqual([[17, 18]], r.ranges_);

    r.remove(new Range(17, 18));
    assertRangeListsEqual([], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveWithNonOverlappingRanges() {
    const r = new RangeSet();

    r.add(new Range(10, 20));

    r.remove(new Range(5, 8));
    assertRangeListsEqual(
        'Non-overlapping ranges should be ignored.', [[10, 20]], r.ranges_);

    r.remove(new Range(20, 30));
    assertRangeListsEqual(
        'Non-overlapping ranges should be ignored.', [[10, 20]], r.ranges_);

    r.remove(new Range(15, 15));
    assertRangeListsEqual(
        'Zero-length ranges should be ignored.', [[10, 20]], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveWithIdenticalRanges() {
    const r = new RangeSet();

    r.add(new Range(10, 20));
    r.add(new Range(30, 40));
    r.add(new Range(50, 60));
    assertRangeListsEqual([[10, 20], [30, 40], [50, 60]], r.ranges_);

    r.remove(new Range(30, 40));
    assertRangeListsEqual([[10, 20], [50, 60]], r.ranges_);

    r.remove(new Range(50, 60));
    assertRangeListsEqual([[10, 20]], r.ranges_);

    r.remove(new Range(10, 20));
    assertRangeListsEqual([], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveWithOverlappingSubsets() {
    const r = new RangeSet();

    r.add(new Range(1, 10));

    r.remove(new Range(1, 4));
    assertRangeListsEqual([[4, 10]], r.ranges_);

    r.remove(new Range(8, 10));
    assertRangeListsEqual([[4, 8]], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveMultiple() {
    const r = new RangeSet();

    r.add(new Range(5, 8));
    r.add(new Range(10, 20));
    r.add(new Range(30, 35));

    for (let i = 20; i < 30; i += 2) {
      r.add(new Range(i, i + 1));
    }

    assertRangeListsEqual(
        'Setting up the test data seems to have failed, how embarrassing.',
        [[5, 8], [10, 21], [22, 23], [24, 25], [26, 27], [28, 29], [30, 35]],
        r.ranges_);

    r.remove(new Range(15, 32));
    assertRangeListsEqual([[5, 8], [10, 15], [32, 35]], r.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveWithRealNumbers() {
    const r = new RangeSet();

    r.add(new Range(2, 4));

    r.remove(new Range(1.1, 2.72));
    assertRangeListsEqual([[2.72, 4]], r.ranges_);

    r.remove(new Range(3.14, 5));
    assertRangeListsEqual([[2.72, 3.14]], r.ranges_);

    r.remove(new Range(2.8, 3));
    assertRangeListsEqual([[2.72, 2.8], [3, 3.14]], r.ranges_);
  },

  testEquals() {
    const a = new RangeSet();
    const b = new RangeSet();

    assertTrue(RangeSet.equals(a, b));

    a.add(new Range(3, 9));
    assertFalse(RangeSet.equals(a, b));

    b.add(new Range(4, 9));
    assertFalse(RangeSet.equals(a, b));

    b.add(new Range(3, 4));
    assertTrue(RangeSet.equals(a, b));

    a.add(new Range(12, 14));
    b.add(new Range(11, 14));
    assertFalse(RangeSet.equals(a, b));

    a.add(new Range(11, 12));
    assertTrue(RangeSet.equals(a, b));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testContains() {
    const r = new RangeSet();

    assertFalse(r.contains(7, 9));

    r.add(new Range(5, 6));
    r.add(new Range(10, 20));

    assertFalse(r.contains(new Range(7, 9)));
    assertFalse(r.contains(new Range(9, 11)));
    assertFalse(r.contains(new Range(18, 22)));

    assertTrue(r.contains(new Range(17, 19)));
    assertTrue(r.contains(new Range(5, 6)));

    assertTrue(r.contains(new Range(5.9, 5.999)));

    assertFalse(
        'An empty input range should always return false.',
        r.contains(new Range(15, 15)));

    const badRange = new Range(15, 15);
    badRange.end = 14;
    assertFalse(
        'An invalid range should always return false.', r.contains(badRange));
  },

  testContainsValue() {
    const r = new RangeSet();

    assertFalse(r.containsValue(5));

    r.add(new Range(1, 4));
    r.add(new Range(10, 20));

    assertFalse(r.containsValue(0));
    assertFalse(r.containsValue(0.999));
    assertFalse(r.containsValue(5));
    assertFalse(r.containsValue(25));
    assertFalse(r.containsValue(20));

    assertTrue(r.containsValue(3));
    assertTrue(r.containsValue(10));
    assertTrue(r.containsValue(19));
    assertTrue(r.containsValue(19.999));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUnion() {
    const a = new RangeSet();

    a.add(new Range(1, 5));
    a.add(new Range(10, 11));
    a.add(new Range(15, 20));

    const b = new RangeSet();

    b.add(new Range(0, 5));
    b.add(new Range(8, 18));

    const test1 = a.union(b);
    assertRangeListsEqual([[0, 5], [8, 20]], test1.ranges_);

    const test2 = b.union(a);
    assertRangeListsEqual([[0, 5], [8, 20]], test2.ranges_);

    const test3 = a.union(a);
    assertRangeListsEqual(a.ranges_, test3.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDifference() {
    const a = new RangeSet();

    a.add(new Range(1, 5));
    a.add(new Range(10, 11));
    a.add(new Range(15, 20));

    const b = new RangeSet();

    b.add(new Range(0, 5));
    b.add(new Range(8, 18));

    const test1 = a.difference(b);
    assertRangeListsEqual([[18, 20]], test1.ranges_);

    const test2 = b.difference(a);
    assertRangeListsEqual([[0, 1], [8, 10], [11, 15]], test2.ranges_);

    const test3 = a.difference(a);
    assertRangeListsEqual([], test3.ranges_);

    const test4 = b.difference(b);
    assertRangeListsEqual([], test4.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testIntersection() {
    const a = new RangeSet();

    a.add(new Range(1, 5));
    a.add(new Range(10, 11));
    a.add(new Range(15, 20));

    const b = new RangeSet();

    b.add(new Range(0, 5));
    b.add(new Range(8, 18));

    const test1 = a.intersection(b);
    assertRangeListsEqual([[1, 5], [10, 11], [15, 18]], test1.ranges_);

    const test2 = b.intersection(a);
    assertRangeListsEqual([[1, 5], [10, 11], [15, 18]], test2.ranges_);

    const test3 = a.intersection(a);
    assertRangeListsEqual(a.ranges_, test3.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSlice() {
    const r = new RangeSet();

    r.add(new Range(2, 4));
    r.add(new Range(5, 6));
    r.add(new Range(9, 15));

    let test = r.slice(new Range(0, 2));
    assertRangeListsEqual([], test.ranges_);

    test = r.slice(new Range(2, 4));
    assertRangeListsEqual([[2, 4]], test.ranges_);

    test = r.slice(new Range(7, 20));
    assertRangeListsEqual([[9, 15]], test.ranges_);

    test = r.slice(new Range(4, 30));
    assertRangeListsEqual([[5, 6], [9, 15]], test.ranges_);

    test = r.slice(new Range(2, 15));
    assertRangeListsEqual([[2, 4], [5, 6], [9, 15]], test.ranges_);

    test = r.slice(new Range(10, 10));
    assertRangeListsEqual(
        'An empty range should produce an empty set.', [], test.ranges_);

    const badRange = new Range(10, 10);
    badRange.end = 9;
    test = r.slice(badRange);
    assertRangeListsEqual(
        'An invalid range should produce an empty set.', [], test.ranges_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testInverse() {
    const r = new RangeSet();

    r.add(new Range(1, 3));
    r.add(new Range(5, 6));
    r.add(new Range(8, 10));

    let test = r.inverse(new Range(10, 20));
    assertRangeListsEqual([[10, 20]], test.ranges_);

    test = r.inverse(new Range(1, 3));
    assertRangeListsEqual([], test.ranges_);

    test = r.inverse(new Range(0, 2));
    assertRangeListsEqual([[0, 1]], test.ranges_);

    test = r.inverse(new Range(9, 12));
    assertRangeListsEqual([[10, 12]], test.ranges_);

    test = r.inverse(new Range(2, 9));
    assertRangeListsEqual([[3, 5], [6, 8]], test.ranges_);

    test = r.inverse(new Range(4, 9));
    assertRangeListsEqual([[4, 5], [6, 8]], test.ranges_);

    test = r.inverse(new Range(9, 9));
    assertRangeListsEqual(
        'An empty range should produce an empty set.', [], test.ranges_);

    const badRange = new Range(9, 9);
    badRange.end = 8;
    test = r.inverse(badRange);
    assertRangeListsEqual(
        'An invalid range should produce an empty set.', [], test.ranges_);
  },

  testCoveredLength() {
    const r = new RangeSet();
    assertEquals(0, r.coveredLength());

    r.add(new Range(5, 9));
    assertEquals(4, r.coveredLength());

    r.add(new Range(0, 3));
    r.add(new Range(12, 13));
    assertEquals(8, r.coveredLength());

    r.add(new Range(-1, 13));
    assertEquals(14, r.coveredLength());

    r.add(new Range(13, 13.5));
    assertEquals(14.5, r.coveredLength());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetBounds() {
    const r = new RangeSet();

    assertNull(r.getBounds());

    r.add(new Range(12, 54));
    assertRangesEqual([12, 54], r.getBounds());

    r.add(new Range(108, 139));
    assertRangesEqual([12, 139], r.getBounds());
  },

  testIsEmpty() {
    const r = new RangeSet();
    assertTrue(r.isEmpty());

    r.add(new Range(0, 1));
    assertFalse(r.isEmpty());

    r.remove(new Range(0, 1));
    assertTrue(r.isEmpty());
  },

  testClear() {
    const r = new RangeSet();

    r.add(new Range(1, 2));
    r.add(new Range(3, 5));
    r.add(new Range(8, 13));

    assertFalse(r.isEmpty());

    r.clear();
    assertTrue(r.isEmpty());
  },

  testIter() {
    const r = new RangeSet();

    r.add(new Range(1, 3));
    r.add(new Range(5, 6));
    r.add(new Range(8, 10));

    assertRangeListsEqual([[1, 3], [5, 6], [8, 10]], iter.toArray(r));

    let i = 0;
    iter.forEach(
        r, /**
              @suppress {visibility} suppression added to enable type checking
            */
        (testRange) => {
          assertRangesEqual(
              'Iterated set values should match the originals.', r.ranges_[i],
              testRange);
          assertNotEquals(
              'Iterated range should not be a reference to the original.',
              r.ranges_[i], testRange);
          i++;
        });
  },
});
