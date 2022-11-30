/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.RectTest');
goog.setTestOnly();

const Box = goog.require('goog.math.Box');
const Coordinate = goog.require('goog.math.Coordinate');
const GoogRect = goog.require('goog.math.Rect');
const Size = goog.require('goog.math.Size');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Produce legible assertion results. If two rects are not equal, the error
 * message will be of the form
 * "Expected <(1, 2 - 10 x 10)> (Object) but was <(3, 4 - 20 x 20)> (Object)"
 */
function assertRectsEqual(expected, actual) {
  if (!GoogRect.equals(expected, actual)) {
    assertEquals(expected, actual);
  }
}

/**
 * Create a GoogRect with given coordinates.
 * @param {Array<number>} a Array of numbers with given coordinates for rect,
 *     with expected order [x1, y1, x2, y2].
 * @return {?GoogRect} A rectangle, if coordinates were given.
 */
function createRect(a) {
  return a ? new GoogRect(a[0], a[1], a[2] - a[0], a[3] - a[1]) : null;
}

/**
 * Create a rect like object.
 * @param {Array<number>} a Array of numbers with given coordinates for rect,
 *     with expected order [x1, y1, x2, y2].
 * @return {?{left: number, top: number, width: number, height: number}} A rect
 *     like object.
 */
function createIRect(a) {
  if (a) {
    return {left: a[0], top: a[1], width: a[2] - a[0], height: a[3] - a[1]};
  }
  return null;
}

function assertDifference(a, b, expected) {
  const r0 = createRect(a);
  const r1 = createRect(b);
  const diff1 = GoogRect.difference(r0, r1);

  assertEquals(
      'Wrong number of rectangles in difference ', expected.length,
      diff1.length);

  for (let j = 0; j < expected.length; ++j) {
    const e = createRect(expected[j]);
    if (!GoogRect.equals(e, diff1[j])) {
      alert(`${j}: ${e} != ` + diff1[j]);
    }
    assertRectsEqual(e, diff1[j]);
  }

  // Test in place version
  const diff2 = r0.difference(r1);

  assertEquals(
      'Wrong number of rectangles in in-place difference ', expected.length,
      diff2.length);

  for (let j = 0; j < expected.length; ++j) {
    const e = createRect(expected[j]);
    if (!GoogRect.equals(e, diff2[j])) {
      alert(`${j}: ${e} != ` + diff2[j]);
    }
    assertRectsEqual(e, diff2[j]);
  }
}

function rectToBoxAndBackTest(rect) {
  const box = rect.toBox();
  const rect2 = GoogRect.createFromBox(box);

  // Use toString for this test since otherwise NaN != NaN.
  assertObjectEquals(rect.toString(), rect2.toString());
}

function boxToRectAndBackTest(box) {
  const rect = GoogRect.createFromBox(box);
  const box2 = rect.toBox();

  // Use toString for this test since otherwise NaN != NaN.
  assertEquals(box.toString(), box2.toString());
}

testSuite({
  testRectClone() {
    const r = new GoogRect(0, 0, 0, 0);
    assertRectsEqual(r, r.clone());
    r.left = -10;
    r.top = -20;
    r.width = 10;
    r.height = 20;
    assertRectsEqual(r, r.clone());
  },

  testRectIntersection() {
    const tests = [
      [[10, 10, 20, 20], [15, 15, 25, 25], [15, 15, 20, 20]],
      [[10, 10, 20, 20], [20, 0, 30, 10], [20, 10, 20, 10]],
      [[0, 0, 1, 1], [10, 11, 12, 13], null],
      [[11, 12, 98, 99], [22, 23, 34, 35], [22, 23, 34, 35]],
    ];

    const intersectTest = (r0, r1, expected) => {
      assertRectsEqual(expected, GoogRect.intersection(r0, r1));
      assertRectsEqual(expected, GoogRect.intersection(r1, r0));

      // Test in place methods.
      const clone = r0.clone();

      assertRectsEqual(expected, clone.intersection(r1) ? clone : null);

      if (r1.intersection) {
        assertRectsEqual(expected, r1.intersection(r0) ? r1 : null);
      }
    };

    for (let i = 0; i < tests.length; ++i) {
      const t = tests[i];
      const r0 = createRect(t[0]);
      const r1 = createRect(t[1]);

      const expected = createRect(t[2]);

      intersectTest(r0, r1, expected);
    }

    // Run same tests with IRects.

    for (let i = 0; i < tests.length; ++i) {
      const t = tests[i];
      const r0 = createRect(t[0]);
      const r1 = createIRect(t[1]);

      const expected = createRect(t[2]);

      intersectTest(r0, r1, expected);
    }
  },

  testRectIntersects() {
    const r0 = createRect([10, 10, 20, 20]);
    const r1 = createRect([15, 15, 25, 25]);
    const r2 = createRect([0, 0, 1, 1]);
    const ri0 = createIRect([10, 10, 20, 20]);
    const ri1 = createIRect([15, 15, 25, 25]);
    const ri2 = createIRect([0, 0, 1, 1]);

    assertTrue(GoogRect.intersects(r0, r1));
    assertTrue(GoogRect.intersects(r1, r0));
    assertTrue(r0.intersects(r1));
    assertTrue(r1.intersects(r0));

    assertTrue(GoogRect.intersects(r0, ri1));
    assertTrue(GoogRect.intersects(ri0, ri1));
    assertTrue(GoogRect.intersects(ri1, r0));
    assertTrue(r0.intersects(ri1));

    assertFalse(GoogRect.intersects(r0, r2));
    assertFalse(GoogRect.intersects(r2, r0));
    assertFalse(r0.intersects(r2));
    assertFalse(r2.intersects(r0));

    assertFalse(GoogRect.intersects(ri0, ri2));
    assertFalse(GoogRect.intersects(ri2, ri0));
    assertFalse(r0.intersects(ri2));
    assertFalse(r2.intersects(ri0));
  },

  testRectBoundingRect() {
    const tests = [
      [[10, 10, 20, 20], [15, 15, 25, 25], [10, 10, 25, 25]],
      [[10, 10, 20, 20], [20, 0, 30, 10], [10, 0, 30, 20]],
      [[0, 0, 1, 1], [10, 11, 12, 13], [0, 0, 12, 13]],
      [[11, 12, 98, 99], [22, 23, 34, 35], [11, 12, 98, 99]],
    ];
    for (let i = 0; i < tests.length; ++i) {
      const t = tests[i];
      const r0 = createRect(t[0]);
      const r1 = createRect(t[1]);
      const expected = createRect(t[2]);
      assertRectsEqual(expected, GoogRect.boundingRect(r0, r1));
      assertRectsEqual(expected, GoogRect.boundingRect(r1, r0));

      // Test in place methods.
      const clone = r0.clone();

      clone.boundingRect(r1);
      assertRectsEqual(expected, clone);

      r1.boundingRect(r0);
      assertRectsEqual(expected, r1);
    }
  },

  testRectDifference() {
    // B is the same as A.
    assertDifference([10, 10, 20, 20], [10, 10, 20, 20], []);
    // B does not touch A.
    assertDifference([10, 10, 20, 20], [0, 0, 5, 5], [[10, 10, 20, 20]]);
    // B overlaps top half of A.
    assertDifference([10, 10, 20, 20], [5, 15, 25, 25], [[10, 10, 20, 15]]);
    // B overlaps bottom half of A.
    assertDifference([10, 10, 20, 20], [5, 5, 25, 15], [[10, 15, 20, 20]]);
    // B overlaps right half of A.
    assertDifference([10, 10, 20, 20], [15, 5, 25, 25], [[10, 10, 15, 20]]);
    // B overlaps left half of A.
    assertDifference([10, 10, 20, 20], [5, 5, 15, 25], [[15, 10, 20, 20]]);
    // B touches A at its bottom right corner
    assertDifference([10, 10, 20, 20], [20, 20, 30, 30], [[10, 10, 20, 20]]);
    // B touches A at its top left corner
    assertDifference([10, 10, 20, 20], [5, 5, 10, 10], [[10, 10, 20, 20]]);
    // B touches A along its bottom edge
    assertDifference([10, 10, 20, 20], [12, 20, 17, 25], [[10, 10, 20, 20]]);
    // B splits A horizontally.
    assertDifference(
        [10, 10, 20, 20], [5, 12, 25, 18],
        [[10, 10, 20, 12], [10, 18, 20, 20]]);
    // B splits A vertically.
    assertDifference(
        [10, 10, 20, 20], [12, 5, 18, 25],
        [[10, 10, 12, 20], [18, 10, 20, 20]]);
    // B subtracts a notch from the top of A.
    assertDifference(
        [10, 10, 20, 20], [12, 5, 18, 15],
        [[10, 15, 20, 20], [10, 10, 12, 15], [18, 10, 20, 15]]);
    // B subtracts a notch from the bottom left of A
    assertDifference([1, 6, 3, 9], [1, 7, 2, 9], [[1, 6, 3, 7], [2, 7, 3, 9]]);
    // B subtracts a notch from the bottom right of A
    assertDifference([1, 6, 3, 9], [2, 7, 3, 9], [[1, 6, 3, 7], [1, 7, 2, 9]]);
    // B subtracts a notch from the top left of A
    assertDifference([1, 6, 3, 9], [1, 6, 2, 8], [[1, 8, 3, 9], [2, 6, 3, 8]]);
    // B subtracts a notch from the top left of A (no coinciding edge)
    assertDifference([1, 6, 3, 9], [0, 5, 2, 8], [[1, 8, 3, 9], [2, 6, 3, 8]]);
    // B subtracts a hole from the center of A.
    assertDifference([-20, -20, -10, -10], [-18, -18, -12, -12], [
      [-20, -20, -10, -18],
      [-20, -12, -10, -10],
      [-20, -18, -18, -12],
      [-12, -18, -10, -12],
    ]);
  },

  testRectToBox() {
    const r = new GoogRect(0, 0, 0, 0);
    assertObjectEquals(new Box(0, 0, 0, 0), r.toBox());

    r.top = 10;
    r.left = 10;
    r.width = 20;
    r.height = 20;
    assertObjectEquals(new Box(10, 30, 30, 10), r.toBox());

    r.top = -10;
    r.left = 0;
    r.width = 10;
    r.height = 10;
    assertObjectEquals(new Box(-10, 10, 0, 0), r.toBox());
  },

  testBoxToRect() {
    const box = new Box(0, 0, 0, 0);
    assertObjectEquals(new GoogRect(0, 0, 0, 0), GoogRect.createFromBox(box));

    box.top = 10;
    box.left = 15;
    box.right = 23;
    box.bottom = 27;
    assertObjectEquals(
        new GoogRect(15, 10, 8, 17), GoogRect.createFromBox(box));

    box.top = -10;
    box.left = 3;
    box.right = 12;
    box.bottom = 7;
    assertObjectEquals(
        new GoogRect(3, -10, 9, 17), GoogRect.createFromBox(box));
  },

  testBoxToRectAndBack() {
    rectToBoxAndBackTest(new GoogRect(8, 11, 20, 23));
    rectToBoxAndBackTest(new GoogRect(9, 13, NaN, NaN));
    rectToBoxAndBackTest(new GoogRect(10, 13, NaN, 21));
    rectToBoxAndBackTest(new GoogRect(5, 7, 14, NaN));
  },

  testRectToBoxAndBack() {
    // This doesn't work if left or top is undefined.
    boxToRectAndBackTest(new Box(11, 13, 20, 17));
    boxToRectAndBackTest(new Box(10, NaN, NaN, 11));
    boxToRectAndBackTest(new Box(9, 14, NaN, 11));
    boxToRectAndBackTest(new Box(10, NaN, 22, 15));
  },

  testRectContainsRect() {
    const r = new GoogRect(-10, 0, 20, 10);
    assertTrue(r.contains(r));
    assertFalse(r.contains(new GoogRect(NaN, NaN, NaN, NaN)));
    const r2 = new GoogRect(0, 2, 5, 5);
    assertTrue(r.contains(r2));
    assertFalse(r2.contains(r));
    r2.left = -11;
    assertFalse(r.contains(r2));
    r2.left = 0;
    r2.width = 15;
    assertFalse(r.contains(r2));
    r2.width = 5;
    r2.height = 10;
    assertFalse(r.contains(r2));
    r2.top = 0;
    assertTrue(r.contains(r2));
  },

  testRectContainsCoordinate() {
    const r = new GoogRect(20, 40, 60, 80);

    // Test middle.
    assertTrue(r.contains(new Coordinate(50, 80)));

    // Test edges.
    assertTrue(r.contains(new Coordinate(20, 40)));
    assertTrue(r.contains(new Coordinate(50, 40)));
    assertTrue(r.contains(new Coordinate(80, 40)));
    assertTrue(r.contains(new Coordinate(80, 80)));
    assertTrue(r.contains(new Coordinate(80, 120)));
    assertTrue(r.contains(new Coordinate(50, 120)));
    assertTrue(r.contains(new Coordinate(20, 120)));
    assertTrue(r.contains(new Coordinate(20, 80)));

    // Test outside.
    assertFalse(r.contains(new Coordinate(0, 0)));
    assertFalse(r.contains(new Coordinate(50, 0)));
    assertFalse(r.contains(new Coordinate(100, 0)));
    assertFalse(r.contains(new Coordinate(100, 80)));
    assertFalse(r.contains(new Coordinate(100, 160)));
    assertFalse(r.contains(new Coordinate(50, 160)));
    assertFalse(r.contains(new Coordinate(0, 160)));
    assertFalse(r.contains(new Coordinate(0, 80)));
  },

  testGetSize() {
    assertObjectEquals(
        new Size(60, 80), new GoogRect(20, 40, 60, 80).getSize());
  },

  testGetBottomRight() {
    assertObjectEquals(
        new Coordinate(40, 60), new GoogRect(10, 20, 30, 40).getBottomRight());
  },

  testGetCenter() {
    assertObjectEquals(
        new Coordinate(25, 40), new GoogRect(10, 20, 30, 40).getCenter());
  },

  testGetTopLeft() {
    assertObjectEquals(
        new Coordinate(10, 20), new GoogRect(10, 20, 30, 40).getTopLeft());
  },

  testRectCeil() {
    const rect = new GoogRect(11.4, 26.6, 17.8, 9.2);
    assertEquals(
        'The function should return the target instance', rect, rect.ceil());
    assertRectsEqual(new GoogRect(12, 27, 18, 10), rect);
  },

  testRectFloor() {
    const rect = new GoogRect(11.4, 26.6, 17.8, 9.2);
    assertEquals(
        'The function should return the target instance', rect, rect.floor());
    assertRectsEqual(new GoogRect(11, 26, 17, 9), rect);
  },

  testRectRound() {
    const rect = new GoogRect(11.4, 26.6, 17.8, 9.2);
    assertEquals(
        'The function should return the target instance', rect, rect.round());
    assertRectsEqual(new GoogRect(11, 27, 18, 9), rect);
  },

  testRectTranslateCoordinate() {
    const rect = new GoogRect(10, 40, 30, 20);
    const c = new Coordinate(10, 5);
    assertEquals(
        'The function should return the target instance', rect,
        rect.translate(c));
    assertRectsEqual(new GoogRect(20, 45, 30, 20), rect);
  },

  testRectTranslateXY() {
    const rect = new GoogRect(10, 20, 40, 35);
    assertEquals(
        'The function should return the target instance', rect,
        rect.translate(15, 10));
    assertRectsEqual(new GoogRect(25, 30, 40, 35), rect);
  },

  testRectTranslateX() {
    const rect = new GoogRect(12, 34, 113, 88);
    assertEquals(
        'The function should return the target instance', rect,
        rect.translate(10));
    assertRectsEqual(new GoogRect(22, 34, 113, 88), rect);
  },

  testRectScaleXY() {
    const rect = new GoogRect(10, 30, 100, 60);
    assertEquals(
        'The function should return the target instance', rect,
        rect.scale(2, 5));
    assertRectsEqual(new GoogRect(20, 150, 200, 300), rect);
  },

  testRectScaleFactor() {
    const rect = new GoogRect(12, 34, 113, 88);
    assertEquals(
        'The function should return the target instance', rect, rect.scale(10));
    assertRectsEqual(new GoogRect(120, 340, 1130, 880), rect);
  },

  testSquaredDistance() {
    const rect = new GoogRect(-10, -20, 15, 25);

    // Test regions:
    // 1  2  3
    //   +-+
    // 4 |5| 6
    //   +-+
    // 7  8  9

    // Region 5 (inside the rectangle).
    assertEquals(0, rect.squaredDistance(new Coordinate(-10, 5)));
    assertEquals(0, rect.squaredDistance(new Coordinate(5, -20)));

    // 1, 2, and 3.
    assertEquals(25, rect.squaredDistance(new Coordinate(9, 8)));
    assertEquals(36, rect.squaredDistance(new Coordinate(2, 11)));
    assertEquals(53, rect.squaredDistance(new Coordinate(12, 7)));

    // 4 and 6.
    assertEquals(81, rect.squaredDistance(new Coordinate(-19, -10)));
    assertEquals(64, rect.squaredDistance(new Coordinate(13, 0)));

    // 7, 8, and 9.
    assertEquals(20, rect.squaredDistance(new Coordinate(-12, -24)));
    assertEquals(9, rect.squaredDistance(new Coordinate(0, -23)));
    assertEquals(34, rect.squaredDistance(new Coordinate(8, -25)));
  },

  testDistance() {
    const rect = new GoogRect(2, 4, 8, 16);

    // Region 5 (inside the rectangle).
    assertEquals(0, rect.distance(new Coordinate(2, 4)));
    assertEquals(0, rect.distance(new Coordinate(10, 20)));

    // 1, 2, and 3.
    assertRoughlyEquals(
        Math.sqrt(8), rect.distance(new Coordinate(0, 22)), .0001);
    assertEquals(8, rect.distance(new Coordinate(9, 28)));
    assertRoughlyEquals(
        Math.sqrt(50), rect.distance(new Coordinate(15, 25)), .0001);

    // 4 and 6.
    assertEquals(7, rect.distance(new Coordinate(-5, 6)));
    assertEquals(10, rect.distance(new Coordinate(20, 10)));

    // 7, 8, and 9.
    assertEquals(5, rect.distance(new Coordinate(-2, 1)));
    assertEquals(2, rect.distance(new Coordinate(5, 2)));
    assertRoughlyEquals(
        Math.sqrt(10), rect.distance(new Coordinate(1, 1)), .0001);
  },
});
