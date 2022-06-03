/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.RangeTest');
goog.setTestOnly();

const Range = goog.require('goog.math.Range');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Produce legible assertion results. If two ranges are not equal, the error
 * message will be of the form
 * "Expected <[1, 2]> (Object) but was <[3, 4]> (Object)"
 */
function assertRangesEqual(expected, actual) {
  if (!Range.equals(expected, actual)) {
    assertEquals(expected, actual);
  }
}

function createRange(a) {
  return a ? new Range(a[0], a[1]) : null;
}

testSuite({
  testFromPair() {
    let range = Range.fromPair([1, 2]);
    assertEquals(1, range.start);
    assertEquals(2, range.end);
    range = Range.fromPair([2, 1]);
    assertEquals(1, range.start);
    assertEquals(2, range.end);
  },

  testRangeIntersection() {
    const tests = [
      [[1, 2], [3, 4], null],
      [[1, 3], [2, 4], [2, 3]],
      [[1, 4], [2, 3], [2, 3]],
      [[-1, 2], [-1, 2], [-1, 2]],
      [[1, 2], [2, 3], [2, 2]],
      [[1, 1], [1, 1], [1, 1]],
    ];
    for (let i = 0; i < tests.length; ++i) {
      const t = tests[i];
      const r0 = createRange(t[0]);
      const r1 = createRange(t[1]);
      const expected = createRange(t[2]);
      assertRangesEqual(expected, Range.intersection(r0, r1));
      assertRangesEqual(expected, Range.intersection(r1, r0));

      assertEquals(expected != null, Range.hasIntersection(r0, r1));
      assertEquals(expected != null, Range.hasIntersection(r1, r0));
    }
  },

  testBoundingRange() {
    const tests = [
      [[1, 2], [3, 4], [1, 4]],
      [[1, 3], [2, 4], [1, 4]],
      [[1, 4], [2, 3], [1, 4]],
      [[-1, 2], [-1, 2], [-1, 2]],
      [[1, 2], [2, 3], [1, 3]],
      [[1, 1], [1, 1], [1, 1]],
    ];
    for (let i = 0; i < tests.length; ++i) {
      const t = tests[i];
      const r0 = createRange(t[0]);
      const r1 = createRange(t[1]);
      const expected = createRange(t[2]);
      assertRangesEqual(expected, Range.boundingRange(r0, r1));
      assertRangesEqual(expected, Range.boundingRange(r1, r0));
    }
  },

  testRangeContains() {
    const tests = [
      [[0, 4], [2, 1], true],
      [[-4, -1], [-2, -3], true],
      [[1, 3], [2, 4], false],
      [[-1, 0], [0, 1], false],
      [[0, 2], [3, 5], false],
    ];
    for (let i = 0; i < tests.length; ++i) {
      const t = tests[i];
      const r0 = createRange(t[0]);
      const r1 = createRange(t[1]);
      const expected = t[2];
      assertEquals(expected, Range.contains(r0, r1));
    }
  },

  testRangeClone() {
    const r = new Range(5.6, -3.4);
    assertRangesEqual(r, r.clone());
  },

  testGetLength() {
    assertEquals(2, new Range(1, 3).getLength());
    assertEquals(2, new Range(3, 1).getLength());
  },

  testRangeContainsPoint() {
    const r = new Range(0, 1);
    assert(Range.containsPoint(r, 0));
    assert(Range.containsPoint(r, 1));
    assertFalse(Range.containsPoint(r, -1));
    assertFalse(Range.containsPoint(r, 2));
  },

  testIncludePoint() {
    const r = new Range(0, 2);
    r.includePoint(0);
    assertObjectEquals(new Range(0, 2), r);
    r.includePoint(1);
    assertObjectEquals(new Range(0, 2), r);
    r.includePoint(2);
    assertObjectEquals(new Range(0, 2), r);
    r.includePoint(-1);
    assertObjectEquals(new Range(-1, 2), r);
    r.includePoint(3);
    assertObjectEquals(new Range(-1, 3), r);
  },

  testIncludeRange() {
    const r = new Range(0, 4);
    r.includeRange(r);
    assertObjectEquals(new Range(0, 4), r);
    r.includeRange(new Range(1, 3));
    assertObjectEquals(new Range(0, 4), r);
    r.includeRange(new Range(-1, 2));
    assertObjectEquals(new Range(-1, 4), r);
    r.includeRange(new Range(2, 5));
    assertObjectEquals(new Range(-1, 5), r);
    r.includeRange(new Range(-2, 6));
    assertObjectEquals(new Range(-2, 6), r);
  },
});
